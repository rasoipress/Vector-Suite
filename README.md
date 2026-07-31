# Vector Suite

Vector Suite aggiunge ad Adobe Illustrator 2026 ventidue moduli di disegno,
geometria e produzione, riuniti in un solo plug-in nativo. È composta di due
elementi:

| Elemento | Che cos'è | Dove vive |
|---|---|---|
| **VectorSuiteNative.aip** | il plug-in vero e proprio | dentro Illustrator |
| **Vector Suite.app** | il gestore che lo installa e lo verifica | in Applicazioni |

Il plug-in è costruito con l'SDK ufficiale di Illustrator. Non richiede
estensioni CEP, non incorpora un runtime web dentro Illustrator e non dipende
da bundle di terze parti: è codice nativo che Illustrator carica all'avvio,
esattamente come i propri strumenti. L'app di gestione serve a installarlo,
aggiornarlo e diagnosticarlo; chiuderla non tocca il plug-in.

---

## Che cosa fa il plug-in

Il bundle registra **22 moduli** e **25 strumenti** in un unico gruppo nella
barra strumenti di Illustrator. Ogni modulo lavora sul documento attivo con le
API native, quindi le operazioni restano annullabili e compatibili con la
cronologia di Illustrator.

### Disegno

| Modulo | Comportamento |
|---|---|
| Precision Pen | segmenti con vincolo angolare e controllo della distanza |
| Fluid Sketch | tracciato gestuale smussato in tempo reale |
| Ink Studio | tratto calligrafico morbido, sensibile a velocità e pressione |
| Width Studio | regolazione interattiva del profilo di spessore |

### Geometria

| Modulo | Comportamento |
|---|---|
| Path Studio | semplificazione controllata dei tracciati |
| Geometry Lab | costruzione di cerchi, archi e tangenti |
| Collision Align | posizionamento della selezione per contatto |
| Mirror Studio | duplicazione e riflessione su asse |
| Shape Reform | rimodellazione dell'ancora più vicina al puntatore |
| Projection Studio | linea, rettangolo, ellisse e box su griglia assonometrica |
| Fractal Grove | alberi frattali parametrici con dodici controlli e seme |

### Aspetto

| Modulo | Comportamento |
|---|---|
| Live Style | regolazione interattiva dell'opacità e degli effetti |
| Color Lab | trasformazione tonale monocromatica |
| Texture Lab | retino vettoriale entro un'area |
| Stipple Lab | puntinatura vettoriale entro un'area |
| Randomize | variazioni deterministiche di posizione, scala e rotazione |

### Workflow e sistema

| Modulo | Comportamento |
|---|---|
| Smart Find | selezione dei tracciati con attributi affini |
| Vector Repair | rimozione di punti e segmenti duplicati |
| Raster Lab | selezione delle immagini raster e collegate |
| Auto Save | salvataggio immediato del documento attivo |
| Direct Settings | accesso diretto al pannello della suite |
| Suite Core | pannello, ricerca e coordinamento degli strumenti |

---

## Come si usa dentro Illustrator

- **Finestra ▸ Vector Suite** apre il pannello nativo, con ricerca, filtro per
  categoria e le schede dei moduli. Un clic su una scheda attiva lo strumento.
- Il **gruppo Vector Suite** nella barra strumenti raccoglie tutti gli
  strumenti: tenendo premuto il pulsante si apre il menu completo.
- L'**editor della barra strumenti** di Illustrator permette di estrarre i
  singoli strumenti e sistemarli in una toolbar personalizzata.
- **Control + trascina** su una scheda del pannello ne cambia la posizione.
  L'ordine viene ricordato fra una sessione e l'altra.

Fractal Grove porta nel pannello l'intero generatore: dodici cursori, quattro
livelli di ondulazione, seme deterministico, modalità automatica e avanzata,
raggruppamento, sostituzione, nuovo seme e ripristino.

---

## Come si usa l'app di gestione

L'app individua da sola l'installazione di Illustrator più recente e legge il
bundle direttamente dal disco. Per ogni modulo mostra versione, identifier,
architetture, firma e percorso.

- Pallino **pieno**: bundle presente e integro.
- Pallino **vuoto**: bundle presente ma con qualcosa da sistemare; il pannello
  di destra elenca che cosa.
- Pallino **grigio**: bundle assente.

Dal menu **Suite** si apre Illustrator, si rilegge la cartella dei plug-in, si
sceglie una cartella diversa da quella rilevata e si avvia l'installazione.
`Cmd+F` porta il fuoco sulla ricerca, `Esc` la svuota.

---

## Installazione

### Per chi usa Illustrator

