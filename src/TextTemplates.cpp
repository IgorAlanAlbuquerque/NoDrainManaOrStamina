#include "TextTemplates.h"

#include <cstring>
#include <string>
#include <string_view>

#include "PCH.h"
#include "ScalingConfig.h"
#include "common.h"

using AV = RE::ActorValue;
using Arch = RE::EffectArchetypes::ArchetypeID;

namespace {
    struct FireTaperOrig {
        float weight;
        bool initialized;
    };

    static std::unordered_map<RE::FormID, FireTaperOrig> g_fireTaperOrig;  // NOSONAR: Global cache

    constexpr RE::FormID kWallFrost = 0x0008F3F7;
    constexpr RE::FormID kWallShock = 0x000BB96F;
    constexpr RE::FormID kCloakFrost = 0x0003AEA0;
    constexpr RE::FormID kCloakShock = 0x0003AEA1;
    constexpr RE::FormID kFireStormMgef = 0x0007A82A;
    constexpr RE::FormID kBlizzardMgef = 0x000B79FE;
    constexpr RE::FormID kFreeze = 0x0402732E;
    constexpr RE::FormID kUnboundedStormsMgef = 0xFE009807;
    constexpr RE::FormID kUnboundedFreezingMgef = 0xFE009809;
    constexpr const char* kExtraNeedle = "extra damage";
    constexpr const char* kExtraSentence = " Targets on fire take extra damage over time.";

    void ApplyFireTaper(RE::EffectSetting* m, float fireMult) {
        if (!m) return;

        auto fid = m->GetFormID();
        auto& entry = g_fireTaperOrig[fid];

        if (!entry.initialized) {
            entry.weight = m->data.taperWeight;
            entry.initialized = true;
        }

        if (fireMult <= 1e-6f) {
            m->data.taperWeight = 0.0f;
            return;
        }

        if (std::fabs(fireMult - 1.0f) < 1e-3f) {
            m->data.taperWeight = entry.weight;
            return;
        }

        m->data.taperWeight = entry.weight * fireMult;
    }
    inline bool WantsPerSecond(const RE::EffectSetting* m) {
        if (!m) return false;
        if (m->data.castingType == RE::MagicSystem::CastingType::kConcentration) return true;
        std::string_view sv = m->magicItemDescription.c_str();
        return !sv.empty() && sv.contains("per second");
    }

    inline void SetDesc(RE::EffectSetting* m, std::string_view s) {
        m->magicItemDescription = RE::BSFixedString{s.data()};
    }

    inline bool SameDesc(const RE::EffectSetting* m, std::string_view s) {
        const char* cur = m ? m->magicItemDescription.c_str() : nullptr;
        return cur && s == cur;
    }

    enum class AreaClass { None, Small, Large, Massive };

    inline AreaClass ClassFromValue(float v) {
        using enum AreaClass;
        if (v >= 500.0f) return Massive;
        if (v >= 100.0f) return Large;
        if (v >= 50.0f) return Small;
        return AreaClass::None;
    }

    inline const char* AreaSuffix(AreaClass c) {
        using enum AreaClass;
        switch (c) {
            case Small:
                return " in an area";
            case Large:
                return " in a large area";
            case Massive:
                return " in a massive area";
            default:
                return "";
        }
    }

    inline AreaClass AreaFromMGEF_Expl_Proj_Spellmaking(const RE::EffectSetting* m) {
        if (!m) return AreaClass::None;

        if (auto const* expl = m->data.explosion) {
            const float r = expl->data.radius;
            return ClassFromValue(r);
        }

        if (auto const* proj = m->data.projectileBase) {
            const float w = proj->data.collisionRadius;
            return ClassFromValue(w);
        }

        return AreaClass::None;
    }

    inline bool ShouldPrintDurationClause(const RE::EffectSetting* m) noexcept {
        if (!m) {
            return false;
        }

        const auto& dnam = m->magicItemDescription;
        if (dnam.empty()) {
            return false;
        }

        const char* c = dnam.c_str();
        if (!c || !*c) {
            return false;
        }

        return std::string_view(c).contains("<dur>");
    }

    inline std::string BuildDVME_Desc_Wall(const char* elem, float mult, const bool perSec, const char* secAV) {
        std::string s;
        s += "Creates a wall that deals <mag> ";
        s += elem;
        s += " damage";
        if (perSec) s += " per second";
        if (mult <= 1e-6f) {
            s += " to Health";
        } else if (std::fabs(mult - 1.0f) < 1e-3f) {
            s += " to Health and ";
            s += secAV;
        } else {
            s += " to Health and <sec> to ";
            s += secAV;
        }
        s += ".";
        return s;
    }

