#!/usr/bin/env python3
"""Sorgente unica della grafica Vector Suite.

Questo file contiene la geometria delle venticinque icone dei moduli e legge il
marchio dal disegno originale. Da qui vengono generati tutti i formati usati dal
progetto, così l'app standalone e il plug-in per Illustrator non possono
divergere.

    ./scripts/generate-assets.py            scrive gli asset
    ./scripts/generate-assets.py --check    verifica che siano allineati

Sorgente del marchio, disegnata a mano e MAI riscritta da questo script:

    Resources/VectorSuiteLogo.svg

Output generati:

    Resources/ui/icons.js                                icone e marchio dell'app
    native/VectorSuiteNative/Resources/raw/VSIcon-*.svg  icone del plug-in
    native/VectorSuiteNative/Resources/Win/*.png         le stesse, per Windows
    native/VectorSuiteNative/Resources/Win/icons.rc      tabella risorse Windows
    build/icon/VectorSuiteIcon.svg                       master per il file .icns

I PNG per Windows servono perché GDI+ non legge SVG: sono bianchi su fondo
trasparente e il pannello li ricolora al volo. Vengono riscritti solo se sulla
macchina c'è un rasterizzatore; altrimenti restano quelli già presenti.

Convenzioni geometriche
-----------------------
Ogni icona è costruita su una griglia 24×24 con centro esatto (12,12) e area
utile 2→22. Tratto 1.5, terminazioni e giunzioni tonde. Tangenze, simmetrie e
spaziature sono calcolate, mai approssimate a occhio.

Il canvas del plug-in è 44×36, la misura richiesta da Illustrator per le icone
degli strumenti. La griglia 24 viene portata a 32×32 e centrata nel canvas
(`translate(6 2) scale(4/3)`), così le icone Vector Suite hanno lo stesso peso
ottico degli strumenti nativi di Illustrator invece di apparire più piccole.
"""

import argparse
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

GRID = 24
PLUGIN_CANVAS = (44, 36)
PLUGIN_SCALE = 4.0 / 3.0
PLUGIN_OFFSET = (6, 2)
STROKE = 1.5

# ---------------------------------------------------------------------------
# Marchio
# ---------------------------------------------------------------------------
#
# Il marchio NON è definito qui. Vive in Resources/VectorSuiteLogo.svg, che è un
# disegno vero, modificabile in Illustrator, e questo script non lo riscrive
# mai: lo legge soltanto. Da quel file ricava due cose,
#
#   il marchio per l'interfaccia dell'app (window.VSMark in icons.js);
#   il master 1024×1024 da cui il build ricava il file .icns.
#
# L'SVG esportato da Illustrator porta i colori dentro un foglio di stile con
# classi `.stN`. Per l'interfaccia web quelle classi vengono risolte in
# attributi espliciti e i due valori vengono rimappati sulle variabili del tema:
# il bianco diventa il colore del glifo, il nero il colore della pastiglia. Così
# il marchio si inverte da solo in tema scuro senza essere ridisegnato.

LOGO_FILE = "Resources/VectorSuiteLogo.svg"

# Metriche dell'icona applicativa macOS: quadrato utile 824×824 dentro un
# canvas 1024×1024. Sono le proporzioni della maschera di sistema: rispettarle è
# ciò che fa sedere l'icona alla stessa altezza ottica delle altre nel Dock. Il
# disegno del marchio arriva a piena tela, quindi qui viene rimpicciolito e
# centrato invece di essere ritoccato.
ICON_CANVAS = 1024
ICON_CONTENT = 824
ICON_INSET = (ICON_CANVAS - ICON_CONTENT) / 2
ICON_FILE = "build/icon/VectorSuiteIcon.svg"

WEB_COLOURS = {
    "#fff": "currentColor", "#ffffff": "currentColor", "white": "currentColor",
    "#000": "var(--inverse)", "#000000": "var(--inverse)", "black": "var(--inverse)",
}


