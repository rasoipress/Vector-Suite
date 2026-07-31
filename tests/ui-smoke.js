#!/usr/bin/env node
/*
 * Prova a freddo dell'interfaccia dell'app di gestione.
 *
 * Esegue Resources/ui/app.js su un DOM minimo simulato, poi verifica che:
 *   - il codice giri senza eccezioni, sia al primo avvio sia dopo una
 *     scansione ricevuta dal lato nativo;
 *   - ogni frammento HTML prodotto sia ben formato;
 *   - ogni modulo abbia davvero la propria icona in icons.js, con lo stesso
 *     nome della chiave usata dal catalogo nativo;
 *   - gli elementi dichiarati in index.html esistano tutti.
 *
 *     node tests/ui-smoke.js
 */

"use strict";

const fs = require("fs");
const path = require("path");
const vm = require("vm");

const root = path.join(__dirname, "..");
const uiDirectory = path.join(root, "Resources", "ui");
const failures = [];

function check(condition, message) {
  if (!condition) failures.push(message);
}

// --- DOM minimo ----------------------------------------------------------

const elements = new Map();

function element(id) {
  const node = {
    id,
    innerHTML: "",
    textContent: "",
    className: "",
    title: "",
    value: "",
    hidden: false,
    listeners: {},
    addEventListener(type, handler) { this.listeners[type] = handler; },
    focus() {},
    select() {}
  };
  elements.set(id, node);
  return node;
}

const html = fs.readFileSync(path.join(uiDirectory, "index.html"), "utf8");
const declaredIds = [...html.matchAll(/id="([^"]+)"/g)].map((match) => match[1]);
declaredIds.forEach(element);

const context = {
  window: {},
  document: {
    getElementById: (id) => elements.get(id) || null,
    addEventListener() {}
  },
  console
};
context.window.document = context.document;
vm.createContext(context);

function run(file) {
  vm.runInContext(fs.readFileSync(path.join(uiDirectory, file), "utf8"), context, {
    filename: file
  });
}

// --- Esecuzione ----------------------------------------------------------

const requestedIds = new Set();
const originalGet = context.document.getElementById;
context.document.getElementById = (id) => {
  requestedIds.add(id);
  return originalGet(id);
};

try {
  run("icons.js");
  run("app.js");
} catch (error) {
  failures.push("app.js ha sollevato un'eccezione all'avvio: " + error.message);
}

// Ogni identificatore chiesto dal codice deve esistere nel markup.
requestedIds.forEach((id) => {
  check(declaredIds.includes(id), "index.html non contiene l'elemento #" + id);
});

// --- Parità fra moduli e icone ------------------------------------------

const icons = context.window.VSIcons || {};
const uiIcons = context.window.VSUIIcons || {};
const moduleIds = [...fs.readFileSync(path.join(uiDirectory, "app.js"), "utf8")
  .matchAll(/module\("([a-z-]+)"/g)].map((match) => match[1]);

check(moduleIds.length === 22, "app.js dichiara " + moduleIds.length + " moduli invece di 22");
moduleIds.forEach((id) => {
  check(Object.prototype.hasOwnProperty.call(icons, id),
    "icons.js non contiene l'icona del modulo " + id);
});
check(Object.keys(icons).length === 25,
  "icons.js contiene " + Object.keys(icons).length + " icone di modulo invece di 25");
["all", "draw", "geometry", "appearance", "workflow", "system", "search",
 "rescan", "check", "empty"].forEach((key) => {
  check(Object.prototype.hasOwnProperty.call(uiIcons, key),
    "icons.js non contiene l'icona di interfaccia " + key);
});
check(typeof context.window.VSMark === "string" && context.window.VSMark.length > 0,
  "icons.js non contiene il marchio");

// --- Frammenti prodotti --------------------------------------------------

function wellFormed(fragment, label) {
  const open = (fragment.match(/<(?!\/)(?![^>]*\/>)[a-zA-Z]/g) || []).length;
  const close = (fragment.match(/<\//g) || []).length;
  const selfClosing = (fragment.match(/\/>/g) || []).length;
  check(open === close,
    label + ": " + open + " tag aperti contro " + close + " chiusi" +
    " (" + selfClosing + " autochiusi)");
  check(!/undefined|\[object Object\]/.test(fragment),
    label + " contiene un valore non risolto");
}

function snapshot(stage) {
  wellFormed(elements.get("categories").innerHTML, stage + " · categorie");
  wellFormed(elements.get("moduleGrid").innerHTML, stage + " · griglia");
  wellFormed(elements.get("detailPanel").innerHTML, stage + " · dettaglio");
  wellFormed(elements.get("brandMark").innerHTML, stage + " · marchio");
}

snapshot("avvio");

const cards = (elements.get("moduleGrid").innerHTML.match(/class="module-card/g) || []).length;
check(cards === 22, "la griglia mostra " + cards + " schede invece di 22");

// Scansione simulata: un modulo integro, uno con avviso, gli altri assenti.
try {
  context.window.receiveScan({
    directory: "/Applications/Adobe Illustrator 2026/Plug-ins.localized",
    directorySource: "Installazione Adobe Illustrator 2026",
    modules: [
      {
        slot: "module-01", detected: true, issues: [],
        bundleName: "VectorSuiteNative", expectedBundle: "VectorSuiteNative",
        identifier: "studio.vectorsuite.plugin.core", version: "5",
        hostVersion: "30.0", architectures: ["arm64", "x86_64"], signed: true,
        path: "/Applications/Adobe Illustrator 2026/Plug-ins.localized/VectorSuiteNative.aip"
      },
      {
        slot: "module-02", detected: true, expectedBundle: "VectorSuiteNative",
        issues: ["Attributo di quarantena presente."], architectures: ["arm64"]
      }
    ]
  });
} catch (error) {
  failures.push("receiveScan ha sollevato un'eccezione: " + error.message);
}

snapshot("dopo la scansione");
check(/Rilevato/.test(elements.get("moduleGrid").innerHTML),
  "lo stato «Rilevato» non compare dopo la scansione");
check(/Avviso/.test(elements.get("moduleGrid").innerHTML),
  "lo stato «Avviso» non compare dopo la scansione");
check(/2 di 22 rilevati/.test(elements.get("detectedLabel").textContent),
  "il riepilogo laterale non riflette la scansione: " +
  elements.get("detectedLabel").textContent);

// Selezione da menu: deve puntare al modulo giusto e rigenerare il dettaglio.
try {
  context.window.selectModule("module-22");
} catch (error) {
  failures.push("selectModule ha sollevato un'eccezione: " + error.message);
}
check(/Fractal Grove/.test(elements.get("detailPanel").innerHTML),
  "selectModule non ha aperto il dettaglio di Fractal Grove");

// Ricerca senza risultati: la griglia si svuota e appare lo stato vuoto.
const searchField = elements.get("searchInput");
searchField.value = "zzzz";
searchField.listeners.input();
check(elements.get("emptyState").hidden === false,
  "lo stato vuoto non compare quando la ricerca non trova nulla");
searchField.value = "";
searchField.listeners.input();
check(elements.get("emptyState").hidden === true,
  "lo stato vuoto non si nasconde quando la ricerca viene svuotata");

// --- Esito ---------------------------------------------------------------

if (failures.length > 0) {
  failures.forEach((message) => console.error("  [!] " + message));
  console.error("\nProva interfaccia fallita: " + failures.length + " problemi.");
  process.exit(1);
}

console.log("Interfaccia: 22 moduli, 25 icone, marchio, scansione, ricerca e " +
  "selezione da menu verificati.");
