#include <iostream>
#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include "StudioConfig.hpp"
#include "LiveMonitor.hpp"
#include "SessionReplayer.hpp"
#include <filesystem>

#include <agk/RecordingEngine/IRecordingEngine.hpp>

// Helper for Vulkan
static VkAllocationCallbacks*   g_Allocator = nullptr;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int                      g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

// Callback for Vulkan errors
static void check_vk_result(VkResult err) {
    if (err == 0) return;
    std::cerr << "[Vulkan] Error: VkResult = " << err << std::endl;
    if (err < 0) abort();
}

// Function to setup Vulkan
static void SetupVulkan(const char* const* extensions, uint32_t extensions_count) {
    VkResult err;

    // Create Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = extensions_count;
        create_info.ppEnabledExtensionNames = extensions;
        err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
        check_vk_result(err);
    }

    // Select Physical Device
    {
        uint32_t gpu_count;
        err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, nullptr);
        check_vk_result(err);
        std::vector<VkPhysicalDevice> gpus(gpu_count);
        err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus.data());
        check_vk_result(err);
        g_PhysicalDevice = gpus[0]; // Just take the first one
    }

    // Select Queue Family
    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues.data());
        for (uint32_t i = 0; i < count; i++) {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                g_QueueFamily = i;
                break;
            }
        }
    }

    // Create Logical Device
    {
        int device_extension_count = 1;
        const char* device_extensions[] = { "VK_KHR_swapchain" };
        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = 1;
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extension_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

static void CleanupVulkan() {
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

int main(int argc, char** argv) {
    agk::StudioConfig config;
    if (!config.Load("tracker_config.json")) {
        config.Save("tracker_config.json"); // Create default
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Window* window = SDL_CreateWindow("Autogeek Tracker Studio", config.window.width, config.window.height, window_flags);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Vulkan Setup
    uint32_t extensions_count = 0;
    const char *const * extensions = SDL_Vulkan_GetInstanceExtensions(&extensions_count);
    SetupVulkan(extensions, extensions_count);

    // Create Window Surface
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, g_Instance, g_Allocator, &surface)) {
        std::cerr << "Failed to create Vulkan surface" << std::endl;
        return -1;
    }

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
    if (res != VK_TRUE) {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR }; // VSync
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, present_modes, IM_ARRAYSIZE(present_modes));

    // Create SwapChain, RenderPass, etc
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, w, h, g_MinImageCount);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Optional for multi-window

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.RenderPass = wd->RenderPass;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    // Recording Engine Integration
    auto engine = agk::CreateEngine();
    engine->Initialize();

    agk::LiveMonitor liveMonitor(g_Device, g_PhysicalDevice, g_DescriptorPool, g_QueueFamily, g_Queue);
    liveMonitor.Initialize(config.preview.width, config.preview.height);

    agk::SessionReplayer replayer(g_Device, g_PhysicalDevice, g_DescriptorPool, g_QueueFamily, g_Queue);

    engine->SetPreviewCallback([&liveMonitor](const agk::PreviewFrame& frame) {
        liveMonitor.OnPreviewFrame(frame);
    }, config.preview.width, config.preview.height);

    // Main Loop
    bool done = false;
    bool isRecording = false;
    uint64_t lastTime = SDL_GetTicks();
    std::string selectedSession = "";
    std::vector<std::string> sessions;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) done = true;
        }

        // Resize
        if (g_SwapChainRebuild) {
            int width, height;
            SDL_GetWindowSize(window, &width, &height);
            if (width > 0 && height > 0) {
                ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
                g_MainWindowData.FrameIndex = 0;
                g_SwapChainRebuild = false;
            }
        }

        // GUI
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        uint64_t currentTime = SDL_GetTicks();
        double deltaTime = (currentTime - lastTime) / 1000.0;
        lastTime = currentTime;

        liveMonitor.UpdateTexture();
        replayer.Update(deltaTime);

        // --- Studio UI ---
        {
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

            ImGui::Begin("Autogeek AI - Recording Dash", nullptr, ImGuiWindowFlags_MenuBar);
            
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Exit")) done = true;
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGui::Text("Autogeek Tracker Studio - v0.1");
            ImGui::Separator();

            if (ImGui::BeginTabBar("StudioTabs")) {
                if (ImGui::BeginTabItem("Live Monitoring")) {
                    if (ImGui::Button(isRecording ? "Stop Recording" : "Start Recording")) {
                        if (isRecording) engine->Stop();
                        else engine->Start();
                        isRecording = !isRecording;
                    }

                    ImGui::SameLine();
                    ImGui::Text("Status: %s", isRecording ? "RECORDING" : "IDLE");

                    ImGui::BeginChild("LiveView", ImVec2((float)config.preview.width, (float)config.preview.height), true);
                    if (liveMonitor.GetTextureID()) {
                        ImGui::Image(liveMonitor.GetTextureID(), ImVec2((float)config.preview.width, (float)config.preview.height));
                    } else {
                        ImGui::Text("Waiting for live stream...");
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Session Replayer")) {
                    if (ImGui::Button("Refresh Sessions")) {
                        sessions.clear();
                        if (std::filesystem::exists(config.last_session_path)) {
                            for (const auto& entry : std::filesystem::directory_iterator(config.last_session_path)) {
                                if (entry.is_directory()) sessions.push_back(entry.path().string());
                            }
                        }
                    }

                    static int item_current = 0;
                    if (!sessions.empty()) {
                        std::vector<const char*> session_names;
                        for (const auto& s : sessions) session_names.push_back(s.c_str());
                        
                        if (ImGui::Combo("Select Session", &item_current, session_names.data(), (int)session_names.size())) {
                            selectedSession = sessions[item_current];
                        }

                        if (ImGui::Button("Load Session")) {
                            replayer.OpenSession(selectedSession);
                        }
                    }

                    ImGui::Separator();
                    replayer.RenderUI();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            wd->ClearValue.color.float32[0] = 0.1f;
            wd->ClearValue.color.float32[1] = 0.1f;
            wd->ClearValue.color.float32[2] = 0.1f;
            wd->ClearValue.color.float32[3] = 1.0f;

            // Vulkan Render Frame
            VkResult err;
            uint32_t image_index;
            VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->FrameIndex].ImageAcquiredSemaphore;
            VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->FrameIndex].RenderCompleteSemaphore;
            err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &image_index);
            if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
                g_SwapChainRebuild = true;
                continue;
            }
            check_vk_result(err);

            ImGui_ImplVulkanH_Frame* fd = &wd->Frames[image_index];
            {
                err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
                check_vk_result(err);
                err = vkResetFences(g_Device, 1, &fd->Fence);
                check_vk_result(err);
            }
            {
                err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
                check_vk_result(err);
                VkCommandBufferBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
                check_vk_result(err);
            }
            {
                VkRenderPassBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                info.renderPass = wd->RenderPass;
                info.framebuffer = fd->Framebuffer;
                info.renderArea.extent.width = wd->Width;
                info.renderArea.extent.height = wd->Height;
                info.clearValueCount = 1;
                info.pClearValues = &wd->ClearValue;
                vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
            }

            ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

            vkCmdEndRenderPass(fd->CommandBuffer);

            {
                VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                info.waitSemaphoreCount = 1;
                info.pWaitSemaphores = &image_acquired_semaphore;
                info.pWaitDstStageMask = &wait_stage;
                info.commandBufferCount = 1;
                info.pCommandBuffers = &fd->CommandBuffer;
                info.signalSemaphoreCount = 1;
                info.pSignalSemaphores = &render_complete_semaphore;

                err = vkEndCommandBuffer(fd->CommandBuffer);
                check_vk_result(err);
                err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
                check_vk_result(err);
            }

            // Present
            {
                VkPresentInfoKHR info = {};
                info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                info.waitSemaphoreCount = 1;
                info.pWaitSemaphores = &render_complete_semaphore;
                info.swapchainCount = 1;
                info.pSwapchains = &wd->Swapchain;
                info.pImageIndices = &image_index;
                err = vkQueuePresentKHR(g_Queue, &info);
                if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
                    g_SwapChainRebuild = true;
                    continue;
                }
                check_vk_result(err);
                wd->FrameIndex = (wd->FrameIndex + 1) % g_MinImageCount;
            }
        }
    }

    // Cleanup
    VkResult err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
    CleanupVulkan();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
