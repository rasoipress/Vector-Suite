#!/bin/sh
# Verifica la build e la coerenza dei sorgenti.
# Verifica app standalone, bundle nativo incorporato e coerenza del catalogo.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
APP_BUNDLE="$PROJECT_DIR/build/Vector Suite.app"
DMG_PATH="$PROJECT_DIR/build/Vector Suite.dmg"
NODE_BIN=${NODE_BIN:-$(command -v node || true)}

echo "== Bundle applicativo"
test -x "$APP_BUNDLE/Contents/MacOS/VectorSuite"
plutil -lint "$APP_BUNDLE/Contents/Info.plist"
test -f "$APP_BUNDLE/Contents/Resources/VectorSuite.icns"
test -f "$APP_BUNDLE/Contents/Resources/ui/index.html"
test -f "$APP_BUNDLE/Contents/Resources/ui/app.js"
test -f "$APP_BUNDLE/Contents/Resources/ui/icons.js"
test -f "$APP_BUNDLE/Contents/Resources/ui/styles.css"
test -d "$APP_BUNDLE/Contents/Resources/Native/VectorSuiteNative.aip"
VERIFY_STAGE=$(mktemp -d "${TMPDIR:-/tmp}/vector-suite-verify.XXXXXX")
trap 'rm -rf "$VERIFY_STAGE"' EXIT HUP INT TERM
ditto --norsrc --noextattr --noacl "$APP_BUNDLE" "$VERIFY_STAGE/Vector Suite.app"
xattr -cr "$VERIFY_STAGE/Vector Suite.app" 2>/dev/null || true
codesign --verify --deep --strict "$VERIFY_STAGE/Vector Suite.app"
rm -rf "$VERIFY_STAGE"
trap - EXIT HUP INT TERM
echo "  ok"

echo "== Pacchetto DMG"
test -f "$DMG_PATH"
hdiutil verify "$DMG_PATH" >/dev/null
echo "  ok"

echo "== Marcatura SVG"
xmllint --noout "$PROJECT_DIR/Resources/VectorSuiteLogo.svg"
xmllint --noout "$PROJECT_DIR/Resources/icon/VectorSuiteMark.svg"
test -f "$PROJECT_DIR/Resources/icon/VectorSuiteMark.png"
echo "  ok"

echo "== Versione"
python3 "$SCRIPT_DIR/apply-version.py" --check

echo "== Asset generati"
python3 "$SCRIPT_DIR/generate-assets.py" --check

echo "== Coerenza del catalogo"
python3 "$SCRIPT_DIR/verify-catalog.py"

echo "== Sintassi JavaScript"
if [ -n "$NODE_BIN" ]; then
  "$NODE_BIN" --check "$PROJECT_DIR/Resources/ui/app.js"
  "$NODE_BIN" --check "$PROJECT_DIR/Resources/ui/icons.js"
  echo "  ok"
else
  echo "  saltata (node non disponibile)"
fi

echo "== Interfaccia dell'app"
if [ -n "$NODE_BIN" ]; then
  "$NODE_BIN" "$PROJECT_DIR/tests/ui-smoke.js"
else
  echo "  saltata (node non disponibile)"
fi

echo "== Bundle nativo"
"$SCRIPT_DIR/verify-native.sh"

echo
echo "Build valida."