def read_logo():
    """Legge il disegno del marchio e ne restituisce lato, stili e contenuto."""
    text = (ROOT / LOGO_FILE).read_text(encoding="utf-8")

    view = re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', text)
    if not view:
        raise SystemExit("%s non dichiara un viewBox leggibile." % LOGO_FILE)
    box = float(view.group(1))

    styles = {}
    for block in re.findall(r"<style>(.*?)</style>", text, re.S):
        for selector, body in re.findall(r"([^{}]+)\{([^{}]*)\}", block):
            declarations = {}
            for declaration in body.split(";"):
                if ":" not in declaration:
                    continue
                name, value = declaration.split(":", 1)
                declarations[name.strip()] = value.strip().removesuffix("px")
            for name in selector.split(","):
                name = name.strip()
                if name.startswith("."):
                    styles.setdefault(name[1:], {}).update(declarations)

    markup = re.sub(r"<defs>.*?</defs>", "", text, flags=re.S)
    markup = re.sub(r"<!--.*?-->", "", markup, flags=re.S)
    markup = re.search(r"<svg[^>]*>(.*)</svg>", markup, re.S).group(1)
    return box, styles, markup.strip()


def inline_styles(markup, styles, colours):
    """Sostituisce `class="stN"` con gli attributi di presentazione risolti."""
    def replace(match):
        declarations = styles.get(match.group(1), {})
        return " ".join('%s="%s"' % (name, colours.get(value.lower(), value))
                        for name, value in declarations.items())
    return re.sub(r'class="([\w-]+)"', replace, markup)


def strip_plate(markup, box):
    """Toglie il riquadro di fondo: nell'interfaccia lo disegna la pastiglia."""
    pattern = r'<rect[^>]*width="%s"[^>]*/>\s*' % re.escape(("%g" % box))
    return re.sub(pattern, "", markup, count=1)


def mark_fragment():
    """Marchio pronto per l'interfaccia dell'app, senza riquadro di fondo."""
    box, styles, markup = read_logo()
    fragment = inline_styles(strip_plate(markup, box), styles, WEB_COLOURS)
    return box, re.sub(r"\s+", " ", fragment).strip()


def app_icon_svg():
    """Master dell'icona macOS: il disegno rimpicciolito nel riquadro utile."""
    box, styles, markup = read_logo()
    scale = ICON_CONTENT / box
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" width="{c}" height="{c}" '
        'viewBox="0 0 {c} {c}">\n'
        '  <!-- Generato da scripts/generate-assets.py a partire da {source}.\n'
        '       Il disegno originale non viene modificato: qui viene solo\n'
        '       centrato nel quadrato utile 824×824 previsto da macOS. -->\n'
        '  <g transform="translate({i} {i}) scale({k:.9f})">\n'
        '    {mark}\n'
        '  </g>\n'
        '</svg>\n'
    ).format(c=ICON_CANVAS, i=int(ICON_INSET), k=scale, source=LOGO_FILE,
             mark=inline_styles(markup, styles, {}))

# ---------------------------------------------------------------------------
# Icone dei moduli — una per ogni chiave del catalogo nativo
# ---------------------------------------------------------------------------
#
# `class="fill-on"` marca gli elementi che devono risultare pieni invece che
# tracciati. Nel plug-in viene tradotto in attributi espliciti, nell'app è
# gestito dal foglio di stile.

