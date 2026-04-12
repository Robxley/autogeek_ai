#include "LiveMonitor.hpp"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#include "IconsFontAwesome6.h"
#include <iostream>
#include <cstring>

namespace agk {

    LiveMonitor::LiveMonitor(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue)
        : m_device(device), m_physDevice(physDevice), m_pool(pool), m_queueFamily(queueFamily), m_queue(queue) {}

    LiveMonitor::~LiveMonitor() {
        DestroyTexture();
    }

    void LiveMonitor::Initialize(int width, int height) {
        if (width != m_width || height != m_height) {
            DestroyTexture();
            CreateTexture(width, height);
            m_width = width;
            m_height = height;
        }
    }

    void LiveMonitor::PushTelemetry(const EngineStats& stats, float targetFPS) {
        m_lastStats = stats;
        m_lastTargetFPS = targetFPS;
    }

    void LiveMonitor::DrawTelemetryUI() {
        // --- HUD: Summary Cards ---
        if (ImGui::BeginTable("##StatusCards", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            
            // Card 1: FPS
            {
                float fps = m_lastStats.currentFPS;
                float ratio = m_lastTargetFPS > 0 ? fps / m_lastTargetFPS : 0.0f;
                ImVec4 statusColor = ratio >= 0.95f ? ImVec4(0.2f, 0.9f, 0.4f, 1.0f) : (ratio >= 0.8f ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                
                ImGui::BeginChild("CardFPS", ImVec2(0, 70), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextDisabled(ICON_FA_GAUGE " LIVE PERFORMANCE");
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font but maybe bigger? 
                ImGui::TextColored(statusColor, "%.1f", fps);
                ImGui::SameLine();
                ImGui::TextDisabled("FPS");
                ImGui::PopFont();
                
                // Status bar
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(), 
                    ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x * std::min(1.0f, ratio), ImGui::GetCursorScreenPos().y + 4),
                    ImColor(statusColor)
                );
                ImGui::EndChild();
            }

            ImGui::TableNextColumn();
            
            // Card 2: Duration
            {
                uint64_t ms = m_lastStats.recordingTimeMs;
                int hours = (int)(ms / 3600000);
                int mins = (int)((ms % 3600000) / 60000);
                int secs = (int)((ms % 60000) / 1000);
                
                ImGui::BeginChild("CardTime", ImVec2(0, 70), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextDisabled(ICON_FA_CLOCK " SESSION CLOCK");
                ImGui::Text("%02d:%02d:%02d", hours, mins, secs);
                
                // Pulsing recording indicator
                if (m_lastStats.isRecording) {
                    float pulse = (float)(1.0 + sin(ImGui::GetTime() * 6.0)) * 0.5f;
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 0.5f + pulse * 0.5f), "REC");
                }
                
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(), 
                    ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x, ImGui::GetCursorScreenPos().y + 4),
                    ImColor(0.2f, 0.2f, 0.2f, 1.0f)
                );
                ImGui::EndChild();
            }

            ImGui::TableNextColumn();
            
            // Card 3: Activity
            {
                ImGui::BeginChild("CardActivity", ImVec2(0, 70), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextDisabled(ICON_FA_KEYBOARD " INPUT ACTIVITY");
                
                ImGui::Text("M: %llu", m_lastStats.mouseDeltaActivity);
                ImGui::SameLine();
                ImGui::Text("K: %llu", m_lastStats.keyboardActivityLevel);
                
                // VU Meter for Audio
                float audio = std::min(1.0f, m_lastStats.audioLevelRMS * 5.0f); // Boost for visibility
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(), 
                    ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x * audio, ImGui::GetCursorScreenPos().y + 4),
                    ImColor(0.2f, 0.9f, 0.4f, 1.0f)
                );
                ImGui::EndChild();
            }

            ImGui::EndTable();
        }
    }

    void LiveMonitor::OnPreviewFrame(const PreviewFrame& frame) {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (m_pendingData.size() != frame.width * frame.height * 4) {
            m_pendingData.resize(frame.width * frame.height * 4);
        }
        std::memcpy(m_pendingData.data(), frame.data, m_pendingData.size());
        m_hasNewData = true;
    }

    void LiveMonitor::UpdateTexture() {
        if (!m_hasNewData || m_image == VK_NULL_HANDLE) return;

        std::vector<uint8_t> dataToUpload;
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            dataToUpload = m_pendingData;
            m_hasNewData = false;
        }

        // Upload to image (simplest way: Staging + copy)
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        VkDeviceSize bufferSize = dataToUpload.size();

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
        std::memcpy(mapped, dataToUpload.data(), bufferSize);
        vkUnmapMemory(m_device, stagingMemory);

        // Copy to Image
        VkCommandBufferAllocateInfo cmdAllocInfo = {};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VkCommandPool cmdPool; // Temporary pool
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_queueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        vkCreateCommandPool(m_device, &poolInfo, nullptr, &cmdPool);
        cmdAllocInfo.commandPool = cmdPool;
        cmdAllocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Transition layout to DST
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

        // Transition to SHADER_READ
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

    void LiveMonitor::CreateTexture(int width, int height) {
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
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler);

        m_descriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void LiveMonitor::DestroyTexture() {
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
