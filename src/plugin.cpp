#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "Hook_DualValueModifier.h"
#include "PCH.h"
#include "ScalingConfig.h"
#include "UI_RemoveDrains.h"

#ifndef DLLEXPORT
    #include "REL/Relocation.h"
#endif
#ifndef DLLEXPORT
    #define DLLEXPORT __declspec(dllexport)
#endif

namespace RDDM {
    const std::filesystem::path& _GetThisDllDir() {
        static std::filesystem::path cached = []() {
            HMODULE hMod = nullptr;

            if (!::GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&_GetThisDllDir), &hMod)) {
                return std::filesystem::current_path();
            }

            std::vector<wchar_t> buf(MAX_PATH);
            DWORD len = 0;
            for (;;) {
                len = ::GetModuleFileNameW(hMod, buf.data(), static_cast<DWORD>(buf.size()));
                if (len == 0) {
                    return std::filesystem::current_path();
                }
                if (len < buf.size() - 1) break;
                buf.resize(buf.size() * 2);
            }

            std::filesystem::path full{std::wstring{buf.data(), len}};
            return full.parent_path();
        }();

        return cached;
    }
}

namespace {
    void InitializeLogger_RD() {
        if (auto path = SKSE::log::log_directory()) {
            *path /= "RemoveDrains.log";
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
            auto logger = std::make_shared<spdlog::logger>("global", sink);
            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::info);
            spdlog::flush_on(spdlog::level::info);
            spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
            spdlog::info("[RD] Logger iniciado.");
        }
    }

    void OnMessage(SKSE::MessagingInterface::Message* msg) {
        if (!msg) return;
        switch (msg->type) {
            case SKSE::MessagingInterface::kDataLoaded: {
                RDDM_UI::Register();
                break;
            }
            default:
                break;
        }
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    InitializeLogger_RD();

    spdlog::info("[RD] SKSEPlugin_Load");
    RDDM::GetScaling().Load();
    spdlog::info("[RD] Config carregada: frostStamina={}, shockMagicka={}", RDDM::GetScaling().frostStamina.load(),
                 RDDM::GetScaling().shockMagicka.load());

    RDDM_Hook::Install_DualValueModifier();
    spdlog::info("[RD] Hooks instalados.");

    if (const auto mi = SKSE::GetMessagingInterface()) {
        mi->RegisterListener(OnMessage);
    }

    return true;
}
