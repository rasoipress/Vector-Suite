"use strict";

function randomGenerator(seed) {
  var state = Math.max(1, Math.floor(seed)) >>> 0;
  function next() {
    state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
    return state / 4294967296;
  }
  for (var index = 0; index < 8; index += 1) next();
  return {
    bipolar: function () { return next() * 2 - 1; },
    jitter: function (amount) { return 1 + (next() * 2 - 1) * amount; }
  };
}

function generate(seed) {
  var random = randomGenerator(seed);
  var count = 0;
  var checksum = 0;

  function branch(x, y, angle, length, width, depth) {
    if (count >= 3000 || depth >= 11 || length < 2) return;
    var radians = angle * Math.PI / 180;
    var endX = x + Math.cos(radians) * length;
    var endY = y + Math.sin(radians) * length;
    checksum += endX * 0.17 + endY * 0.31 + width * 0.53 + random.bipolar() * 0.04;
    count += 1;

    var spread = 25 / Math.pow(1, depth);
    var nextWidth = Math.max(0.05, width * 0.72);
    var baseLength = length * 0.75;
    branch(endX, endY, angle + spread * random.jitter(0.15),
      baseLength * random.jitter(0.15), nextWidth, depth + 1);
    branch(endX, endY, angle - spread * random.jitter(0.15),
      baseLength * random.jitter(0.15), nextWidth, depth + 1);
  }

  branch(0, 0, 90, 150, 40, 0);
  return { count: count, checksum: checksum };
}

var first = generate(999);
var repeated = generate(999);
var different = generate(1000);

if (first.count !== 2047) throw new Error("Conteggio predefinito inatteso: " + first.count);
if (first.count !== repeated.count || first.checksum !== repeated.checksum) {
  throw new Error("Il seme non produce un risultato deterministico.");
}
if (first.checksum === different.checksum) {
  throw new Error("Semi differenti producono la stessa geometria.");
}

process.stdout.write("Fractal Grove: OK · " + first.count + " rami deterministici\n");
