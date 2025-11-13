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

    inline bool HasMagicSlowKW(const RE::EffectSetting* m) noexcept {
        if (!m) return false;

        if (static RE::BGSKeyword const* kMagicSlowPtr = []() -> RE::BGSKeyword* {
                return RE::TESForm::LookupByID<RE::BGSKeyword>(0x000B729E);
            }();
            kMagicSlowPtr && m->HasKeyword(kMagicSlowPtr))
            return true;
        return const_cast<RE::EffectSetting*>(m)->HasKeyword("MagicSlow");
    }

    inline bool IsFrostSlow(const RE::EffectSetting* m) noexcept {
        using Arch = RE::EffectArchetypes::ArchetypeID;
        if (!m) return false;

        const bool isPeak = (m->GetArchetype() == Arch::kPeakValueModifier);
        const bool isSpeed = (m->data.primaryAV == RE::ActorValue::kSpeedMult);
        if (!isPeak || !isSpeed) return false;

        const bool isDest = (m->data.associatedSkill == RE::ActorValue::kDestruction);
        const bool hasKW = HasMagicSlowKW(m);

        return isDest || hasKW;
    }
}