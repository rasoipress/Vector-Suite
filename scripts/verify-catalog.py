#!/usr/bin/env python3
"""Controllo di coerenza fra le quattro dichiarazioni del catalogo.

Lo stesso elenco di moduli esiste in quattro posti diversi, ciascuno in un
linguaggio diverso. Se uno scivola, il pannello mostra un'icona sbagliata o
Illustrator registra uno strumento senza risorsa. Questo script confronta:

    native/…/Source/VectorSuiteCatalog.cpp   moduli e strumenti nativi
    native/…/Resources/raw/IDToFile.txt      mappa risorsa → file
    native/…/Resources/raw/VSIcon-*.svg      icone del plug-in
    Resources/ui/app.js                      libreria dell'app di gestione
    Sources/main.m                           slot analizzati dall'app

    ./scripts/verify-catalog.py
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
NATIVE = ROOT / "native/VectorSuiteNative"

EXPECTED_MODULES = 22
EXPECTED_TOOLS = 25

problems = []


def fail(message):
    problems.append(message)


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


# --- Catalogo nativo ------------------------------------------------------

catalog = read("native/VectorSuiteNative/Source/VectorSuiteCatalog.cpp")

module_entries = re.findall(
    r"\{kVS\w+, \"([a-z-]+)\", \"[^\"]+\", \"[^\"]+\",\s*\n?\s*\"[^\"]*\", (\d+)\}",
    catalog)
tool_entries = re.findall(
    r"\{kVSTool\w+, kVS\w+, \"Vector Suite [^\"]+\", \"[^\"]+\", (\d+)\}", catalog)

module_keys = [key for key, _ in module_entries]
module_icon_ids = {key: identifier for key, identifier in module_entries}
tool_icon_ids = set(tool_entries)

if len(module_keys) != EXPECTED_MODULES:
    fail("VectorSuiteCatalog.cpp dichiara %d moduli invece di %d"
         % (len(module_keys), EXPECTED_MODULES))
if len(tool_entries) != EXPECTED_TOOLS:
    fail("VectorSuiteCatalog.cpp dichiara %d strumenti invece di %d"
         % (len(tool_entries), EXPECTED_TOOLS))

# --- Icone del plug-in ----------------------------------------------------

icon_files = sorted(path.name for path in (NATIVE / "Resources/raw").glob("VSIcon-*.svg"))
icon_keys = {name[len("VSIcon-"):-len(".svg")] for name in icon_files}

if len(icon_files) != EXPECTED_TOOLS:
    fail("Trovate %d icone invece di %d" % (len(icon_files), EXPECTED_TOOLS))

for key in module_keys:
    if key not in icon_keys:
        fail("Manca l'icona del modulo %s" % key)

# --- IDToFile -------------------------------------------------------------

mapping = {}
for line in read("native/VectorSuiteNative/Resources/raw/IDToFile.txt").splitlines():
    parts = line.split()
    if len(parts) != 2:
        continue
    resource, filename = parts
    mapping[resource.replace(".svg", "")] = filename

if len(mapping) != EXPECTED_TOOLS:
    fail("IDToFile.txt mappa %d risorse invece di %d" % (len(mapping), EXPECTED_TOOLS))

for identifier, filename in mapping.items():
    if filename not in icon_files:
        fail("IDToFile.txt punta a %s, che non esiste" % filename)

for identifier in tool_icon_ids:
    if identifier not in mapping:
        fail("Lo strumento con risorsa %s non è mappato in IDToFile.txt" % identifier)

for key, identifier in module_icon_ids.items():
    expected = "VSIcon-%s.svg" % key
    if mapping.get(identifier) != expected:
        fail("Il modulo %s dichiara la risorsa %s, mappata su %s invece che su %s"
             % (key, identifier, mapping.get(identifier), expected))

# --- App di gestione ------------------------------------------------------

app_keys = re.findall(r'module\("([a-z-]+)"', read("Resources/ui/app.js"))
if app_keys != module_keys:
    fail("app.js e VectorSuiteCatalog.cpp non elencano gli stessi moduli "
         "nello stesso ordine")

slots = re.findall(r'VSModuleDescriptor slot:@"(module-\d\d)"', read("Sources/main.m"))
if len(slots) != EXPECTED_MODULES:
    fail("main.m dichiara %d slot invece di %d" % (len(slots), EXPECTED_MODULES))
if slots != sorted(slots):
    fail("main.m elenca gli slot fuori ordine")

app_slots = re.findall(r'"(module-\d\d)"', read("Resources/ui/app.js"))
if app_slots != slots:
    fail("Gli slot di app.js non corrispondono a quelli di main.m")

# --- Esito ----------------------------------------------------------------

if problems:
    for message in problems:
        print("  [!] %s" % message)
    print("\nCatalogo incoerente: %d problemi." % len(problems))
    sys.exit(1)

print("Catalogo coerente: %d moduli, %d strumenti, %d icone, mappa risorse allineata."
      % (len(module_keys), len(tool_entries), len(icon_files)))
