#include "SessionReplayer.hpp"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include <agk/RecordingEngine/Log.hpp>
#include <cstring>
#include <chrono>
#include <fstream>

namespace agk {

    SessionReplayer::SessionReplayer(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue)
        : m_device(device), m_physDevice(physDevice), m_pool(pool), m_queueFamily(queueFamily), m_queue(queue) {}

    SessionReplayer::~SessionReplayer() {
        Close();
    }

    bool SessionReplayer::OpenSession(const std::string& sessionPath) {
        Close();
        std::string videoPath = sessionPath + "/video.mkv";
        AVFormatContext* fmtCtxRaw = nullptr;
        if (avformat_open_input(&fmtCtxRaw, videoPath.c_str(), nullptr, nullptr) != 0) {
            AGK_ERROR("[Replayer] Failed to open video: {}", videoPath);
            return false;
        }
        m_fmtCtx.reset(fmtCtxRaw);

        if (avformat_find_stream_info(m_fmtCtx.get(), nullptr) < 0) return false;

        for (uint32_t i = 0; i < m_fmtCtx->nb_streams; i++) {
            if (m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                m_videoStreamIndex = i;
                break;
            }
        }

        if (m_videoStreamIndex == -1) return false;

        AVCodecParameters* codecPar = m_fmtCtx->streams[m_videoStreamIndex]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
        m_codecCtx.reset(avcodec_alloc_context3(codec));
        avcodec_parameters_to_context(m_codecCtx.get(), codecPar);
        if (avcodec_open2(m_codecCtx.get(), codec, nullptr) < 0) return false;

        m_width = m_codecCtx->width;
        m_height = m_codecCtx->height;
        m_frameDuration = av_q2d(m_fmtCtx->streams[m_videoStreamIndex]->avg_frame_rate);
        if (m_frameDuration > 0) m_frameDuration = 1.0 / m_frameDuration;
        else m_frameDuration = 1.0 / 60.0; // Default

        m_frame.reset(av_frame_alloc());
        m_frameRGBA.reset(av_frame_alloc());
        m_frameRGBA->width = m_width;
        m_frameRGBA->height = m_height;
        m_frameRGBA->format = AV_PIX_FMT_BGRA;
        av_frame_get_buffer(m_frameRGBA.get(), 0);

        m_swsCtx.reset(sws_getContext(m_width, m_height, m_codecCtx->pix_fmt,
                                  m_width, m_height, AV_PIX_FMT_BGRA,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr));

        CreateVulkanResources(m_width, m_height);
        m_isPlaying = false;
        m_currentTime = 0.0;

        LoadEvents(sessionPath + "/events.jsonl");

        m_currentSessionPath = sessionPath;
        // Decode first frame
        DecodeNextFrame();

        return true;
    }