MODULE_ICONS = {
    # Corpo a parallelogramma: i due lati lunghi sono esattamente paralleli e
    # la base del pennino è perpendicolare all'asse del corpo.
    "precision-pen":
        '<path d="M5 19 7 13 17 3l4 4-10 10-6 2Z"/>'
        '<path d="M14 6 18 10M7 13 11 17"/>',

    # Onda simmetrica attorno a y=14: la seconda campata è il riflesso esatto
    # della prima. Il punto di penna chiude il tratto sull'estremo destro.
    "fluid-sketch":
        '<path d="M3 14c3-7 6-7 9 0s6 7 9 0"/>'
        '<circle class="fill-on" cx="21" cy="14" r="1.5"/>',

    # Rombo regolare: diagonali 12 e 18, simmetrico su entrambi gli assi.
    # Fenditura e foro giacciono sull'asse verticale, il foro nel centro esatto.
    "ink-studio":
        '<path d="M12 3 18 12 12 21 6 12Z"/>'
        '<path d="M12 6.5V10"/>'
        '<circle cx="12" cy="12" r="1.6"/>',

    # Tratto a spessore variabile: trapezio simmetrico sull'asse y=12, da 7
    # unità di spessore a sinistra a 2.5 a destra, con il punto di larghezza
    # esattamente a metà tracciato.
    "width-studio":
        '<path d="M3 8.5 21 10.75V13.25L3 15.5Z"/>'
        '<path d="M12 9.625V14.375"/>'
        '<circle class="fill-on" cx="12" cy="12" r="1.4"/>',

    # Curva cubica simmetrica: per t=0.5 il vertice cade in (12, 9.5), dove
    # passano la maniglia orizzontale e il punto di controllo. Le ancore sono
    # quadrati centrati sui due estremi.
    "path-studio":
        '<path d="M4 17C4 7 20 7 20 17"/>'
        '<path d="M8 9.5h8"/>'
        '<rect x="2.5" y="15.5" width="3" height="3"/>'
        '<rect x="18.5" y="15.5" width="3" height="3"/>'
        '<circle class="fill-on" cx="12" cy="9.5" r="1.4"/>',

    # Tangente nel punto a 45°: (10+6/√2, 14−6/√2). La retta è perpendicolare
    # al raggio e simmetrica di ±9 attorno al punto di contatto, marcato.
    "geometry-lab":
        '<circle cx="10" cy="14" r="6"/>'
        '<path d="M7.879 3.393 20.607 16.121"/>'
        '<circle class="fill-on" cx="14.243" cy="9.757" r="1.3"/>',

    # Due cunei speculari che convergono sull'asse di collisione.
    "collision-align":
        '<path d="M3 12 9 6v12Z"/>'
        '<path d="M21 12 15 6v12Z"/>'
        '<path d="M12 4v16"/>',

    # Asse tratteggiato: 4 trattini da 3 e 3 spazi da 2 fanno 18 esatti, così
    # il tratteggio comincia e finisce con un pieno.
    "mirror-studio":
        '<path d="M12 3v18" stroke-dasharray="3 2" stroke-linecap="butt"/>'
        '<path d="M8 6 3 12l5 6"/>'
        '<path d="M16 6l5 6-5 6"/>',

    # Profilo di partenza tratteggiato (4 trattini da 2.5, 3 spazi da 2) e profilo
    # rimodellato: per t=0.5 il vertice cade esattamente in (12, 7.75).
    "shape-reform":
        '<path d="M4 16h16" stroke-dasharray="2.5 2" stroke-linecap="butt"/>'
        '<path d="M4 16c4-11 12-11 16 0"/>'
        '<circle class="fill-on" cx="12" cy="7.75" r="1.4"/>',

    # Pila di aspetti: tre contorni sfalsati di 2.5 unità esatte, dal più
    # arretrato al riquadro in primo piano. L'idioma dello stack di effetti,
    # senza riusare i cursori che appartengono a Direct Settings.
    "live-style":
        '<rect x="8" y="8" width="12" height="12" rx="2"/>'
        '<path d="M5.5 17.5V7.5a2 2 0 0 1 2-2h10"/>'
        '<path d="M3 15V5a2 2 0 0 1 2-2h10"/>',

    # Semicerchio pieno di raggio 9, identico al cerchio che lo contiene.
    "color-lab":
        '<circle cx="12" cy="12" r="9"/>'
        '<path class="fill-on" d="M12 3a9 9 0 0 1 0 18Z"/>',

    # Diagonali su x+y = 14, 21, 28, 35: passo costante di 7 unità.
    "texture-lab":
        '<rect x="4" y="4" width="16" height="16"/>'
        '<path d="M4 10 10 4M4 17 17 4M8 20 20 8M15 20 20 15"/>',

    # Matrice 3×3 a passo 6.5: il raggio dipende solo dalla colonna, così la
    # densità cresce da sinistra a destra in modo regolare.
    "stipple-lab":
        '<g class="fill-on">'
        '<circle cx="5.5" cy="5.5" r="1.1"/><circle cx="12" cy="5.5" r="1.8"/>'
        '<circle cx="18.5" cy="5.5" r="2.5"/>'
        '<circle cx="5.5" cy="12" r="1.1"/><circle cx="12" cy="12" r="1.8"/>'
        '<circle cx="18.5" cy="12" r="2.5"/>'
        '<circle cx="5.5" cy="18.5" r="1.1"/><circle cx="12" cy="18.5" r="1.8"/>'
        '<circle cx="18.5" cy="18.5" r="2.5"/>'
        '</g>',

    # Cinque punti sulle diagonali di un quadrato, come una faccia di dado.
    "randomize":
        '<rect x="4" y="4" width="16" height="16" rx="3.5"/>'
        '<g class="fill-on">'
        '<circle cx="8.5" cy="8.5" r="1.3"/><circle cx="15.5" cy="8.5" r="1.3"/>'
        '<circle cx="12" cy="12" r="1.3"/>'
        '<circle cx="8.5" cy="15.5" r="1.3"/><circle cx="15.5" cy="15.5" r="1.3"/>'
        '</g>',

    # Impugnatura tangente al cerchio nel punto a 45°: (10.5+6.5/√2, idem).
    "smart-find":
        '<circle cx="10.5" cy="10.5" r="6.5"/>'
        '<path d="M15.096 15.096 20 20"/>',

    # Tracciato aperto: tre lati di un rombo regolare, il quarto mancante. Le
    # due ancore libere sono i quadrati sugli estremi da ricongiungere.
    "vector-repair":
        '<path d="M12 3.5 20.5 12 12 20.5 3.5 12"/>'
        '<rect x="10.5" y="2" width="3" height="3"/>'
        '<rect x="2" y="10.5" width="3" height="3"/>',

    # Il profilo parte e termina esattamente sui bordi verticali della cornice.
    "raster-lab":
        '<rect x="3" y="4" width="18" height="16" rx="2.5"/>'
        '<circle cx="7.5" cy="8.5" r="1.8"/>'
        '<path d="M3 18 8.5 12.5l3 3 3.5-4L21 18"/>',

    # Arco di 270° chiuso da una punta piena tangente al percorso, e lancette
    # sul centro esatto del quadrante: la corta segna le 4 in punto.
    "auto-save":
        '<path d="M12 4.5A7.5 7.5 0 1 0 19.5 12"/>'
        '<path class="fill-on" d="M19.5 8.5 22 12.5H17Z"/>'
        '<path d="M12 8V12l3.118 1.8"/>',

    # Cursori verticali: ogni traccia copre 4→20 e si interrompe sulla manopola.
    "direct-settings":
        '<path d="M7 4v3M7 11v9M12 4v9M12 17v3M17 4v6M17 14v6"/>'
        '<circle cx="7" cy="9" r="2"/>'
        '<circle cx="12" cy="15" r="2"/>'
        '<circle cx="17" cy="12" r="2"/>',

    # Piedini simmetrici 2→6 e 18→22 su entrambi gli assi, nucleo concentrico.
    "suite-core":
        '<rect x="6" y="6" width="12" height="12" rx="2"/>'
        '<rect x="9.5" y="9.5" width="5" height="5" rx="1"/>'
        '<path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M18 9h4M2 15h4M18 15h4"/>',

    # Terna assonometrica: tre bracci lunghi 9 a 30°, 150° e 270°. Il centro è
    # posto a y=9.75 perché il rombo circoscritto risulti centrato sulla griglia.
    "projection-studio":
        '<path d="M12 9.75 19.794 5.25M12 9.75 4.206 5.25M12 9.75V18.75"/>'
        '<circle class="fill-on" cx="12" cy="9.75" r="1.4"/>',

    # Rombo con lati a 30°: rapporto semiassi 5.5/9.526 = tan 30°.
    "projection-rectangle":
        '<path d="M12 6.5 21.526 12 12 17.5 2.474 12Z"/>',

    # Ellisse iscritta nello stesso rombo, ruotata di 30° esatti.
    "projection-ellipse":
        '<ellipse cx="12" cy="12" rx="9.526" ry="5.5" transform="rotate(-30 12 12)"/>',

    # Cubo isometrico: spigoli superiori a 30°, montanti verticali di 8 unità.
    "projection-box":
        '<path d="M12 3.5 19.79 8v8L12 20.5 4.21 16V8Z"/>'
        '<path d="M4.21 8 12 12.5l7.79-4.5M12 12.5v8"/>',

    # Ramificazione a due livelli: ogni biforcazione è a 45° e la lunghezza si
    # riduce a ogni passaggio. Gli attacchi sono sfalsati sul tronco, come li
    # produce il generatore: una ramificazione perfettamente simmetrica non
    # sarebbe un albero, sarebbe un'antenna.
    "fractal-grove":
        '<path d="M12 22V12"/>'
        '<path d="M12 15 7 10M12 12l5-5"/>'
        '<path d="M7 10 4.5 7.5M7 10l1.5-4M17 7l-.5-4M17 7l3.5-2.5"/>'
        '<path d="M8.5 22h7"/>',
}

