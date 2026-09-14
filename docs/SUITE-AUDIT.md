# Inventario Vector Suite

## Architettura

Tutti i moduli sono implementati nel bundle unico:

```text
VectorSuiteNative.aip
studio.vectorsuite.plugin.core
```

Il bundle è compilato come universal binary `arm64` + `x86_64`, contiene un
pannello AppKit nativo e registra 25 strumenti interni nascosti dalla toolbar
di Illustrator. Le 25 icone SVG sono originali e monocromatiche, costruite su una griglia 24×24
con tratto 1.5 e generate dalla stessa sorgente delle icone dell'app di
gestione (`scripts/generate-assets.py`), su tela 44×36 come richiede
Illustrator per le icone degli strumenti.

## Catalogo dei moduli

| Slot | Modulo | Categoria | Comportamento principale |
|---|---|---|---|
| 01 | Precision Pen | Disegno | segmenti con vincolo angolare |
| 02 | Fluid Sketch | Disegno | tracciato gestuale smussato |
| 03 | Ink Studio | Disegno | tratto vettoriale morbido e spesso |
| 04 | Width Studio | Disegno | regolazione interattiva dello spessore |
| 05 | Path Studio | Geometria | semplificazione controllata dei tracciati |
| 06 | Geometry Lab | Geometria | costruzione di cerchi e tangenti |
| 07 | Collision Align | Geometria | posizionamento interattivo della selezione |
| 08 | Mirror Studio | Geometria | duplicazione e riflessione su asse |
| 09 | Shape Reform | Geometria | rimodellazione dell’ancora più vicina |
| 10 | Live Style | Aspetto | regolazione interattiva dell’opacità |
| 11 | Color Lab | Aspetto | trasformazione tonale monocromatica |
| 12 | Texture Lab | Aspetto | retino vettoriale entro un’area |
| 13 | Stipple Lab | Aspetto | puntinatura vettoriale entro un’area |
| 14 | Randomize | Aspetto | variazioni deterministiche di trasformazione |
| 15 | Smart Find | Workflow | selezione di tracciati con attributi affini |
| 16 | Vector Repair | Workflow | rimozione di punti e segmenti duplicati |
| 17 | Raster Lab | Workflow | selezione di immagini raster e collegate |
| 18 | Auto Save | Workflow | salvataggio immediato del documento attivo |
| 19 | Direct Settings | Sistema | accesso diretto al pannello della suite |
| 20 | Suite Core | Sistema | pannello, ricerca e coordinamento strumenti |
| 21 | Projection Studio | Geometria | linea, rettangolo, ellisse e box proiettati |
| 22 | Fractal Grove | Geometria | alberi frattali con 12 parametri e seme |

## Superfici in Illustrator

- `Finestra > Vector Suite`: pannello nativo completo.
- Nessuna icona Vector Suite nella toolbar: gli strumenti si attivano dal
  pannello senza occupare la barra strumenti.
- Tema: colori dinamici di sistema; le icone sono ricolorate al volo sul colore
  della pastiglia, così seguono il tema chiaro o scuro.
- Controlli: cursori, caselle, selettore a segmenti e pulsante principale sono
  ridisegnati in bianco, nero e grigio per non ereditare il colore di accento.

## Controlli automatici

`scripts/verify.sh` controlla:

- firma e struttura dell’app standalone;
- presenza del bundle nativo incorporato;
- parità del catalogo fra C++, Objective-C, JavaScript e `IDToFile.txt`;
- allineamento fra la sorgente grafica e gli asset generati;
- validità XML delle 25 icone e del marchio;
- prova a freddo dell'interfaccia dell'app (`tests/ui-smoke.js`);
- identifier, firma e architetture del plug-in;
- assenza di riferimenti personali o identificativi di prodotti esterni nei
  sorgenti Vector Suite.
