#include "StudioApp.hpp"
#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h" // For DockBuilder
#include "implot.h"
#include "portable-file-dialogs.h"

// FontAwesome integration
#include "IconsFontAwesome6.h"

#include "LiveMonitor.hpp"
#include "SessionReplayer.hpp"
#include "Widgets/SessionExplorer.hpp"
#include "Widgets/RecordingConfigUI.hpp"
#include "Widgets/AppLogUI.hpp"
#include <agk/RecordingEngine/Log.hpp>

namespace agk {

    static void check_vk_result(VkResult err) {
        if (err == 0) return;
        AGK_ERROR("[Vulkan] Error: VkResult = {}", (int)err);
        if (err < 0) abort();
    }

    StudioApp::StudioApp() {}

    StudioApp::~StudioApp() {
        Shutdown();
    }

    bool StudioApp::Initialize() {
        agk::Log::Init();
        m_logUI = std::make_shared<Widgets::AppLogUI>();
        agk::Log::AddSink(m_logUI);

        if (!m_config.Load("tracker_config.json")) {
            m_config.Save("tracker_config.json");
        }

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            AGK_CRITICAL("SDL_Init Error: {}", SDL_GetError());
            return false;
        }

        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        m_window = SDL_CreateWindow("Autogeek Tracker Studio", m_config.window.width, m_config.window.height, window_flags);
        
        uint32_t extensions_count = 0;
        const char *const * extensions = SDL_Vulkan_GetInstanceExtensions(&extensions_count);
        SetupVulkan(extensions, extensions_count);

        VkSurfaceKHR surface;
        SDL_Vulkan_CreateSurface(m_window, m_instance, m_allocator, &surface);
        
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        ImGui_ImplVulkanH_Window* wd = &m_mainWindowData;
        wd->Surface = surface;

        // Formats & Present modes (Simplified from previous main.cpp)
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(m_physDevice, wd->Surface, requestSurfaceImageFormat, 1, requestSurfaceColorSpace);
        VkPresentModeKHR present_modes[] = { m_config.window.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR };
        wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(m_physDevice, wd->Surface, present_modes, 1);