# Icone della sola interfaccia dell'app. Le categorie riusano il glifo del
# modulo che le rappresenta: la relazione fra categoria e modulo resta visibile.
UI_ICONS = {
    "all":
        '<rect x="3.5" y="3.5" width="7" height="7" rx="1.2"/>'
        '<rect x="13.5" y="3.5" width="7" height="7" rx="1.2"/>'
        '<rect x="3.5" y="13.5" width="7" height="7" rx="1.2"/>'
        '<rect x="13.5" y="13.5" width="7" height="7" rx="1.2"/>',
    "draw": MODULE_ICONS["precision-pen"],
    "geometry": MODULE_ICONS["path-studio"],
    "appearance": MODULE_ICONS["color-lab"],
    "workflow": MODULE_ICONS["smart-find"],
    "system": MODULE_ICONS["suite-core"],
    "search": MODULE_ICONS["smart-find"],
    "rescan":
        '<path d="M12 4.5A7.5 7.5 0 1 0 19.5 12"/>'
        '<path class="fill-on" d="M19.5 8.5 22 12.5H17Z"/>',
    # Spezzata di due segmenti entrambi a 45°, centrata sulla griglia.
    "check": '<path d="M4.5 12.5 9.5 17.5 19.5 7.5"/>',
    "empty": '<rect x="3.5" y="3.5" width="17" height="17" rx="3" stroke-dasharray="3 2.5" stroke-linecap="butt"/>',
}

