#include "Hooks.h"

#include <spdlog/spdlog.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

#include "PCH.h"
#include "RE/A/ActiveEffect.h"
#include "RE/B/BSContainer.h"
#include "RE/D/DualValueModifierEffect.h"
#include "RE/M/MagicItem.h"
#include "RE/M/MagicItemTraversalFunctor.h"
#include "ScalingConfig.h"
#include "common.h"

using RDDM::GetScaling;

namespace {
    /*using MIDescOp_t = RE::BSContainer::ForEachResult (*)(void*, RE::Effect*);
    static MIDescOp_t g_MI_Op_Orig_Main = nullptr;
    static MIDescOp_t g_MI_Op_Orig_Underscore = nullptr;

    static bool Prefer6ByteBranch(std::uintptr_t addr) {
        const auto* b = reinterpret_cast<const std::uint8_t*>(addr);

        if (b[0] == 0x48 && b[1] == 0x8B && b[2] == 0xC4) return true;
        if (b[0] == 0x48 && b[1] == 0x89 && b[2] == 0x5C && b[3] == 0x24) return true;

        return true;
    }

    static void DumpHead16(const char* tag, std::uintptr_t addr) {
        const auto* b = reinterpret_cast<const std::uint8_t*>(addr);
        spdlog::info(
            "[RD] {} 0x{:X}  head = "
            "{:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}  "
            "{:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
            tag, static_cast<std::uint64_t>(addr), b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10],
            b[11], b[12], b[13], b[14], b[15]);
    }

    static bool IsExecutable(std::uintptr_t addr) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) return false;
        const DWORD p = mbi.Protect;
        return (p & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    }

    static std::uintptr_t FollowAllJumps(std::uintptr_t addr, int maxHops = 8) {
        for (int hop = 0; hop < maxHops; ++hop) {
            const auto* p = reinterpret_cast<const std::uint8_t*>(addr);

            if (p[0] == 0xE9) {
                const auto disp = *reinterpret_cast<const std::int32_t*>(p + 1);
                addr = addr + 5 + disp;
                continue;
            }

            if (p[0] == 0xEB) {
                const auto disp = *reinterpret_cast<const std::int8_t*>(p + 1);
                addr = addr + 2 + disp;
                continue;
            }

            if (p[0] == 0xFF && p[1] == 0x25) {
                const auto disp = *reinterpret_cast<const std::int32_t*>(p + 2);
                const auto ptrLoc = addr + 6 + disp;
                const auto target = *reinterpret_cast<const std::uint64_t*>(ptrLoc);
                addr = static_cast<std::uintptr_t>(target);
                continue;
            }

            if (p[0] == 0x48 && p[1] == 0xB8) {
                const auto imm = *reinterpret_cast<const std::uint64_t*>(p + 2);
                const auto* p2 = p + 10;
                if (p2[0] == 0xFF && p2[1] == 0xE0) {
                    addr = static_cast<std::uintptr_t>(imm);
                    continue;
                }
            }

            break;
        }
        return addr;
    }

    static bool find_deals_and_number(const std::string& s, size_t beforePos, size_t& numStart, size_t& numLen,
                                      double& value) {
        if (beforePos == 0) return false;

        auto ci_eq = [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); };

        static constexpr const char* kw = "deals ";
        const size_t klen = 6;
        size_t i = (beforePos >= klen) ? (beforePos - klen) : 0;
        for (;;) {
            bool ok = true;
            for (size_t j = 0; j < klen; ++j) {
                if (i + j >= s.size() || !ci_eq(s[i + j], kw[j])) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                size_t p = i + klen;
                while (p < s.size() && std::isspace((unsigned char)s[p])) ++p;
                if (p >= s.size()) return false;

                size_t start = p;
                bool hasDigit = false;
                while (p < s.size()) {
                    char c = s[p];
                    if ((c >= '0' && c <= '9') || c == '.') {
                        hasDigit = true;
                        ++p;
                    } else
                        break;
                }
                if (!hasDigit) return false;

                numStart = start;
                numLen = p - start;
                try {
                    value = std::stod(s.substr(numStart, numLen));
                } catch (...) {
                    return false;
                }
                return true;
            }
            if (i == 0) break;
            --i;
        }
        return false;
    }

    static std::optional<double> ComputeSecFactor(const RE::SpellItem* sp) {
        if (!sp) return std::nullopt;
        for (auto* eff : sp->effects) {
            if (!eff || !eff->baseEffect) continue;
            auto* m = eff->baseEffect;
            if (Common::IsDV_Health_Stamina(m)) {
                const double slider = RDDM::GetScaling().frostStamina.load(std::memory_order_relaxed);
                return double(m->data.secondAVWeight) * slider;
            }
            if (Common::IsDV_Health_Magicka(m)) {
                const double slider = RDDM::GetScaling().shockMagicka.load(std::memory_order_relaxed);
                return double(m->data.secondAVWeight) * slider;
            }
        }
        return std::nullopt;
    }

    static RE::BSString* GetLineMember(void* self) {
        static std::atomic<int> cachedOff{-1};
        int off = cachedOff.load(std::memory_order_relaxed);
        if (off >= 0) return reinterpret_cast<RE::BSString*>((char*)self + off);

        for (int o = 0; o <= 0x100; o += 8) {
            auto* cand = reinterpret_cast<RE::BSString*>((char*)self + o);
            const char* s = cand->c_str();
            if (!s || !*s) continue;

            if (std::strstr(s, "<sec>") || std::strstr(s, "deal") || std::strstr(s, "damage")) {
                cachedOff.store(o, std::memory_order_relaxed);
                return cand;
            }
        }
        return nullptr;
    }*/

