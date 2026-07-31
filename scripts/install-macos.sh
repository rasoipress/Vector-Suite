#!/bin/sh
# Installa il bundle nativo unico di Vector Suite in Adobe Illustrator.
#
#   ./scripts/install-macos.sh
#   ./scripts/install-macos.sh "/Applications/Adobe Illustrator 2026/Plug-ins.localized"
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SOURCE="$PROJECT_ROOT/build/native/release/VectorSuiteNative.aip"
BACKUP_ROOT="$PROJECT_ROOT/backups/native"
STAMP=$(date +%Y%m%d-%H%M%S)

if [ ! -d "$SOURCE" ]; then
  "$SCRIPT_DIR/build-native.sh"
fi
"$SCRIPT_DIR/verify-native.sh"

TARGET_ROOT="${1:-}"
if [ -z "$TARGET_ROOT" ]; then
  for APP_ROOT in /Applications/Adobe\ Illustrator*; do
    [ -d "$APP_ROOT" ] || continue
    if [ -d "$APP_ROOT/Plug-ins.localized" ]; then
      TARGET_ROOT="$APP_ROOT/Plug-ins.localized"
    elif [ -d "$APP_ROOT/Plug-ins" ]; then
      TARGET_ROOT="$APP_ROOT/Plug-ins"
    fi
  done
fi

if [ -z "$TARGET_ROOT" ] || [ ! -d "$TARGET_ROOT" ]; then
  echo "Cartella Plug-ins di Illustrator non trovata." >&2
  exit 1
fi

DESTINATION="$TARGET_ROOT/VectorSuiteNative.aip"
TEMPORARY="$TARGET_ROOT/.VectorSuiteNative.installing"
mkdir -p "$BACKUP_ROOT"

copy_as_user() {
  rm -rf "$TEMPORARY"
  ditto --norsrc --noextattr --noacl "$SOURCE" "$TEMPORARY"
  codesign --verify --deep --strict "$TEMPORARY"
  if [ -e "$DESTINATION" ]; then
    ditto --norsrc --noextattr --noacl "$DESTINATION" "$BACKUP_ROOT/VectorSuiteNative-$STAMP.aip"
  fi
  rm -rf "$DESTINATION"
  mv "$TEMPORARY" "$DESTINATION"
  xattr -cr "$DESTINATION" 2>/dev/null || true
}

copy_as_admin() {
  if [ -e "$DESTINATION" ]; then
    sudo ditto --norsrc --noextattr --noacl "$DESTINATION" "$BACKUP_ROOT/VectorSuiteNative-$STAMP.aip"
  fi
  sudo rm -rf "$TEMPORARY"
  sudo ditto --norsrc --noextattr --noacl "$SOURCE" "$TEMPORARY"
  sudo codesign --verify --deep --strict "$TEMPORARY"
  sudo rm -rf "$DESTINATION"
  sudo mv "$TEMPORARY" "$DESTINATION"
  sudo xattr -cr "$DESTINATION" 2>/dev/null || true
}

if [ -w "$TARGET_ROOT" ]; then
  copy_as_user
else
  echo "La cartella di Illustrator richiede privilegi amministrativi."
  copy_as_admin
fi

codesign --verify --deep --strict "$DESTINATION"
echo
echo "Vector Suite installato in:"
echo "  $DESTINATION"
echo "Riavvia Illustrator e apri Finestra > Vector Suite."