# Ordine del catalogo: deve coincidere con VectorSuiteCatalog.cpp.
MODULE_ORDER = [
    "precision-pen", "fluid-sketch", "ink-studio", "width-studio",
    "path-studio", "geometry-lab", "collision-align", "mirror-studio",
    "shape-reform", "live-style", "color-lab", "texture-lab", "stipple-lab",
    "randomize", "smart-find", "vector-repair", "raster-lab", "auto-save",
    "direct-settings", "suite-core", "projection-studio",
    "projection-rectangle", "projection-ellipse", "projection-box",
    "fractal-grove",
]

# Il pannello compatto di Illustrator riusa la risorsa di Suite Core. La card
# nell'app conserva il proprio glifo, mentre la risorsa nativa mostra il vero
# marchio Vector Suite in versione monocromatica e leggibile a 16–24 pt.
PANEL_MARK_ICON = (
    '<path d="M12 2.5 21 7.7v10.4L12 23.3 3 18.1V7.7Z"/>'
    '<path d="M12 12.9 21 7.7M12 12.9 3 7.7M12 12.9v10.4"/>'
    '<path d="M7.25 15.65 12 12.9l4.75 2.75"/>'
    '<circle cx="7.25" cy="15.65" r="1.25" fill="#ffffff"/>'
    '<circle cx="16.75" cy="15.65" r="1.25" fill="#ffffff"/>'
)


def solidify(fragment):
    """Traduce `class="fill-on"` in attributi espliciti.

    Il plug-in disegna gli SVG senza foglio di stile: gli elementi pieni devono
    dichiarare fill e stroke da soli.
    """
    return fragment.replace('class="fill-on"', 'fill="#000000" stroke="none"')


def plugin_icon(fragment):
    offset_x, offset_y = PLUGIN_OFFSET
    width, height = PLUGIN_CANVAS
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        'viewBox="0 0 {w} {h}">\n'
        '  <g transform="translate({x} {y}) scale({s:.10f})" fill="none" '
        'stroke="#000000" stroke-width="{stroke}" stroke-linecap="round" '
        'stroke-linejoin="round">\n'
        '    {body}\n'
        '  </g>\n'
        '</svg>\n'
    ).format(
        w=width, h=height, x=offset_x, y=offset_y, s=PLUGIN_SCALE,
        stroke=STROKE, body=solidify(fragment))


