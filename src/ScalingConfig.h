#pragma once
#include <atomic>
#include <filesystem>

namespace RDDM {
    struct ScalingConfig {
        std::atomic<float> frostSlow{1.0f};
        std::atomic<float> frostStamina{1.0f};
        std::atomic<float> shockMagicka{1.0f};
        std::atomic<float> fireBurning{1.0f};

        void Load();
        void Save() const;

    private:
        static std::filesystem::path IniPath();
    };

    ScalingConfig& GetScaling();
}
