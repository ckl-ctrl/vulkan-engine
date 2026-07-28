# Rendergraph 说明文档

> 自动依赖管理：你只需声明资源和通道，Rendergraph 自动处理同步、资源分配和执行顺序。

---

## 1. 总体架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Rendergraph                            │
│                                                             │
│  ┌──────────┐    ┌─────────────┐    ┌───────────────────┐  │
│  │ Resource │    │     Pass     │    │  执行引擎 (GPU)   │  │
│  │  资源表  │───▶│   渲染通道   │───▶│ Execute / Submit │  │
│  └──────────┘    └─────────────┘    └───────────────────┘  │
│       │               │                      │             │
│       ▼               ▼                      ▼             │
│  AddResource     AddPass               vkQueueSubmit       │
│       │               │                      │             │
│       ▼               ▼                      ▼             │
│  Compile() ←──────────────────────────── Execute()         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**三个核心概念：**

| 概念 | 作用 | 生命周期 |
|------|------|----------|
| `Resource` | GPU 图像资源（纹理、附件） | `AddResource` 声明 → `Compile` 创建 |
| `Pass` | 一次渲染操作（深度、延迟、后处理） | `AddPass` 声明 → `Execute` 执行 |
| `Semaphore` | GPU 同步原语，自动生成 | `Compile` 创建 → `Execute` 使用 |

---

## 2. Resource（资源）

### 2.1 数据模型

```
 Resource
┌──────────────────────────────────────┐
│  元数据（AddResource 时填入）          │
│  ┌──────────────┬─────────────────┐  │
│  │ name         │ "GBufferColor"  │  │
│  │ format       │ RGBA8_UNORM     │  │
│  │ extent       │ 1920 x 1080     │  │
│  │ usage        │ COLOR | SAMPLED │  │
│  │ initialLayout│ eUndefined      │  │
│  │ finalLayout  │ eShaderReadOnly │  │
│  └──────────────┴─────────────────┘  │
│                                      │
│  GPU 对象（Compile 时创建）           │
│  ┌──────────────┬─────────────────┐  │
│  │ image        │ vkImage 句柄    │  │
│  │ memory       │ 显存分配         │  │
│  │ view         │ ImageView 句柄  │  │
│  └──────────────┴─────────────────┘  │
└──────────────────────────────────────┘
```

### 2.2 布局状态机

资源在 Pass 执行过程中经历三次布局转换：

```
 initialLayout ──(输入屏障)──▶  eShaderReadOnlyOptimal  ──(执行)──▶  finalLayout
  │                               ▲                                     │
  │   也可直接转写入               │                                     │
  └─(输出屏障)──▶ eColorAttachmentOptimal ──(执行)──▶  finalLayout ─────┘
```

---

## 3. Pass（渲染通道）

### 3.1 结构

```
 Pass "DeferredLighting"
┌─────────────────────────────────────┐
│  inputs:  [ "GBufferColor",        │ ◀── 读取依赖
│             "GBufferNormal",        │
│             "GBufferDepth"  ]       │
│                                     │
│  outputs: [ "LightingResult" ]      │ ◀── 写入目标
│                                     │
│  executeFunc: lambda(commandBuffer) │ ◀── 实际渲染代码
└─────────────────────────────────────┘
```

### 3.2 依赖图示例

假设有三个 Pass：

```
          Pass 0                  Pass 1                    Pass 2
      "GBufferPass"         "SSAOPass"             "LightingPass"
    ┌──────────────┐      ┌──────────────┐        ┌──────────────┐
    │ inputs: []   │      │ input:       │        │ inputs:      │
    │              │      │  GBufferDepth│        │  GBufferColor│
    │ outputs:     │─────▶│  GBufferNorm │───────▶│  GBufferNorm │
    │  GBufferColor│      │              │        │  SSAOResult  │
    │  GBufferNorm │      │ outputs:     │        │              │
    │  GBufferDepth│      │  SSAOResult  │        │ outputs:     │
    └──────────────┘      └──────────────┘        │  FinalImage  │
                                                   └──────────────┘
         │                     │                         │
         ▼                     ▼                         ▼
    写入 Color/Norm/Depth   写入 SSAOResult          写入 FinalImage
```

