#include "UI_RemoveDrains.h"

#include "TextTemplates.h"

using RDDM::GetScaling;

void __stdcall RDDM_UI::DrawScales() {
    ImGui::TextUnformatted("RVDME: Scaling");
    ImGui::Separator();

    auto& cfg = GetScaling();

    auto slider = [](const char* label, std::atomic<float>& ref) {
        float v = ref.load(std::memory_order_relaxed);
        ImGui::SetNextItemWidth(220.0f);
        bool changed = ImGui::SliderFloat(label, &v, 0.0f, 2.0f, "%.1f");
        if (changed) {
            v = std::clamp(v, 0.0f, 2.0f);
            ref.store(v, std::memory_order_relaxed);
        }
        return changed;
    };

    static bool pending = false;
    bool dirty = false;

    dirty |= slider("Frost: Slow", cfg.frostSlow);
    dirty |= slider("Frost: Stamina drain", cfg.frostStamina);
    dirty |= slider("Shock: Magicka drain", cfg.shockMagicka);
    dirty |= slider("Fire: Burning DoT", cfg.fireBurning);

    if (dirty) pending = true;

    if (pending) {
        ImGui::SameLine();
        ImGui::TextDisabled("(pending)");
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!pending);
    bool pressed = ImGui::Button("Apply changes", ImVec2(140.0f, 0.0f));
    ImGui::EndDisabled();
    if (pressed) {
        cfg.Save();
        TextDes::ApplyDVME_TextTemplates();

        pending = false;

        ImGui::SameLine();
        ImGui::TextDisabled("✓ applied");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Tip: 0.0 remove, 1.0 normal, 2.0 double.");
}

void RDDM_UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) return;
    SKSEMenuFramework::SetSection("Remove Vanilla Destruction Magic Effects");
    SKSEMenuFramework::AddSectionItem("Scaling", RDDM_UI::DrawScales);
}
