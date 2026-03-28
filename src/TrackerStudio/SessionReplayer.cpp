#include "SessionReplayer.hpp"
#include "imgui_impl_vulkan.h"
#include <iostream>
#include <cstring>
#include <chrono>

namespace agk {

    SessionReplayer::SessionReplayer(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue)
        : m_device(device), m_physDevice(physDevice), m_pool(pool), m_queueFamily(queueFamily), m_queue(queue) {}

    SessionReplayer::~SessionReplayer() {
        Close();
    }

    bool SessionReplayer::OpenSession(const std::string& sessionPath) {
        Close();
        std::string videoPath = sessionPath + "/video.mkv";
        
        if (avformat_open_input(&m_fmtCtx, videoPath.c_str(), nullptr, nullptr) != 0) {
            std::cerr << "[Replayer] Failed to open video: " << videoPath << std::endl;
            return false;
        }

        if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) return false;

        for (uint32_t i = 0; i < m_fmtCtx->nb_streams; i++) {
            if (m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                m_videoStreamIndex = i;
                break;
            }
        }

        if (m_videoStreamIndex == -1) return false;

        AVCodecParameters* codecPar = m_fmtCtx->streams[m_videoStreamIndex]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
        m_codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(m_codecCtx, codecPar);
        if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) return false;

        m_width = m_codecCtx->width;
        m_height = m_codecCtx->height;
        m_frameDuration = av_q2d(m_fmtCtx->streams[m_videoStreamIndex]->avg_frame_rate);
        if (m_frameDuration > 0) m_frameDuration = 1.0 / m_frameDuration;
        else m_frameDuration = 1.0 / 60.0; // Default

        m_frame = av_frame_alloc();
        m_frameRGBA = av_frame_alloc();
        m_frameRGBA->width = m_width;
        m_frameRGBA->height = m_height;
        m_frameRGBA->format = AV_PIX_FMT_BGRA;
        av_frame_get_buffer(m_frameRGBA, 0);

        m_swsCtx = sws_getContext(m_width, m_height, m_codecCtx->pix_fmt,
                                  m_width, m_height, AV_PIX_FMT_BGRA,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);

        CreateVulkanResources(m_width, m_height);
        m_isPlaying = false;
        m_currentTime = 0.0;

        // Decode first frame
        DecodeNextFrame();

        return true;
    }

    void SessionReplayer::Close() {
        DestroyVulkanResources();
        if (m_swsCtx) sws_freeContext(m_swsCtx);
        if (m_frame) av_frame_free(&m_frame);
        if (m_frameRGBA) av_frame_free(&m_frameRGBA);
        if (m_codecCtx) avcodec_free_context(&m_codecCtx);
        if (m_fmtCtx) avformat_close_input(&m_fmtCtx);
        
        m_fmtCtx = nullptr;
        m_codecCtx = nullptr;
        m_frame = nullptr;
        m_frameRGBA = nullptr;
        m_swsCtx = nullptr;
    }

    void SessionReplayer::Update(double deltaTime) {
        if (!m_isPlaying || !m_fmtCtx) return;

        m_currentTime += deltaTime;
        if (m_currentTime >= m_frameDuration) {
            if (DecodeNextFrame()) {
                m_currentTime = 0.0;
            } else {
                m_isPlaying = false; // EOF
            }
        }
    }

    bool SessionReplayer::DecodeNextFrame() {
        AVPacket* pkt = av_packet_alloc();
        bool frameReady = false;

        while (av_read_frame(m_fmtCtx, pkt) == 0) {
            if (pkt->stream_index == m_videoStreamIndex) {
                if (avcodec_send_packet(m_codecCtx, pkt) == 0) {
                    if (avcodec_receive_frame(m_codecCtx, m_frame) == 0) {
                        // Convert to RGBA
                        sws_scale(m_swsCtx, m_frame->data, m_frame->linesize, 0, m_height, m_frameRGBA->data, m_frameRGBA->linesize);
                        UpdateVulkanTexture(m_frameRGBA->data[0], m_frameRGBA->linesize[0]);
                        frameReady = true;
                        av_packet_unref(pkt);
                        av_packet_free(&pkt);
                        return true;
                    }
                }
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
        return false;
    }

    void SessionReplayer::RenderUI() {
        if (!IsLoaded()) {
            ImGui::Text("No session loaded.");
            return;
        }

        if (ImGui::Button(m_isPlaying ? "Pause" : "Play")) {
            m_isPlaying = !m_isPlaying;
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            m_isPlaying = false;
            av_seek_frame(m_fmtCtx, m_videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
            m_currentTime = 0.0;
            DecodeNextFrame();
        }

        ImGui::BeginChild("ReplayView", ImVec2(0, 0), true);
        if (m_descriptorSet) {
            float aspect = (float)m_width / (float)m_height;
            float targetW = ImGui::GetContentRegionAvail().x;
            float targetH = targetW / aspect;
            ImGui::Image(GetTextureID(), ImVec2(targetW, targetH));
        }
        ImGui::EndChild();
    }

    void SessionReplayer::CreateVulkanResources(int width, int height) {
        // Implementation is nearly identical to LiveMonitor::CreateTexture
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vkCreateImage(m_device, &imageInfo, nullptr, &m_image);

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(m_device, m_image, &memReq);
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;

        VkPhysicalDeviceMemoryProperties memProp;
        vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memProp);
        for (uint32_t i = 0; i < memProp.memoryTypeCount; i++) {
            if ((memReq.memoryTypeBits & (1 << i)) && (memProp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }
        vkAllocateMemory(m_device, &allocInfo, nullptr, &m_memory);
        vkBindImageMemory(m_device, m_image, m_memory, 0);

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageView);

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler);

        m_descriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void SessionReplayer::UpdateVulkanTexture(const uint8_t* data, int linesize) {
        // Simplified staging and upload (Identical pattern to LiveMonitor::UpdateTexture)
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        VkDeviceSize bufferSize = (VkDeviceSize)m_width * m_height * 4;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer);

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReq);
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;

        VkPhysicalDeviceMemoryProperties memProp;
        vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memProp);
        for (uint32_t i = 0; i < memProp.memoryTypeCount; i++) {
            if ((memReq.memoryTypeBits & (1 << i)) && (memProp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }
        vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory);
        vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);

        void* mapped;
        vkMapMemory(m_device, stagingMemory, 0, bufferSize, 0, &mapped);
        memcpy(mapped, data, bufferSize);
        vkUnmapMemory(m_device, stagingMemory);

        // Copy
        VkCommandPool cmdPool;
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_queueFamily;
        vkCreateCommandPool(m_device, &poolInfo, nullptr, &cmdPool);

        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = cmdPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {(uint32_t)m_width, (uint32_t)m_height, 1};
        vkCmdCopyBufferToImage(cmd, stagingBuffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_queue);

        vkFreeCommandBuffers(m_device, cmdPool, 1, &cmd);
        vkDestroyCommandPool(m_device, cmdPool, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
    }

    void SessionReplayer::DestroyVulkanResources() {
        if (m_descriptorSet) {
            ImGui_ImplVulkan_RemoveTexture(m_descriptorSet);
            m_descriptorSet = VK_NULL_HANDLE;
        }
        if (m_sampler) vkDestroySampler(m_device, m_sampler, nullptr);
        if (m_imageView) vkDestroyImageView(m_device, m_imageView, nullptr);
        if (m_image) vkDestroyImage(m_device, m_image, nullptr);
        if (m_memory) vkFreeMemory(m_device, m_memory, nullptr);
    }

} // namespace agk
