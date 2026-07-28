
#include <string>
#include <unordered_map>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
// 渲染图 —— 自动依赖管理的综合实现
class Rendergraph {
private:
    // 资源描述与管理结构体
    // 表示渲染过程中使用的图像资源（纹理）
    struct Resource {
        std::string name;                     // 可读标识符，用于调试和引用
        vk::Format format;                    // 像素格式（RGBA8、Depth24Stencil8 等）
        vk::Extent2D extent;                  // 2D 资源尺寸（像素）
        vk::ImageUsageFlags usage;            // 资源用途（颜色附件、纹理等）
        vk::ImageLayout initialLayout;        // 帧开始时预期的布局
        vk::ImageLayout finalLayout;          // 帧结束时要求的布局

        // 实际 GPU 资源 —— 编译期间填充
        vk::raii::Image image = nullptr;      // GPU 图像对象
        vk::raii::DeviceMemory memory = nullptr;  // 显存分配
        vk::raii::ImageView view = nullptr;   // 着色器可访问的图像视图
    };

    // 渲染图中渲染通道的表示
    // 每个通道表示一个独立的渲染操作，具有定义的输入和输出
    struct Pass {
        std::string name;                     // 描述性名称，用于调试和性能分析
        std::vector<std::string> inputs;      // 此通道读取的资源（依赖项）
        std::vector<std::string> outputs;     // 此通道写入的资源（产物）
        std::function<void(vk::raii::CommandBuffer&)> executeFunc;  // 实际渲染代码
    };

    // 渲染图系统的核心数据存储
    std::unordered_map<std::string, Resource> resources;  // 图中引用的所有资源
    std::vector<Pass> passes;                             // 按定义顺序排列的所有渲染通道
    std::vector<size_t> executionOrder;                   // 计算出的最优执行顺序

    // 自动同步管理
    // 这些对象确保 GPU 执行顺序正确，无需手动设置屏障
    std::vector<vk::raii::Semaphore> semaphores;          // GPU 同步原语
    std::vector<std::pair<size_t, size_t>> semaphoreSignalWaitPairs;  // (发出信号的通道, 等待的通道)

    vk::raii::Device& device;  // 用于资源创建的 Vulkan 设备

public:
    explicit Rendergraph(vk::raii::Device& dev) : device(dev) {}

        // 资源注册接口，用于声明渲染过程中使用的所有资源
    // 此方法建立资源元数据，不创建实际 GPU 资源
    void AddResource(const std::string& name, vk::Format format, vk::Extent2D extent,
                    vk::ImageUsageFlags usage, vk::ImageLayout initialLayout,
                    vk::ImageLayout finalLayout) {
        resources[name].name = name;                    // 存储可读标识符
        resources[name].format = format;                // 定义像素格式和位深
        resources[name].extent = extent;                // 设置资源尺寸
        resources[name].usage = usage;                  // 指定预期用途
        resources[name].initialLayout = initialLayout; // 定义起始布局状态
        resources[name].finalLayout = finalLayout;     // 定义要求的结束状态
    }

    // 通道注册接口，用于定义渲染操作及其依赖关系
    // 此方法建立渲染的逻辑结构，不立即执行
    void AddPass(const std::string& name,
                const std::vector<std::string>& inputs,
                const std::vector<std::string>& outputs,
                std::function<void(vk::raii::CommandBuffer&)> executeFunc) {
        Pass pass;
        pass.name = name;                        // 分配描述性标识符
        pass.inputs = inputs;                    // 列出此通道读取的所有资源
        pass.outputs = outputs;                  // 列出此通道写入的所有资源
        pass.executeFunc = executeFunc;          // 存储实际渲染实现

        passes.push_back(pass);                  // 添加到有序通道列表
    }

