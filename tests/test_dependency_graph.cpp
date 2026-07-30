#include <gtest/gtest.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "rendergraph.hpp"

namespace {

class VulkanTest : public ::testing::Test {
protected:
    void SetUp() override {
        vk::ApplicationInfo appInfo;
        appInfo.setPApplicationName("GraphTest")
               .setApplicationVersion(1)
               .setPEngineName("Engine")
               .setEngineVersion(1)
               .setApiVersion(VK_API_VERSION_1_1);

        vk::InstanceCreateInfo instanceInfo;
        instanceInfo.setPApplicationInfo(&appInfo);

        instance = vk::raii::Instance(context, instanceInfo);

        physicalDevices = instance.enumeratePhysicalDevices();
        ASSERT_FALSE(physicalDevices.empty()) << "No Vulkan physical device found";

        auto queueFamilyProps = physicalDevices[0].getQueueFamilyProperties();
        uint32_t queueFamilyIndex = 0;
        for (uint32_t i = 0; i < queueFamilyProps.size(); ++i) {
            if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                queueFamilyIndex = i;
                break;
            }
        }

        float priority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo;
        queueInfo.setQueueFamilyIndex(queueFamilyIndex)
                 .setQueueCount(1)
                 .setPQueuePriorities(&priority);

        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.setQueueCreateInfoCount(1)
                  .setPQueueCreateInfos(&queueInfo);

        device = vk::raii::Device(physicalDevices[0], deviceInfo);
    }

    vk::raii::Context         context;
    vk::raii::Instance        instance        = nullptr;
    vk::raii::PhysicalDevices physicalDevices = nullptr;
    vk::raii::Device          device          = nullptr;
};

auto kNoOp = [](vk::raii::CommandBuffer&) {};

} // namespace

// ============================================================
// 声明阶段：AddResource / AddPass / GetResource
// ============================================================

TEST_F(VulkanTest, DeclareResourcesAndPasses) {
    Rendergraph graph(device);

    graph.AddResource("A", vk::Format::eR8G8B8A8Unorm, {1920, 1080},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

    graph.AddPass("P0", {}, {"A"}, kNoOp);
    graph.AddPass("P1", {"A"}, {"B"}, kNoOp);

    EXPECT_NE(graph.GetResource("A"), nullptr);
    EXPECT_EQ(graph.GetResource("B"), nullptr);
}

// ============================================================
// 循环依赖：resourceWriters 前向分析不会产生环（P0 读 B 时 P1 尚未声明 B）
// ============================================================

TEST_F(VulkanTest, CrossDependencyNoCycle) {
    Rendergraph graph(device);

    graph.AddResource("A", vk::Format::eR8G8B8A8Unorm, {1024, 768},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
    graph.AddResource("B", vk::Format::eR8G8B8A8Unorm, {1024, 768},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

    graph.AddPass("P0", {"B"}, {"A"}, kNoOp);
    graph.AddPass("P1", {"A"}, {"B"}, kNoOp);

    EXPECT_NO_THROW(graph.Compile());
}

// ============================================================
// 合法线性图 Compile
// ============================================================

TEST_F(VulkanTest, CompileLinearGraph) {
    Rendergraph graph(device);

    graph.AddResource("A", vk::Format::eR8G8B8A8Unorm, {800, 600},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
    graph.AddResource("B", vk::Format::eR8G8B8A8Unorm, {800, 600},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

    graph.AddPass("P0", {},    {"A"}, kNoOp);
    graph.AddPass("P1", {"A"}, {"B"}, kNoOp);
    graph.AddPass("P2", {"B"}, {},    kNoOp);

    EXPECT_NO_THROW(graph.Compile());

    EXPECT_NE(graph.GetResource("A"), nullptr);
    EXPECT_NE(graph.GetResource("B"), nullptr);
}

// ============================================================
// 未注册 Resource 的 Pass output
// ============================================================

TEST_F(VulkanTest, PassWithUnregisteredResource) {
    Rendergraph graph(device);

    graph.AddResource("A", vk::Format::eR8G8B8A8Unorm, {640, 480},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

    graph.AddPass("P0", {},    {"A"},  kNoOp);
    graph.AddPass("P1", {"A"}, {"B"},  kNoOp);

    EXPECT_NO_THROW(graph.Compile());
}

// ============================================================
// 已知局限：barrier 的 oldLayout 始终硬取 resource.initialLayout
//
// 触发条件：同一个资源被超过一个 Pass 使用（读或写）。
// P0 输出 A → A 最终布局变为 eShaderReadOnlyOptimal
// P1 输入 A → 但 barrier 里的 oldLayout 仍然是 eUndefined（错！）
// P1 输出 B → B 最终布局变为 eShaderReadOnlyOptimal
// P2 输入 B → 但 barrier 里的 oldLayout 仍然是 eUndefined（错！）
// ============================================================

TEST_F(VulkanTest, SubsequentPassBarrierUsesWrongOldLayout) {
    Rendergraph graph(device);

    graph.AddResource("A", vk::Format::eR8G8B8A8Unorm, {640, 480},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
    graph.AddResource("B", vk::Format::eR8G8B8A8Unorm, {640, 480},
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);

    // P0: 写 A → A 最终布局 eShaderReadOnlyOptimal
    // P1: 读 A 写 B → A 的 input barrier: oldLayout = eUndefined（实际已被 P0 切成 eShaderReadOnlyOptimal）
    //                 → B 的 output barrier: oldLayout = eUndefined（合理，首次用）
    // P2: 读 B   → B 的 input barrier: oldLayout = eUndefined（实际已被 P1 切成 eShaderReadOnlyOptimal）
    graph.AddPass("P0", {},    {"A"}, kNoOp);
    graph.AddPass("P1", {"A"}, {"B"}, kNoOp);  // ← A 的 barrier 错误
    graph.AddPass("P2", {"B"}, {},    kNoOp);  // ← B 的 barrier 错误

    EXPECT_NO_THROW(graph.Compile());
}