    inline std::string BuildDVME_Desc_Cloak(const char* elem, float mult, const char* secAV, bool frost) {
        std::string s;
        const float frostF = RDDM::GetScaling().frostStamina.load(std::memory_order_relaxed);
        const float shockF = RDDM::GetScaling().shockMagicka.load(std::memory_order_relaxed);
        mult = frost ? frostF : shockF;
        s += "For <dur> seconds, nearby enemies take <mag> ";
        s += elem;
        s += " damage per second";
        if (mult <= 1e-6f) {
            s += " to Health";
        } else if (std::fabs(mult - 1.0f) < 1e-3f) {
            s += " to Health and ";
            s += secAV;
        } else {
            s += " to Health and <sec> to ";
            s += secAV;
        }
        s += ".";
        return s;
    }

    inline std::string BuildDVME_Desc(const RE::EffectSetting* m, float mult, bool frost) {
        const auto fid = m->GetFormID();
        const bool isWall = (fid == kWallFrost) || (fid == kWallShock);
        const bool isCloak = (fid == kCloakFrost) || (fid == kCloakShock);

        const bool perSec = WantsPerSecond(m);
        const char* elem = frost ? "Frost" : "Shock";
        const char* secAV = frost ? "Stamina" : "Magicka";

        if (isWall) return BuildDVME_Desc_Wall(elem, mult, perSec, secAV);

        if (isCloak) return BuildDVME_Desc_Cloak(elem, mult, secAV, frost);

        std::string s;
        s += "Deals <mag> ";
        s += elem;
        s += " damage";
        if (perSec) s += " per second";

        if (m->GetFormID() == kBlizzardMgef) {
            const float frostF = RDDM::GetScaling().frostStamina.load(std::memory_order_relaxed);
            const float shockF = RDDM::GetScaling().shockMagicka.load(std::memory_order_relaxed);
            mult = frost ? frostF : shockF;
        }

        if (mult <= 1e-6f) {
            s += " to Health";
        } else if (std::fabs(mult - 1.0f) < 1e-3f) {
            s += " to Health and ";
            s += secAV;
        } else {
            s += " to Health and <sec> to ";
            s += secAV;
        }
        const AreaClass ac = AreaFromMGEF_Expl_Proj_Spellmaking(m);
        s += AreaSuffix(ac);

        if (ShouldPrintDurationClause(m)) s += " for <dur> seconds";

        if (fid == kFreeze) {
            s += " and slows the target for <15> seconds";
        }

        s += ".";
        return s;
    }

    inline void ApplyUnboundedStormsDesc(RE::EffectSetting* m, float shockMult) {
        if (!m) {
            return;
        }

        std::string s = "Targets in melee range take <40> points of shock damage per second to Health";

        if (shockMult <= 1e-6f) {
            s += ". Random lightning strikes deal an additional <30> damage.";
        } else if (std::fabs(shockMult - 1.0f) < 1e-3f) {
            s += ", and half that to Magicka. Random lightning strikes deal an additional <30> damage.";
        } else {
            s += " and <sec> to Magicka. Random lightning strikes deal an additional <30> damage.";
        }

        if (!SameDesc(m, s)) {
            SetDesc(m, s);
        }
    }

    inline void ApplyUnboundedFreezingDesc(RE::EffectSetting* m, float frostMult) {
        if (!m) {
            return;
        }

        std::string s =
            "A freezing wind envelops the caster, knocking down nearby enemies and freezing them for <50> points of "
            "damage per second to Health";

        if (frostMult <= 1e-6f) {
            s += ".";
        } else if (std::fabs(frostMult - 1.0f) < 1e-3f) {
            s += " and Stamina.";
        } else {
            s += " and <sec> to Stamina.";
        }

        if (!SameDesc(m, s)) {
            SetDesc(m, s);
        }
    }

