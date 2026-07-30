# 优化点：Barrier 布局追踪

## 现状问题

`Execute()` 中所有 input/output barrier 的 `oldLayout` 均硬编码为 `resource.initialLayout`：

```cpp
// rendergraph.hpp:251 / 279
barrier.setOldLayout(resource.initialLayout)  // 永远是注册时的初值
```

这导致**只有第一个使用该资源的 Pass 的 barrier 正确**。后续任何 Pass（不管读还是写）都会产生错误的 `oldLayout`，不符合 Vulkan 规范。

## 触发条件

同一个资源被多个 Pass 使用。例如 `resources/A` 参与 `P0(写A) → P1(读A) → P2(写A)`：

| Pass | 操作 | A 的实际布局 | barrier 声明的 oldLayout | 正确性 |
|------|------|-------------|-------------------------|--------|
| P0 | 写 A | eUndefined | eUndefined | ✅ |
| P0 收尾 | 切最终布局 | eShaderReadOnlyOptimal | — | — |
| P1 | 读 A | **eShaderReadOnlyOptimal** | eUndefined | ❌ |
| P2 | 写 A | **eShaderReadOnlyOptimal** | eUndefined | ❌ |

## 设计方案

### 核心思路

给 `Resource` 增加一个 `currentLayout` 字段，在渲染过程中追踪资源的实际当前布局，屏障使用当前布局作为起点而非初始布局。

### 改动的文件

`rendergraph.hpp`

### 改动点详情

#### 1. Resource 结构体 (行 13-25)

```cpp
struct Resource {
    // ... 现有字段 ...
    vk::ImageLayout currentLayout;  // 新增：追踪当前布局
};
```

#### 2. Compile() 末尾——资源创建后初始化 currentLayout (行 197 附近)

```cpp
// 创建 view 之后
resource.currentLayout = resource.initialLayout;
```

#### 3. Execute() 中三轮 barrier

**Input barrier (行 247-272)：**

```cpp
// 改为
barrier.setOldLayout(resource.currentLayout)
       .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
// barrier 插入后
resource.currentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
```

**Output barrier (行 275-300)：**

```cpp
// 改为
barrier.setOldLayout(resource.currentLayout)
       .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
// barrier 插入后
resource.currentLayout = vk::ImageLayout::eColorAttachmentOptimal;
```

**Final barrier (行 308-333)：**

```cpp
// 改为
barrier.setOldLayout(resource.currentLayout)
       .setNewLayout(resource.finalLayout);
// barrier 插入后
resource.currentLayout = resource.finalLayout;
```

### 可选增强

| 增强项 | 说明 |
|--------|------|
| **跳过多余 barrier** | 若 `currentLayout == newLayout`（如连续两个 Pass 都只读），整段屏障可跳过，节省 GPU 同步开销 |
| **追踪 accessMask** | 增加 `currentAccess` 字段，使 `srcAccessMask` 也更精确，而非 `<InternalEnd> hardcoded eMemoryWrite / eMemoryRead |
