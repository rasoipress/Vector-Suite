#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$PROJECT_DIR/build"
APP_BUNDLE="$BUILD_DIR/Vector Suite.app"
CONTENTS_DIR="$APP_BUNDLE/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"
ICONSET_DIR="$BUILD_DIR/VectorSuite.iconset"
ICON_MASTER_SVG="$BUILD_DIR/icon/VectorSuiteIcon.svg"
MASTER_ICON="$BUILD_DIR/VectorSuite-1024.png"
ICON_PREVIEW_DIR="$BUILD_DIR/icon-preview"
NATIVE_BUNDLE="$BUILD_DIR/native/release/VectorSuiteNative.aip"
MODULE_CACHE="${TMPDIR:-/tmp}/vector-suite-manager-module-cache"
DMG_PATH="$BUILD_DIR/Vector Suite.dmg"

mkdir -p "$MACOS_DIR" "$RESOURCES_DIR"
mkdir -p "$MODULE_CACHE"

# Marchio e icone sono generati da una sola sorgente geometrica: rigenerarli
# a ogni build impedisce che il bundle e i sorgenti si allontanino.
python3 "$SCRIPT_DIR/generate-assets.py"

"$SCRIPT_DIR/build-native.sh"

clang \
  "$PROJECT_DIR/Sources/main.m" \
  -fobjc-arc \
  -fmodules \
  -fmodules-cache-path="$MODULE_CACHE" \
  -arch arm64 \
  -arch x86_64 \
  -mmacosx-version-min=14.0 \
  -framework Cocoa \
  -framework WebKit \
  -o "$MACOS_DIR/VectorSuite"

cp "$PROJECT_DIR/Resources/Info.plist" "$CONTENTS_DIR/Info.plist"
rm -rf "$RESOURCES_DIR/ui"
cp -R "$PROJECT_DIR/Resources/ui" "$RESOURCES_DIR/ui"
rm -rf "$RESOURCES_DIR/Native"
mkdir -p "$RESOURCES_DIR/Native"
ditto --norsrc --noextattr --noacl "$NATIVE_BUNDLE" "$RESOURCES_DIR/Native/VectorSuiteNative.aip"

rm -rf "$ICONSET_DIR"
rm -rf "$ICON_PREVIEW_DIR"
mkdir -p "$ICONSET_DIR" "$ICON_PREVIEW_DIR"
# Il master non è il disegno originale ma la sua versione centrata nel quadrato
# utile di macOS, prodotta da generate-assets.py: il file disegnato a mano in
# Illustrator resta intatto. Nel master le classi CSS sono già risolte in
# attributi, perché i rasterizzatori che ignorano il foglio di stile
# restituirebbero un quadrato nero.
# QuickLook ha bisogno di una sessione grafica: su una macchina di build
# automatica non produce niente. Se c'è rsvg-convert si usa quello, che non
# dipende dal server delle finestre; altrimenti si ripiega su QuickLook.
if command -v rsvg-convert >/dev/null 2>&1; then
  rsvg-convert -w 1024 -h 1024 -o "$MASTER_ICON" "$ICON_MASTER_SVG"
else
  qlmanage -t -s 1024 -o "$ICON_PREVIEW_DIR" "$ICON_MASTER_SVG" >/dev/null 2>&1 || true
  if [ -f "$ICON_PREVIEW_DIR/VectorSuiteIcon.svg.png" ]; then
    cp "$ICON_PREVIEW_DIR/VectorSuiteIcon.svg.png" "$MASTER_ICON"
  fi
fi

if [ ! -f "$MASTER_ICON" ]; then
  echo "Errore: nessun rasterizzatore ha prodotto il master dell'icona." >&2
  echo "Installa librsvg (brew install librsvg) oppure genera a mano il PNG" >&2
  echo "1024×1024 in $MASTER_ICON e rilancia." >&2
  exit 1
