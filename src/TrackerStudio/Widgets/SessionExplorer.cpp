#include "SessionExplorer.hpp"
#include "imgui.h"
#include <filesystem>
#include "portable-file-dialogs.h"
#include "../IconsFontAwesome6.h"
#include <agk/RecordingEngine/Log.hpp>
#include <algorithm>
#include "UIHelpers.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../3rdparty/stb_image.h"
#include "imgui_impl_vulkan.h"

namespace agk {
namespace Widgets {
    
    struct SessionTexture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
        int width = 0;
        int height = 0;
    };

    SessionExplorer::SessionExplorer(VkDevice device, VkPhysicalDevice physDevice, VkDescriptorPool pool, uint32_t queueFamily, VkQueue queue)
        : m_device(device), m_physDevice(physDevice), m_pool(pool), m_queueFamily(queueFamily), m_queue(queue) {}

    SessionExplorer::~SessionExplorer() {
        for (auto& pair : m_thumbnailCache) {
            DestroyTexture(pair.second);
        }
        m_thumbnailCache.clear();
    }

    void SessionExplorer::DestroyTexture(SessionTexture* tex) {
        if (!tex) return;
        if (tex->descriptor) ImGui_ImplVulkan_RemoveTexture(tex->descriptor);
        if (tex->sampler) vkDestroySampler(m_device, tex->sampler, nullptr);
        if (tex->view) vkDestroyImageView(m_device, tex->view, nullptr);
        if (tex->image) vkDestroyImage(m_device, tex->image, nullptr);
        if (tex->memory) vkFreeMemory(m_device, tex->memory, nullptr);
        delete tex;
    }

    SessionTexture* SessionExplorer::LoadThumbnail(const std::string& filepath) {
        int w, h, channels;
        uint8_t* pixels = stbi_load(filepath.c_str(), &w, &h, &channels, 4);
        if (!pixels) return nullptr;

        SessionTexture* tex = new SessionTexture();
        tex->width = w;
        tex->height = h;

        VkDeviceSize imageSize = w * h * 4;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
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
        vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingBufferMemory);
        vkBindBufferMemory(m_device, stagingBuffer, stagingBufferMemory, 0);

        void* data;
        vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(m_device, stagingBufferMemory);

        stbi_image_free(pixels);

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = w;
        imageInfo.extent.height = h;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        
        vkCreateImage(m_device, &imageInfo, nullptr, &tex->image);

        vkGetImageMemoryRequirements(m_device, tex->image, &memReq);
        allocInfo.allocationSize = memReq.size;
        for (uint32_t i = 0; i < memProp.memoryTypeCount; i++) {
            if ((memReq.memoryTypeBits & (1 << i)) && (memProp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }
        vkAllocateMemory(m_device, &allocInfo, nullptr, &tex->memory);
        vkBindImageMemory(m_device, tex->image, tex->memory, 0);

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_queueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        VkCommandPool cmdPool;
        vkCreateCommandPool(m_device, &poolInfo, nullptr, &cmdPool);

        VkCommandBufferAllocateInfo allocCmdInfo = {};
        allocCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocCmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocCmdInfo.commandPool = cmdPool;
        allocCmdInfo.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_device, &allocCmdInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.image = tex->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
        vkCmdCopyBufferToImage(cmd, stagingBuffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

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
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = tex->image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_device, &viewInfo, nullptr, &tex->view);

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(m_device, &samplerInfo, nullptr, &tex->sampler);

        tex->descriptor = ImGui_ImplVulkan_AddTexture(tex->sampler, tex->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return tex;
    }

    void SessionExplorer::Render(StudioConfig& config, std::shared_ptr<IRecordingEngine> engine, SessionReplayer* replayer) {
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##SetRoot", ImVec2(28, 28))) {
            std::string new_path = pfd::select_folder("Choose Recordings Folder", config.last_session_path).result();
            if (!new_path.empty()) {
                AGK_CORE_INFO("[SessionExplorer] User changed root folder to: {}", new_path);
                config.last_session_path = new_path;
                config.Save("tracker_config.json");
            }
        }
        agk::UI::ItemTooltip("Set Root Folder");
        
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_LINK "##OpenExp", ImVec2(28, 28)) && !config.last_session_path.empty()) {
            std::string cmd = "explorer \"" + config.last_session_path + "\"";
            system(cmd.c_str());
        }
        agk::UI::ItemTooltip("Open in File Explorer");

