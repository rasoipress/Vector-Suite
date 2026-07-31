#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
PROJECT_FILE="$PROJECT_ROOT/native/VectorSuiteNative/VectorSuiteNative.xcodeproj"
DERIVED_DATA="${TMPDIR:-/tmp}/vector-suite-native-derived-data"
BUNDLE="$PROJECT_ROOT/build/native/release/VectorSuiteNative.aip"
PROJECT_ROOT_ESCAPED=$(printf '%s' "$PROJECT_ROOT" | sed 's/ /\\ /g')
SDK_DIR="$PROJECT_ROOT/sdk/Adobe Illustrator 2026 SDK"

# L'SDK non è nel repository perché la licenza Adobe non ne permette la
# ridistribuzione: va scaricato una volta e messo al suo posto.
if [ ! -d "$SDK_DIR/illustratorapi" ]; then
  cat >&2 <<'MISSING'
Errore: manca l'Adobe Illustrator SDK.

  Atteso in:  sdk/Adobe Illustrator 2026 SDK/

  L'SDK non è incluso nel progetto: è coperto dalla licenza Adobe e non si può
  ridistribuire. Va scaricato da Adobe, gratuitamente, con un Adobe ID:

    https://developer.adobe.com/console/servicesandapis/ai
    https://developer.adobe.com/console/downloads

  Scompatta l'archivio dentro sdk/ mantenendo il nome della cartella.
  Le istruzioni complete sono in sdk/README.md.
MISSING
  exit 1
fi

python3 "$SCRIPT_DIR/generate-assets.py"

xcodebuild \
  -quiet \
  -project "$PROJECT_FILE" \
  -scheme VectorSuiteNative \
  -configuration release \
  -destination "generic/platform=macOS" \
  -derivedDataPath "$DERIVED_DATA" \
  CODE_SIGNING_ALLOWED=NO \
  COMPILER_INDEX_STORE_ENABLE=NO \
  "OTHER_CFLAGS=-ffile-prefix-map=$PROJECT_ROOT_ESCAPED=/VectorSuite" \
  "OTHER_CPLUSPLUSFLAGS=-ffile-prefix-map=$PROJECT_ROOT_ESCAPED=/VectorSuite" \
  clean build

xattr -cr "$BUNDLE" 2>/dev/null || true
codesign --force --sign - "$BUNDLE"
codesign --verify --deep --verbose=2 "$BUNDLE"
rm -rf "$PROJECT_ROOT/build/native/release/VectorSuiteNative.aip.dSYM"

echo
echo "Build nativa pronta:"
echo "  $BUNDLE"
echo "  architetture: $(lipo -archs "$BUNDLE/Contents/MacOS/VectorSuiteNative")"
