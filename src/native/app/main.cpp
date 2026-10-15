// Minimal SDL3 + Vulkan host for the authored GameLoop.  The scene itself is
// rendered by previewRenderScene(), so this presentation layer deliberately
// contains no duplicate road/car/shader code.
#include "assets/native_model.h"
#include "gameplay/game_loop.h"
#include "preview/preview.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr VkMemoryPropertyFlags kHostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

struct Presenter {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkDeviceSize stagingSize = 0;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderDone = VK_NULL_HANDLE;
    VkFence frameDone = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memory{};
};

bool vkOk(VkResult result, const char* what, std::string& error) {
    if (result == VK_SUCCESS) return true;
    error = std::string(what) + " (VkResult " + std::to_string(result) + ")";
    return false;
}

std::uint32_t memoryType(const Presenter& p, std::uint32_t bits, VkMemoryPropertyFlags flags) {
    for (std::uint32_t i = 0; i < p.memory.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (p.memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
    return UINT32_MAX;
}

void destroySwapchain(Presenter& p) {
    if (p.staging) vkDestroyBuffer(p.device, p.staging, nullptr);
    if (p.stagingMemory) vkFreeMemory(p.device, p.stagingMemory, nullptr);
    p.staging = VK_NULL_HANDLE; p.stagingMemory = VK_NULL_HANDLE; p.stagingSize = 0;
    if (p.swapchain) vkDestroySwapchainKHR(p.device, p.swapchain, nullptr);
    p.swapchain = VK_NULL_HANDLE; p.images.clear(); p.extent = {};
}

void destroy(Presenter& p) {
    if (p.device) vkDeviceWaitIdle(p.device);
    destroySwapchain(p);
    if (p.frameDone) vkDestroyFence(p.device, p.frameDone, nullptr);
    if (p.renderDone) vkDestroySemaphore(p.device, p.renderDone, nullptr);
    if (p.imageAvailable) vkDestroySemaphore(p.device, p.imageAvailable, nullptr);
    if (p.pool) vkDestroyCommandPool(p.device, p.pool, nullptr);
    if (p.device) vkDestroyDevice(p.device, nullptr);
    if (p.surface) vkDestroySurfaceKHR(p.instance, p.surface, nullptr);
    if (p.instance) vkDestroyInstance(p.instance, nullptr);
    p = {};
}

bool makeSwapchain(Presenter& p, SDL_Window* window, std::string& error) {
    int pixelW = 0, pixelH = 0;
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);
    if (pixelW <= 0 || pixelH <= 0) return true; // Minimized; retry on the next size event.
    vkDeviceWaitIdle(p.device);
    destroySwapchain(p);
    VkSurfaceCapabilitiesKHR caps{};
    if (!vkOk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p.physical, p.surface, &caps), "surface capabilities", error)) return false;
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(p.physical, p.surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(p.physical, p.surface, &count, formats.data());
    if (formats.empty()) { error = "surface has no formats"; return false; }
    const auto preferred = std::find_if(formats.begin(), formats.end(), [](const auto& f) {
        return f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM;
    });
    p.format = (preferred == formats.end() ? formats.front() : *preferred).format;
    p.extent = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent : VkExtent2D{
        std::clamp(static_cast<std::uint32_t>(pixelW), caps.minImageExtent.width, caps.maxImageExtent.width),
        std::clamp(static_cast<std::uint32_t>(pixelH), caps.minImageExtent.height, caps.maxImageExtent.height)};
    std::uint32_t modes = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(p.physical, p.surface, &modes, nullptr);
    std::vector<VkPresentModeKHR> presentModes(modes);
    vkGetPhysicalDeviceSurfacePresentModesKHR(p.physical, p.surface, &modes, presentModes.data());
    const bool mailbox = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end();
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = p.surface; ci.minImageCount = std::max(2u, caps.minImageCount);
    if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount) ci.minImageCount = caps.maxImageCount;
    ci.imageFormat = p.format; ci.imageColorSpace = (preferred == formats.end() ? formats.front() : *preferred).colorSpace;
    ci.imageExtent = p.extent; ci.imageArrayLayers = 1; ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; ci.presentMode = mailbox ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;
    if (!vkOk(vkCreateSwapchainKHR(p.device, &ci, nullptr, &p.swapchain), "vkCreateSwapchainKHR", error)) return false;
    vkGetSwapchainImagesKHR(p.device, p.swapchain, &count, nullptr); p.images.resize(count);
    vkGetSwapchainImagesKHR(p.device, p.swapchain, &count, p.images.data());
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(p.extent.width) * p.extent.height * 4;
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; buffer.size = bytes; buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!vkOk(vkCreateBuffer(p.device, &buffer, nullptr, &p.staging), "vkCreateBuffer(staging)", error)) return false;
    VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(p.device, p.staging, &requirements);
    const std::uint32_t type = memoryType(p, requirements.memoryTypeBits, kHostMemory);
    if (type == UINT32_MAX) { error = "no host-visible staging memory"; return false; }
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; alloc.allocationSize = requirements.size; alloc.memoryTypeIndex = type;
    if (!vkOk(vkAllocateMemory(p.device, &alloc, nullptr, &p.stagingMemory), "vkAllocateMemory(staging)", error) ||
        !vkOk(vkBindBufferMemory(p.device, p.staging, p.stagingMemory, 0), "vkBindBufferMemory(staging)", error)) return false;
    p.stagingSize = bytes;
    return true;
}

