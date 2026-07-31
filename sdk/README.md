# Adobe Illustrator SDK

**L'SDK non è versionato e non viene distribuito: va scaricato da Adobe.**

L'SDK di Illustrator è coperto da una licenza d'uso Adobe che non ne permette la
ridistribuzione. Per questo `.gitignore` esclude tutto il contenuto di questa
cartella tranne il file che stai leggendo: se cloni il repository la trovi
vuota, ed è normale. Senza SDK il plug-in non si compila, mentre l'app di
gestione e tutte le verifiche funzionano lo stesso.

## Come scaricarlo

1. Vai su **[Adobe Developer Console — Illustrator](https://developer.adobe.com/console/servicesandapis/ai)**
   e accedi con un Adobe ID (l'account gratuito è sufficiente).
2. In alternativa parti da **[Download SDKs](https://developer.adobe.com/console/downloads)**
   e scegli Illustrator, oppure dalla pagina
   **[Illustrator per sviluppatori](https://developer.adobe.com/illustrator/)**.
3. Scarica l'SDK della versione che ti serve, **Adobe Illustrator 2026 SDK**.
4. Scompatta l'archivio e mettilo qui dentro, così:

```text
sdk/
└── Adobe Illustrator 2026 SDK/
    ├── illustratorapi/
    ├── samplecode/
    └── ...
```

Il nome della cartella conta: gli xcconfig e il progetto Xcode cercano
esattamente `sdk/Adobe Illustrator 2026 SDK`. Se usi una versione diversa
dell'SDK, rinomina la cartella oppure aggiorna il percorso in
`native/VectorSuiteNative/Config/VectorSuiteNativeCommon.xcconfig` e in
`native/VectorSuiteNative/CMakeLists.txt`.

## Per la compilazione automatica

Se usi GitHub Actions, l'SDK non può stare nel repository per lo stesso motivo.
Il workflow lo scarica da un indirizzo che fornisci tu nel segreto
`AI_SDK_URL`: può essere un archivio su uno storage privato, o un asset di una
release privata. Se il segreto non c'è, i lavori di compilazione si saltano da
soli e restano solo le verifiche.

Il percorso dentro l'archivio deve essere lo stesso indicato sopra.
