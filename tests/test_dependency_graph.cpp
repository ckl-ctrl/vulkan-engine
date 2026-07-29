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