        ImGui::Separator();
        ImGui::TextDisabled("Root: %s", config.last_session_path.empty() ? "None" : config.last_session_path.c_str());
        
        // --- SEARCH BAR ---
        ImGui::PushItemWidth(-1);
        if (ImGui::InputTextWithHint("##SearchSessions", ICON_FA_MAGNIFYING_GLASS " Filter sessions...", m_searchFilter, IM_ARRAYSIZE(m_searchFilter))) {
            // Filter updated
        }
        ImGui::PopItemWidth();
        
        ImGui::Separator();

        if (config.last_session_path.empty() || !std::filesystem::exists(config.last_session_path)) {
            ImGui::TextWrapped("Please set a valid root folder to view sessions.");
            return;
        }

        ImGui::BeginChild("SessionList", ImVec2(0, 0), true);

        try {
            // Sort paths in reverse chronological order
            std::vector<std::filesystem::path> paths;
            std::string filter = m_searchFilter;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

            for (const auto& entry : std::filesystem::directory_iterator(config.last_session_path)) {
                if (entry.is_directory()) {
                    std::string name = entry.path().filename().string();
                    std::string lowerName = name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    
                    if (filter.empty() || lowerName.find(filter) != std::string::npos) {
                        paths.push_back(entry.path());
                    }
                }
            }
            
            std::sort(paths.begin(), paths.end(), std::greater<std::filesystem::path>());

            for (const auto& path : paths) {
                std::string folderName = path.filename().string();
                ImGui::PushID(folderName.c_str());

                // Use a folder icon
                bool isSelected = (replayer->GetCurrentSessionPath() == path.string());
                
                // --- CONTEXT MENU ---
                if (ImGui::BeginPopupContextItem("SessionActions")) {
                    if (ImGui::MenuItem(ICON_FA_LINK " Open in Explorer")) {
                        std::string cmd = "explorer \"" + path.string() + "\"";
                        system(cmd.c_str());
                    }
                    if (ImGui::MenuItem(ICON_FA_TRASH_CAN " Delete Session")) {
                        ImGui::OpenPopup("Delete?##Confirm");
                    }
                    ImGui::EndPopup();
                }

                // Delete Confirmation Modal
                if (ImGui::BeginPopupModal("Delete?##Confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("Are you sure you want to delete this session?\nThis will permanently remove the video and all data.");
                    ImGui::Separator();
                    if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                        try {
                            std::filesystem::remove_all(path);
                        } catch(...) {}
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                    ImGui::EndPopup();
                }

                // Look for thumbnail
                std::string thumbPath = path.string() + "/thumbnail.png";
                SessionTexture* thumbTex = nullptr;
                auto it = m_thumbnailCache.find(thumbPath);
                if (it == m_thumbnailCache.end()) {
                    if (std::filesystem::exists(thumbPath)) {
                        thumbTex = LoadThumbnail(thumbPath);
                        m_thumbnailCache[thumbPath] = thumbTex;
                    } else {
                        m_thumbnailCache[thumbPath] = nullptr;
                    }
                } else {
                    thumbTex = it->second;
                    // Retry loading if we cached a nullptr previously but the file now exists
                    if (!thumbTex && std::filesystem::exists(thumbPath)) {
                        thumbTex = LoadThumbnail(thumbPath);
                        m_thumbnailCache[thumbPath] = thumbTex;
                    }
                }

                if (thumbTex && thumbTex->descriptor) {
                    ImGui::Image((ImTextureID)thumbTex->descriptor, ImVec2(64, 36)); 
                    ImGui::SameLine();
                }

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (thumbTex ? (36.0f - ImGui::GetTextLineHeight()) * 0.5f : 0));
                
                if (thumbTex) {
                    ImGui::TreeNodeEx((void*)path.c_str(), flags, "%s", folderName.c_str());
                } else {
                    ImGui::TreeNodeEx((void*)path.c_str(), flags, "%s %s", ICON_FA_FOLDER, folderName.c_str());
                }
                
                if (thumbTex) ImGui::Spacing(); // reset spacing

                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    AGK_CORE_INFO("[SessionExplorer] User requested to load session: {}", folderName);
                    replayer->OpenSession(path.string());
                }

                ImGui::PopID();
            }
        } catch (const std::exception& e) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error reading directory: %s", e.what());
        }

        ImGui::EndChild();
    }

}
}
