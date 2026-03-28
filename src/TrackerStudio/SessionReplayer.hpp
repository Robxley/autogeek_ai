#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <vulkan/vulkan.h>
#include "imgui.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace agk {

    class SessionReplayer {
    public:
        SessionReplayer(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue);
        ~SessionReplayer();

        bool OpenSession(const std::string& sessionPath);
        void Close();

        void Update(double deltaTime);
        void RenderUI();

        ImTextureID GetTextureID() const { return (ImTextureID)m_descriptorSet; }
        bool IsLoaded() const { return m_fmtCtx != nullptr; }

    private:
        bool DecodeNextFrame();
        void CreateVulkanResources(int width, int height);
        void DestroyVulkanResources();
        void UpdateVulkanTexture(const uint8_t* data, int linesize);

        // FFmpeg
        AVFormatContext* m_fmtCtx = nullptr;
        AVCodecContext* m_codecCtx = nullptr;
        AVFrame* m_frame = nullptr;
        AVFrame* m_frameRGBA = nullptr;
        SwsContext* m_swsCtx = nullptr;
        int m_videoStreamIndex = -1;

        // Vulkan
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

        // Playback State
        bool m_isPlaying = false;
        double m_currentTime = 0.0;
        double m_frameDuration = 0.0;
        int m_width = 0;
        int m_height = 0;
    };

} // namespace agk
