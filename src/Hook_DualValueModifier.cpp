#include "Hook_DualValueModifier.h"

#include <spdlog/spdlog.h>

#include "PCH.h"
#include "RE/A/ActiveEffect.h"
#include "RE/D/DualValueModifierEffect.h"
#include "ScalingConfig.h"

using RDDM::GetScaling;

namespace {
    inline bool IsDestruction(const RE::EffectSetting* mgef) noexcept {
        return mgef && mgef->data.associatedSkill == RE::ActorValue::kDestruction;
    }
    inline bool IsDV_Health_Stamina(const RE::EffectSetting* m) noexcept {
        return m && m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kDualValueModifier &&
               m->data.primaryAV == RE::ActorValue::kHealth && m->data.secondaryAV == RE::ActorValue::kStamina &&
               IsDestruction(m);
    }
    inline bool IsDV_Health_Magicka(const RE::EffectSetting* m) noexcept {
        return m && m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kDualValueModifier &&
               m->data.primaryAV == RE::ActorValue::kHealth && m->data.secondaryAV == RE::ActorValue::kMagicka &&
               IsDestruction(m);
    }
    template <class T>
    struct GetSecondaryWeightHook {
        static constexpr std::size_t kIndex = 0x22;

        using Fn = float(const T*);
        static inline Fn* _orig{};

        static float thunk(const T* self) {
            auto* m = self ? const_cast<RE::EffectSetting*>(self->GetBaseObject()) : nullptr;
            if (!m) return _orig(self);

            const float base = m->data.secondAVWeight;
            if (IsDV_Health_Stamina(m)) {
                const float f = GetScaling().frostStamina.load(std::memory_order_relaxed);
                const float out = base * f;

                return out;
            }
            if (IsDV_Health_Magicka(m)) {
                const float f = GetScaling().shockMagicka.load(std::memory_order_relaxed);
                const float out = base * f;

                return out;
            }

            return _orig(self);
        }

        static void Install(const char* label) {
            REL::Relocation<std::uintptr_t> vtbl{T::VTABLE[0]};
            auto old = vtbl.write_vfunc(kIndex, &thunk);
            _orig = reinterpret_cast<Fn*>(old);
        }
    };
}

void RDDM_Hook::Install_DualValueModifier() {
    GetSecondaryWeightHook<RE::DualValueModifierEffect>::Install("DualValueModifierEffect");
}