    // 渲染图编译 —— 将声明式描述转换为可执行管线
    // 此方法执行依赖分析、资源分配和执行规划
    void Compile() {
        // 依赖图构建
        // 建立通道之间的双向依赖关系
        std::vector<std::vector<size_t>> dependencies(passes.size());  // 每个通道依赖什么
        std::vector<std::vector<size_t>> dependents(passes.size());    // 什么依赖每个通道

        // 跟踪哪个通道产出了每个资源（写后写依赖）
        std::unordered_map<std::string, size_t> resourceWriters;

        // 通过资源使用分析发现依赖关系
        // 分析每个通道以确定数据流关系
        for (size_t i = 0; i < passes.size(); ++i) {
            const auto& pass = passes[i];

            // 处理输入依赖 —— 此通道必须等待生产者
            for (const auto& input : pass.inputs) {
                auto it = resourceWriters.find(input);
                if (it != resourceWriters.end()) {
                    // 找到产生此输入的通道 —— 创建依赖链接
                    dependencies[i].push_back(it->second);      // 此通道依赖于生产者
                    dependents[it->second].push_back(i);        // 生产者以该通道为下游
                }
            }

            // 注册输出产物 —— 后续通道可能依赖这些
            for (const auto& output : pass.outputs) {
                resourceWriters[output] = i;                    // 记录此通道为生产者
            }
        }

        // 拓扑排序以获取最优执行顺序
        // 使用深度优先搜索计算有效执行序列，同时检测循环
        std::vector<bool> visited(passes.size(), false);       // 跟踪已完成的节点
        std::vector<bool> inStack(passes.size(), false);       // 跟踪当前递归路径

        std::function<void(size_t)> visit = [&](size_t node) {
            if (inStack[node]) {
                // 循环检测 —— 发现循环依赖
                throw std::runtime_error("渲染图中检测到循环依赖");
            }

            if (visited[node]) {
                return;  // 已处理过此节点及其依赖
            }

            inStack[node] = true;   // 标记为正在处理

            // 先递归处理所有下游通道（后序遍历）
            for (auto dependent : dependents[node]) {
                visit(dependent);
            }

            inStack[node] = false;  // 从当前路径中移除
            visited[node] = true;   // 标记为完全处理
            executionOrder.push_back(node);  // 添加到执行序列
        };

        // 处理所有未访问节点以处理不连通的图组件
        for (size_t i = 0; i < passes.size(); ++i) {
            if (!visited[i]) {
                visit(i);
            }
        }
        // 自动同步对象创建
        // 为分析过程中识别的所有依赖关系生成信号量
        for (size_t i = 0; i < passes.size(); ++i) {
            for (auto dep : dependencies[i]) {
                // 为此依赖关系创建 GPU 信号量
                // 下游通道在执行前将等待此信号量
                semaphores.emplace_back(device.createSemaphore({}));
                semaphoreSignalWaitPairs.emplace_back(dep, i);    // (生产者, 消费者) 对
            }
        }

        // 物理资源分配与创建
        // 将资源描述转换为实际 GPU 对象
        for (auto& [name, resource] : resources) {
            // 基于资源描述配置图像创建参数
            vk::ImageCreateInfo imageInfo;
            imageInfo.setImageType(vk::ImageType::e2D)                    // 2D 纹理/渲染目标
                     .setFormat(resource.format)                          // 来自描述的像素格式
                     .setExtent({resource.extent.width, resource.extent.height, 1})  // 尺寸
                     .setMipLevels(1)                                      // 简化起见使用单级 mip
                     .setArrayLayers(1)                                    // 单层（非数组纹理）
                     .setSamples(vk::SampleCountFlagBits::e1)              // 无多重采样
                     .setTiling(vk::ImageTiling::eOptimal)                 // GPU 最优内存布局
                     .setUsage(resource.usage)                             // 来自注册的使用标志
                     .setSharingMode(vk::SharingMode::eExclusive)          // 单队列族访问
                     .setInitialLayout(vk::ImageLayout::eUndefined);       // 初始布局（将被转换）

            resource.image = device.createImage(imageInfo);               // 创建 GPU 图像对象

            // 为图像分配显存
            vk::MemoryRequirements memRequirements = resource.image.getMemoryRequirements();

            vk::MemoryAllocateInfo allocInfo;
            allocInfo.setAllocationSize(memRequirements.size)             // 所需内存大小
                     .setMemoryTypeIndex(FindMemoryType(memRequirements.memoryTypeBits,
                                                        vk::MemoryPropertyFlagBits::eDeviceLocal));  // GPU 本地内存

            resource.memory = device.allocateMemory(allocInfo);           // 分配 GPU 内存
            resource.image.bindMemory(*resource.memory, 0);               // 将内存绑定到图像

            // 创建着色器访问所需的图像视图
            vk::ImageViewCreateInfo viewInfo;
            viewInfo.setImage(*resource.image)                            // 引用已创建的图像
                    .setViewType(vk::ImageViewType::e2D)                   // 2D 视图类型
                    .setFormat(resource.format)                            // 匹配图像格式
                    .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});  // 完整图像访问

