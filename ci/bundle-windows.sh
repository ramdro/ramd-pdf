#!/usr/bin/env bash
set -euo pipefail
mkdir -p dist
cp build/ramd-pdf.exe build/editor-tests.exe /ucrt64/bin/qpdf.exe dist/
windeployqt6 --release --compiler-runtime --no-translations dist/ramd-pdf.exe
# The tests use the offscreen Qt platform plugin.
qt_plugins="$(cygpath -u "$(qtpaths6 --query QT_INSTALL_PLUGINS)")"
mkdir -p dist/platforms
cp "$qt_plugins/platforms/qoffscreen.dll" dist/platforms/
printf '[Paths]\nPlugins=.\n' > dist/qt.conf
# Resolve Poppler/qpdf and Qt plugin dependencies, including transitive DLLs.
while IFS= read -r -d '' binary; do
  ntldd -R "$binary" > dependencies.txt
  while IFS= read -r dep; do
    dep="$(cygpath -u "$dep")"
    case "$dep" in
      */ucrt64/bin/*) if [[ ! -f "dist/$(basename "$dep")" ]]; then cp "$dep" dist/; fi ;;
    esac
  done < <(sed -n 's/.*=> \(.*\) (0x[0-9a-fA-F]*).*/\1/p' dependencies.txt | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
done < <(find dist -type f \( -name '*.exe' -o -name '*.dll' \) -print0)
mkdir -p dist/licenses
cp -R /ucrt64/share/licenses/. dist/licenses/
pacman -Q > dist/BUILD-DEPENDENCIES.txt
git archive --format=zip -o dist/RAMD-PDF-corresponding-source.zip HEAD
