#!/bin/sh
# Diagnostica non distruttiva di Vector Suite e Adobe Illustrator.
set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_BUNDLE="$PROJECT_ROOT/build/native/release/VectorSuiteNative.aip"
PROBLEMS=0

note()  { printf '  %s\n' "$1"; }
ok()    { printf '  [ok] %s\n' "$1"; }
warn()  { printf '  [!]  %s\n' "$1"; PROBLEMS=$((PROBLEMS + 1)); }
title() { printf '\n== %s\n' "$1"; }

title "Adobe Illustrator"
ILLUSTRATOR_ROOT=""
for CANDIDATE in /Applications/Adobe\ Illustrator*; do
  [ -d "$CANDIDATE" ] || continue
  ILLUSTRATOR_ROOT="$CANDIDATE"
done

if [ -z "$ILLUSTRATOR_ROOT" ]; then
  warn "Nessuna installazione trovata in /Applications."
  PLUGIN_ROOT=""
else
  ok "$ILLUSTRATOR_ROOT"
  HOST_APP="$ILLUSTRATOR_ROOT/Adobe Illustrator.app"
  HOST_VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$HOST_APP/Contents/Info.plist" 2>/dev/null || echo "?")
  HOST_ARCHS=$(lipo -archs "$HOST_APP/Contents/MacOS/Adobe Illustrator" 2>/dev/null || echo "?")
  note "Versione: $HOST_VERSION"
  note "Architetture: $HOST_ARCHS"
  if [ -d "$ILLUSTRATOR_ROOT/Plug-ins.localized" ]; then
    PLUGIN_ROOT="$ILLUSTRATOR_ROOT/Plug-ins.localized"
  else
    PLUGIN_ROOT="$ILLUSTRATOR_ROOT/Plug-ins"
  fi
  [ -d "$PLUGIN_ROOT" ] && ok "$PLUGIN_ROOT" || warn "Cartella Plug-ins non trovata."
fi

check_bundle() {
  LABEL="$1"
  BUNDLE="$2"
  title "$LABEL"
  if [ ! -d "$BUNDLE" ]; then
    warn "Bundle assente: $BUNDLE"
    return
  fi

  EXECUTABLE="$BUNDLE/Contents/MacOS/VectorSuiteNative"
  [ -x "$EXECUTABLE" ] || { warn "Eseguibile mancante."; return; }
  IDENTIFIER=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$BUNDLE/Contents/Info.plist" 2>/dev/null || echo "?")
  VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$BUNDLE/Contents/Info.plist" 2>/dev/null || echo "?")
  ARCHS=$(lipo -archs "$EXECUTABLE" 2>/dev/null || echo "?")
  ICONS=$(find "$BUNDLE/Contents/Resources/svg" -maxdepth 1 -type f -name 'VSIcon-*.svg' 2>/dev/null | wc -l | tr -d ' ')
  [ "$IDENTIFIER" = "studio.vectorsuite.plugin.core" ] && ok "Identifier: $IDENTIFIER" || warn "Identifier inatteso: $IDENTIFIER"
  case "$ARCHS" in *arm64*) ok "arm64 presente" ;; *) warn "arm64 mancante: $ARCHS" ;; esac
  case "$ARCHS" in *x86_64*) ok "x86_64 presente" ;; *) warn "x86_64 mancante: $ARCHS" ;; esac
  [ "$ICONS" -eq 25 ] && ok "25 icone native" || warn "Icone native: $ICONS/25"
  codesign --verify --deep --strict "$BUNDLE" >/dev/null 2>&1 && ok "Firma valida" || warn "Firma non valida"
  if xattr -p com.apple.quarantine "$BUNDLE" >/dev/null 2>&1; then
    warn "Attributo di quarantena presente"
  else
    ok "Nessuna quarantena"
  fi
  note "Versione: $VERSION"
}

check_bundle "Build locale" "$BUILD_BUNDLE"
if [ -n "${PLUGIN_ROOT:-}" ]; then
  check_bundle "Installazione Illustrator" "$PLUGIN_ROOT/VectorSuiteNative.aip"
fi

title "Sorgenti"
MODULES=$(grep -c 'VSModuleDescriptor slot:@"module-' "$PROJECT_ROOT/Sources/main.m" 2>/dev/null || echo 0)
TOOLS=$(grep -c 'kVSTool.*, kVS.*"Vector Suite' "$PROJECT_ROOT/native/VectorSuiteNative/Source/VectorSuiteCatalog.cpp" 2>/dev/null || echo 0)
[ "$MODULES" -eq 22 ] && ok "22 moduli catalogati" || warn "Catalogo standalone: $MODULES/22"
[ "$TOOLS" -eq 25 ] && ok "25 strumenti registrati" || warn "Strumenti nativi registrati: $TOOLS/25"

printf '\n== Esito\n'
if [ "$PROBLEMS" -eq 0 ]; then
  printf '  Nessun problema rilevato.\n'
  exit 0
fi
printf '  Problemi rilevati: %d\n' "$PROBLEMS"
exit 1
