#!/usr/bin/env python3
"""Ricava dal marchio la sagoma unica richiesta da Icon Composer.

Perché serve
------------
Resources/VectorSuiteLogo.svg è dipinto per sovrapposizione: un esagono bianco
e sopra, in nero, le fenditure, le maniglie e i cerchi dei pomelli. Funziona
finché dietro c'è la piastra nera.

Le icone Liquid Glass di macOS 26 non hanno una piastra: il fondo lo disegna il
sistema, e nelle modalità Clear e Tinted i livelli vengono ricolorati tutti allo
stesso modo. Di un disegno a sovrapposizione resterebbe solo la sagoma esterna:
il nero verrebbe rimappato e il cubo diventerebbe un esagono pieno.

Questo script converte il disegno in ciò che serve davvero: **una sola forma il
cui canale alfa contiene già i vuoti**. Le fenditure, le maniglie e gli anelli
dei pomelli diventano buchi veri, sottratti geometricamente. Qualunque colore o
materiale il sistema applichi, il disegno regge.

Uso
---
    pip3 install shapely
    ./scripts/generate-glass-mark.py

Output
------
    Resources/icon/VectorSuiteMark.png   livello per Icon Composer
    Resources/icon/VectorSuiteMark.svg   la stessa sagoma in vettoriale

Il PNG è bianco su fondo trasparente a 1024 px, che è la forma in cui Icon
Composer vuole i livelli: sembra vuoto se lo apri su fondo bianco, ed è
normale. Non è un capriccio — i livelli importati come SVG risultano non
ricevere gli effetti Liquid Glass (rapporto FB18097334), quindi il file da
trascinare in Icon Composer è il PNG. L'SVG resta come sorgente vettoriale, in
nero perché sia leggibile aprendolo.

La geometria è letta da Resources/VectorSuiteLogo.svg: se il marchio cambia,
si rilancia questo script e le misure seguono.
"""

import pathlib
import re
import subprocess
import sys

try:
    from shapely.geometry import Point, Polygon
    from shapely.ops import unary_union
except ImportError:
    sys.exit("Serve shapely:  pip3 install shapely")

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOGO = ROOT / "Resources/VectorSuiteLogo.svg"
OUT_DIR = ROOT / "Resources/icon"
PNG_SIZE = 1024

# Le curve non esistono in questo marchio: i cerchi vengono approssimati con un
# poligono a passo fine, che a 1024 px resta sotto il decimo di pixel.
CIRCLE_SEGMENTS = 512


def numbers(text):
    return [float(value) for value in re.findall(r"-?\d*\.?\d+(?:[eE][-+]?\d+)?", text)]


TOKENS = re.compile(r"([MmLlHhVvZz])|(-?\d*\.?\d+(?:[eE][-+]?\d+)?)")


def subpaths(data):
    """Legge un attributo d e restituisce le sue spezzate come liste di punti.

    Illustrator esporta con comandi relativi (`l`, `v`) e ripetizioni implicite:
    leggerli come assoluti produce una geometria degenere, quindi qui i comandi
    vengono interpretati per davvero. Bastano i tratti rettilinei: in questo
    marchio non ci sono curve.
    """
    tokens = [(command, value) for command, value in TOKENS.findall(data)]
    result = []
    current = []
    x = y = 0.0
    start = (0.0, 0.0)
    command = None
    queue = []

    def flush():
        if len(current) > 1:
            result.append(list(current))

    index = 0
    while index < len(tokens):
        token, value = tokens[index]
        if token:
            command = token
            index += 1
            if command in "Zz":
                if current and current[0] != current[-1]:
                    current.append(current[0])
                flush()
                current = []
                x, y = start
                continue
        # raccoglie gli argomenti necessari al comando corrente
        needed = 1 if command in "HhVv" else 2
        queue = []
        while len(queue) < needed and index < len(tokens) and not tokens[index][0]:
            queue.append(float(tokens[index][1]))
            index += 1
        if len(queue) < needed:
            break

        if command == "M":
            flush()
            x, y = queue
            start = (x, y)
            current = [(x, y)]
            command = "L"
        elif command == "m":
            flush()
            x, y = x + queue[0], y + queue[1]
            start = (x, y)
            current = [(x, y)]
            command = "l"
        elif command == "L":
            x, y = queue
            current.append((x, y))
        elif command == "l":
            x, y = x + queue[0], y + queue[1]
            current.append((x, y))
        elif command == "H":
            x = queue[0]
            current.append((x, y))
        elif command == "h":
            x = x + queue[0]
            current.append((x, y))
        elif command == "V":
            y = queue[0]
            current.append((x, y))
        elif command == "v":
            y = y + queue[0]
            current.append((x, y))

    flush()
    return result


