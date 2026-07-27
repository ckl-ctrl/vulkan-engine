#pragma once

#include <string>
#include <unordered_map>
#include <typeindex>
#include <memory>

#include "resource_manager.hpp"
#include "concepts.hpp"

#include <vulkan/vulkan.hpp>
class Resource {
private:
    std::string resourceId; ///< 资源名称
    bool loaded = false;    ///< 资源是否已加载

public:
    explicit Resource(const std::string& id) : resourceId(id) {}
    virtual ~Resource() = default;

    const std::string& GetId() const { return resourceId; }
    bool IsLoaded() const { return loaded; }

    bool Load() {
        if (!loaded) {
            loaded = OnLoad();
        }
        return loaded;
    }

    void Unload() {
        if (loaded) {
            OnUnload();
            loaded = false;
        }
    }
protected:
    virtual bool OnLoad() = 0;   ///< 由派生类实现的加载逻辑
    virtual void OnUnload() = 0; ///< 由派生类实现的卸载逻辑
};

class ResourceManager {
private:
    std::unordered_map<std::type_index,
                       std::unordered_map<std::string, std::shared_ptr<Resource>>> resources; 

    std::unordered_map<std::string, std::size_t> refCounts;

public:
    template<typename T>
    ResourceHandle<T> Load(const std::string& resourceId) {
        static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

        // Step 3a: Check existing resource cache to avoid redundant loading
        auto& typeResources = resources[std::type_index(typeid(T))];
        auto it = typeResources.find(resourceId);

        if (it != typeResources.end()) {
            // Resource exists in cache - increment reference count and return handle
            refCounts[resourceId]++;
            return ResourceHandle<T>(resourceId, this);
        }

        // Step 3b: Create new resource instance and attempt loading
        auto resource = std::make_shared<T>(resourceId);
        if (!resource->Load()) {
            // Loading failed - return invalid handle rather than corrupting cache
            return ResourceHandle<T>();
        }

        // Step 3c: Cache successful resource and initialize reference tracking
        typeResources[resourceId] = resource;
        refCounts[resourceId] = 1;

        return ResourceHandle<T>(resourceId, this);
    }

    template<typename T>
    T* GetResource(const std::string& resourceId) {
        auto& typeResources = resources[std::type_index(typeid(T))];
        auto it = typeResources.find(resourceId);
        if (it != typeResources.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    template<typename T>
    bool HasResource(const std::string& resourceId) {
        auto& typeResources = resources[std::type_index(typeid(T))];
        auto it = typeResources.find(resourceId);
        return it != typeResources.end();
    }

    void Release(const std::string& resourceId) {
        // Locate reference count entry for this resource
        auto it = refCounts.find(resourceId);
        if (it != refCounts.end()) {
            it->second--;

            // Check if resource has no remaining references
            if (it->second <= 0) {
                // Step 5a: Locate and unload the unreferenced resource across all type containers
                for (auto& [type, typeResources] : resources) {
                    auto resourceIt = typeResources.find(resourceId);
                    if (resourceIt != typeResources.end()) {
                        resourceIt->second->Unload();      // Allow resource to clean up its data
                        typeResources.erase(resourceIt);   // Remove from cache
                        break;
                    }
                }

                // Step 5b: Clean up reference counting entry
                refCounts.erase(it);
            }
        }
    }

    void UnloadAll() {
        // Emergency cleanup method for system shutdown or major state changes
        for (auto& [type, typeResources] : resources) {
            for (auto& [id, resource] : typeResources) {
                resource->Unload();     // Ensure all resources clean up properly
            }
            typeResources.clear();      // Clear type-specific containers
        }
        refCounts.clear();              // Reset all reference counts
    }
};

// Texture resource
class Texture : public Resource {
private:
    // Core Vulkan GPU resources for texture representation
    vk::Image image;              // GPU image object containing pixel data
    vk::DeviceMemory memory;      // GPU memory allocation backing the image
    vk::DeviceSize offset;        // Offset within the memory allocation for this texture
    vk::ImageView imageView;      // Shader-accessible view into the image
    vk::Sampler sampler;          // Sampling configuration (filtering, wrapping, etc.)

    // Texture metadata for validation and debugging
    int width = 0;                // Image width in pixels
    int height = 0;               // Image height in pixels
    int channels = 0;             // Number of color channels (RGB=3, RGBA=4, etc.)

public:
    explicit Texture(const std::string& id) : Resource(id) {}

    ~Texture() override {
        Unload();                 // Ensure proper cleanup when object is destroyed
    }

    bool OnLoad() override {
        // Step 2a: Construct file path using resource ID and expected format
        std::string filePath = "textures/" + GetId() + ".ktx";

        // Step 2b: Load raw image data from disk with format detection
        unsigned char* data = LoadImageData(filePath, &width, &height, &channels);
        if (!data) {
            return false;           // Failed to load - return failure without partial state
        }

        // Step 2c: Transform raw pixel data into Vulkan GPU resources
        CreateVulkanImage(data, width, height, channels);

        // Step 2d: Clean up temporary CPU memory to prevent leaks
        FreeImageData(data);

        return true;    // Mark resource as successfully loaded
    }

    void OnUnload() override {
        // Only perform cleanup if resource is currently loaded
        if (IsLoaded()) {
            // Step 3a: Obtain device handle for resource destruction
            vk::Device device = GetDevice();

            // Step 3b: Destroy GPU objects in reverse creation order
            // This ordering prevents use-after-free errors in GPU drivers
            device.destroySampler(sampler);       // Destroy sampling configuration
            device.destroyImageView(imageView);   // Destroy shader view
            device.destroyImage(image);           // Destroy image object
            device.freeMemory(memory);            // Release GPU memory allocation

        }
    }

    // Public interface for accessing Vulkan resources safely
    vk::Image GetImage() const { return image; }
    vk::ImageView GetImageView() const { return imageView; }
    vk::Sampler GetSampler() const { return sampler; }

private:
    unsigned char* LoadImageData(const std::string& filePath, int* width, int* height, int* channels) {
        // Implementation using stb_image or ktx library
        // This method abstracts the details of different image format support
        // and provides a consistent interface for pixel data loading
        // ...
        return nullptr; // Placeholder
    }

    void FreeImageData(unsigned char* data) {
        // Implementation using stb_image or ktx library
        // Ensures proper cleanup of image loader specific memory allocations
        // Different libraries may require different cleanup approaches
        // ...
    }

    void CreateVulkanImage(unsigned char* data, int width, int height, int channels) {
        // Implementation to create Vulkan image, allocate memory, and upload data
        // This involves complex Vulkan operations including:
        // - Format selection based on channel count and data type
        // - Memory allocation with appropriate usage flags
        // - Image creation with optimal tiling and layout
        // - Data upload via staging buffers for efficiency
        // - Image view creation for shader access
        // - Sampler creation with appropriate filtering settings
        // ...
    }

    vk::Device GetDevice() {
        // Get device from somewhere (e.g., singleton or parameter)
        // Production code would use dependency injection or service location
        // to provide the Vulkan device handle without tight coupling
        // ...
        return vk::Device(); // Placeholder
    }
};