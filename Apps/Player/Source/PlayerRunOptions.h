#pragma once

#include <cstdint>
#include <optional>

namespace GameEngine
{

/// How long and in what mode the Player runs, from the launch command.
struct PlayerRunOptions
{
    /// --headless: no window, render device, audio device or input device.
    bool Headless = false;
    /// --fixed-fps under --headless: the fixed step, in frames per second.
    double HeadlessFps = 60.0;
    /// --quit-after: the run exits after this many frames; 0 runs until asked to quit.
    uint64_t QuitAfterFrames = 0;
};

/// Parses --headless, --fixed-fps and --quit-after. Nullopt, with the reason and its
/// fix logged, when the command line is refused: a --quit-after that is not a
/// non-negative frame count, or under --headless a --fixed-fps that is not a positive
/// rate or a flag whose output needs a render device (--write-movie, --screenshot and
/// the HDR output flags). HDR that game.config or the environment enables is not
/// refused: it describes the display a windowed run targets, and a headless run has
/// none. A --fixed-fps outside --headless and movie capture is logged as ignored.
std::optional<PlayerRunOptions> ParsePlayerRunOptions(int argc, const char* const* argv);

/// The whole of `text` as a number; false for empty, partial or trailing text.
bool TryParsePlayerDouble(const char* text, double& out);
/// The whole of `text` as a non-negative integer; false for a sign, partial or trailing text.
bool TryParsePlayerUint64(const char* text, uint64_t& out);

} // namespace GameEngine
