#pragma once

#include <string>
#include <memory>
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "StudioConfig.hpp"
#include <agk/RecordingEngine/IRecordingEngine.hpp>

// Forward declarations for widgets
namespace agk {
    class LiveMonitor;
    class SessionReplayer;
    namespace Widgets { 
        class AppLogUI; 
        class SessionExplorer;
    }
}

namespace agk {

    class StudioApp {
    public:
        StudioApp();
        ~StudioApp();

        bool Initialize();
        void Run();
        void Shutdown();

    private:
        void SetupVulkan(const char* const* extensions, uint32_t extensions_count);
        void CleanupVulkan();
        void FrameRender(ImDrawData* draw_data);
        void FramePresent();
        void SetupDockspace();
        void SetupFontAwesome();
        void SetupImGuiStyle();

        // SDL & Vulkan Core
        SDL_Window* m_window = nullptr;
        VkAllocationCallbacks* m_allocator = nullptr;
        VkInstance m_instance = VK_NULL_HANDLE;
        VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        uint32_t m_queueFamily = (uint32_t)-1;
        VkQueue m_queue = VK_NULL_HANDLE;
        VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        
        ImGui_ImplVulkanH_Window m_mainWindowData;
        int m_minImageCount = 2;
        bool m_swapChainRebuild = false;

        // Config & Engine
        StudioConfig m_config;
        std::shared_ptr<IRecordingEngine> m_engine;

        // Widgets / Modules
        std::unique_ptr<LiveMonitor> m_liveMonitor;
        std::unique_ptr<SessionReplayer> m_replayer;
        std::unique_ptr<Widgets::SessionExplorer> m_sessionExplorer;
        std::shared_ptr<Widgets::AppLogUI> m_logUI;

        // UI State
        std::string m_selectedSession;
        bool m_firstLayout = true;
    };

} // namespace agk
