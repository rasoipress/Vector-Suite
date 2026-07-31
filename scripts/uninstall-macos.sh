#!/bin/sh
# Disinstalla Vector Suite in modo recuperabile, spostando il bundle nei backup.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BACKUP_ROOT="$PROJECT_ROOT/backups/uninstalled"
STAMP=$(date +%Y%m%d-%H%M%S)
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

TARGET="$TARGET_ROOT/VectorSuiteNative.aip"
if [ ! -e "$TARGET" ]; then
  echo "Vector Suite non è installato in $TARGET_ROOT"
  exit 0
fi

mkdir -p "$BACKUP_ROOT"
BACKUP="$BACKUP_ROOT/VectorSuiteNative-$STAMP.aip"
if [ -w "$TARGET_ROOT" ]; then
  mv "$TARGET" "$BACKUP"
else
  sudo mv "$TARGET" "$BACKUP"
  sudo chown -R "$(id -u):$(id -g)" "$BACKUP"
fi

echo "Bundle rimosso da Illustrator e conservato in:"
echo "  $BACKUP"