def icons_js():
    mark_box, mark = mark_fragment()
    lines = [
        "/* Generato da scripts/generate-assets.py — non modificare a mano. */",
        "/* La geometria vive in un solo posto: l'app e il plug-in per",
        "   Illustrator disegnano le stesse identiche forme. */",
        "/* Il marchio è letto da %s: quel disegno" % LOGO_FILE,
        "   è la sorgente e non viene mai riscritto. */",
        "window.VSGrid = %d;" % GRID,
        "window.VSMarkBox = %g;" % mark_box,
        "window.VSMark = %s;" % js_string(mark),
        "window.VSIcons = {",
    ]
    for key in MODULE_ORDER:
        lines.append("  %s: %s," % (js_string(key), js_string(MODULE_ICONS[key])))
    lines.append("};")
    lines.append("window.VSUIIcons = {")
    for key in sorted(UI_ICONS):
        lines.append("  %s: %s," % (js_string(key), js_string(UI_ICONS[key])))
    lines.append("};")
    return "\n".join(lines) + "\n"


def js_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def targets():
    """File generati e confrontati da --check.

    Resources/VectorSuiteLogo.svg non compare qui, e non deve comparirci: è il
    disegno originale del marchio, si modifica in Illustrator e nessuno script
    lo sovrascrive.
    """
    raw = ROOT / "native/VectorSuiteNative/Resources/raw"
    files = {ROOT / "Resources/ui/icons.js": icons_js()}
    for key in MODULE_ORDER:
        fragment = PANEL_MARK_ICON if key == "suite-core" else MODULE_ICONS[key]
        files[raw / ("VSIcon-%s.svg" % key)] = plugin_icon(fragment)
    return files


WIN_DIR = "native/VectorSuiteNative/Resources/Win"
WIN_PNG_SIZE = 64
WIN_RESOURCE_BASE = 24000


