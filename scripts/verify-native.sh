#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
PLUGIN_ROOT="$PROJECT_ROOT/native/VectorSuiteNative"
BUNDLE="$PROJECT_ROOT/build/native/release/VectorSuiteNative.aip"

plutil -lint "$PLUGIN_ROOT/Config/Info.plist"
python3 "$SCRIPT_DIR/generate-assets.py" --check
python3 "$SCRIPT_DIR/verify-catalog.py"
ICON_COUNT=$(find "$PLUGIN_ROOT/Resources/raw" -maxdepth 1 -type f -name 'VSIcon-*.svg' | wc -l | tr -d ' ')
test "$ICON_COUNT" -eq 25 || { echo "Attese 25 icone native, trovate $ICON_COUNT."; exit 1; }
for ICON in "$PLUGIN_ROOT"/Resources/raw/VSIcon-*.svg; do
  xmllint --noout "$ICON"
done

test -f "$PLUGIN_ROOT/VectorSuiteNative.xcodeproj/project.pbxproj"
test -f "$PLUGIN_ROOT/Source/VectorSuiteCatalog.cpp"
test -f "$PLUGIN_ROOT/Source/VectorSuiteGeometry.cpp"
test -f "$PLUGIN_ROOT/Source/VectorSuiteModules.cpp"
test -f "$PLUGIN_ROOT/Source/VectorSuitePanel.mm"
test -f "$PLUGIN_ROOT/Resources/raw/IDToFile.txt"

if grep -R -Eiq '/Users/|[[:alnum:]._%+-]+@[[:alnum:].-]+\.[[:alpha:]]{2,}' \
  "$PLUGIN_ROOT/Config" "$PLUGIN_ROOT/Source" "$PLUGIN_ROOT/Resources" "$PLUGIN_ROOT/README.md"; then
  echo "Rilevato un percorso personale o indirizzo e-mail nei sorgenti nativi."
  exit 1
fi

if [ -d "$BUNDLE" ]; then
  EXECUTABLE="$BUNDLE/Contents/MacOS/VectorSuiteNative"
  test -x "$EXECUTABLE"
  test -f "$BUNDLE/Contents/Resources/pipl/plugin.pipl"
  test -f "$BUNDLE/Contents/Resources/txt/IDToFile.txt"
  test "$(defaults read "$BUNDLE/Contents/Info.plist" CFBundleIdentifier)" = "studio.vectorsuite.plugin.core"
  BUNDLE_ICON_COUNT=$(find "$BUNDLE/Contents/Resources/svg" -maxdepth 1 -type f -name 'VSIcon-*.svg' | wc -l | tr -d ' ')
  test "$BUNDLE_ICON_COUNT" -eq 25 || { echo "Bundle incompleto: $BUNDLE_ICON_COUNT/25 icone."; exit 1; }
  codesign --verify --deep --verbose=2 "$BUNDLE"
  ARCHS=$(lipo -archs "$EXECUTABLE")
  case "$ARCHS" in *arm64*) : ;; *) echo "Slice arm64 mancante."; exit 1 ;; esac
  case "$ARCHS" in *x86_64*) : ;; *) echo "Slice x86_64 mancante."; exit 1 ;; esac
  echo "Architetture: $ARCHS"
  echo "Icone: $BUNDLE_ICON_COUNT/25"
else
  echo "Sorgenti validi; esegui scripts/build-native.sh per creare il bundle."
fi
