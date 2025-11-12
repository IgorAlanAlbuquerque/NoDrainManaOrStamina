#pragma once
#include "PCH.h"

namespace Common {
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

    inline bool IsFireMGEF(const RE::EffectSetting* m) noexcept {
        return m && m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kValueModifier &&
               m->data.primaryAV == RE::ActorValue::kHealth && IsDestruction(m);
    }

    inline bool IsFireBurningMGEF(const RE::EffectSetting* m) {
        using AV = RE::ActorValue;
        return m && m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kValueModifier &&
               m->data.primaryAV == AV::kHealth && IsDestruction(m);
    }
}