1. Chiudere Illustrator.
2. Aprire `Vector Suite.dmg` e trascinare **Vector Suite** in Applicazioni.
3. Avviare Vector Suite.
4. Scegliere **Suite ▸ Installa Vector Suite in Illustrator…**, oppure premere
   lo stesso pulsante nella scheda di un modulo.
5. Se la cartella di Illustrator appartiene al sistema, macOS chiede la
   password amministrativa con il proprio pannello standard.
6. Riavviare Illustrator e aprire **Finestra ▸ Vector Suite**.

Al primo avvio macOS può chiedere conferma perché la firma è ad hoc: in quel
caso aprire l'app una volta con il tasto destro ▸ **Apri**.

### L'SDK di Adobe va scaricato

**Il progetto non include l'SDK di Illustrator e non può includerlo:** è coperto
dalla licenza Adobe, che non ne permette la ridistribuzione. Senza SDK il
plug-in non si compila, mentre l'app di gestione e tutte le verifiche
funzionano lo stesso.

Si scarica gratuitamente con un Adobe ID da
**[Adobe Developer Console — Illustrator](https://developer.adobe.com/console/servicesandapis/ai)**
oppure dalla pagina **[Download SDKs](https://developer.adobe.com/console/downloads)**.
Va scompattato così:

```text
sdk/Adobe Illustrator 2026 SDK/
```

Il nome della cartella conta: gli script lo cercano esattamente con quel nome e
si fermano con l'indirizzo di download se non lo trovano. Il dettaglio è in
[`sdk/README.md`](sdk/README.md).

### Da sorgente, su macOS

Prerequisiti: Xcode con i Command Line Tools, Python 3 e l'SDK come sopra.

```sh
./scripts/build-native.sh    # solo il plug-in
./scripts/build-macos.sh     # plug-in + app + DMG
```

Risultato:

```text
build/native/release/VectorSuiteNative.aip
build/Vector Suite.app
build/Vector Suite.dmg
```

App e plug-in sono universal binary (`arm64` + `x86_64`) e firmati ad hoc. Per
la distribuzione pubblica la firma ad hoc va sostituita con un Developer ID e
il pacchetto va notarizzato da Apple.

Per installare senza passare dall'app:

```sh
./scripts/install-macos.sh
```

Lo script rileva l'installazione più recente di Illustrator, conserva una copia
della versione precedente in `backups/native` e installa il bundle dopo averlo
verificato. Accetta anche un percorso esplicito:

```sh
./scripts/install-macos.sh "/Applications/Adobe Illustrator 2026/Plug-ins.localized"
```

### Icona Liquid Glass per macOS 26

Su macOS 26 un'icona `.icns` da sola non riceve il trattamento in vetro: il
sistema disegna il proprio riquadro e ci mette dentro l'immagine, con un bordo
grigio visibile quando l'app è inattiva o con gli stili Dark, Clear e Tinted.
Per l'effetto pieno serve un pacchetto `.icon`, e va preparato una volta sola:

1. apri **Icon Composer** (Xcode ▸ Open Developer Tool ▸ Icon Composer);
2. crea una nuova icona e imposta il fondo su **Solid**, nero;
3. trascina sul canvas `Resources/icon/VectorSuiteMark.png`;
4. per quel livello imposta **Fill** bianco in Default e in Dark, e bianco
   pieno in Mono;
5. **salva** — non esportare — come `Resources/VectorSuite.icon`.

Da lì in poi `build-macos.sh` fa tutto da solo: compila il pacchetto con
`actool`, incorpora `Assets.car` nel bundle prima della firma e scrive
`CFBundleIconName` nell'Info.plist. Il file `.icns` resta al suo posto per i Mac
più vecchi. Senza Xcode 26 o senza il pacchetto `.icon` il passaggio si salta da
solo e l'app resta perfettamente valida.

Il livello si può rigenerare dal marchio, se il disegno cambia:

```sh
pip3 install shapely
./scripts/generate-glass-mark.py
```

Serve perché `VectorSuiteLogo.svg` è dipinto per sovrapposizione — nero sopra
bianco — e senza la piastra nera dietro quel nero verrebbe ricolorato dal
sistema, lasciando un esagono pieno. Lo script sottrae davvero le fenditure, le
maniglie e gli anelli dei pomelli, così il disegno tiene con qualunque
materiale.

### Da sorgente, su Windows

Prerequisiti: Visual Studio 2022 con il carico «Sviluppo di applicazioni
desktop con C++», CMake, Python 3 e l'SDK come sopra.

```powershell
.\scripts\build-windows.ps1
.\scripts\install-windows.ps1   # da una console come amministratore
```

Risultato: `build\win\Release\VectorSuiteNative.aip`, da copiare nella
cartella `Plug-ins` di Illustrator.

Su Windows esiste **solo il plug-in**, non l'app di gestione: quella è scritta
in Cocoa e vive solo su macOS. Il pannello dentro Illustrator è ridisegnato in
Win32 e mostra gli stessi moduli, le stesse icone e lo stesso marchio, con una
differenza dichiarata: non si riordinano le schede con Control + trascina.

---

## Versione

Il numero di versione ha una sorgente sola: il file **`VERSION`** alla radice.

```sh
./scripts/apply-version.py            # lo riporta nei due Info.plist
./scripts/apply-version.py --check    # verifica che siano allineati
./scripts/apply-version.py --print    # stampa la versione
```

Gli script di build lo applicano da soli prima di compilare, quindi in
condizioni normali non serve lanciarlo a mano. Il numero di build
(`CFBundleVersion`) è ricavato dalla versione con `maggiore·10000 +
minore·100 + patch`: cresce sempre e non può divergere da quello visibile.

Per pubblicare una versione nuova:

```sh
echo "0.7.0" > VERSION
./scripts/apply-version.py
git commit -am "Versione 0.7.0"
git tag v0.7.0
git push --follow-tags
```

Il tag e il file `VERSION` devono coincidere: il workflow di rilascio si ferma
se non è così, prima di compilare qualsiasi cosa. Serve a non pubblicare una
release che dichiara una versione e installa un bundle che ne dichiara
un'altra.

---

## Compilazione automatica su GitHub

Due workflow in `.github/workflows`:

| File | Quando | Cosa fa |
|---|---|---|
| `verifiche.yml` | a ogni push e pull request | versione, catalogo, asset, SVG, JavaScript, interfaccia, sintassi degli script. Gira su Linux, non serve l'SDK |
| `rilascio.yml` | su un tag `v*` | compila per macOS e Windows, produce il DMG e allega tutto alla release |

Il rilascio ha bisogno del segreto **`AI_SDK_URL`**: l'indirizzo di un archivio
`.zip` dell'SDK su uno storage tuo o su una release privata, perché l'SDK non
può stare nel repository. Senza quel segreto i lavori di compilazione si
fermano subito con un messaggio, e le verifiche continuano a girare.

Le verifiche controllano anche che l'SDK non sia stato versionato per sbaglio.

---

## Disinstallazione

Su macOS:

```sh
./scripts/uninstall-macos.sh
```

Su Windows:

```powershell
.\scripts\uninstall-windows.ps1
```

Il bundle non viene cancellato ma spostato in `backups/uninstalled`, con data e
ora nel nome: se serve tornare indietro basta reinstallarlo da lì. Riavviare
Illustrator per completare la rimozione. Per togliere anche il gestore su
macOS, basta trascinare **Vector Suite** dal Finder nel Cestino.

---

## Controlli e verifiche

```sh
./scripts/verify.sh                 # verifica completa di build e sorgenti
./scripts/verify-native.sh          # solo il plug-in
./scripts/verify-catalog.py         # coerenza del catalogo dei moduli
./scripts/diagnose-illustrator.sh   # diagnosi non distruttiva dell'installazione
node tests/ui-smoke.js              # prova a freddo dell'interfaccia dell'app
```

`verify.sh` controlla in sequenza:

- struttura, firma e contenuto dell'app e del pacchetto DMG;
- versione allineata fra `VERSION` e i due `Info.plist`;
- presenza e integrità del bundle nativo incorporato;
- validità XML del marchio e delle 25 icone;
- allineamento fra la sorgente grafica e i file generati;
- coerenza del catalogo fra C++, Objective-C, JavaScript e mappa delle risorse;
- sintassi JavaScript e comportamento dell'interfaccia;
- identifier, firma e architetture del plug-in;
- assenza di percorsi personali nei sorgenti.

`diagnose-illustrator.sh` è l'unico che non tocca nulla e si può lanciare in
qualsiasi momento: riporta versione e architetture di Illustrator, identifier e
firma del bundle installato, presenza dell'attributo di quarantena e conteggio
delle icone. Ogni riga problematica è marcata `[!]` e lo script esce con codice
diverso da zero se trova qualcosa.

---

## Struttura della cartella

```text
VERSION           il numero di versione, sorgente unica
Resources/        interfaccia e risorse dell'app di gestione
Resources/icon/   livello del marchio per Icon Composer
Sources/          sorgente dell'app di gestione, solo macOS
native/           sorgente del plug-in: Xcode per macOS, CMake per Windows
scripts/          build, installazione, verifica
tests/            prove automatiche
docs/             mappa dei moduli
.github/          compilazione automatica
sdk/              qui va scaricato l'SDK di Adobe, vedi sdk/README.md
```

Le cartelle `build/` e `backups/` vengono create al primo utilizzo e si possono
cancellare in qualsiasi momento.
