#include <spdlog/spdlog.h>

#include <cstring>
#include <string>

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

    static std::unordered_map<RE::FormID, FireTaperOrig> g_fireTaperOrig;

    constexpr RE::FormID kWallFrost = 0x0008F3F7;
    constexpr RE::FormID kWallShock = 0x000BB96F;
    constexpr RE::FormID kCloakFrost = 0x0003AEA0;
    constexpr RE::FormID kCloakShock = 0x0003AEA1;

    static void ApplyFireTaper(RE::EffectSetting* m, float fireMult) {
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
        auto s = m->magicItemDescription.c_str();
        return s && std::strstr(s, "per second") != nullptr;
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
        if (v <= 0.0f) return AreaClass::None;
        if (v >= 500.0f) return AreaClass::Massive;
        if (v >= 40.0f) return AreaClass::Large;
        return AreaClass::Small;
    }

    inline const char* AreaSuffix(AreaClass c) {
        switch (c) {
            case AreaClass::Small:
                return " in an area";
            case AreaClass::Large:
                return " in a large area";
            case AreaClass::Massive:
                return " in a massive area";
            default:
                return "";
        }
    }

    inline AreaClass AreaFromMGEF_Expl_Proj_Spellmaking(const RE::EffectSetting* m) {
        if (!m) return AreaClass::None;

        if (auto* expl = m->data.explosion) {
            const float r = expl->data.radius;
            return ClassFromValue(r);
        }

        if (auto* proj = m->data.projectileBase) {
            const float w = proj->data.collisionRadius;
            return ClassFromValue(w);
        }

        return AreaClass::None;
    }

    inline bool HasDurationFlag(const RE::EffectSetting* m) noexcept {
        using Flag = RE::EffectSetting::EffectSettingData::Flag;
        return m && !m->data.flags.all(Flag::kNoDuration);
    }

    inline bool IsConcentration(const RE::EffectSetting* m) noexcept {
        return m && m->data.castingType == RE::MagicSystem::CastingType::kConcentration;
    }

    inline bool ShouldPrintDurationClause(const RE::EffectSetting* m) noexcept {
        return HasDurationFlag(m) && !IsConcentration(m);
    }

    inline std::string BuildDVME_Desc(const RE::EffectSetting* m, float mult, bool frost) {
        const auto fid = m->GetFormID();
        const bool isWall = (fid == kWallFrost) || (fid == kWallShock);
        const bool isCloak = (fid == kCloakFrost) || (fid == kCloakShock);

        const bool perSec = WantsPerSecond(m);
        const char* elem = frost ? "Frost" : "Shock";
        const char* secAV = frost ? "Stamina" : "Magicka";

        std::string s;

        if (isWall) {
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

        if (isCloak) {
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

        s += "Deals <mag> ";
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
        const AreaClass ac = AreaFromMGEF_Expl_Proj_Spellmaking(m);
        s += AreaSuffix(ac);

        if (ShouldPrintDurationClause(m)) s += " for <dur> seconds";

        s += ".";
        return s;
    }

    inline bool ci_contains(std::string_view hay, std::string_view needle) {
        if (needle.empty() || needle.size() > hay.size()) return false;
        for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
            bool ok = true;
            for (size_t j = 0; j < needle.size(); ++j) {
                unsigned char a = static_cast<unsigned char>(hay[i + j]);
                unsigned char b = static_cast<unsigned char>(needle[j]);
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

    inline void ApplyFireDescOne(RE::EffectSetting* m, float fireMult) {
        const char* kExtraNeedle = "extra damage";
        const char* kExtraSentence = " Targets on fire take extra damage over time.";

        const char* curC = m->magicItemDescription.c_str();
        std::string cur = curC ? curC : std::string{};
        TrimTrailingSpaces(cur);

        const bool hasExtra = ci_contains(cur, kExtraNeedle);

        if (fireMult <= 1e-6f) {
            if (hasExtra) {
                const size_t dot = cur.find('.');
                if (dot != std::string::npos) {
                    std::string onlyFirst = cur.substr(0, dot + 1);
                    if (!SameDesc(m, onlyFirst)) SetDesc(m, onlyFirst);
                } else {
                    const size_t pos = cur.find(kExtraSentence);
                    if (pos != std::string::npos) {
                        cur.erase(pos, std::strlen(kExtraSentence));
                        TrimTrailingSpaces(cur);
                        if (!EndsWithDot(cur)) cur += ".";
                        if (!SameDesc(m, cur)) SetDesc(m, cur);
                    }
                }
            }
            return;
        }

        if (!hasExtra) {
            if (!EndsWithDot(cur) && !cur.empty()) cur += ".";
            cur += kExtraSentence;
            if (!SameDesc(m, cur)) SetDesc(m, cur);
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

        if (IsCC_ElementalExplosion(m->GetFormID())) {
            const float cfg = RDDM::GetScaling().frostStamina.load(std::memory_order_relaxed);
            const float fireF = RDDM::GetScaling().fireBurning.load(std::memory_order_relaxed);

            const bool keepHalf = std::fabs(cfg - 1.0f) < 1e-3f;

            const std::string txt = BuildCC_ElementalExplosionDesc(cfg, keepHalf);

            if (!SameDesc(m, txt)) {
                SetDesc(m, txt);
            }
            ApplyFireTaper(m, fireF);
            continue;
        }
        if (Common::IsElementFrost(m)) {
            const float mult = m->data.secondAVWeight * frostF;

            const std::string txt = BuildDVME_Desc(m, mult, true);
            if (!SameDesc(m, txt)) {
                SetDesc(m, txt);
            }
            continue;
        }
        if (Common::IsElementShock(m)) {
            const float mult = m->data.secondAVWeight * shockF;

            const std::string txt = BuildDVME_Desc(m, mult, false);
            if (!SameDesc(m, txt)) {
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
