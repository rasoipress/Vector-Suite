# Test Vector Suite

## Controlli automatici

Dal root del progetto:

```sh
./scripts/verify.sh
node tests/fractal-reference.js
```

## Smoke test in Illustrator 2026

1. Chiudere Illustrator ed eseguire `./scripts/install-macos.sh`.
2. Avviare Illustrator e creare un documento RGB.
3. Aprire `Finestra > Vector Suite`.
4. Verificare ricerca, categorie, tema e tutti i 22 moduli.
5. Aprire l’editor della toolbar e verificare i 25 strumenti Vector Suite.
6. Tracciare almeno una forma con ciascun tool interattivo.
7. In Fractal Grove generare con i valori predefiniti: il risultato atteso è
   un gruppo denominato `Vector Suite — Fractal Grove` con 2047 tracciati.
8. Cambiare seme, ondulazione e casualità; attivare Auto e Sostituisci.
9. Verificare Annulla/Ripeti e salvare/riaprire il documento.