def read_logo():
    """Estrae dal disegno i pochi numeri che servono."""
    text = LOGO.read_text(encoding="utf-8")

    box = numbers(re.search(r'viewBox="([^"]+)"', text).group(1))[2]

    widths = {}
    for selector, body in re.findall(r"([^{}]+)\{([^{}]*)\}",
                                     re.search(r"<style>(.*?)</style>", text, re.S).group(1)):
        match = re.search(r"stroke-width:\s*([\d.]+)", body)
        if not match:
            continue
        for name in selector.split(","):
            name = name.strip().lstrip(".")
            if name:
                widths[name] = float(match.group(1))

    paths = re.findall(r'<path class="([\w-]+)" d="([^"]+)"', text)
    circles = re.findall(r'<circle class="([\w-]+)"[^>]*cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)"',
                         text)

    hexagon = None
    strokes = []
    for name, data in paths:
        lines = subpaths(data)
        if name not in widths:                 # classe di solo riempimento: la sagoma
            hexagon = lines[0]
        else:
            strokes.append((name, lines))

    if not (hexagon and strokes and circles):
        sys.exit("Il disegno non ha la struttura attesa: sagoma, tratti e pomelli.")
    if len(hexagon) < 6:
        sys.exit("La sagoma letta ha %d vertici: il tracciato non è stato "
                 "interpretato correttamente." % len(hexagon))
    return box, hexagon, strokes, circles, widths


def band(start, end, width):
    """Rettangolo che rappresenta un tratto con terminazioni piatte."""
    (x0, y0), (x1, y1) = start, end
    length = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
    half = width / 2.0
    nx, ny = -(y1 - y0) / length * half, (x1 - x0) / length * half
    return Polygon([(x0 + nx, y0 + ny), (x1 + nx, y1 + ny),
                    (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)])


def ring(cx, cy, radius, width):
    """Anello del pomello: il cerchio è tracciato, quindi il vuoto è la corona."""
    outer = Point(cx, cy).buffer(radius + width / 2.0, CIRCLE_SEGMENTS // 4)
    inner = Point(cx, cy).buffer(radius - width / 2.0, CIRCLE_SEGMENTS // 4)
    return outer.difference(inner)


def build_shape():
    box, hexagon, strokes, circles, widths = read_logo()

    voids = []
    for name, lines in strokes:
        for points in lines:
            for index in range(len(points) - 1):
                voids.append(band(points[index], points[index + 1], widths[name]))
    for name, cx, cy, radius in circles:
        voids.append(ring(float(cx), float(cy), float(radius), widths[name]))

    shape = Polygon(hexagon).buffer(0).difference(unary_union(voids).buffer(0))
    return box, shape


def path_data(shape, decimals=3):
    """Traduce la geometria in un attributo d con riempimento even-odd."""
    polygons = list(getattr(shape, "geoms", [shape]))
    parts = []
    for polygon in polygons:
        for ring_coords in [polygon.exterior] + list(polygon.interiors):
            points = list(ring_coords.coords)[:-1]
            parts.append("M" + " ".join(
                "%s %s" % (round(x, decimals), round(y, decimals))
                for x, y in points) + "Z")
    return "".join(parts)


def main():
    box, shape = build_shape()
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    svg = (
        '<svg xmlns="http://www.w3.org/2000/svg" width="{b:g}" height="{b:g}" '
        'viewBox="0 0 {b:g} {b:g}">\n'
        '  <!-- Generato da scripts/generate-glass-mark.py a partire da\n'
        '       Resources/VectorSuiteLogo.svg. Sagoma unica: fenditure, maniglie\n'
        '       e anelli dei pomelli sono buchi veri, non vernice sopra.\n'
        '       Il colore non conta, conta il canale alfa: è quello che macOS\n'
        '       usa per applicare i materiali Liquid Glass. -->\n'
        '  <path fill="#000000" fill-rule="evenodd" d="{d}"/>\n'
        '</svg>\n'
    ).format(b=box, d=path_data(shape))

    svg_path = OUT_DIR / "VectorSuiteMark.svg"
    svg_path.write_text(svg, encoding="utf-8")

    # Il livello per Icon Composer va bianco su trasparente: il sistema usa la
    # forma, non il colore, e parte dal bianco per applicare i suoi materiali.
    png_path = OUT_DIR / "VectorSuiteMark.png"
    rendered = subprocess.run(
        ["convert", "-background", "none", "-density", "600", str(svg_path),
         "-resize", "%dx%d" % (PNG_SIZE, PNG_SIZE),
         "-gravity", "center", "-extent", "%dx%d" % (PNG_SIZE, PNG_SIZE),
         # Porta il colore a bianco lasciando intatto il canale alfa: è
         # l'alfa a contenere il disegno, e va conservato com'è.
         "-channel", "RGB", "-evaluate", "set", "100%", "+channel",
         "PNG32:" + str(png_path)],
        capture_output=True)
    if rendered.returncode != 0:
        print("  [!] PNG non prodotto: manca un rasterizzatore SVG.")

    area = shape.area / (box * box) * 100.0
    print("Sagoma unica: %d contorni, %.1f%% del riquadro."
          % (len(path_data(shape).split("M")) - 1, area))
    print("  %s" % svg_path.relative_to(ROOT))
    if png_path.exists():
        print("  %s" % png_path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
