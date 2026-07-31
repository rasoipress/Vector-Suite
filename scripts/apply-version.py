#!/usr/bin/env python3
"""Porta il numero di versione dal file VERSION dentro i due bundle.

Il numero di versione viveva in tre posti che non si parlavano: il tag git, il
plist dell'app di gestione e il plist del plug-in. Bastava dimenticarne uno per
pubblicare una release che dichiara una versione e installa un bundle che ne
dichiara un'altra — e la scheda «Versione» dell'app di gestione, che legge il
bundle dal disco, avrebbe mostrato il valore vecchio.

Ora la sorgente è una sola: il file VERSION alla radice. Questo script la
riporta dove serve, e gli script di build lo lanciano prima di compilare.

    ./scripts/apply-version.py            scrive la versione nei plist
    ./scripts/apply-version.py --check    verifica che siano allineati
    ./scripts/apply-version.py --print    stampa la versione e basta

Il numero di build (CFBundleVersion) non si scrive a mano: è ricavato dalla
versione con major·10000 + minor·100 + patch. Cresce sempre, che è l'unica cosa
che macOS pretende, e non può divergere dalla versione visibile.
"""

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
VERSION_FILE = ROOT / "VERSION"

PLISTS = [
    ("Resources/Info.plist", "app di gestione"),
    ("native/VectorSuiteNative/Config/Info.plist", "plug-in per Illustrator"),
]


def read_version():
    if not VERSION_FILE.exists():
        sys.exit("Manca il file VERSION alla radice del progetto.")
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        sys.exit("VERSION contiene «%s»: serve la forma maggiore.minore.patch, "
                 "per esempio 0.6.0." % version)
    return version


def build_number(version):
    major, minor, patch = (int(part) for part in version.split("."))
    return major * 10000 + minor * 100 + patch


def replace_key(text, key, value):
    """Sostituisce il valore di una chiave del plist lasciando intatto il resto.

    Si lavora sul testo invece che con plistlib apposta: riscrivere il plist da
    un dizionario ne cambierebbe ordine e formattazione, e ogni build
    produrrebbe una differenza inutile.
    """
    pattern = re.compile(
        r"(<key>%s</key>\s*<string>)([^<]*)(</string>)" % re.escape(key))
    updated, count = pattern.subn(lambda match: match.group(1) + value + match.group(3),
                                  text, count=1)
    if count == 0:
        raise SystemExit("Chiave %s non trovata nel plist." % key)
    return updated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verifica senza scrivere")
    parser.add_argument("--print", dest="show", action="store_true",
                        help="stampa la versione e termina")
    parser.add_argument("--expect", metavar="VERSIONE",
                        help="fallisce se VERSION non coincide, "
                             "anche con la v iniziale del tag git")
    arguments = parser.parse_args()

    version = read_version()
    if arguments.show:
        print(version)
        return 0

    if arguments.expect:
        expected = arguments.expect.lstrip("vV")
        if expected != version:
            print("Il tag dichiara %s ma il file VERSION dice %s."
                  % (expected, version))
            print("Aggiorna VERSION, rilancia ./scripts/apply-version.py e "
                  "ritagga: il pacchetto pubblicato deve dichiarare la stessa "
                  "versione che installa.")
            return 1
        print("Tag e VERSION coincidono: %s" % version)
        return 0

    build = str(build_number(version))
    problems = 0

    for relative, description in PLISTS:
        path = ROOT / relative
        original = path.read_text(encoding="utf-8")
        updated = replace_key(original, "CFBundleShortVersionString", version)
        updated = replace_key(updated, "CFBundleVersion", build)

        if arguments.check:
            if updated != original:
                print("Non allineato: %s (%s)" % (relative, description))
                problems += 1
        elif updated != original:
            path.write_text(updated, encoding="utf-8")
            print("  %s → %s (build %s)" % (relative, version, build))

    if arguments.check:
        if problems:
            print("\nEsegui ./scripts/apply-version.py per allineare i plist.")
            return 1
        print("Versione allineata ovunque: %s (build %s)" % (version, build))
        return 0

    print("Versione %s (build %s) applicata." % (version, build))
    return 0


if __name__ == "__main__":
    sys.exit(main())
