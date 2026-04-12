#pragma once

#include <vulkan/vulkan.h>
#include <agk/RecordingEngine/IRecordingEngine.hpp>
#include <vector>
#include <mutex>
#include <deque>
#include "imgui.h"

namespace agk {

    struct TelemetryFrame {
        double time;
        float fps_ratio;
        float audio;
        float mouse;
        float keyboard;
    };

    class LiveMonitor {
    public:
        LiveMonitor(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue);
        ~LiveMonitor();

        void Initialize(int width, int height);
        void UpdateTexture();
        ImTextureID GetTextureID() const { return (ImTextureID)m_descriptorSet; }

        void OnPreviewFrame(const PreviewFrame& frame);
        
        void PushTelemetry(const EngineStats& stats, float targetFPS);
        void DrawTelemetryUI();

    private:
        void CreateTexture(int width, int height);
        void DestroyTexture();

        VkDevice m_device;
        VkPhysicalDevice m_physDevice;
        VkDescriptorPool m_pool;
        uint32_t m_queueFamily;
        VkQueue m_queue;

        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

        int m_width = 0;
        int m_height = 0;
        std::vector<uint8_t> m_pendingData;
        std::mutex m_dataMutex;
        bool m_hasNewData = false;
        
        std::deque<TelemetryFrame> m_telemetryHistory;
        double m_lastTelemetryTime = 0.0;
        EngineStats m_lastStats = {};
        float m_lastTargetFPS = 60.0f;
    };

} // namespace agk