**依赖关系：**

```
 dependencies[1] = {0}   ← SSAOPass 依赖 GBufferPass 的 GBufferDepth/Normal
 dependencies[2] = {0,1} ← LightingPass 依赖 GBufferPass 和 SSAOPass
```

**拓扑排序结果：** `executionOrder = [2, 1, 0]`（逆序存入，即执行顺序 0→1→2）

---

## 4. Compile() —— 编译流程

```
 Compile()
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  ① 依赖发现                                                       │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  遍历所有 Pass 的 inputs/outputs                            │  │
│  │  Pass A 输出 "X", Pass B 输入 "X"  →  B 依赖 A             │  │
│  │  构建 dependencies[] / dependents[]                        │  │
│  └────────────────────────────────────────────────────────────┘  │
│                           │                                      │
│                           ▼                                      │
│  ② 拓扑排序 (DFS)                                                │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  按 dependents 关系后序遍历                                 │  │
│  │  生产者在前，消费者在后                                      │  │
│  │  检测循环依赖 (inStack[]) → 发现环则抛异常                   │  │
│  └────────────────────────────────────────────────────────────┘  │
│                           │                                      │
│                           ▼                                      │
│  ③ 信号量生成                                                     │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  为每对 (生产者, 消费者) 创建一个 vkSemaphore               │  │
│  │  存入 semaphores[] 和 semaphoreSignalWaitPairs[]            │  │
│  └────────────────────────────────────────────────────────────┘  │
│                           │                                      │
│                           ▼                                      │
│  ④ 物理资源创建                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  遍历 resources 表                                          │  │
│  │  CreateImage → AllocateMemory → BindMemory → CreateView    │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 5. Execute() —— 执行流程

```
 Execute(commandBuffer, queue)
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  for each passIdx in executionOrder:                             │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                                                             │  │
│  │  ① 收集同步依赖                                              │  │
│  │  ┌─────────────────────────────────────────────────────┐   │  │
│  │  │  遍历 semaphoreSignalWaitPairs                       │   │  │
│  │  │  本 Pass 需要 Wait 的信号量 → waitSemaphores[]       │   │  │
│  │  │  本 Pass 需要 Signal 的信号量 → signalSemaphores[]   │   │  │
│  │  └─────────────────────────────────────────────────────┘   │  │
│  │                         │                                    │  │
│  │                         ▼                                    │  │
│  │  ② 布局转换 (输入资源)                                       │  │
│  │  ┌─────────────────────────────────────────────────────┐   │  │
│  │  │  对每个 input:                                        │   │  │
│  │  │    initialLayout → eShaderReadOnlyOptimal             │   │  │
│  │  │    插入 pipelineBarrier (写→读屏障)                   │   │  │
│  │  └─────────────────────────────────────────────────────┘   │  │
│  │                         │                                    │  │
│  │                         ▼                                    │  │
│  │  ③ 布局转换 (输出资源)                                       │  │
│  │  ┌─────────────────────────────────────────────────────┐   │  │
│  │  │  对每个 output:                                       │   │  │
│  │  │    initialLayout → eColorAttachmentOptimal            │   │  │
│  │  │    插入 pipelineBarrier (读→写屏障)                   │   │  │
│  │  └─────────────────────────────────────────────────────┘   │  │
│  │                         │                                    │  │
│  │                         ▼                                    │  │
│  │  ④ 执行渲染逻辑                                               │  │
│  │  ┌─────────────────────────────────────────────────────┐   │  │
│  │  │  pass.executeFunc(commandBuffer)                     │   │  │
│  │  └─────────────────────────────────────────────────────┘   │  │
│  │                         │                                    │  │
│  │                         ▼                                    │  │
│  │  ⑤ 最终布局转换                                              │  │
│  │  ┌─────────────────────────────────────────────────────┐   │  │
│  │  │  对每个 output:                                       │   │  │
│  │  │    eColorAttachmentOptimal → finalLayout              │   │  │
│  │  │    插入 pipelineBarrier (写→读屏障)                   │   │  │
│  │  └─────────────────────────────────────────────────────┘   │  │
│  │                         │                                    │  │
│  │                         ▼                                    │  │
│  │  ⑥ vkQueueSubmit (带信号量)                                  │  │
│  │  ┌─────────────────────────────────────────────────────┐   │  │
│  │  │  Wait:  waitSemaphores    @ ColorAttachmentOutput    │   │  │
│  │  │  Exec:  commandBuffer                                │   │  │
│  │  │  Signal: signalSemaphores                            │   │  │
│  │  └─────────────────────────────────────────────────────┘   │  │
│  │                                                             │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 5.1 单 Pass 内的布局转换时序

