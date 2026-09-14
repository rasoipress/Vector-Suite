# Parità funzionale Vector Suite

Questo documento è la checklist di accettazione della reimplementazione
clean-room. I nomi e l'interfaccia pubblica restano quelli di Vector Suite; i
bundle di riferimento servono soltanto per osservare risultati, combinazioni di
tasti e casi limite. Una scheda non è considerata completa finché le sue
operazioni non sono implementate, annullabili e verificate in Illustrator.

## Stato

| Modulo | Implementazione verificabile attuale | Funzioni ancora necessarie per la parità |
|---|---|---|
| Precision Pen | linea o curva Bezier parametrica; vincolo angolare, lunghezza fissa opzionale, curvatura e spessore persistenti | modifica completa di punti, maniglie e segmenti esistenti; connector point e rimozione intelligente |
| Fluid Sketch | gesto Bezier con smoothing, campionamento, spessore e chiusura regolabili durante l’anteprima | larghezza da velocità o pressione, repeat sketch, modifica/continua/unisci, trim/join |
| Ink Studio | sagoma calligrafica vettoriale chiusa con pennino ellittico, larghezza, aspetto, angolo e smoothing | preset, curve di pressione/inclinazione, bleed/feather/splatter, effetto live ed espansione |
| Width Studio | spessore esatto o moltiplicativo su tracciati, gruppi e compound path; terminali e giunzioni | marker di larghezza locali, distribuzione/media, stamp, brush, eraser e ottimizzazione marker |
| Path Studio | semplificazione con protezione delle curve; ancoraggi netti/morbidi; inversione direzione; gruppi e compound path | forme e angoli dinamici, misura, estensione, riposizionamento, intersezioni ed effetti live |
| Geometry Lab | cerchio, tangente, costruzione combinata e arco semicircolare con spessore configurabile | cerchi per tre punti/curvatura, archi generali e connessioni avanzate |
| Collision Align | disposizione sequenziale a contatto in quattro direzioni, gap e allineamento dei centri | rotazione alla collisione, super-marquee, space fill e inflate/deflate |
| Mirror Studio | copie verticale, orizzontale, doppia e radiale fino a 32 settori, con asse spostabile | assi persistenti per livello, settore attivo, trim/join e copia/incolla assi |
| Shape Reform | deformazione locale multi-ancoraggio con raggio, falloff morbido, forza e conservazione maniglie | marker e profili salvabili, testo, clipping, transizioni e smart join |
| Live Style | opacità, metodi di fusione e isolamento applicati in modo non distruttivo a oggetti e gruppi | transform/free distort/offset, ombre/glow, blur, effetti aggiuntivi ed explorer |
| Color Lab | luminosità, contrasto, saturazione e tonalità su RGB, CMYK e grigi, anche dentro gruppi | curve/livelli avanzati, colori campione/gradienti, separazioni e retini live |
| Texture Lab | texture vettoriale ritagliata al rettangolo, angolo/spaziatura/spessore e crosshatch | texture live modificabile, libreria/importazione e pennelli di rimozione |
| Stipple Lab | puntinatura sfalsata deterministica con spaziatura, raggio, variazione e limite oggetti | effetto live sulla forma, simboli personalizzati e modifica non distruttiva |
| Randomize | trasformazioni deterministiche di posizione, rotazione e scala; seed, distribuzione uniforme/centrata e scala indipendente | colore, ordine e selezione random, ulteriori distribuzioni e anteprima live |
| Smart Find | ricerca separata per aspetto, geometria o criterio combinato; confronto di colore, traccia, opacità, punti, apertura e dimensioni; oggetti bloccati/nascosti esclusi; applicazione dello stile corrente e rapporto risultati | ricerca di testo e attributi avanzati, ambito livello/tavola/selezione, sostituzione con sorgente dedicata, tolleranze regolabili e preset |
| Vector Repair | pulizia batch conservativa di duplicati e punti collineari superflui; supporto a gruppi e compound path; protezione dei punti curvi; rapporto risultati | diagnosi e selezione preventiva delle altre anomalie, opzioni di tolleranza, chiusura gap, maniglie anomale, oggetti non dipinti e preset di pulizia |
| Raster Lab | selezione immagini, incorporamento collegamenti, metadato PPI e copia ricampionata con tre algoritmi | ritaglio immagine, relink/unembed, ricampionamento in-place opzionale e batch con rapporto dettagliato |
| Auto Save | timer SDK 1–120 minuti, solo documenti modificati, Salva ora per file senza nome e copie versionate “Vector Suite Backups” | limite/rotazione delle versioni, browser di recupero e gestione multi-documento |
| Direct Settings | preferenze del pannello, tema adattivo e ripristino dell’ordine dei moduli | scorciatoie personalizzabili e preset globali importabili/esportabili |
| Suite Core | stato dei 22 moduli, catalogo, pannello, preferenze condivise e infrastruttura comandi | serializzazione nel documento e infrastruttura completa degli effetti live |
| Projection Studio | disegno su tre piani; quattro trasformazioni; proiezione/deproiezione; copia opzionale; metadati per inversione esatta; spostamento numerico sugli assi X/Z/Y; estrusione wireframe di tracciati, gruppi e compound path; scala U/V, rotazione e inclinazione sul piano; misura U/V e diagonale | arc, tangent, estrusione con facce e illuminazione, fastener, riferimenti comuni, zone, auxiliary, perspective, transform live e draw styles |
| Fractal Grove | generatore parametrico con seed e opzioni | test documento e rifinitura prestazioni su alberi molto grandi |

## Regole di completamento

Per ogni riga:

1. l'operazione deve modificare realmente l'artwork, non soltanto selezionare
   uno strumento;
2. deve funzionare su tracciati, gruppi e compound path quando applicabile;
3. deve rispettare selezione, blocchi e gerarchia senza trasformare due volte i
   figli selezionati;
4. deve avere undo/redo con un nome comprensibile;
5. i parametri persistenti devono essere salvati senza dati personali;
6. la geometria deterministica deve avere test automatici;
7. il bundle universal deve compilare, firmarsi e caricarsi in Illustrator;
8. l'ultimo controllo è un confronto visivo e operativo con il comportamento di
   riferimento su uno stesso documento campione.
