#!/usr/bin/env bash
# The portable tests again, built for Windows (MinGW-w64) and run under Wine: the same assertions
# through the _WIN32 code paths (BCrypt, DPAPI, ReplaceFileW, wide paths). Wine is not Windows;
# this complements, never replaces, running them on a real PC (tools\native-tests.bat).
#   bash tools/wine-tests.sh            (needs x86_64-w64-mingw32-g++ and wine64)
set -u
cd "$(dirname "$0")/.."
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINE=${WINE:-$(command -v wine64 || command -v wine || echo /usr/lib/wine/wine64)}
export WINEDEBUG=${WINEDEBUG:--all}
out=build/wine-tests; mkdir -p $out/obj $out/bin
F="-std=c++20 -O1 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DNOMINMAX"
objs=()
# (a header or a generated table newer than an object rebuilds it too: the translations live in
# i18n_catalog.inc, which i18n.cpp includes)
newest=$(ls -t src/*/*.h src/*/*.inc | head -1)
for src in src/core/*.cpp src/ui/*.cpp installer/setup_ui.cpp; do
  o=$out/obj/$(basename "${src%.cpp}").o
  [[ $o -nt $src && $o -nt $newest ]] || $CXX $F -c "$src" -o "$o" || { echo "FAILED compile $src"; exit 1; }
  objs+=("$o")
done
bad=0; n=0
for t in tests/*_test.cpp; do
  [[ $t == tests/*_engine_test.cpp ]] && continue   # need the engine DLL (tools/native-tests.bat)
  name=$(basename "${t%.cpp}")
  extra=(); [[ $name == ui_language_win_test ]] && extra=(src/win/ui_language.cpp -municode)
  [[ $name == recent_thumbs_win_test ]] && extra=(src/win/recent_thumbs.cpp src/win/cover_image.cpp src/win/platform.cpp -lwindowscodecs -ldwmapi -lshcore -lcomdlg32 -lwinmm)
  [[ $name == remote_path_win_test ]] && extra=(src/win/platform.cpp -lwindowscodecs -ldwmapi -lshcore -lcomdlg32 -lwinmm -ladvapi32 -luser32 -lgdi32)
  $CXX $F "$t" "${extra[@]}" "${objs[@]}" -static -lbcrypt -lcrypt32 -lole32 -lshell32 -luuid -o "$out/bin/$name.exe" 2>"$out/$name.build.log" ||
    { echo "FAILED compile $name: $(grep -m1 error "$out/$name.build.log")"; bad=1; continue; }
  if timeout 600 "$WINE" "$out/bin/$name.exe" > "$out/$name.log" 2>&1; then n=$((n + 1)); else echo "FAILED $name: $(tail -2 "$out/$name.log" | tr '\n' ' ')"; bad=1; fi
done
[[ $bad == 0 ]] && echo "wine: all $n Windows builds of the portable tests passed" || { echo "wine: $n passed, some failed"; exit 1; }