```
时间 ──────────────────────────────────────────────────────────▶

Pipeline Stage
    │
    ├─ ALL_COMMANDS ─── (屏障: 输入资源布局转换) 
    │                    initialLayout → ShaderReadOnly
    │
    ├─ FRAGMENT_SHADER ─── pass.executeFunc(commandBuffer)
    │                       Pass 在此阶段执行渲染
    │
    ├─ COLOR_ATTACHMENT ─── (屏障: 输出资源布局转换) 
    │                       initialLayout → ColorAttachmentOptimal
    │
    ├─ COLOR_ATTACHMENT ─── pass.executeFunc(commandBuffer)
    │                       着色器写入颜色附件
    │
    ├─ COLOR_ATTACHMENT ─── (屏障: 最终布局转换)
    │                       ColorAttachmentOptimal → finalLayout
    │
    └─ ALL_COMMANDS ─── 后续 Pass 可安全读取
```

---

## 6. 信号量同步机制

### 6.1 信号量配对

```
  semaphoreSignalWaitPairs = [(0,1), (0,2), (1,2)]
       ▲                         ▲  ▲
       │                         │  └── Pass2 等待 Pass1
       │                         └───── Pass2 等待 Pass0
       └─────────────────────────────── Pass1 等待 Pass0

  Pass0 执行 → Signal sem[0]          ┐
                                     ├── Wait sem[0] → Pass1 执行 → Signal sem[1]
                                     │                                 │
                                     └── Wait sem[1] ──────────────────┘
                                                                        │
                                                              Wait sem[1] → Pass2 执行
                                                                        │
                                      Wait sem[0] ───────────────────────┘
```

### 6.2 提交时序图

```
GPU Timeline
══════════════════════════════════════════════════════════════▶

Queue
  │
  ├─ Submit(Pass0) ── Pass0 执行 ── Signal S0, Signal S1
  │
  │         ┌── Wait S0 ── Pass1 执行 ── Signal S2 ──┐
  │         │                                         │
  ├─ Submit(Pass1)                                    │
  │                                        ┌── Wait S1 ──┐
  │                                        │  Wait S2 ──│── Pass2 执行
  │                                        │             │
  └─ Submit(Pass2) ───────────────────────┘─────────────┘
```

---

## 7. 典型使用示例

```cpp
// 1. 创建 Rendergraph
vk::raii::Device device = /* ... */;
Rendergraph graph(device);

// 2. 声明资源（只声明元数据，不创建 GPU 对象）
graph.AddResource("GBufferColor",  vk::Format::eR8G8B8A8Unorm,
                  {1920, 1080},
                  vk::ImageUsageFlagBits::eColorAttachment |
                  vk::ImageUsageFlagBits::eSampled,
                  vk::ImageLayout::eUndefined,
                  vk::ImageLayout::eShaderReadOnlyOptimal);

graph.AddResource("GBufferDepth",  vk::Format::eD32Sfloat,
                  {1920, 1080},
                  vk::ImageUsageFlagBits::eDepthStencilAttachment,
                  vk::ImageLayout::eUndefined,
                  vk::ImageLayout::eShaderReadOnlyOptimal);

graph.AddResource("FinalImage",    vk::Format::eR8G8B8A8Unorm,
                  {1920, 1080},
                  vk::ImageUsageFlagBits::eColorAttachment |
                  vk::ImageUsageFlagBits::eSampled,
                  vk::ImageLayout::eUndefined,
                  vk::ImageLayout::eShaderReadOnlyOptimal);

// 3. 声明 Pass（声明依赖关系，不执行）
graph.AddPass("GBufferPass",
    {},                                          // inputs: 无
    {"GBufferColor", "GBufferDepth"},            // outputs: 产物
    [&](vk::raii::CommandBuffer& cmd) {
        // 绘制 GBuffer 的渲染代码
        // cmd.bindPipeline(...); cmd.draw(...);
    });

graph.AddPass("LightingPass",
    {"GBufferColor", "GBufferDepth"},            // inputs: 依赖 GBufferPass
    {"FinalImage"},                               // outputs: 最终图像
    [&](vk::raii::CommandBuffer& cmd) {
        // 延迟光照的渲染代码
        // cmd.bindPipeline(...); cmd.draw(...);
    });

// 4. 编译：分析依赖 → 创建 GPU 资源 → 生成信号量
graph.Compile();

// 5. 每帧执行：自动同步 + 自动布局转换
vk::raii::CommandBuffer cmd = /* ... */;
vk::Queue queue = /* ... */;
graph.Execute(cmd, queue);
```