    inline bool ci_contains(std::string_view hay, std::string_view needle) {
        if (needle.empty() || needle.size() > hay.size()) return false;
        for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
            bool ok = true;
            for (size_t j = 0; j < needle.size(); ++j) {
                auto a = static_cast<unsigned char>(hay[i + j]);
                auto b = static_cast<unsigned char>(needle[j]);
                if (std::tolower(a) != std::tolower(b)) {
                    ok = false;
                    break;
                }
            }
            if (ok) return true;
        }
        return false;
    }

    inline void TrimTrailingSpaces(std::string& s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    }

    inline bool EndsWithDot(std::string_view s) {
        if (s.empty()) return false;
        char c = s.back();
        return c == '.' || c == '!' || c == '?';
    }

    inline bool IsFireDisabled(float fireMult) noexcept { return fireMult <= 1e-6f; }

    inline void RemoveExtraSentenceFirstDot(RE::EffectSetting* m, std::string& cur) {
        const size_t dot = cur.find('.');
        if (dot == std::string::npos) {
            return;
        }

        std::string onlyFirst = cur.substr(0, dot + 1);
        if (!SameDesc(m, onlyFirst)) {
            SetDesc(m, onlyFirst);
        }
    }

    inline void RemoveExtraSentenceByPattern(RE::EffectSetting* m, std::string& cur) {
        const size_t pos = cur.find(kExtraSentence);
        if (pos == std::string::npos) {
            return;
        }

        cur.erase(pos, std::strlen(kExtraSentence));
        TrimTrailingSpaces(cur);
        if (!EndsWithDot(cur)) {
            cur += ".";
        }

        if (!SameDesc(m, cur)) {
            SetDesc(m, cur);
        }
    }

    inline void RemoveExtraDamageSentence(RE::EffectSetting* m, std::string& cur) {
        if (cur.contains('.')) {
            RemoveExtraSentenceFirstDot(m, cur);
        } else {
            RemoveExtraSentenceByPattern(m, cur);
        }
    }

    inline void AddExtraDamageSentence(RE::EffectSetting* m, std::string& cur) {
        if (!EndsWithDot(cur) && !cur.empty()) {
            cur += ".";
        }

        cur += kExtraSentence;

        if (!SameDesc(m, cur)) {
            SetDesc(m, cur);
        }
    }

    inline void ApplyFireDescOne(RE::EffectSetting* m, float fireMult) {
        if (m->GetFormID() == kFireStormMgef) {
            return;
        }
        const char* curC = m->magicItemDescription.c_str();
        std::string cur = curC ? curC : std::string{};
        TrimTrailingSpaces(cur);

        const bool hasExtra = ci_contains(cur, kExtraNeedle);

        if (IsFireDisabled(fireMult)) {
            if (hasExtra) {
                RemoveExtraDamageSentence(m, cur);
            }
            return;
        }

        if (!hasExtra) {
            AddExtraDamageSentence(m, cur);
        }
    }

    inline bool IsCC_ElementalExplosion(RE::FormID id) noexcept {
        switch (id) {
            case 0xFE009846:
            case 0xFE009845:
            case 0xFE009844:
            case 0xFE009840:
                return true;
            default:
                return false;
        }
    }

    inline std::string BuildCC_ElementalExplosionDesc(float staminaMult, bool useHalfText) {
        std::string s = "An elemental explosion for <mag> points of magic damage";

        if (staminaMult <= 1e-6f) {
            s += ". Deals shock damage on impact. Benefits from perks affecting fire, shock and frost spell damage.";
            return s;
        }

        if (useHalfText) {
            s += " and half as much damage to stamina. ";
        } else {
            s += " and <sec> damage to stamina. ";
        }

        s += "Deals shock damage on impact. Benefits from perks affecting fire, shock and frost spell damage.";
        return s;
    }
}

void TextDes::ApplyDVME_TextTemplates() {
    auto* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) return;

    const float frostF = RDDM::GetScaling().frostStamina.load(std::memory_order_relaxed);
    const float shockF = RDDM::GetScaling().shockMagicka.load(std::memory_order_relaxed);

    for (auto* m : dh->GetFormArray<RE::EffectSetting>()) {
        if (!m) continue;

        const auto fid = m->GetFormID();

        if (IsCC_ElementalExplosion(m->GetFormID())) {
            const float cfg = RDDM::GetScaling().frostStamina.load(std::memory_order_relaxed);
            const float fireF = RDDM::GetScaling().fireBurning.load(std::memory_order_relaxed);

            const bool keepHalf = std::fabs(cfg - 1.0f) < 1e-3f;

            if (const std::string txt = BuildCC_ElementalExplosionDesc(cfg, keepHalf); !SameDesc(m, txt)) {
                SetDesc(m, txt);
            }
            ApplyFireTaper(m, fireF);
            continue;
        }
        if (fid == kUnboundedStormsMgef) {
            ApplyUnboundedStormsDesc(m, shockF);
            continue;
        }
        if (fid == kUnboundedFreezingMgef) {
            ApplyUnboundedFreezingDesc(m, frostF);
            continue;
        }
        if (Common::IsElementFrost(m)) {
            const float mult = m->data.secondAVWeight * frostF;

            if (const std::string txt = BuildDVME_Desc(m, mult, true); !SameDesc(m, txt)) {
                SetDesc(m, txt);
            }
            continue;
        }
        if (Common::IsElementShock(m)) {
            const float mult = m->data.secondAVWeight * shockF;

            if (const std::string txt = BuildDVME_Desc(m, mult, false); !SameDesc(m, txt)) {
                SetDesc(m, txt);
            }
            continue;
        }

        if (Common::IsElementFire(m)) {
            const float fireF = RDDM::GetScaling().fireBurning.load(std::memory_order_relaxed);
            ApplyFireDescOne(m, fireF);
            ApplyFireTaper(m, fireF);
            continue;
        }
    }
}
