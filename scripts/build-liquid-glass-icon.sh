#!/bin/sh
# Compila l'icona Liquid Glass di macOS 26 e la incorpora nell'app.
#
#   ./scripts/build-liquid-glass-icon.sh ["percorso/Vector Suite.app"]
#
# Sorgente: Resources/VectorSuite.icon, il pacchetto prodotto da Icon Composer.
# Se non c'è, lo script spiega come crearlo e termina senza errore: l'app resta
# valida con la sola icona .icns, semplicemente su macOS 26 non riceve il
# trattamento in vetro.
#
# Il passaggio deve avvenire PRIMA della firma: Assets.car finisce dentro il
# bundle e la firma deve coprirlo.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ICON_SOURCE="$PROJECT_DIR/Resources/VectorSuite.icon"
APP_BUNDLE="${1:-$PROJECT_DIR/build/Vector Suite.app}"
RESOURCES_DIR="$APP_BUNDLE/Contents/Resources"
INFO_PLIST="$APP_BUNDLE/Contents/Info.plist"
STAGE="$PROJECT_DIR/build/icon/compiled"
PARTIAL_PLIST="$PROJECT_DIR/build/icon/icon-info.plist"
ICON_NAME=$(basename "$ICON_SOURCE" .icon)

if [ ! -d "$ICON_SOURCE" ]; then
  cat <<INSTRUCTIONS
== Icona Liquid Glass
  Saltata: manca $ICON_SOURCE

  Per crearla, una volta sola:
    1. apri Icon Composer (Xcode 26 > Open Developer Tool > Icon Composer);
    2. crea una nuova icona e imposta il fondo su Solid, nero;
    3. trascina sul canvas Resources/icon/VectorSuiteMark.png
       (è bianco su trasparente: sembra vuoto in anteprima, è corretto);
    4. per il livello, imposta Fill bianco in Default e in Dark, e bianco
       pieno in Mono;
    5. salva — non esportare — come Resources/VectorSuite.icon.

  Poi rilancia questo script o direttamente scripts/build-macos.sh.
INSTRUCTIONS
  exit 0
fi

if [ ! -d "$APP_BUNDLE" ]; then
  echo "== Icona Liquid Glass"
  echo "  Saltata: non trovo $APP_BUNDLE. Compila prima l'app."
  exit 0
fi

if ! xcrun --find actool >/dev/null 2>&1; then
  echo "== Icona Liquid Glass"
  echo "  Saltata: actool non disponibile. Serve Xcode 26 o successivo."
  exit 0
fi

echo "== Icona Liquid Glass"
rm -rf "$STAGE"
mkdir -p "$STAGE"

# actool rifiuta il formato .icon con SDK precedenti al 26: in quel caso non è
# un errore del progetto, è una macchina che non può ancora produrre l'asset.
if ! xcrun actool "$ICON_SOURCE" \
  --compile "$STAGE" \
  --output-format human-readable-text \
  --notices --warnings --errors \
  --output-partial-info-plist "$PARTIAL_PLIST" \
  --app-icon "$ICON_NAME" \
  --include-all-app-icons \
  --enable-on-demand-resources NO \
  --development-region it \
  --target-device mac \
  --minimum-deployment-target 26.0 \
  --platform macosx; then
  echo "  [!] actool non è riuscito a compilare $ICON_NAME.icon."
  echo "      Serve Xcode 26 con l'SDK di macOS 26. L'app resta valida con"
  echo "      la sola icona .icns."
  exit 0
fi

if [ ! -f "$STAGE/Assets.car" ]; then
  echo "  [!] actool non ha prodotto Assets.car. L'app resta valida con la"
  echo "      sola icona .icns."
  exit 0
fi

cp "$STAGE/Assets.car" "$RESOURCES_DIR/Assets.car"

# Il nome da mettere in CFBundleIconName lo dichiara actool stesso nel plist
# parziale. Il nome del file è solo un ripiego se quel plist non lo contiene.
RESOLVED_NAME=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIconName" "$PARTIAL_PLIST" 2>/dev/null || echo "")
[ -n "$RESOLVED_NAME" ] || RESOLVED_NAME="$ICON_NAME"

/usr/libexec/PlistBuddy -c "Delete :CFBundleIconName" "$INFO_PLIST" >/dev/null 2>&1 || true
/usr/libexec/PlistBuddy -c "Add :CFBundleIconName string $RESOLVED_NAME" "$INFO_PLIST"
plutil -lint "$INFO_PLIST" >/dev/null

echo "  Assets.car incorporato, CFBundleIconName = $RESOLVED_NAME"
echo "  Il file .icns resta nel bundle per macOS 25 e precedenti."
