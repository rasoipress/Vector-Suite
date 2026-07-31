# Vector Suite Native

Plug-in nativo per Adobe Illustrator 2026, costruito con l'SDK ufficiale.

Il bundle registra un solo gruppo Vector Suite nella barra strumenti. Il gruppo
contiene 22 moduli e 25 strumenti, inclusi Projection Studio e Fractal Grove.
Il primo strumento apre il pannello nativo; ogni modulo può anche essere
inserito singolarmente nelle toolbar personalizzate di Illustrator.

Il pannello adotta lo stesso sistema visivo dell'app di gestione: marchio
isometrico, pastiglie di icona invertite, stessa scala tipografica e stessi
raggi. Le tinte sono colori dinamici di sistema, quindi il pannello segue il
tema chiaro o scuro senza introdurre alcun colore oltre a bianco, nero e i
grigi che ne derivano. Cursori, caselle, selettore a segmenti e pulsante
principale sono ridisegnati proprio per non ereditare il colore di accento di
macOS.

Le icone sono SVG monocromatici su tela 44×36, la misura che Illustrator usa
per le icone degli strumenti; l'area di disegno è il quadrato 32×32 centrato.
Vengono generate insieme a quelle dell'app da `scripts/generate-assets.py`.

Fractal Grove integra nel pannello 12 cursori, seme deterministico, quattro
livelli di ondulazione, modalità automatica e avanzata, raggruppamento,
sostituzione, nuovo seme e ripristino.

## Build

Dal root del progetto:

```sh
./scripts/build-native.sh
```

Lo script rigenera gli asset grafici e poi compila. Output:

```text
build/native/release/VectorSuiteNative.aip
```

Il progetto non dipende da bundle o licenze di terze parti.