    void SessionReplayer::Close() {
        DestroyVulkanResources();
        m_swsCtx.reset();
        m_frame.reset();
        m_frameRGBA.reset();
        m_codecCtx.reset();
        m_fmtCtx.reset();
        
        m_currentSessionPath.clear();
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

        while (av_read_frame(m_fmtCtx.get(), pkt) == 0) {
            if (pkt->stream_index == m_videoStreamIndex) {
                if (avcodec_send_packet(m_codecCtx.get(), pkt) == 0) {
                    if (avcodec_receive_frame(m_codecCtx.get(), m_frame.get()) == 0) {
                        // Convert to RGBA
                        sws_scale(m_swsCtx.get(), m_frame->data, m_frame->linesize, 0, m_height, m_frameRGBA->data, m_frameRGBA->linesize);
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
            av_seek_frame(m_fmtCtx.get(), m_videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
            m_currentTime = 0.0;
            DecodeNextFrame();
        }

        ImGui::BeginChild("ReplayView", ImVec2(0, ImGui::GetContentRegionAvail().y - 120), true);
        if (m_descriptorSet) {
            float aspect = (float)m_width / (float)m_height;
            float targetW = ImGui::GetContentRegionAvail().x;
            float targetH = targetW / aspect;
            
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImGui::Image(GetTextureID(), ImVec2(targetW, targetH));
            ImVec2 p1 = ImVec2(p0.x + targetW, p0.y + targetH);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Draw Overlays synchronized with m_currentTime
            for (const auto& ev : m_events) {
                // Forgiveness window of 0.3s for visual tracking
                if (ev.time_sec >= m_currentTime - 0.3 && ev.time_sec <= m_currentTime + 0.1) {
                    if (ev.type == "mouse_click") {
                        // Normalize back from screen logic
                        // In Autogeek event serializer: x, y are screen coords. But if it's monitor_crop, they might be offset.
                        // Assuming normalized 0.0 - 1.0 or raw resolution. For safety, wrap to video resolution ratio:
                        float nx = (float)ev.x / (float)1920; 
                        float ny = (float)ev.y / (float)1080;
                        float cx = p0.x + (nx * targetW);
                        float cy = p0.y + (ny * targetH);
                        // ImDrawList AddCircle
                        drawList->AddCircleFilled(ImVec2(cx, cy), 15.0f * (1.0f - (float)(m_currentTime - ev.time_sec)/0.3f), IM_COL32(255, 0, 0, 150));
                        drawList->AddCircle(ImVec2(cx, cy), 20.0f, IM_COL32(255, 0, 0, 255), 0, 3.0f);
                    } else if (ev.type == "keyboard") {
                        drawList->AddText(ImVec2(p0.x + 10, p0.y + targetH - 30), IM_COL32(0, 255, 0, 255), ev.key_name.c_str());
                    }
                }
            }
        }
        ImGui::EndChild();

        // Timeline (ImPlot Gantt style)
        if (ImPlot::BeginPlot("Events Timeline", ImVec2(-1, 0), ImPlotFlags_NoLegend)) {
            ImPlot::SetupAxes("Time (s)", "Input", 0, ImPlotAxisFlags_NoDecorations);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, m_currentTime + 5.0, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 2, ImGuiCond_Always);

            // current time marker
            double vlsX[] = { m_currentTime, m_currentTime };
            double vlsY[] = { -1.0, 2.0 };
            ImPlot::PlotLine("Cursor", vlsX, vlsY, 2);

            std::vector<double> xsClick, ysClick;
            std::vector<double> xsKey, ysKey;
            for (const auto& ev : m_events) {
                if (ev.type == "mouse_click") { xsClick.push_back(ev.time_sec); ysClick.push_back(1.0); }
                else if (ev.type == "keyboard") { xsKey.push_back(ev.time_sec); ysKey.push_back(0.0); }
            }

            if (!xsClick.empty()) ImPlot::PlotScatter("Clicks", xsClick.data(), ysClick.data(), xsClick.size());
            if (!xsKey.empty()) ImPlot::PlotScatter("Keys", xsKey.data(), ysKey.data(), xsKey.size());

            ImPlot::EndPlot();
        }
    }

    bool SessionReplayer::LoadEvents(const std::string& eventsPath) {
        m_events.clear();
        std::ifstream file(eventsPath);
        if (!file.is_open()) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            try {
                auto j = nlohmann::json::parse(line);
                SessionEvent ev;
                ev.type = j.value("type", "");
                ev.timestamp = j.value("timestamp", 0ull);
                
                if (m_sessionStartTime == 0) m_sessionStartTime = ev.timestamp;
                ev.time_sec = (ev.timestamp - m_sessionStartTime) / 1000000.0; // microseconds to seconds
                
                if (ev.type == "mouse_click") {
                    ev.x = j.value("x", 0);
                    ev.y = j.value("y", 0);
                    ev.button = j.value("button", 0);
                } else if (ev.type == "keyboard") {
                    ev.key_code = j.value("key_code", 0);
                    ev.key_name = j.value("key_name", "UNKNOWN");
                }
                m_events.push_back(ev);
            } catch (...) {}
        }
        return true;
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
