(function () {
  "use strict";

  // Le icone arrivano da icons.js, generato da scripts/generate-assets.py:
  // la stessa geometria che finisce nel bundle del plug-in. Nessun disegno
  // vive in questo file, così le due interfacce non possono divergere.
  var ICONS = window.VSIcons || {};
  var UI = window.VSUIIcons || {};
  var GRID = window.VSGrid || 24;
  var MARK_BOX = window.VSMarkBox || 100;

  var categories = [
    { id: "all", name: "Tutti", icon: "all", eyebrow: "Suite completa" },
    { id: "draw", name: "Disegno", icon: "draw", eyebrow: "Strumenti di tracciamento" },
    { id: "geometry", name: "Geometria", icon: "geometry", eyebrow: "Costruzione e trasformazione" },
    { id: "appearance", name: "Aspetto", icon: "appearance", eyebrow: "Colore, texture, effetti" },
    { id: "workflow", name: "Workflow", icon: "workflow", eyebrow: "Ricerca, pulizia, backup" },
    { id: "system", name: "Sistema", icon: "system", eyebrow: "Pannello e preferenze" }
  ];

  // `id` è anche la chiave dell'icona e del modulo nel catalogo nativo.
  var modules = [
    module("precision-pen", "Precision Pen", "module-01", "draw",
      "Creazione precisa e modifica controllata dei tracciati.",
      ["Penna di precisione", "Vincoli di distanza", "Punti e maniglie"]),
    module("fluid-sketch", "Fluid Sketch", "module-02", "draw",
      "Disegno vettoriale naturale con mouse o penna.",
      ["Smoothing dinamico", "Modifica del tratto", "Input sensibile al gesto"]),
    module("ink-studio", "Ink Studio", "module-03", "draw",
      "Tratti calligrafici e simulazione dell’inchiostro.",
      ["Pennini espressivi", "Pressione e velocità", "Lettering vettoriale"]),
    module("width-studio", "Width Studio", "module-04", "draw",
      "Controllo avanzato dei profili di larghezza.",
      ["Width Brush", "Profili variabili", "Regolazione locale"]),
    module("path-studio", "Path Studio", "module-05", "geometry",
      "Costruzione, pulizia e modifica intelligente delle forme.",
      ["Path editing", "Angoli dinamici", "Forme e misure", "Semplificazione"]),
    module("geometry-lab", "Geometry Lab", "module-06", "geometry",
      "Costruzioni geometriche, tangenti e allineamenti.",
      ["Tangenti", "Cerchi e archi", "Orientamento", "Trim e join"]),
    module("collision-align", "Collision Align", "module-07", "geometry",
      "Posizionamento e allineamento tramite collisioni.",
      ["Collisione oggetti", "Snap dinamico", "Rotazione in contatto"]),
    module("mirror-studio", "Mirror Studio", "module-08", "geometry",
      "Simmetria radiale e speculare in tempo reale.",
      ["Assi multipli", "Anteprima live", "Simmetria su selezione o livello"]),
    module("shape-reform", "Shape Reform", "module-09", "geometry",
      "Rimodellazione diretta e intuitiva della grafica.",
      ["Sculpt", "Reprofile", "Editing visivo delle curve"]),
    module("live-style", "Live Style", "module-10", "appearance",
      "Controllo visuale degli effetti e dell’aspetto.",
      ["Widget sul canvas", "Effetti live", "Modifica dell’aspetto"]),
    module("color-lab", "Color Lab", "module-11", "appearance",
      "Correzione colore ed effetti mezzatinta.",
      ["Curve e livelli", "Tonalità e saturazione", "Halftone vettoriale"]),
    module("texture-lab", "Texture Lab", "module-12", "appearance",
      "Texture vettoriali e raster controllabili sul canvas.",
      ["Texture Brush", "Opacity Brush", "Posizionamento interattivo"]),
    module("stipple-lab", "Stipple Lab", "module-13", "appearance",
      "Puntinatura e simboli distribuiti in modo parametrico.",
      ["Stipple live", "Densità e scala", "Symbol stipple"]),
    module("randomize", "Randomize", "module-14", "appearance",
      "Variazioni controllate e casuali degli oggetti.",
      ["Posizione e rotazione", "Scala e opacità", "Colori casuali"]),
    module("smart-find", "Smart Find", "module-15", "workflow",
      "Ricerca, selezione e sostituzione di oggetti.",
      ["Ricerca per attributi", "Selezione mirata", "Sostituzione grafica"]),
    module("vector-repair", "Vector Repair", "module-16", "workflow",
      "Diagnosi e riparazione dei documenti vettoriali.",
      ["Pulizia tracciati", "Riduzione punti", "Controlli documento"]),
    module("raster-lab", "Raster Lab", "module-17", "workflow",
      "Controllo, ritaglio e gestione delle immagini raster.",
      ["Crop immagini", "Risoluzione", "Ricollegamento e incorporamento"]),
    module("auto-save", "Auto Save", "module-18", "workflow",
      "Salvataggio automatico, backup e promemoria.",
      ["Autosave", "Backup versionati", "Promemoria temporizzati"]),
    module("direct-settings", "Direct Settings", "module-19", "system",
      "Preferenze Illustrator essenziali sempre accessibili.",
      ["Snap e tolleranze", "Guide e maniglie", "Accesso rapido"]),
    module("suite-core", "Suite Core", "module-20", "system",
      "Pannello e servizi condivisi dei moduli Vector Suite.",
      ["Pannello nativo unico", "Preferenze condivise", "Coordinamento moduli"]),
    module("projection-studio", "Projection Studio", "module-21", "geometry",
      "Costruzioni assonometriche e proiezioni vettoriali.",
      ["Linee assonometriche", "Rettangoli ed ellissi proiettati", "Solidi isometrici"]),
    module("fractal-grove", "Fractal Grove", "module-22", "geometry",
      "Alberi vettoriali frattali con ramificazione parametrica.",
      ["Dodici parametri numerici", "Seme, ondulazione e casualità",
       "Auto, avanzato, gruppo e sostituzione"])
  ];

  var state = {
    category: "all",
    selected: "precision-pen",
    query: "",
    scans: {},
    directory: "",
    directorySource: "",
    extras: []
  };

  var grid = document.getElementById("moduleGrid");
  var categoryRoot = document.getElementById("categories");
  var detail = document.getElementById("detailPanel");
  var search = document.getElementById("searchInput");

  function module(id, name, slot, category, summary, capabilities) {
    return {
      id: id,
      name: name,
      slot: slot,
      category: category,
      summary: summary,
      capabilities: capabilities
    };
  }

  function glyph(body, box) {
    var size = box || GRID;
    return '<svg class="glyph" viewBox="0 0 ' + size + " " + size +
      '" aria-hidden="true">' + body + "</svg>";
  }

  function icon(key) {
    return glyph(ICONS[key] || UI.all || "");
  }

  function uiIcon(key) {
    return glyph(UI[key] || "");
  }

  function escapeHtml(value) {
    return String(value === undefined || value === null ? "" : value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function categoryOf(id) {
    return categories.filter(function (entry) {
      return entry.id === id;
    })[0] || categories[0];
  }

  function countIn(id) {
    return id === "all" ? modules.length : modules.filter(function (item) {
      return item.category === id;
    }).length;
  }

  function renderChrome() {
    document.getElementById("brandMark").innerHTML =
      '<svg viewBox="0 0 ' + MARK_BOX + " " + MARK_BOX + '" aria-hidden="true">' +
      (window.VSMark || "") + "</svg>";
    document.getElementById("searchIcon").innerHTML = uiIcon("search");
    document.getElementById("rescanButton").innerHTML = uiIcon("rescan");
    document.getElementById("emptyMark").innerHTML = uiIcon("empty");
  }

  function renderCategories() {
    categoryRoot.innerHTML = categories.map(function (category) {
      return '<button class="category-button' +
        (state.category === category.id ? " active" : "") +
        '" data-category="' + category.id + '">' + uiIcon(category.icon) +
        "<span>" + category.name + '</span><span class="count">' +
        countIn(category.id) + "</span></button>";
    }).join("");
  }

  function scanFor(item) {
    return state.scans[item.slot] || { detected: false, issues: [] };
  }

  /** Tre stati distinti: assente, rilevato con avvisi, rilevato e integro. */
  function statusFor(scan) {
    if (!scan.detected) return { key: "missing", label: "Assente" };
    if (scan.issues && scan.issues.length > 0) return { key: "warning", label: "Avviso" };
    return { key: "ready", label: "Rilevato" };
  }

  function visibleModules() {
    var query = state.query.toLowerCase();
    return modules.filter(function (item) {
      var matchesCategory = state.category === "all" || item.category === state.category;
      var scan = scanFor(item);
      var haystack = (item.name + " " + item.summary + " " +
        item.capabilities.join(" ") + " " + (scan.bundleName || "") + " " +
        (scan.expectedBundle || "")).toLowerCase();
      return matchesCategory && (!query || haystack.indexOf(query) !== -1);
    });
  }

  function renderModules() {
    var items = visibleModules();
    grid.innerHTML = items.map(function (item) {
      var scan = scanFor(item);
      var status = statusFor(scan);
      var footer = scan.detected
        ? (scan.bundleName || scan.expectedBundle || "") +
          (scan.version ? " " + scan.version : "")
        : (scan.expectedBundle || item.slot.toUpperCase());
      return '<button class="module-card' +
        (state.selected === item.id ? " selected" : "") +
        '" data-module="' + item.id + '">' +
        '<span class="module-top"><span class="tile">' + icon(item.id) +
        '</span><span class="module-state"><i class="' + status.key + '"></i>' +
        status.label + "</span></span>" +
        '<span class="card-title">' + escapeHtml(item.name) + "</span>" +
        '<span class="card-summary">' + escapeHtml(item.summary) + "</span>" +
        '<span class="module-version">' + escapeHtml(footer) + "</span></button>";
    }).join("");

    document.getElementById("emptyState").hidden = items.length !== 0;

    var category = categoryOf(state.category);
    document.getElementById("pageEyebrow").textContent = category.eyebrow;
    document.getElementById("pageTitle").textContent = category.name;
    document.getElementById("pageSubtitle").textContent =
      items.length + (items.length === 1 ? " modulo" : " moduli") +
      (state.query ? " per “" + state.query + "”" : "");
  }

  function metadata(label, value) {
    return "<div><span>" + escapeHtml(label) + "</span><strong>" +
      escapeHtml(value || "—") + "</strong></div>";
  }

  function renderDetail() {
    var item = modules.filter(function (entry) {
      return entry.id === state.selected;
    })[0] || modules[0];
    var scan = scanFor(item);
    var status = statusFor(scan);
    var category = categoryOf(item.category);

    var issues = (scan.issues || []).map(function (issue) {
      return "<li>" + escapeHtml(issue) + "</li>";
    }).join("");

    var html = '<div class="detail-inner">' +
      '<div class="detail-top"><span class="tile">' + icon(item.id) +
      '</span><span class="pill">' + escapeHtml(category.name) + "</span></div>" +
      "<h2>" + escapeHtml(item.name) + "</h2>" +
      '<p class="detail-summary">' + escapeHtml(item.summary) + "</p>" +
      '<div class="rule"></div>' +
      '<section class="detail-section"><p class="eyebrow">Funzioni</p>' +
      '<ul class="capability-list">' +
      item.capabilities.map(function (capability) {
        return "<li>" + uiIcon("check") + "<span>" + escapeHtml(capability) +
          "</span></li>";
      }).join("") + "</ul></section>" +
      '<section class="detail-section"><p class="eyebrow">Bundle</p>' +
      '<div class="metadata">' +
      metadata("Stato", status.label) +
      metadata("Bundle atteso", scan.expectedBundle ? scan.expectedBundle + ".aip" : "—") +
      metadata("Identifier", scan.identifier || scan.expectedIdentifier) +
      metadata("Versione", scan.version) +
      metadata("Illustrator", scan.hostVersion ? "build " + scan.hostVersion : "—") +
      metadata("Architetture", (scan.architectures || []).join(" · ")) +
      metadata("Firma", scan.detected ? (scan.signed ? "presente" : "assente") : "—") +
      metadata("Percorso", scan.path) +
      "</div></section>";

    if (issues) {
      html += '<section class="detail-section"><p class="eyebrow">Da risolvere</p>' +
        '<ul class="issue-list">' + issues + "</ul></section>";
    }

    html += '<button class="primary" data-native="launchIllustrator">Apri Illustrator</button>' +
      '<button class="secondary" data-native="installIntoIllustrator">Installa in Illustrator…</button>' +
      (scan.detected
        ? '<button class="secondary" data-reveal="' + item.slot + '">Mostra nel Finder</button>'
        : "") +
      '<p class="note">Versione, identifier, architettura e firma sono letti dal ' +
      'bundle sul disco. I 22 moduli appartengono allo stesso bundle nativo e ' +
      'vengono caricati da Illustrator all’avvio.</p></div>';

    detail.innerHTML = html;
  }

  function postNative(action, extra) {
    var payload = extra || {};
    payload.action = action;
    if (window.webkit && window.webkit.messageHandlers &&
        window.webkit.messageHandlers.native) {
      window.webkit.messageHandlers.native.postMessage(payload);
    }
  }

  function shortenPath(path) {
    var parts = String(path).split("/").filter(Boolean);
    return parts.length <= 2 ? path : ".../" + parts.slice(-2).join("/");
  }

  function updateGlobalStatus() {
    var ready = 0;
    var warned = 0;
    modules.forEach(function (item) {
      var status = statusFor(scanFor(item));
      if (status.key === "ready") ready += 1;
      if (status.key === "warning") warned += 1;
    });

    var label = (ready + warned) + " di " + modules.length + " rilevati";
    if (warned > 0) label += " · " + warned + " con avvisi";
    document.getElementById("detectedLabel").textContent = label;
    document.getElementById("globalDot").className =
      ready === modules.length ? "complete" : (ready + warned > 0 ? "partial" : "");

    var source = document.getElementById("sourcePath");
    var directory = state.directory || "Cartella non trovata";
    source.textContent = state.directorySource
      ? state.directorySource + " · " + shortenPath(directory)
      : shortenPath(directory);
    source.title = directory;
  }

  /**
   * Riceve il risultato della scansione nativa. Accetta sia il payload
   * completo sia il vecchio formato ad array, per non rompere build
   * disallineate.
   */
  window.receiveScan = function (payload) {
    var records = [];
    if (Array.isArray(payload)) {
      records = payload;
    } else if (payload && typeof payload === "object") {
      records = payload.modules || [];
      state.directory = payload.directory || "";
      state.directorySource = payload.directorySource || "";
      state.extras = payload.extras || [];
    }

    state.scans = {};
    records.forEach(function (record) {
      if (record && record.slot) state.scans[record.slot] = record;
    });

    updateGlobalStatus();
    renderModules();
    renderDetail();
  };

  window.selectModule = function (identifier) {
    var match = modules.filter(function (entry) {
      return entry.id === identifier || entry.slot === identifier;
    })[0];
    if (!match) return;
    state.category = "all";
    state.query = "";
    search.value = "";
    state.selected = match.id;
    renderCategories();
    renderModules();
    renderDetail();
  };

  categoryRoot.addEventListener("click", function (event) {
    var button = event.target.closest("[data-category]");
    if (!button) return;
    state.category = button.getAttribute("data-category");
    renderCategories();
    renderModules();
  });

  grid.addEventListener("click", function (event) {
    var card = event.target.closest("[data-module]");
    if (!card) return;
    state.selected = card.getAttribute("data-module");
    renderModules();
    renderDetail();
  });

  document.addEventListener("click", function (event) {
    var nativeButton = event.target.closest("[data-native]");
    var revealButton = event.target.closest("[data-reveal]");
    if (nativeButton) postNative(nativeButton.getAttribute("data-native"));
    if (revealButton) postNative("reveal", { slot: revealButton.getAttribute("data-reveal") });
  });

  search.addEventListener("input", function () {
    state.query = search.value.trim();
    renderModules();
  });

  // Cmd+F porta il fuoco sulla ricerca, Esc la svuota: le due scorciatoie che
  // un utente macOS si aspetta in una libreria.
  document.addEventListener("keydown", function (event) {
    if (event.metaKey && event.key === "f") {
      event.preventDefault();
      search.focus();
      search.select();
    } else if (event.key === "Escape" && state.query) {
      search.value = "";
      state.query = "";
      renderModules();
    }
  });

  renderChrome();
  renderCategories();
  renderModules();
  renderDetail();
  updateGlobalStatus();
  postNative("ready");
}());
