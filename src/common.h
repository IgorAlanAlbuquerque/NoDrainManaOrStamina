#pragma once
#include "PCH.h"

namespace Common {
    inline bool IsElementFrost(const RE::EffectSetting* m) noexcept {
        if (!m) return false;
        if (m->data.resistVariable == RE::ActorValue::kResistFrost) return true;
        return const_cast<RE::EffectSetting*>(m)->HasKeyword("MagicDamageFrost");
    }

    inline bool IsElementShock(const RE::EffectSetting* m) noexcept {
        if (!m) return false;
        if (m->data.resistVariable == RE::ActorValue::kResistShock) return true;
        return const_cast<RE::EffectSetting*>(m)->HasKeyword("MagicDamageShock");
    }

    inline bool IsElementFire(const RE::EffectSetting* m) noexcept {
        if (!m) return false;
        if (m->data.resistVariable == RE::ActorValue::kResistFire) return true;
        return const_cast<RE::EffectSetting*>(m)->HasKeyword("MagicDamageFire");
    }

    inline bool IsDV_Health_Stamina(const RE::EffectSetting* m) noexcept {
        if (!m) return false;
        const bool elem = IsElementFrost(m);
        const bool vanilla = m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kDualValueModifier &&
                             m->data.primaryAV == RE::ActorValue::kHealth &&
                             m->data.secondaryAV == RE::ActorValue::kStamina;
        const bool cloak = m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kCloak;
        return elem && (vanilla || cloak);
    }
    inline bool IsDV_Health_Magicka(const RE::EffectSetting* m) noexcept {
        const bool elem = IsElementShock(m);
        const bool vanilla = m && m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kDualValueModifier &&
                             m->data.primaryAV == RE::ActorValue::kHealth &&
                             m->data.secondaryAV == RE::ActorValue::kMagicka;
        const bool cloak = m->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kCloak;
        return elem && (vanilla || cloak);
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

    inline bool isCCSpells(const RE::EffectSetting* m) noexcept {}
}