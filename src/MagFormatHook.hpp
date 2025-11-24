#pragma once

#include "HookUtil.hpp"
#include "PCH.h"

namespace RDDM {
    std::string OnEffectFormatted(RE::Effect* effect, std::string full, const std::string& inside,
                                  const std::string& prefix, const std::string& suffix);
}

class MagFormatHook {
private:
    union EffectUnion {
        RE::Effect* common;
        RE::ActiveEffect* active;
    };

    struct EffectStruct {
        RE::BSString buffer;
        EffectUnion effect;
        const char* prefix;
        const char* suffix;
    };

    struct StringData {
        std::string p;
        std::string s;
        std::size_t pSize;
        std::size_t sSize;

        StringData(EffectStruct* arg) {
            auto prefixPtr = arg->prefix;
            auto suffixPtr = arg->suffix;

            p = prefixPtr ? prefixPtr : "";
            s = suffixPtr ? suffixPtr : "";
            pSize = p.size();
            sSize = s.size();
        }
    };

    static void LogBaseEffect(RE::Effect* eff) {
        std::uint32_t formID = 0;
        if (eff) {
            if (auto base = eff->baseEffect; base) {
                formID = base->formID;
            }
        }
        spdlog::info("MagFormatHook: baseEffect = {:08X}", formID);
    }

    static_assert(offsetof(EffectStruct, buffer) == 0x00);
    static_assert(offsetof(EffectStruct, effect) == 0x10);
    static_assert(offsetof(EffectStruct, prefix) == 0x18);
    static_assert(offsetof(EffectStruct, suffix) == 0x20);

    struct CallEffectA {
        inline static std::uintptr_t func = 0;

        static const char* thunk(EffectStruct* arg, char* type, char* unused, char* edid) {
            spdlog::info("MagFormatHook.CallEffectA::thunk chamado");
            using Fn = const char* (*)(EffectStruct*, char*, char*, char*);

            if (!arg) {
                spdlog::warn("MagFormatHook.CallEffectA: arg == nullptr (CTD vindo)");
                auto orig = reinterpret_cast<Fn>(func);
                return orig ? orig(arg, type, unused, edid) : nullptr;
            }

            const auto ps = StringData(arg);
            LogBaseEffect(arg->effect.common);

            auto orig = reinterpret_cast<Fn>(func);
            auto result = orig ? orig(arg, type, unused, edid) : nullptr;
            if (!result) {
                return result;
            }

            const std::size_t fSize = std::strlen(result);
            std::string full = result;
            std::string inside = full.substr(ps.pSize, fSize - ps.pSize - ps.sSize);

            std::string newFull = RDDM::OnEffectFormatted(arg->effect.common, std::move(full), inside, ps.p, ps.s);

            if (newFull != result) {
                arg->buffer = newFull.c_str();
                result = arg->buffer.c_str();
            }

            return result;
        }

        static void Install() {
            Hook::stl::write_call<CallEffectA>(REL::VariantID(10937, 11027, 0x108FF0),
                                               REL::VariantOffset(0x1AD, 0x2B2, 0x1AD));

            spdlog::info("MagFormatHook.CallEffectA instalado (func=0x{:X})", static_cast<unsigned>(func));
        }
    };

    struct CallActiveEffect {
        inline static std::uintptr_t func = 0;

        static const char* thunk(EffectStruct* arg, char* type, char* unused, char* edid) {
            spdlog::info("MagFormatHook.CallActiveEffect::thunk chamado");
            using Fn = const char* (*)(EffectStruct*, char*, char*, char*);

            if (!arg) {
                spdlog::warn("MagFormatHook.CallActiveEffect: arg == nullptr (CTD vindo)");
                auto orig = reinterpret_cast<Fn>(func);
                return orig ? orig(arg, type, unused, edid) : nullptr;
            }

            const auto ps = StringData(arg);
            auto active = arg->effect.active;
            RE::Effect* eff = active ? active->effect : nullptr;

            LogBaseEffect(eff);

            auto orig = reinterpret_cast<Fn>(func);
            auto result = orig ? orig(arg, type, unused, edid) : nullptr;
            if (!result) {
                return result;
            }

            const std::size_t fSize = std::strlen(result);
            std::string full = result;
            std::string inside = full.substr(ps.pSize, fSize - ps.pSize - ps.sSize);

            std::string newFull = RDDM::OnEffectFormatted(eff, std::move(full), inside, ps.p, ps.s);

            if (newFull != result) {
                arg->buffer = newFull.c_str();
                result = arg->buffer.c_str();
            }

            return result;
        }

        static void Install() {
            Hook::stl::write_call<CallActiveEffect>(REL::VariantID(33326, 34105, 0x543320),
                                                    REL::VariantOffset(0x1AD, 0x2B2, 0x1AD));

            spdlog::info("MagFormatHook.CallActiveEffect instalado (func=0x{:X})", static_cast<unsigned>(func));
        }
    };

    struct CallEffectB {
        inline static std::uintptr_t func = 0;

        static const char* thunk(EffectStruct* arg, char* type, char* unused, char* edid) {
            spdlog::info("MagFormatHook.CallEffectB::thunk chamado");
            using Fn = const char* (*)(EffectStruct*, char*, char*, char*);

            if (!arg) {
                spdlog::warn("MagFormatHook.CallEffectB: arg == nullptr (CTD vindo)");
                auto orig = reinterpret_cast<Fn>(func);
                return orig ? orig(arg, type, unused, edid) : nullptr;
            }

            const auto ps = StringData(arg);
            LogBaseEffect(arg->effect.common);

            auto orig = reinterpret_cast<Fn>(func);
            auto result = orig ? orig(arg, type, unused, edid) : nullptr;
            if (!result) {
                return result;
            }

            spdlog::info("MagFormatHook.CallEffectB result after orig: '{}'", result);

            const std::size_t fSize = std::strlen(result);
            std::string full = result;
            std::string inside = full.substr(ps.pSize, fSize - ps.pSize - ps.sSize);
            spdlog::info("MagFormatHook.CallEffectB full='{}' inside='{}'", full, inside);

            if (std::string newFull = RDDM::OnEffectFormatted(arg->effect.common, std::move(full), inside, ps.p, ps.s);
                newFull != result) {
                arg->buffer = newFull.c_str();
                result = arg->buffer.c_str();
                spdlog::info("MagFormatHook.CallEffectB newFull='{}'", result);
            }

            return result;
        }

        static void Install() {
            Hook::stl::write_call<CallEffectB>(REL::VariantID(51024, 51902, 0x8C0D60),
                                               REL::VariantOffset(0x1AD, 0x2B2, 0x1AD));

            spdlog::info("MagFormatHook.CallEffectB instalado (func=0x{:X})", static_cast<unsigned>(func));
        }
    };

public:
    static void Install() {
        CallEffectA::Install();
        CallActiveEffect::Install();
        CallEffectB::Install();
    }
};