#pragma once
#include <string>
#include <string_view>

// A song's cover, found without the playback engine, for small pictures such as the phone's
// queue: the picture embedded in the file's tags (ID3v2 APIC/PIC, FLAC PICTURE, MP4 covr,
// Ogg Vorbis/Opus METADATA_BLOCK_PICTURE), else a picture beside it (the song's own
// "<name>.jpg", then cover/folder/front). The result is the encoded image (JPEG, PNG, WebP…)
// for the platform's decoder; empty when there is none. Reads at most kMaxCoverBytes.
namespace cover_art {

inline constexpr size_t kMaxCoverBytes = 32u << 20;

// From the bytes of a tag block or a whole small file (tests; any format above).
std::string embedded_in(std::string_view bytes);
// From a file on disk (UTF-8 path): embedded first, then a picture beside it.
std::string embedded_in_file(const std::string& utf8_path);
std::string beside(const std::string& utf8_path);
std::string find(const std::string& utf8_path);

}  // namespace cover_art
