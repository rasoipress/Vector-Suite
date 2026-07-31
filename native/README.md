# Vector Suite Native

`VectorSuiteNative` è il core C++/Objective-C++ della suite per Adobe
Illustrator 2026.

Il plug-in registra:

- un pannello AppKit nativo accessibile da `Finestra > Vector Suite`;
- un solo gruppo Vector Suite nella toolbar;
- 25 strumenti inseribili anche singolarmente nelle toolbar personalizzate;
- 22 moduli con nomi, categorie e descrizioni condivisi con l’app standalone;
- il pannello parametrico Fractal Grove con 12 slider e tutte le opzioni di
  generazione, persistenti tra le sessioni.

L’interfaccia non usa CEP. Colori, controlli e icone seguono il tema di sistema;
gli SVG monocromatici sono trattati come template.

## Sorgenti principali

```text
VectorSuiteNative/Source/VectorSuiteCatalog.cpp
VectorSuiteNative/Source/VectorSuiteModules.cpp
VectorSuiteNative/Source/VectorSuiteGeometry.cpp
VectorSuiteNative/Source/VectorSuitePanel.mm
```

## Build

Dal root del progetto:

```sh
./scripts/build-native.sh
./scripts/verify-native.sh
```

Output:

```text
build/native/release/VectorSuiteNative.aip
```
