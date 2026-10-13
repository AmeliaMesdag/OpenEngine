#pragma once

#include <cstdint>
#include <string_view>

namespace GameEngine {
namespace Components {

/// The display transform a PostProcessVolume renders its image with. Stored in a scene by name
/// (`PostProcessVolume.look = Filmic`), never by number, so reordering this enum cannot change
/// what a saved scene means. The values are tonemap.frag's `look` push constant.
enum class Look : int32_t
{
    Neutral = 0,      ///< Khronos PBR Neutral: colors stay as authored; the default.
    Filmic = 1,       ///< ACES 2: film contrast, highlights roll off with little hue shift.
    Photographic = 2, ///< AgX: gentle contrast, highlights fade like film stock.
};

inline constexpr int32_t kLookCount = 3;

/// The one name table for the looks: the scene format, the inspector's list and the loader's
/// message all read it.
inline constexpr std::string_view kLookNames[kLookCount] = {"Neutral", "Filmic", "Photographic"};

constexpr std::string_view LookName(Look look)
{
    return kLookNames[static_cast<int32_t>(look)];
}

} // namespace Components
} // namespace GameEngine
