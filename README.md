# Vector Suite

22 moduli per Adobe Illustrator 2026: disegno, geometria, assonometrie,
alberi frattali e gestione degli oggetti. Un pannello, interfaccia monocromatica.

Il progetto è in sviluppo: le funzioni non sono ancora equivalenti a quelle
dei plug-in di riferimento. I limiti sono elencati in
[Stato dei moduli](docs/FUNCTIONAL-PARITY.md).

## Download e installazione

I pacchetti pubblicati si trovano nella sezione **Releases** di questo repository.
Se non ci sono allegati, non è ancora disponibile un download per quella versione.

### Mac

Scarica `Vector Suite.dmg`, aprilo e trascina l'app in Applicazioni.
Chiudi Illustrator, apri Vector Suite e scegli **Installa Vector Suite in Illustrator**.
Riavvia Illustrator e apri **Finestra → Vector Suite**.

La build locale contiene app e plug-in per Apple Silicon e Intel.
Non è notarizzata. L'app di gestione può essere chiusa dopo l'installazione.

## Uso

Seleziona un modulo nel pannello e regola le sue opzioni.
Su Mac, clicca un campo numerico e usa la rotellina per modificarlo;
Option riduce il passo, Shift lo aumenta. Control + trascina riordina le schede.
Negli strumenti di disegno della suite, Shift vincola la direzione a 45°.

Projection Studio comprende i preset cavaliera e monometrica e genera guide
assonometriche utilizzabili con la Penna di Illustrator. Le guide non impongono
un vincolo esclusivo agli assi. Su Mac, Direct Settings → Snap di Illustrator
apre le preferenze globali delle Guide sensibili. Illustrator 2026 supporta
già la tangente con Penna e Linea tramite le Guide geometriche.
Direct Settings → Snap Vector Suite aggiunge otto modalità, incluse tangente,
perpendicolare e punto medio sulle curve Bézier. Le intersezioni aggiuntive
sono limitate ai segmenti retti. Opzioni e limiti:
[Snap](docs/SNAPPING.md).

## Compilazione

Serve l'SDK Illustrator 2026 in `sdk/Adobe Illustrator 2026 SDK/`.
L'SDK e i plug-in di riferimento non sono inclusi.

Mac: Xcode, Python 3 e Node.js; esegui `sh scripts/build-macos.sh`.
Il DMG viene creato in `build/`.

Test senza SDK: `sh tests/run-native-unit-tests.sh`.
Verifica del pacchetto Mac: `sh scripts/verify.sh`.
I test automatici non sostituiscono le prove dentro Illustrator.

Il workflow di rilascio richiede il segreto `AI_SDK_URL` e prepara una bozza,
da pubblicare dopo la verifica su macOS.
