
#include <string>
#include <unordered_map>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
// A comprehensive rendergraph implementation for automated dependency management
class Rendergraph {
private:
    // Resource description and management structure
    // Represents Image resource used during rendering (textures)
    struct Resource {
        std::string name;                     // Human-readable identifier for debugging and referencing
        vk::Format format;                    // Pixel format (RGBA8, Depth24Stencil8, etc.)
        vk::Extent2D extent;                  // Dimensions in pixels for 2D resources
        vk::ImageUsageFlags usage;            // How this resource will be used (color attachment, texture, etc.)
        vk::ImageLayout initialLayout;        // Expected layout when the frame begins
        vk::ImageLayout finalLayout;          // Required layout when the frame ends

        // Actual GPU resources - populated during compilation
        vk::raii::Image image = nullptr;      // The GPU image object
        vk::raii::DeviceMemory memory = nullptr;  // Backing memory allocation
        vk::raii::ImageView view = nullptr;   // Shader-accessible view of the image
    };

    // Render pass representation within the graph structure
    // Each pass represents a distinct rendering operation with defined inputs and outputs
    struct Pass {
        std::string name;                     // Descriptive name for debugging and profiling
        std::vector<std::string> inputs;      // Resources this pass reads from (dependencies)
        std::vector<std::string> outputs;     // Resources this pass writes to (products)
        std::function<void(vk::raii::CommandBuffer&)> executeFunc;  // The actual rendering code
    };

    // Core data storage for the rendergraph system
    std::unordered_map<std::string, Resource> resources;  // All resources referenced in the graph
    std::vector<Pass> passes;                             // All rendering passes in definition order
    std::vector<size_t> executionOrder;                   // Computed optimal execution sequence

    // Automatic synchronization management
    // These objects ensure correct GPU execution order without manual barriers
    std::vector<vk::raii::Semaphore> semaphores;          // GPU synchronization primitives
    std::vector<std::pair<size_t, size_t>> semaphoreSignalWaitPairs;  // (signaling pass, waiting pass)

    vk::raii::Device& device;  // Vulkan device for resource creation

public:
    explicit Rendergraph(vk::raii::Device& dev) : device(dev) {}