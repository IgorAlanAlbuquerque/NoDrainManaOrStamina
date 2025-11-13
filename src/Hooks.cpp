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
#include "RE/M/MagicMenu.h"
#include "ScalingConfig.h"
#include "common.h"

using RDDM::GetScaling;

namespace {
    using GetDesc_fn = void (*)(RE::TESDescription*, RE::BSString&, RE::TESForm*, std::uint32_t);
    static GetDesc_fn g_callsite_orig = nullptr;

    /*static bool Prefer6ByteBranch(std::uintptr_t addr) {
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

    bool find_deals_and_number(const std::string& s, size_t beforePos, size_t& numStart, size_t& numLen,
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

    std::optional<double> ComputeSecFactor(const RE::SpellItem* sp) {
        if (!sp) return std::nullopt;
        for (auto const* eff : sp->effects) {
            if (!eff || !eff->baseEffect) continue;
            auto const* m = eff->baseEffect;
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

    void GetDesc_CallThunk(RE::TESDescription* self, RE::BSString& out, RE::TESForm* parent, std::uint32_t fieldType) {
        static thread_local bool reent = false;
        if (reent) {
            g_callsite_orig(self, out, parent, fieldType);
            return;
        }

        reent = true;
        g_callsite_orig(self, out, parent, fieldType);

        auto* sp = parent ? parent->As<RE::SpellItem>() : nullptr;
        const char* cur = out.c_str();
        if (!sp || !cur) {
            reent = false;
            return;
        }

        std::string s = cur;
        if (s.find("<sec>") == std::string::npos) {
            reent = false;
            return;
        }

        auto secF = ComputeSecFactor(sp);

        size_t pos = 0;
        while ((pos = s.find("<sec>", pos)) != std::string::npos) {
            if (!secF.has_value()) {
                s.erase(pos, 5);
                continue;
            }

            size_t nStart = 0, nLen = 0;
            double magVal = 0.0;
            if (find_deals_and_number(s, pos, nStart, nLen, magVal)) {
                const long iv = (long)std::lround(magVal * (*secF));
                const std::string repl = std::to_string(iv);
                s.replace(pos, 5, repl);
                pos += repl.size();
            } else {
                s.erase(pos, 5);
            }
        }

        out = s.c_str();
        reent = false;
    }

    std::uintptr_t Find_GetDesc_CallInside(std::uintptr_t funcStart, std::size_t maxScanBytes) {
        const std::uint8_t pat[] = {0x41, 0xB9, 0x43, 0x45, 0x53, 0x44};
        auto* p = reinterpret_cast<const std::uint8_t*>(funcStart);
        auto* end = p + maxScanBytes;

        for (auto* cur = p; cur + sizeof(pat) + 5 <= end; ++cur) {
            if (std::memcmp(cur, pat, sizeof(pat)) == 0) {
                for (int off = 6; off <= 48 && cur + off + 5 <= end; ++off) {
                    if (cur[off] == 0xE8) {
                        return reinterpret_cast<std::uintptr_t>(cur + off);
                    }
                }
            }
        }
        return 0;
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

    REL::Relocation<std::uintptr_t> vtblMagic{RE::VTABLE_MagicMenu[0]};
    auto const* processMsg = reinterpret_cast<std::uintptr_t*>(vtblMagic.address());
    auto procAddr = processMsg[0x04];

    constexpr std::size_t kScan = 0xC00;
    const auto callSite = Find_GetDesc_CallInside(procAddr, kScan);
    if (!callSite) {
        spdlog::warn("[RD] Nao encontrei o CALL de TESDescription::GetDescription dentro de MagicMenu::ProcessMessage");
        return;
    }

    auto& tramp = SKSE::GetTrampoline();
    const auto orig = tramp.write_call<5>(callSite, reinterpret_cast<std::uintptr_t>(&GetDesc_CallThunk));
    g_callsite_orig = reinterpret_cast<GetDesc_fn>(orig);

    spdlog::info("[RD] Hook no call-site de GetDescription em MagicMenu::ProcessMessage ok. call=0x{:X}, orig=0x{:X}",
                 (unsigned)callSite, (unsigned)orig);
}