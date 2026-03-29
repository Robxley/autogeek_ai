#pragma once

#include "../StudioConfig.hpp"
#include <agk/RecordingEngine/IRecordingEngine.hpp>
#include "../SessionReplayer.hpp"
#include <map>
#include <vulkan/vulkan.h>

namespace agk {
namespace Widgets {
    struct SessionTexture; // Opaque forward declaration

    class SessionExplorer {
    public:
        SessionExplorer(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue);
        ~SessionExplorer();

        void Render(StudioConfig& config, std::shared_ptr<IRecordingEngine> engine, SessionReplayer* replayer);

    private:
        SessionTexture* LoadThumbnail(const std::string& filepath);
        void DestroyTexture(SessionTexture* tex);

        VkDevice m_device;
        VkPhysicalDevice m_physDevice;
        VkDescriptorPool m_pool;
        uint32_t m_queueFamily;
        VkQueue m_queue;

        std::map<std::string, SessionTexture*> m_thumbnailCache;
    };
}
}
