#pragma once
#include <cstddef>
#include <string>

// Accurate word timing only: a <media-stem>.words.json sidecar, in Whisper's
// segments[].words[] format, or {"words":[{"word":...,"start":...,"end":...}]}.
// Times are absolute media seconds. Ordinary line-timed subtitles are untouched.
constexpr size_t kWordHighlightMaxInput = 4 * 1024 * 1024;
// Returns an empty string for unsupported, malformed or over-budget input.
// libass owns the clock/animation: pause, seek, speed and sub-delay stay native.
std::string word_highlight_ass(const std::string& json, bool reduced_motion = false);