    template <class T>
    struct GetSecondaryWeightHook {
        static constexpr std::size_t kIndex = 0x22;

        using Fn = float(const T*);
        static inline Fn* _orig{};

        static float thunk(const T* self) {
            auto* m = self ? const_cast<RE::EffectSetting*>(self->GetBaseObject()) : nullptr;
            if (!m) return _orig(self);

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

    /*template <class T>
    struct VM_ModifyAV_Hook {
        static constexpr std::size_t kIndex = 0x20;
        using Fn = void(T*, RE::Actor*, float, RE::ActorValue);
        static inline Fn* _orig{};

        static void thunk(T* self, RE::Actor* actor, float value, RE::ActorValue av) {
            auto* mgef = self ? const_cast<RE::EffectSetting*>(self->GetBaseObject()) : nullptr;

            if (mgef && Common::IsFireBurningMGEF(mgef) && self->duration > 0.0f && av == RE::ActorValue::kHealth) {
                const float f = RDDM::GetScaling().fireBurning.load(std::memory_order_relaxed);
                if (f != 1.0f) {
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
    };*/

    /*static RE::BSContainer::ForEachResult MI_Op_Thunk(void* self, RE::Effect* eff) {
        spdlog::info("entrou");
        const auto r = g_MI_Op_Orig_Main ? g_MI_Op_Orig_Main(self, eff) : RE::BSContainer::ForEachResult::kContinue;

        auto* mgef = (eff && eff->baseEffect) ? eff->baseEffect : nullptr;
        if (!mgef) return r;

        double mult = 1.0;
        if (Common::IsDV_Health_Stamina(mgef))
            mult = RDDM::GetScaling().frostStamina.load();
        else if (Common::IsDV_Health_Magicka(mgef))
            mult = RDDM::GetScaling().shockMagicka.load();

        const double secW = mgef->data.secondAVWeight;
        if (secW == 0.0 && mult == 1.0) return r;

        auto* line = GetLineMember(self);
        if (!line) return r;
        const char* cur = line->c_str();
        if (!cur || !*cur) return r;

        std::string s = cur;
        size_t pos = s.find("<sec>");
        if (pos == std::string::npos) return r;

        size_t nStart = 0, nLen = 0;
        double mag = 0.0;
        if (find_deals_and_number(s, pos, nStart, nLen, mag)) {
            const auto repl = fmt::format("{:.1f}", mag * secW * mult);
            s.replace(pos, 5, repl);
        } else {
            const auto repl = fmt::format("{:.1f}", secW * mult);
            s.replace(pos, 5, repl);
        }

        *line = s.c_str();
        return r;
    }


    static RE::BSContainer::ForEachResult MI_Op_Thunk_Underscore(void* self, RE::Effect* eff) {
        spdlog::info("entrou no undersocre");
        const auto r =
            g_MI_Op_Orig_Underscore ? g_MI_Op_Orig_Underscore(self, eff) :
    RE::BSContainer::ForEachResult::kContinue;

        auto* mgef = (eff && eff->baseEffect) ? eff->baseEffect : nullptr;
        if (!mgef) return r;

        double mult = 1.0;
        if (Common::IsDV_Health_Stamina(mgef))
            mult = RDDM::GetScaling().frostStamina.load();
        else if (Common::IsDV_Health_Magicka(mgef))
            mult = RDDM::GetScaling().shockMagicka.load();

        const double secW = mgef->data.secondAVWeight;
        if (secW == 0.0 && mult == 1.0) return r;

        auto* line = GetLineMember(self);
        if (!line) return r;
        const char* cur = line->c_str();
        if (!cur || !*cur) return r;

        std::string s = cur;
        size_t pos = s.find("<sec>");
        if (pos == std::string::npos) return r;

        size_t nStart = 0, nLen = 0;
        double mag = 0.0;
        if (find_deals_and_number(s, pos, nStart, nLen, mag)) {
            const auto repl = fmt::format("{:.1f}", mag * secW * mult);
            s.replace(pos, 5, repl);
        } else {
            const auto repl = fmt::format("{:.1f}", secW * mult);
            s.replace(pos, 5, repl);
        }

        *line = s.c_str();
        return r;
    }*/
}

void RDDM_Hook::Install_Hooks() {
    GetSecondaryWeightHook<RE::DualValueModifierEffect>::Install();
    VM_ModifyAV_Hook_Slow<RE::PeakValueModifierEffect>::Install();
    /*VM_ModifyAV_Hook<RE::ValueModifierEffect>::Install();*/
}

/*void RDDM_Hook::Install_DescHook() {
    {
        REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_GetMagicItemDescriptionFunctor[0]};
        constexpr int kOpIdx = 4;
        const auto old = vtbl.write_vfunc(kOpIdx, reinterpret_cast<std::uintptr_t>(&MI_Op_Thunk));
        g_MI_Op_Orig_Main = reinterpret_cast<MIDescOp_t>(old);
    }

    {
        REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE___GetMagicItemDescriptionFunctor[0]};
        constexpr int kOpIdx = 4;
        const auto old = vtbl.write_vfunc(kOpIdx, reinterpret_cast<std::uintptr_t>(&MI_Op_Thunk_Underscore));
        g_MI_Op_Orig_Underscore = reinterpret_cast<MIDescOp_t>(old);
    }
}*/