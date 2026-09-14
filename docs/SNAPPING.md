# Snap

## Snap Vector Suite (Mac)

In Direct Settings → Snap Vector Suite, selezionare “Attiva snap aggiuntivi”.
Ogni modalità è selezionabile e la tolleranza è regolabile da 1 a 32 pixel.

Implementati: estremi/ancoraggi, punti medi, intersezioni tra segmenti retti,
punto più vicino, perpendicolare, centro geometrico, estremi X/Y e tangente.
Punto vicino, perpendicolare, tangente e punto medio supportano le curve Bézier.
Il punto medio divide la lunghezza del segmento in due parti uguali.
Il centro geometrico è il baricentro di ciascun tracciato chiuso, non del
riempimento complessivo di un compound path con fori.
Tangente e perpendicolare usano il punto iniziale del gesto.
Shift ammette gli agganci compatibili con le direzioni a 45°.

Gli snap operano nel cursore degli strumenti Vector Suite. Le preferenze
native restano indipendenti. Attivare le Guide sensibili per le annotazioni.
I riferimenti sono letti prima del gesto, così l'anteprima non aggancia sé stessa.
Oggetti bloccati o nascosti sono esclusi, anche dentro gruppi.

Verifiche: test geometrici automatici (priorità, zoom, Shift, casi degeneri)
e compilazione Mac universale. Resta da eseguire il collaudo interattivo in Illustrator.
Non sono inclusi snap globali aggiuntivi per la Penna standard,
intersezioni tra curve, estensione e parallelo. Il pannello Windows
non espone ancora questi nuovi controlli.

## Snap nativi

Su Mac: Vector Suite → Direct Settings → Snap di Illustrator.
Il pulsante apre le preferenze globali di Illustrator, senza cambiare
silenziosamente le impostazioni o modificare il documento.

Per lo snap tangente con Penna e Linea, abilitare le Guide geometriche
nelle Guide sensibili. Il comportamento riguarda gli estremi della linea,
non le tangenti a metà segmento.

Fonte: [Adobe, Snap to tangent](https://helpx.adobe.com/illustrator/desktop/measure-and-align/plot-and-measure/snap-to-tangent.html),
verificata il 14 settembre 2026.

Il motore custom dell'SDK viene richiamato dal singolo strumento:
un pannello non basta a estenderlo automaticamente
a tutti gli strumenti standard.

Fractal Grove e Projection Studio restano presenti e invariati.