---

## 8. 数据流全景图

```
              ┌──── AddResource("A", ...)    ← 声明阶段
              │  AddResource("B", ...)
              │  AddResource("C", ...)
              │
              │  AddPass("P0", {},    {"A","B"}, ...)  ← 声明阶段
              │  AddPass("P1", {"A"}, {"C"},     ...)
              │  AddPass("P2", {"B","C"}, {},    ...)
              │
              ▼
┌────── Compile() ───────────────────────────────────────┐
│                                                        │
│  【依赖图】                                              │
│                                                        │
│      P0 ──────── A ──────▶ P1 ──────── C ────────▶ P2 │
│       │                     │          ▲              │
│       └─────── B ──────────┼──────────┘              │
│                             └── B 也输入到 P2 ────────┘
│                                                        │
│  执行顺序: P0 → P1 → P2                                │
│  信号量: S0(P0→P1 A), S1(P0→P2 B), S2(P1→P2 C)       │
│                                                        │
│  【资源创建】 P0 output: A,B → P1 output: C            │
│              createImage → allocMemory → createView   │
└────────────────────────────────────────────────────────┘
              │
              ▼
┌────── Execute() (每帧调用) ───────────────────────────┐
│                                                        │
│  Frame N:                                              │
│                                                        │
│  Pass0: barrier(A:?→RO) barrier(B:?→CA)               │
│          execute(P0)   barrier(A:CA→final)             │
│          barrier(B:CA→final)  Submit(Wait:∅ Signal:S0,S1) │
│                                                        │
│  Pass1: barrier(A:?→RO) barrier(C:?→CA)               │
│          execute(P1)   barrier(C:CA→final)             │
│          Submit(Wait:S0 Signal:S2)                     │
│                                                        │
│  Pass2: barrier(B:?→RO) barrier(C:?→RO)               │
│          execute(P2)                                   │
│          Submit(Wait:S1,S2 Signal:∅)                   │
│                                                        │
└────────────────────────────────────────────────────────┘
```

---

## 9. API 速查表

| 方法 | 阶段 | 说明 |
|------|------|------|
| `AddResource(name, fmt, ext, usage, init, final)` | 声明 | 注册一个 GPU 图像资源 |
| `AddPass(name, inputs, outputs, func)` | 声明 | 注册一个渲染通道 |
| `Compile()` | 编译 | 依赖分析 + 资源创建 + 信号量生成 |
| `GetResource(name) → Resource*` | 运行时 | 获取已创建资源的指针（含 image/view） |
| `Execute(cmdBuf, queue)` | 每帧执行 | 按序执行所有 Pass，自动同步 |

---

## 10. 已知局限

| 问题 | 说明 |
|------|------|
| `FindMemoryType` 未实现 | 返回 0 占位，需补全物理设备查询 |
| 单 CommandBuffer | 所有 Pass 共用一个 CB，大型场景需要多 CB |
| 无 Input Attachment 优化 | 仅用 ImageMemoryBarrier，未利用 RenderPass 的子通道优化 |
| 无 Fence 同步 | CPU-GPU 同步缺失，`Execute` 返回后 GPU 可能未完成 |
| 无资源复用 | 每个 Resource 独占显存，不支持 alias 共享 |