bool init(Presenter& p, SDL_Window* window, std::string& error) {
    std::uint32_t extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!sdlExtensions) { error = SDL_GetError(); return false; }
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);
#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "rt-app"; app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; instance.pApplicationInfo = &app;
    instance.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()); instance.ppEnabledExtensionNames = extensions.data();
#ifdef __APPLE__
    instance.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    if (!vkOk(vkCreateInstance(&instance, nullptr, &p.instance), "vkCreateInstance", error) ||
        !SDL_Vulkan_CreateSurface(window, p.instance, nullptr, &p.surface)) { if (error.empty()) error = SDL_GetError(); return false; }
    std::uint32_t deviceCount = 0; vkEnumeratePhysicalDevices(p.instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount); vkEnumeratePhysicalDevices(p.instance, &deviceCount, devices.data());
    for (const auto physical : devices) {
        std::uint32_t queueCount = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount); vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueCount, queues.data());
        for (std::uint32_t i = 0; i < queueCount; ++i) { VkBool32 present = VK_FALSE; vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, p.surface, &present);
            if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) { p.physical = physical; p.queueFamily = i; break; } }
        if (p.physical) break;
    }
    if (!p.physical) { error = "no graphics/present queue"; return false; }
    vkGetPhysicalDeviceMemoryProperties(p.physical, &p.memory);
    float priority = 1.f; VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; queue.queueFamilyIndex = p.queueFamily; queue.queueCount = 1; queue.pQueuePriorities = &priority;
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef __APPLE__
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; device.queueCreateInfoCount = 1; device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size()); device.ppEnabledExtensionNames = deviceExtensions.data();
    if (!vkOk(vkCreateDevice(p.physical, &device, nullptr, &p.device), "vkCreateDevice", error)) return false;
    vkGetDeviceQueue(p.device, p.queueFamily, 0, &p.queue);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pool.queueFamilyIndex = p.queueFamily; pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandBufferAllocateInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    if (!vkOk(vkCreateCommandPool(p.device, &pool, nullptr, &p.pool), "vkCreateCommandPool", error)) return false;
    command.commandPool = p.pool; command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; command.commandBufferCount = 1;
    if (!vkOk(vkAllocateCommandBuffers(p.device, &command, &p.command), "vkAllocateCommandBuffers", error)) return false;
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (!vkOk(vkCreateSemaphore(p.device, &semaphore, nullptr, &p.imageAvailable), "vkCreateSemaphore", error) ||
        !vkOk(vkCreateSemaphore(p.device, &semaphore, nullptr, &p.renderDone), "vkCreateSemaphore", error) ||
        !vkOk(vkCreateFence(p.device, &fence, nullptr, &p.frameDone), "vkCreateFence", error)) return false;
    return makeSwapchain(p, window, error);
}

