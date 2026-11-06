// The version written in the documents Cue shows (the license reader's "summary and credits",
// the sources note, the licenses) is the program's own: 1.2.3 stayed in the summary after 1.3.
#include <cassert>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include "../src/core/version.h"

static std::string read(const char* path) {
  std::ifstream f(path, std::ios::binary);
  assert(f && "the document is missing");
  std::stringstream s; s << f.rdbuf(); return s.str();
}

int main() {
  const std::string v = CUE_VERSION;
  for (const char* path : {"third_party/libmpv/README.txt", "res/sources-README.txt", "docs/LICENSES.md", "third_party/libmpv/SOURCES.md", "README.md", "README.pt-BR.md"}) {
    const std::string text = read(path);
    // every "Cue x.y", "vx.y" and "Cue-x.y-" in it names this version
    std::regex mention(R"((?:Cue[ -]|tag/v|release v)(\d+\.\d+(?:\.\d+)?))");
    int seen = 0;
    for (std::sregex_iterator it(text.begin(), text.end(), mention), end; it != end; ++it) {
      // (the engine's own history: "unchanged since Cue 1.0", "the same engine build as 1.0")
      if ((*it)[1] == "1.0") continue;
      if ((*it)[1] != v) { std::fprintf(stderr, "%s: %s\n", path, it->str().c_str()); assert(false); }
      ++seen;
    }
    assert(seen > 0);
  }
  std::printf("Version text: the summary, the sources note, the licenses and the readmes all say %s passed\n", CUE_VERSION);
}