        ImGui_ImplVulkanH_CreateOrResizeWindow(m_instance, m_physDevice, m_device, wd, m_queueFamily, m_allocator, w, h, m_minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_window);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.ApiVersion = VK_API_VERSION_1_0;
        init_info.Instance = m_instance;
        init_info.PhysicalDevice = m_physDevice;
        init_info.Device = m_device;
        init_info.QueueFamily = m_queueFamily;
        init_info.Queue = m_queue;
        init_info.PipelineCache = m_pipelineCache;
        init_info.DescriptorPool = m_descriptorPool;
        init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.MinImageCount = m_minImageCount;
        init_info.ImageCount = wd->ImageCount;
        init_info.Allocator = m_allocator;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info);

        SetupFontAwesome();

        m_engine = agk::CreateEngine();
        m_engine->Initialize();

        m_liveMonitor = std::make_unique<LiveMonitor>(m_device, m_physDevice, m_descriptorPool, m_queueFamily, m_queue);
        m_liveMonitor->Initialize(m_config.preview.width, m_config.preview.height);

        m_replayer = std::make_unique<SessionReplayer>(m_device, m_physDevice, m_descriptorPool, m_queueFamily, m_queue);

        LiveMonitor* lm = m_liveMonitor.get();
        m_engine->SetPreviewCallback([lm](const agk::PreviewFrame& frame) {
            lm->OnPreviewFrame(frame);
        }, m_config.preview.width, m_config.preview.height);

        return true;
    }

    void StudioApp::SetupDockspace() {
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        
        if (m_firstLayout) { // First time or ini reset
            m_firstLayout = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            auto dock_main_id = dockspace_id;
            auto dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            auto dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            auto dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Session Explorer", dock_id_left);
            ImGui::DockBuilderDockWindow("Live Monitoring", dock_main_id);
            ImGui::DockBuilderDockWindow("Session Replayer", dock_main_id); // Under same tab as Live Monitoring
            ImGui::DockBuilderDockWindow("Recording Settings", dock_id_right);
            ImGui::DockBuilderDockWindow("Studio Settings", dock_id_right);
            ImGui::DockBuilderDockWindow("Timeline & Logs", dock_id_bottom);
            
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    void StudioApp::SetupFontAwesome() {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontDefault();

        float baseFontSize = 16.0f;
        float iconFontSize = baseFontSize * 2.0f / 3.0f; // FontAwesome icons are usually slightly larger
        
        // Merge in icons from Font Awesome
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        
        std::string fontPath = "assets/fonts/fa-solid-900.ttf";
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), iconFontSize, &icons_config, icons_ranges);
        
        // Build the font atlas
        // Automatically handled by Vulkan backend on NewFrame
    }

    void StudioApp::Run() {
        bool done = false;
        uint64_t lastTime = SDL_GetTicks();
        ImGui_ImplVulkanH_Window* wd = &m_mainWindowData;

        while (!done) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT) done = true;
                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_window)) done = true;
            }

            if (m_swapChainRebuild) {
                int width, height;
                SDL_GetWindowSize(m_window, &width, &height);
                if (width > 0 && height > 0) {
                    ImGui_ImplVulkan_SetMinImageCount(m_minImageCount);
                    ImGui_ImplVulkanH_CreateOrResizeWindow(m_instance, m_physDevice, m_device, wd, m_queueFamily, m_allocator, width, height, m_minImageCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
                    wd->FrameIndex = 0;
                    m_swapChainRebuild = false;
                }
            }

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            uint64_t currentTime = SDL_GetTicks();
            double deltaTime = (currentTime - lastTime) / 1000.0;
            lastTime = currentTime;

            m_liveMonitor->UpdateTexture();
            m_replayer->Update(deltaTime);

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
            
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("MainWorkspace", nullptr, window_flags);
            ImGui::PopStyleVar();

            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
            SetupDockspace(); // Only runs on first layout

            // --- Panel: Session Explorer ---
            ImGui::Begin("Session Explorer");
            agk::Widgets::SessionExplorer::Render(m_config, m_engine, m_replayer.get());
            ImGui::End();

            // --- Panel: Live Monitoring ---
            ImGui::Begin("Live Monitoring");
            if (ImGui::Button(m_isRecording ? ICON_FA_STOP " Stop Recording" : ICON_FA_CIRCLE " Start Recording")) {
                if (m_isRecording) m_engine->Stop();
                else m_engine->Start();
                m_isRecording = !m_isRecording;
            }
            ImGui::SameLine();
            ImGui::Text("Status: %s", m_isRecording ? "RECORDING" : "IDLE");

            if (m_liveMonitor->GetTextureID()) {
                ImGui::Image(m_liveMonitor->GetTextureID(), ImVec2((float)m_config.preview.width, (float)m_config.preview.height));
            } else {
                ImGui::Text("Waiting for live stream...");
            }
            ImGui::End();

            // --- Panel: Session Replayer ---
            ImGui::Begin("Session Replayer");
            m_replayer->RenderUI(); // Replayer internal UI
            ImGui::End();

            // --- Panel: Configs ---
            ImGui::Begin("Recording Settings");
            ImGui::Text(ICON_FA_WRENCH " Recording Settings");
            agk::Widgets::RecordingConfigUI::Render(m_engine);
            ImGui::End();

            ImGui::Begin("Studio Settings");
            ImGui::Text(ICON_FA_DESKTOP " Editor Config");
            ImGui::End();

            m_logUI->Render(ICON_FA_TERMINAL " Timeline & Logs");

            ImGui::End(); // End MainWorkspace

            ImGui::Render();
            ImDrawData* draw_data = ImGui::GetDrawData();
            if (draw_data->DisplaySize.x > 0.0f && draw_data->DisplaySize.y > 0.0f) {
                FrameRender(draw_data);
                FramePresent();
            }
        }
    }

    void StudioApp::FrameRender(ImDrawData* draw_data) {
        ImGui_ImplVulkanH_Window* wd = &m_mainWindowData;
        VkResult err;
        uint32_t image_index;
        
        wd->ClearValue.color.float32[0] = 0.15f; 
        wd->ClearValue.color.float32[1] = 0.15f; 
        wd->ClearValue.color.float32[2] = 0.15f; 
        wd->ClearValue.color.float32[3] = 1.0f;

        err = vkAcquireNextImageKHR(m_device, wd->Swapchain, UINT64_MAX, wd->FrameSemaphores[wd->FrameIndex].ImageAcquiredSemaphore, VK_NULL_HANDLE, &image_index);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) { m_swapChainRebuild = true; return; }
        
        ImGui_ImplVulkanH_Frame* fd = &wd->Frames[image_index];
        vkWaitForFences(m_device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &fd->Fence);
        vkResetCommandPool(m_device, fd->CommandPool, 0);
        
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(fd->CommandBuffer, &info);
        
        VkRenderPassBeginInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_info.renderPass = wd->RenderPass;
        rp_info.framebuffer = fd->Framebuffer;
        rp_info.renderArea.extent.width = wd->Width;
        rp_info.renderArea.extent.height = wd->Height;
        rp_info.clearValueCount = 1;
        rp_info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
        
        ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
        vkCmdEndRenderPass(fd->CommandBuffer);

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &wd->FrameSemaphores[wd->FrameIndex].ImageAcquiredSemaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &fd->CommandBuffer;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &wd->FrameSemaphores[wd->FrameIndex].RenderCompleteSemaphore;
        
        vkEndCommandBuffer(fd->CommandBuffer);
        vkQueueSubmit(m_queue, 1, &submit_info, fd->Fence);
    }

    void StudioApp::FramePresent() {
        if (m_swapChainRebuild) return;
        ImGui_ImplVulkanH_Window* wd = &m_mainWindowData;
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &wd->FrameSemaphores[wd->FrameIndex].RenderCompleteSemaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &wd->Swapchain;
        
        uint32_t image_index = wd->FrameIndex;
        info.pImageIndices = &image_index;
        
        VkResult err = vkQueuePresentKHR(m_queue, &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) m_swapChainRebuild = true;
        
        wd->FrameIndex = (wd->FrameIndex + 1) % m_minImageCount;
    }

    void StudioApp::SetupVulkan(const char* const* extensions, uint32_t extensions_count) {
        VkResult err;
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = extensions_count;
        create_info.ppEnabledExtensionNames = extensions;
        err = vkCreateInstance(&create_info, m_allocator, &m_instance);
        check_vk_result(err);

        uint32_t gpu_count;
        vkEnumeratePhysicalDevices(m_instance, &gpu_count, nullptr);
        std::vector<VkPhysicalDevice> gpus(gpu_count);
        vkEnumeratePhysicalDevices(m_instance, &gpu_count, gpus.data());
        m_physDevice = gpus[0]; 

        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &count, queues.data());
        for (uint32_t i = 0; i < count; i++) {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { m_queueFamily = i; break; }
        }

        const char* device_extensions[] = { "VK_KHR_swapchain" };
        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = m_queueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo dev_info = {};
        dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dev_info.queueCreateInfoCount = 1;
        dev_info.pQueueCreateInfos = queue_info;
        dev_info.enabledExtensionCount = 1;
        dev_info.ppEnabledExtensionNames = device_extensions;
        err = vkCreateDevice(m_physDevice, &dev_info, m_allocator, &m_device);
        check_vk_result(err);
        vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(m_device, &pool_info, m_allocator, &m_descriptorPool);
        check_vk_result(err);
    }

    void StudioApp::Shutdown() {
        if (!m_device) return;
        vkDeviceWaitIdle(m_device);
        
        m_liveMonitor.reset();
        m_replayer.reset();
        m_engine.reset();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        ImGui_ImplVulkanH_DestroyWindow(m_instance, m_device, &m_mainWindowData, m_allocator);
        
        vkDestroyDescriptorPool(m_device, m_descriptorPool, m_allocator);
        vkDestroyDevice(m_device, m_allocator);
        vkDestroyInstance(m_instance, m_allocator);
        
        m_device = VK_NULL_HANDLE;

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
        SDL_Quit();
    }
} // namespace agk