bool present(Presenter& p, const std::vector<std::uint8_t>& rgba, bool& outOfDate, std::string& error) {
    outOfDate = false;
    if (!p.swapchain || rgba.size() != p.stagingSize) return true;
    vkWaitForFences(p.device, 1, &p.frameDone, VK_TRUE, UINT64_MAX);
    std::uint32_t image = 0; const VkResult acquired = vkAcquireNextImageKHR(p.device, p.swapchain, UINT64_MAX, p.imageAvailable, VK_NULL_HANDLE, &image);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) { outOfDate = true; return true; }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR && !vkOk(acquired, "vkAcquireNextImageKHR", error)) return false;
    void* mapped = nullptr; if (!vkOk(vkMapMemory(p.device, p.stagingMemory, 0, p.stagingSize, 0, &mapped), "vkMapMemory(staging)", error)) return false;
    auto* out = static_cast<std::uint8_t*>(mapped);
    if (p.format == VK_FORMAT_B8G8R8A8_UNORM || p.format == VK_FORMAT_B8G8R8A8_SRGB) for (std::size_t i = 0; i < rgba.size(); i += 4) { out[i] = rgba[i + 2]; out[i + 1] = rgba[i + 1]; out[i + 2] = rgba[i]; out[i + 3] = rgba[i + 3]; }
    else std::memcpy(out, rgba.data(), rgba.size());
    vkUnmapMemory(p.device, p.stagingMemory); vkResetFences(p.device, 1, &p.frameDone); vkResetCommandBuffer(p.command, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; vkBeginCommandBuffer(p.command, &begin);
    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; toTransfer.image = p.images[image]; toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(p.command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);
    VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {p.extent.width, p.extent.height, 1};
    vkCmdCopyBufferToImage(p.command, p.staging, p.images[image], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    VkImageMemoryBarrier toPresent = toTransfer; toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; toPresent.dstAccessMask = 0;
    vkCmdPipelineBarrier(p.command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);
    vkEndCommandBuffer(p.command);
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT; VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &p.imageAvailable; submit.pWaitDstStageMask = &waitStage; submit.commandBufferCount = 1; submit.pCommandBuffers = &p.command; submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &p.renderDone;
    if (!vkOk(vkQueueSubmit(p.queue, 1, &submit, p.frameDone), "vkQueueSubmit", error)) return false;
    VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; info.waitSemaphoreCount = 1; info.pWaitSemaphores = &p.renderDone; info.swapchainCount = 1; info.pSwapchains = &p.swapchain; info.pImageIndices = &image;
    const VkResult result = vkQueuePresentKHR(p.queue, &info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { outOfDate = true; return true; }
    return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR || (error = "vkQueuePresentKHR (VkResult " + std::to_string(result) + ")", false);
}

rt::native::assets::NativeModel demoCar() {
    using rt::native::assets::NativeModel; using rt::native::assets::Vertex;
    NativeModel car;
    const std::array<std::array<float, 3>, 8> points{{{-1.f, 0.f, -2.f}, {1.f, 0.f, -2.f}, {1.f, 0.f, 2.f}, {-1.f, 0.f, 2.f}, {-1.f, 1.f, -2.f}, {1.f, 1.f, -2.f}, {1.f, 1.f, 2.f}, {-1.f, 1.f, 2.f}}};
    for (const auto& p : points) car.vertices.push_back(Vertex{p[0], p[1], p[2]});
    car.indices = {0,2,1,0,3,2, 4,5,6,4,6,7, 0,1,5,0,5,4, 1,2,6,1,6,5, 2,3,7,2,7,6, 3,0,4,3,4,7};
    return car;
}
int envFrames() { const char* value = std::getenv("RT_APP_FRAMES"); return value ? std::max(0, std::atoi(value)) : 0; }
} // namespace

int main() {
    constexpr int kInitialWidth = 800, kInitialHeight = 600;
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::cerr << "SDL_Init: " << SDL_GetError() << "\n"; return 1; }
    SDL_Window* window = SDL_CreateWindow("Road Trip authored scene", kInitialWidth, kInitialHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) { std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n"; SDL_Quit(); return 1; }
    Presenter presenter; std::string error;
    if (!init(presenter, window, error)) { std::cerr << "Vulkan init: " << error << "\n"; destroy(presenter); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    int width = static_cast<int>(presenter.extent.width), height = static_cast<int>(presenter.extent.height);
    rt::native::preview::Renderer* renderer = rt::native::preview::previewInit(width, height, error, nullptr);
    if (!renderer) { std::cerr << "scene renderer init: " << error << "\n"; destroy(presenter); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    const bool noRender = std::getenv("RT_APP_NORENDER") != nullptr; const int maxFrames = envFrames();
    const auto recreateSwapchain = [&]() {
        int newW = 0, newH = 0; SDL_GetWindowSizeInPixels(window, &newW, &newH);
        if (newW <= 0 || newH <= 0) return true;
        if (!makeSwapchain(presenter, window, error)) { std::cerr << "swapchain recreate: " << error << "\n"; return false; }
        rt::native::preview::previewDestroy(renderer);
        renderer = rt::native::preview::previewInit(presenter.extent.width, presenter.extent.height, error, nullptr);
        if (!renderer) { std::cerr << "scene resize: " << error << "\n"; return false; }
        return true;
    };
    auto car = demoCar(); rt::native::gameplay::GameLoop loop; bool running = true; int frames = 0;
    std::uint64_t simulationFrame = 0;
    auto previous = std::chrono::steady_clock::now();
    while (running && (!maxFrames || frames < maxFrames)) {
        SDL_Event event; bool resize = false;
        while (SDL_PollEvent(&event)) { if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED || (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) running = false;
            if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) resize = true; }
        if (!running) break;
        if (resize && !recreateSwapchain()) break;
        const auto now = std::chrono::steady_clock::now(); const float dt = std::min(std::chrono::duration<float>(now - previous).count(), 0.1f); previous = now;
        const auto snapshot = loop.tick(dt, {}); simulationFrame = snapshot.frame;
        if (!noRender && presenter.extent.width && presenter.extent.height) {
            bool retry = false;
            do {
                std::vector<std::uint8_t> pixels;
                if (!rt::native::preview::previewRenderScene(renderer, car, snapshot, loop.world(), pixels, error) || !present(presenter, pixels, retry, error)) {
                    std::cerr << "render: " << error << "\n"; running = false; break;
                }
                if (retry && !recreateSwapchain()) { running = false; break; }
            } while (retry && presenter.extent.width && presenter.extent.height);
        }
        ++frames;
    }
    std::cout << "rt-app frames=" << frames << " simulation_frame=" << simulationFrame << "\n";
    rt::native::preview::previewDestroy(renderer); destroy(presenter); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
