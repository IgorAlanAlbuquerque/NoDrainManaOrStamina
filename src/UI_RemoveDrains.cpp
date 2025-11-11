#include "UI_RemoveDrains.h"

using RDDM::GetScaling;

void __stdcall RDDM_UI::DrawScales() {
    ImGui::TextUnformatted("RVDME — Scaling");
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

    bool dirty = false;
    dirty |= slider("Frost: Slow (x)", cfg.frostSlow);
    dirty |= slider("Frost: Stamina drain (x)", cfg.frostStamina);
    dirty |= slider("Shock: Magicka drain (x)", cfg.shockMagicka);
    dirty |= slider("Fire: Burning DoT (x)", cfg.fireBurning);

    if (dirty) {
        GetScaling().Save();
        ImGui::SameLine();
        ImGui::TextDisabled("(applied)");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Tip: 0.0 remove, 1.0 normal, 2.0 double.");
}

void RDDM_UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) return;
    SKSEMenuFramework::SetSection("Remove Vanilla Destruction Magic Effects");
    SKSEMenuFramework::AddSectionItem("Scaling", RDDM_UI::DrawScales);
}
