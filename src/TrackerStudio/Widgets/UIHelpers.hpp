#pragma once
#include <imgui.h>

namespace agk {
namespace UI {

    // Shows a tooltip immediately on hover for the previously drawn ImGui item.
    // Call this immediately AFTER drawing the item (e.g., ImGui::Button).
    // Using ImGui::SetItemTooltip() is preferred in newer ImGui versions, but we provide this fallback.
    inline void ItemTooltip(const char* desc) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("%s", desc);
        }
    }

    // Displays a subtle (?) marker which shows a tooltip when hovered.
    inline void HelpMarker(const char* desc) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

} // namespace UI
} // namespace agk