fi
sips -z 16 16 "$MASTER_ICON" --out "$ICONSET_DIR/icon_16x16.png" >/dev/null
sips -z 32 32 "$MASTER_ICON" --out "$ICONSET_DIR/icon_16x16@2x.png" >/dev/null
sips -z 32 32 "$MASTER_ICON" --out "$ICONSET_DIR/icon_32x32.png" >/dev/null
sips -z 64 64 "$MASTER_ICON" --out "$ICONSET_DIR/icon_32x32@2x.png" >/dev/null
sips -z 128 128 "$MASTER_ICON" --out "$ICONSET_DIR/icon_128x128.png" >/dev/null
sips -z 256 256 "$MASTER_ICON" --out "$ICONSET_DIR/icon_128x128@2x.png" >/dev/null
sips -z 256 256 "$MASTER_ICON" --out "$ICONSET_DIR/icon_256x256.png" >/dev/null
sips -z 512 512 "$MASTER_ICON" --out "$ICONSET_DIR/icon_256x256@2x.png" >/dev/null
sips -z 512 512 "$MASTER_ICON" --out "$ICONSET_DIR/icon_512x512.png" >/dev/null
cp "$MASTER_ICON" "$ICONSET_DIR/icon_512x512@2x.png"
iconutil -c icns "$ICONSET_DIR" -o "$RESOURCES_DIR/VectorSuite.icns"

chmod +x "$SCRIPT_DIR"/*.sh

# Ricostruisce il bundle senza resource fork, attributi estesi e ACL.
# `xattr -cr` da solo non basta: macOS riattacca com.apple.provenance ai file e
# codesign lo rifiuta con "resource fork, Finder information, or similar
# detritus not allowed". ditto copia i dati e scarta i metadati.
sanitise_bundle() {
  BUNDLE="$1"
  find "$BUNDLE" -name '.DS_Store' -delete 2>/dev/null || true
  find "$BUNDLE" -name '._*' -delete 2>/dev/null || true
  xattr -cr "$BUNDLE" 2>/dev/null || true
  rm -rf "$BUNDLE.sanitised"
  ditto --norsrc --noextattr --noacl "$BUNDLE" "$BUNDLE.sanitised"
  rm -rf "$BUNDLE"
  mv "$BUNDLE.sanitised" "$BUNDLE"
  chmod +x "$BUNDLE/Contents/MacOS/VectorSuite"
}

# L'asset Liquid Glass di macOS 26 entra nel bundle prima della firma, che deve
# coprirlo. Se il pacchetto .icon non esiste o la macchina non ha Xcode 26 il
# passaggio si salta da solo e l'app resta valida con la sola icona .icns.
"$SCRIPT_DIR/build-liquid-glass-icon.sh" "$APP_BUNDLE"

# `--deep` è deprecato per la firma e amplifica proprio questo errore: qui non
# ci sono bundle annidati, quindi la firma piatta è sufficiente e corretta.
sanitise_bundle "$APP_BUNDLE"
codesign --force --sign - "$APP_BUNDLE/Contents/Resources/Native/VectorSuiteNative.aip"
codesign --force --sign - "$APP_BUNDLE"

# La cartella Documenti può essere gestita da un file provider che riapplica
# FinderInfo al contenitore .app. Il DMG è quindi prodotto da una copia pulita
# temporanea: è il pacchetto di distribuzione verificabile e installabile.
DMG_STAGE=$(mktemp -d "${TMPDIR:-/tmp}/vector-suite-dmg.XXXXXX")
trap 'rm -rf "$DMG_STAGE"' EXIT HUP INT TERM
ditto --norsrc --noextattr --noacl "$APP_BUNDLE" "$DMG_STAGE/Vector Suite.app"
xattr -cr "$DMG_STAGE/Vector Suite.app" 2>/dev/null || true
codesign --verify --deep --strict "$DMG_STAGE/Vector Suite.app"
ln -s /Applications "$DMG_STAGE/Applications"
rm -f "$DMG_PATH"
hdiutil create -quiet -volname "Vector Suite" -srcfolder "$DMG_STAGE" -format UDZO "$DMG_PATH"
hdiutil verify "$DMG_PATH" >/dev/null
rm -rf "$DMG_STAGE"
trap - EXIT HUP INT TERM

echo "App compilata e firmata: $APP_BUNDLE"
echo "Pacchetto di installazione: $DMG_PATH"

echo "Verifica con: $SCRIPT_DIR/verify.sh"
echo "Diagnostica Illustrator: $SCRIPT_DIR/diagnose-illustrator.sh"
