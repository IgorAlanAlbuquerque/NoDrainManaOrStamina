#include "ScalingConfig.h"

#include <SimpleIni.h>

#include <string>

#include "PCH.h"
using namespace std::string_literals;

namespace {
    double _getD(CSimpleIniA& ini, const char* sec, const char* k, double defVal) {
        const char* v = ini.GetValue(sec, k, nullptr);
        if (!v) return defVal;
        char* end = nullptr;
        const double d = std::strtod(v, &end);
        return (end && *end == '\0') ? d : defVal;
    }
}

namespace RDDM {
    const std::filesystem::path& _GetThisDllDir();

    std::filesystem::path ScalingConfig::IniPath() {
        const auto& base = RDDM::_GetThisDllDir();
        return base / "RemoveDrains.ini";
    }

    void ScalingConfig::Load() {
        CSimpleIniA ini;
        ini.SetUnicode();
        const auto path = IniPath();
        if (SI_Error rc = ini.LoadFile(path.string().c_str()); rc < 0) return;

        auto clamp = [](double v) { return (float)std::clamp(v, 0.0, 2.0); };
        frostSlow.store(clamp(_getD(ini, "Scales", "FrostSlow", 1.0)), std::memory_order_relaxed);
        frostStamina.store(clamp(_getD(ini, "Scales", "FrostStamina", 1.0)), std::memory_order_relaxed);
        shockMagicka.store(clamp(_getD(ini, "Scales", "ShockMagicka", 1.0)), std::memory_order_relaxed);
        fireBurning.store(clamp(_getD(ini, "Scales", "FireBurning", 1.0)), std::memory_order_relaxed);
    }

    void ScalingConfig::Save() const {
        CSimpleIniA ini;
        ini.SetUnicode();
        const auto path = IniPath();
        ini.LoadFile(path.string().c_str());

        ini.SetDoubleValue("Scales", "FrostSlow", frostSlow.load(std::memory_order_relaxed));
        ini.SetDoubleValue("Scales", "FrostStamina", frostStamina.load(std::memory_order_relaxed));
        ini.SetDoubleValue("Scales", "ShockMagicka", shockMagicka.load(std::memory_order_relaxed));
        ini.SetDoubleValue("Scales", "FireBurning", fireBurning.load(std::memory_order_relaxed));

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        ini.SaveFile(path.string().c_str());
    }

    ScalingConfig& GetScaling() {
        static ScalingConfig g{};  // NOSONAR: Global config state
        return g;
    }
}