def write_windows_icons():
    """Rasterizza le icone per il pannello Windows e scrive la tabella .rc.

    GDI+ non sa leggere SVG, quindi il pannello Win32 usa PNG incorporati nel
    binario come risorse. Sono bianchi su fondo trasparente: il disegno sta nel
    canale alfa e il colore lo decide il pannello, esattamente come su macOS.
    """
    directory = ROOT / WIN_DIR
    directory.mkdir(parents=True, exist_ok=True)

    defines = [
        "// Generato da scripts/generate-assets.py — non modificare a mano.",
        "// Incluso sia dal compilatore di risorse sia dal pannello Win32.",
        "#ifndef VECTOR_SUITE_WIN_ICONS_H",
        "#define VECTOR_SUITE_WIN_ICONS_H",
        "",
    ]
    for index, key in enumerate(MODULE_ORDER):
        defines.append("#define IDR_VSICON_%-24s %d"
                       % (key.replace("-", "_").upper(), WIN_RESOURCE_BASE + index))
    defines += [
        "",
        "// La tabella permette al pannello di risalire alla risorsa partendo",
        "// dalla chiave del modulo: l'ordine del catalogo e l'ordine delle",
        "// icone non coincidono, quindi un semplice scarto non basterebbe.",
        "#ifndef RC_INVOKED",
        "struct VSWindowsIcon { const char* key; int resource; };",
        "static const VSWindowsIcon kVSWindowsIcons[] = {",
    ]
    for index, key in enumerate(MODULE_ORDER):
        defines.append('\t{"%s", %d},' % (key, WIN_RESOURCE_BASE + index))
    defines += [
        "};",
        "static const int kVSWindowsIconCount = %d;" % len(MODULE_ORDER),
        "#endif  // RC_INVOKED",
        "",
        "#endif",
        "",
    ]
    (directory / "icons.h").write_text("\n".join(defines), encoding="utf-8")

    # Su Windows le risorse si dichiarano nel .rc. Servono tre gruppi: gli SVG
    # che Illustrator usa per le icone degli strumenti, la mappa IDToFile che
    # li lega agli identificatori del catalogo, e i PNG che il pannello disegna.
    #
    # I nomi delle risorse non possono contenere trattini, quindi la mappa per
    # Windows usa nomi con l'underscore. È un artefatto separato da quello di
    # macOS: là IDToFile è un file dentro il bundle, qui è una risorsa dentro
    # il binario, e i due non devono per forza coincidere.
    mapping = ["// Generato da scripts/generate-assets.py."]
    for index, key in enumerate(MODULE_ORDER):
        mapping.append("%d.svg\t\tVSIcon_%s.svg"
                       % (WIN_RESOURCE_BASE + index, key.replace("-", "_")))
    (directory / "IDToFile.txt").write_text("\n".join(mapping) + "\n", encoding="utf-8")

    lines = [
        "// Generato da scripts/generate-assets.py — non modificare a mano.",
        '#include "icons.h"',
        "",
        "// Mappa identificatore → nome risorsa, letta da Illustrator.",
        'IDToFile txt "IDToFile.txt"',
        "",
        "// Icone degli strumenti: Illustrator le vuole in SVG.",
    ]
    for key in MODULE_ORDER:
        lines.append('VSIcon_%-22s svg "VSIcon-%s.svg"'
                     % (key.replace("-", "_"), key))
    lines += [
        "",
        "// Le stesse icone in PNG: GDI+ non legge SVG e il pannello disegna",
        "// da queste.",
    ]
    for key in MODULE_ORDER:
        lines.append('IDR_VSICON_%-24s PNG "VSIcon-%s.png"'
                     % (key.replace("-", "_").upper(), key))
    lines.append("")
    (directory / "icons.rc").write_text("\n".join(lines), encoding="utf-8")

    converter = shutil.which("rsvg-convert") or shutil.which("convert")
    if not converter:
        return 0

    written = 0
    raw = ROOT / "native/VectorSuiteNative/Resources/raw"
    for key in MODULE_ORDER:
        source = raw / ("VSIcon-%s.svg" % key)
        target = directory / ("VSIcon-%s.png" % key)
        if converter.endswith("rsvg-convert"):
            command = [converter, "-w", str(WIN_PNG_SIZE), "-h", str(WIN_PNG_SIZE),
                       "-o", str(target), str(source)]
        else:
            command = [converter, "-background", "none", "-density", "600",
                       str(source), "-resize", "%dx%d" % (WIN_PNG_SIZE, WIN_PNG_SIZE),
                       "-gravity", "center",
                       "-extent", "%dx%d" % (WIN_PNG_SIZE, WIN_PNG_SIZE),
                       "-channel", "RGB", "-evaluate", "set", "100%", "+channel",
                       "PNG32:" + str(target)]
        if subprocess.run(command, capture_output=True).returncode == 0:
            written += 1
    return written


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verifica senza scrivere")
    arguments = parser.parse_args()

    missing = [key for key in MODULE_ORDER if key not in MODULE_ICONS]
    if missing:
        print("Chiavi assenti nella tabella: %s" % ", ".join(missing))
        return 1
    extra = [key for key in MODULE_ICONS if key not in MODULE_ORDER]
    if extra:
        print("Icone fuori catalogo: %s" % ", ".join(extra))
        return 1

    problems = 0
    for path, content in targets().items():
        if arguments.check:
            current = path.read_text(encoding="utf-8") if path.exists() else None
            if current != content:
                print("Non allineato: %s" % path.relative_to(ROOT))
                problems += 1
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")

    if arguments.check:
        if problems:
            print("Esegui ./scripts/generate-assets.py per rigenerare.")
            return 1
        print("Asset allineati: %d icone, marchio letto da %s."
              % (len(MODULE_ORDER), LOGO_FILE))
        return 0

    icon = ROOT / ICON_FILE
    icon.parent.mkdir(parents=True, exist_ok=True)
    icon.write_text(app_icon_svg(), encoding="utf-8")

    rasterised = write_windows_icons()

    print("Scritti %d file, più il master dell'icona in %s."
          % (len(targets()), ICON_FILE))
    if rasterised:
        print("Rigenerati %d PNG per Windows." % rasterised)
    else:
        print("PNG per Windows lasciati invariati: manca un rasterizzatore.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
