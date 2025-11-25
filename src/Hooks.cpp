#include "Hooks.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "MagFormatHook.hpp"
#include "PCH.h"
#include "ScalingConfig.h"
#include "common.h"

using RDDM::GetScaling;

namespace RDDM {

    static std::mutex g_magMutex;                          // NOSONAR: Mutex for g_lastFormattedMag
    static std::unordered_map<RE::EffectSetting*, double>  // NOSONAR: Cache of last formatted magnitudes
        g_lastFormattedMag;

    std::string OnEffectFormatted(RE::Effect* effect, std::string full, const std::string& inside, const std::string&,
                                  const std::string&) {
        if (!effect || !effect->baseEffect) {
            return full;
        }

        auto* mgef = effect->baseEffect;

        bool isNumber = false;
        double magVal = 0.0;

        if (!inside.empty()) {
            char* endPtr = nullptr;
            magVal = std::strtod(inside.c_str(), &endPtr);
            if (endPtr && *endPtr == '\0') {
                isNumber = true;
            }
        }

        if (isNumber) {
            std::scoped_lock lock(g_magMutex);
            g_lastFormattedMag[mgef] = magVal;

            return full;
        }

        if (inside != "sec") {
            return full;
        }

        double cachedMag = 0.0;
        {
            std::scoped_lock lock(g_magMutex);
            auto it = g_lastFormattedMag.find(mgef);
            if (it == g_lastFormattedMag.end()) {
                return full;
            }
            cachedMag = it->second;
        }

        double slider = 1.0;
        if (Common::IsDV_Health_Stamina(mgef)) {
            slider = GetScaling().frostStamina.load(std::memory_order_relaxed);
        } else if (Common::IsDV_Health_Magicka(mgef)) {
            slider = GetScaling().shockMagicka.load(std::memory_order_relaxed);
        } else {
            return full;
        }

        double secD = cachedMag * static_cast<double>(mgef->data.secondAVWeight) * slider;
        long long sec = std::llround(secD);
        if (sec < 0) {
            sec = 0;
        }

        std::string secStr = std::to_string(sec);

        std::size_t pos = full.find(">sec<");
        if (pos != std::string::npos) {
            pos += 1;
            full.replace(pos, 3, secStr);
            return full;
        }

        pos = full.find("sec");
        if (pos != std::string::npos) {
            full.replace(pos, 3, secStr);
        }

        return full;
    }

    std::optional<double> GetLastFormattedMag(const RE::EffectSetting* mgef) {
        if (!mgef) {
            return std::nullopt;
        }

        std::scoped_lock lock(g_magMutex);
        auto it = g_lastFormattedMag.find(const_cast<RE::EffectSetting*>(mgef));
        if (it == g_lastFormattedMag.end()) {
            return std::nullopt;
        }
        return it->second;
    }
}

namespace {
    template <class T>
    struct GetSecondaryWeightHook {
        static constexpr std::size_t kIndex = 0x22;

        using Fn = float(const T*);
        static inline Fn* _orig{};

        static float thunk(const T* self) {
            auto* m = self ? const_cast<RE::EffectSetting*>(self->GetBaseObject()) : nullptr;
            if (!m) {
                return _orig(self);
            }

            const float base = m->data.secondAVWeight;
            if (Common::IsDV_Health_Stamina(m)) {
                const float f = GetScaling().frostStamina.load(std::memory_order_relaxed);
                const float out = base * f;
                return out;
            }
            if (Common::IsDV_Health_Magicka(m)) {
                const float f = GetScaling().shockMagicka.load(std::memory_order_relaxed);
                const float out = base * f;
                return out;
            }

            return _orig(self);
        }

        static void Install() {
            REL::Relocation<std::uintptr_t> vtbl{T::VTABLE[0]};
            auto old = vtbl.write_vfunc(kIndex, &thunk);
            _orig = reinterpret_cast<Fn*>(old);
        }
    };

    template <class T>
    struct VM_ModifyAV_Hook_Slow {
        static constexpr std::size_t kIndex = 0x20;

        using Fn = void(T*, RE::Actor*, float, RE::ActorValue);
        static inline Fn* _orig{};

        static void thunk(T* self, RE::Actor* actor, float value, RE::ActorValue av) {
            if (auto* mgef = self ? const_cast<RE::EffectSetting*>(self->GetBaseObject()) : nullptr;
                Common::IsFrostSlow(mgef)) {
                if (const float f = RDDM::GetScaling().frostSlow.load(std::memory_order_relaxed); f != 1.0f) {
                    value *= f;
                }
            }

            _orig(self, actor, value, av);
        }

        static void Install() {
            REL::Relocation<std::uintptr_t> vtbl{T::VTABLE[0]};
            auto old = vtbl.write_vfunc(kIndex, &thunk);
            _orig = reinterpret_cast<Fn*>(old);
        }
    };
}

void RDDM_Hook::Install_Hooks() {
    GetSecondaryWeightHook<RE::DualValueModifierEffect>::Install();
    VM_ModifyAV_Hook_Slow<RE::PeakValueModifierEffect>::Install();

    MagFormatHook::Install();
}