            resource.view = device.createImageView(viewInfo);             // 创建着色器可访问的视图
        }
    }

    // 资源访问接口，用于获取已编译的资源
    Resource* GetResource(const std::string& name) {
        auto it = resources.find(name);
        return (it != resources.end()) ? &it->second : nullptr;
    }

    // 渲染图执行引擎 —— 配合自动同步协调通道执行
    // 此方法将编译后的渲染图转换为实际 GPU 工作
    void Execute(vk::raii::CommandBuffer& commandBuffer, vk::Queue queue) {
        // 动态同步的执行状态管理
        std::vector<vk::CommandBuffer> cmdBuffers;           // 命令缓冲区存储
        std::vector<vk::Semaphore> waitSemaphores;           // 当前通道的同步依赖
        std::vector<vk::PipelineStageFlags> waitStages;      // 需要等待的管线阶段
        std::vector<vk::Semaphore> signalSemaphores;         // 当前通道完成后要发出信号的信号量

        // 按序执行通道，自动管理依赖
        // 按计算出的依赖安全顺序执行每个通道
        for (auto passIdx : executionOrder) {
            const auto& pass = passes[passIdx];

            // 同步设置 —— 收集当前通道的依赖项
            // 确定此通道执行前必须等待什么
            waitSemaphores.clear();
            waitStages.clear();

            for (size_t i = 0; i < semaphoreSignalWaitPairs.size(); ++i) {
                if (semaphoreSignalWaitPairs[i].second == passIdx) {
                    // 此通道依赖于另一个通道的完成
                    waitSemaphores.push_back(*semaphores[i]);                           // 等待依赖完成
                    waitStages.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);  // 在输出阶段等待
                }
            }

            // 收集此通道将为下游通道发出的信号量
            signalSemaphores.clear();
            for (size_t i = 0; i < semaphoreSignalWaitPairs.size(); ++i) {
                if (semaphoreSignalWaitPairs[i].first == passIdx) {
                    // 其他通道依赖于此通道的完成
                    signalSemaphores.push_back(*semaphores[i]);                         // 为下游通道发出完成信号
                }
            }

            // 命令缓冲区准备与资源布局转换
            // 设置命令录制并将资源转换为适当的布局
            commandBuffer.begin({});                                                   // 开始命令录制

            // 将输入资源转换为着色器可读的布局
            for (const auto& input : pass.inputs) {
                auto& resource = resources[input];

                vk::ImageMemoryBarrier barrier;
                barrier.setOldLayout(resource.initialLayout)                           // 当前资源布局
                       .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)          // 读取目标布局
                       .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)                // 无队列族转移
                       .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                       .setImage(*resource.image)                                      // 目标图像
                       .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})  // 完整图像范围
                       .setSrcAccessMask(vk::AccessFlagBits::eMemoryWrite)             // 之前的写访问
                       .setDstAccessMask(vk::AccessFlagBits::eShaderRead);             // 需要的读访问

                // 插入管线屏障以进行安全的布局转换
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eAllCommands,                           // 等待所有先前工作
                    vk::PipelineStageFlagBits::eFragmentShader,                        // 启用片元着色器访问
                    vk::DependencyFlagBits::eByRegion,                                 // 区域本地依赖
                    {}, {}, barrier                               // 仅图像屏障
                );
            }

            // 将输出资源转换为渲染目标布局
            for (const auto& output : pass.outputs) {
                auto& resource = resources[output];

                vk::ImageMemoryBarrier barrier;
                barrier.setOldLayout(resource.initialLayout)                           // 当前布局状态
                       .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)         // 颜色输出最优布局
                       .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                       .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                       .setImage(*resource.image)
                       .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                       .setSrcAccessMask(vk::AccessFlagBits::eMemoryRead)              // 之前的读访问
                       .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);   // 需要的写访问

                // 插入屏障以安全转换为可写状态
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eAllCommands,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,                 // 启用颜色附件写入
                    vk::DependencyFlagBits::eByRegion,
                    {}, {}, barrier
                );
            }

            // 通道执行 —— 执行实际渲染逻辑
            // 使用准备好的命令缓冲区调用用户提供的渲染函数
            pass.executeFunc(commandBuffer);                                           // 执行通道特定的渲染

            // 最终布局转换 —— 为后续使用准备资源
            // 将输出资源转换为其要求的最终布局
            for (const auto& output : pass.outputs) {
                auto& resource = resources[output];

                vk::ImageMemoryBarrier barrier;
                barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)         // 当前可写布局
                       .setNewLayout(resource.finalLayout)                             // 要求的最终布局
                       .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                       .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                       .setImage(*resource.image)
                       .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
                       .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)    // 之前的写操作
                       .setDstAccessMask(vk::AccessFlagBits::eMemoryRead);             // 允许后续读取

                // 插入最终屏障以进行布局转换
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,                 // 颜色写入完成后
                    vk::PipelineStageFlagBits::eAllCommands,                           // 任何后续工作之前
                    vk::DependencyFlagBits::eByRegion,
                    {}, {}, barrier
                );
            }

            // 带同步的命令提交
            // 使用适当的依赖和发信信号量提交命令缓冲区
            commandBuffer.end();                                                       // 完成命令录制

            vk::SubmitInfo submitInfo;
            submitInfo.setWaitSemaphoreCount(static_cast<uint32_t>(waitSemaphores.size()))      // 需要等待的依赖
                      .setPWaitSemaphores(waitSemaphores.data())                                 // 依赖信号量
                      .setPWaitDstStageMask(waitStages.data())                                   // 在哪些管线阶段等待
                      .setCommandBufferCount(1)                                                  // 单个命令缓冲区
                      .setPCommandBuffers(&*commandBuffer)                                      // 要执行的命令缓冲区
                      .setSignalSemaphoreCount(static_cast<uint32_t>(signalSemaphores.size()))  // 要发出信号的信号量数
                      .setPSignalSemaphores(signalSemaphores.data());                           // 发信信号量

            queue.submit(1, &submitInfo, nullptr);                                              // 提交到 GPU 队列
        }
    }

private:
    uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        // 查找合适内存类型的实现
        // ...
        return 0; // 占位
    }
};
