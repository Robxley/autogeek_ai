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

#include <nlohmann/json.hpp>

namespace agk {

    class SessionReplayer {
    public:
        SessionReplayer(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue);
        ~SessionReplayer();

        bool OpenSession(const std::string& sessionPath);
        void Close();

        void Update(double deltaTime);
        void RenderUI();

        const std::string& GetCurrentSessionPath() const { return m_currentSessionPath; }
        ImTextureID GetTextureID() const { return (ImTextureID)m_descriptorSet; }
        bool IsLoaded() const { return m_fmtCtx != nullptr; }

    private:
        bool DecodeNextFrame();
        void CreateVulkanResources(int width, int height);
        void DestroyVulkanResources();
        void UpdateVulkanTexture(const uint8_t* data, int linesize);

        std::string m_currentSessionPath;

        // FFmpeg Deleters
        struct AVFormatContextDeleter { void operator()(AVFormatContext* ctx) const { if(ctx) avformat_close_input(&ctx); } };
        struct AVCodecContextDeleter { void operator()(AVCodecContext* ctx) const { if(ctx) avcodec_free_context(&ctx); } };
        struct AVFrameDeleter { void operator()(AVFrame* f) const { if(f) av_frame_free(&f); } };
        struct SwsContextDeleter { void operator()(SwsContext* ctx) const { if(ctx) sws_freeContext(ctx); } };

        // FFmpeg RAII Containers
        std::unique_ptr<AVFormatContext, AVFormatContextDeleter> m_fmtCtx;
        std::unique_ptr<AVCodecContext, AVCodecContextDeleter> m_codecCtx;
        std::unique_ptr<AVFrame, AVFrameDeleter> m_frame;
        std::unique_ptr<AVFrame, AVFrameDeleter> m_frameRGBA;
        std::unique_ptr<SwsContext, SwsContextDeleter> m_swsCtx;
        
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

        // Events
        struct SessionEvent {
            std::string type;     // "mouse_click", "keyboard"
            uint64_t timestamp;   // microseconds
            double time_sec;      // normalized to video start
            int x = 0;            // mouse X
            int y = 0;            // mouse Y
            int button = 0;       // Left=1, Right=2
            int key_code = 0;
            std::string key_name;
        };
        std::vector<SessionEvent> m_events;
        uint64_t m_sessionStartTime = 0;
        
        bool LoadEvents(const std::string& eventsPath);
    };

} // namespace agk
