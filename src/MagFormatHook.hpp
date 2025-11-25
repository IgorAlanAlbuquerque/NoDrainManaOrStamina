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

        explicit StringData(EffectStruct* arg) {
            auto prefixPtr = arg->prefix;
            auto suffixPtr = arg->suffix;

            p = prefixPtr ? prefixPtr : "";
            s = suffixPtr ? suffixPtr : "";
            pSize = p.size();
            sSize = s.size();
        }
    };

    static_assert(offsetof(EffectStruct, buffer) == 0x00);
    static_assert(offsetof(EffectStruct, effect) == 0x10);
    static_assert(offsetof(EffectStruct, prefix) == 0x18);
    static_assert(offsetof(EffectStruct, suffix) == 0x20);

    struct CallEffectB {
        inline static std::uintptr_t func = 0;

        static const char* thunk(EffectStruct* arg, char* type, char* unused, char* edid) {
            using Fn = const char* (*)(EffectStruct*, char*, char*, char*);

            if (!arg) {
                auto orig = reinterpret_cast<Fn>(func);
                return orig ? orig(arg, type, unused, edid) : nullptr;
            }

            const auto ps = StringData(arg);

            auto orig = reinterpret_cast<Fn>(func);
            auto result = orig ? orig(arg, type, unused, edid) : nullptr;
            if (!result) {
                return result;
            }

            const std::size_t fSize = std::strlen(result);
            std::string full = result;
            std::string inside = full.substr(ps.pSize, fSize - ps.pSize - ps.sSize);

            if (std::string newFull = RDDM::OnEffectFormatted(arg->effect.common, std::move(full), inside, ps.p, ps.s);
                newFull != result) {
                arg->buffer = newFull.c_str();
                result = arg->buffer.c_str();
            }

            return result;
        }

        static void Install() {
            Hook::stl::write_call<CallEffectB>(REL::VariantID(51024, 51902, 0x8C0D60),
                                               REL::VariantOffset(0x1AD, 0x2B2, 0x1AD));
        }
    };

public:
    static void Install() { CallEffectB::Install(); }
};