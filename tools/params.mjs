// Source unique de vérité des paramètres du synthé MASALA.
// `node tools/build.mjs` génère shared/params.json (lu par l'appli)
// et plugin/Source/ParamDefs.h (lu par le plugin VST3/AU).

export const TABLES = ["Basic", "Growl", "Vowel", "Metal", "Tribal", "Digital"];
export const WARP_MODES = ["Bend", "Sync", "Mirror"];
export const FILTER_TYPES = ["LP24", "LP12", "BP", "HP", "Notch"];
export const LFO_SHAPES = ["Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H"];
export const DIVS = ["4/1", "2/1", "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/8D", "1/16", "1/16T", "1/32"];
export const DIV_BEATS = [16, 8, 4, 2, 1, 2 / 3, 0.5, 1 / 3, 0.75, 0.25, 1 / 6, 0.125];
export const MOD_SOURCES = ["Off", "LFO 1", "LFO 2", "Env 2", "Velocity", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Mod Wheel", "Note"];
// [nom, échelle] — valeur modulée = base + amt * source * échelle (unités du paramètre de base)
export const MOD_DESTS = [
  ["Off", 0],
  ["Cutoff", 96],          // demi-tons
  ["Resonance", 1],
  ["Osc A Pos", 1],
  ["Osc B Pos", 1],
  ["Osc A Pitch", 24],     // demi-tons
  ["Osc B Pitch", 24],
  ["FM Amount", 1],
  ["Filter Drive", 1],
  ["Osc A Warp", 1],
  ["Osc B Warp", 1],
  ["Osc A Level", 1],
  ["Osc B Level", 1],
  ["Sub Level", 1],
  ["Noise Level", 1],
  ["Amp", 1],
  ["LFO 1 Rate", 4],       // octaves
  ["LFO 2 Rate", 4],
  ["Detune", 1],
  ["Dist Drive", 1],       // global (voix la plus récente)
  ["Delay Mix", 1],
  ["Reverb Mix", 1],
];
export const MOD_SLOTS = 8;

const P = [];
const f = (id, name, min, max, def, extra = {}) => P.push({ id, name, type: "float", min, max, def, ...extra });
const i = (id, name, min, max, def, extra = {}) => P.push({ id, name, type: "int", min, max, def, ...extra });
const b = (id, name, def) => P.push({ id, name, type: "bool", min: 0, max: 1, def });
const c = (id, name, choices, def) => P.push({ id, name, type: "choice", min: 0, max: choices.length - 1, def, choices });

for (const o of ["A", "B"]) {
  const p = `osc${o}.`;
  b(p + "on", `Osc ${o} On`, o === "A" ? 1 : 0);
  c(p + "table", `Osc ${o} Table`, TABLES, 0);
  f(p + "pos", `Osc ${o} Position`, 0, 1, o === "A" ? 0.66 : 0.33);
  i(p + "semi", `Osc ${o} Semi`, -24, 24, 0, { unit: "st" });
  f(p + "fine", `Osc ${o} Fine`, -100, 100, 0, { unit: "ct" });
  f(p + "level", `Osc ${o} Level`, 0, 1, 0.75);
  i(p + "unison", `Osc ${o} Unison`, 1, 7, 1);
  f(p + "detune", `Osc ${o} Detune`, 0, 1, 0.2);
  f(p + "spread", `Osc ${o} Spread`, 0, 1, 0.6);
  c(p + "warpMode", `Osc ${o} Warp Mode`, WARP_MODES, 0);
  f(p + "warp", `Osc ${o} Warp`, 0, 1, 0);
}
b("sub.on", "Sub On", 1);
f("sub.level", "Sub Level", 0, 1, 0.6);
c("sub.oct", "Sub Octave", ["-1", "-2"], 0);
c("sub.shape", "Sub Shape", ["Sine", "Triangle", "Square"], 0);
f("noise.level", "Noise Level", 0, 1, 0);
f("fm.amount", "FM B>A", 0, 1, 0);

b("filter.on", "Filter On", 1);
c("filter.type", "Filter Type", FILTER_TYPES, 0);
f("filter.cutoff", "Cutoff", 20, 20000, 8000, { skew: "log", unit: "Hz" });
f("filter.res", "Resonance", 0, 1, 0.15);
f("filter.drive", "Filter Drive", 0, 1, 0);
f("filter.env", "Filter Env", -1, 1, 0);
f("filter.keytrack", "Keytrack", 0, 1, 0);

for (const e of ["env1", "env2"]) {
  const n = e === "env1" ? "Amp" : "Mod";
  f(e + ".a", `${n} Attack`, 0.0005, 10, 0.002, { skew: "log", unit: "s" });
  f(e + ".d", `${n} Decay`, 0.001, 10, 0.4, { skew: "log", unit: "s" });
  f(e + ".s", `${n} Sustain`, 0, 1, e === "env1" ? 1 : 0);
  f(e + ".r", `${n} Release`, 0.001, 10, 0.12, { skew: "log", unit: "s" });
}
for (const l of ["lfo1", "lfo2"]) {
  const n = l.toUpperCase().replace("LFO", "LFO ");
  c(l + ".shape", `${n} Shape`, LFO_SHAPES, 0);
  b(l + ".sync", `${n} Sync`, 1);
  f(l + ".rate", `${n} Rate`, 0.01, 30, 2, { skew: "log", unit: "Hz" });
  c(l + ".div", `${n} Division`, DIVS, 6);
  b(l + ".retrig", `${n} Retrig`, 1);
}
for (let m = 1; m <= 4; m++) f(`macro${m}`, `Macro ${m}`, 0, 1, 0);
for (let s = 1; s <= MOD_SLOTS; s++) {
  c(`mod${s}.src`, `Mod ${s} Source`, MOD_SOURCES, 0);
  c(`mod${s}.dst`, `Mod ${s} Dest`, MOD_DESTS.map((d) => d[0]), 0);
  f(`mod${s}.amt`, `Mod ${s} Amount`, -1, 1, 0);
}
i("global.poly", "Voices", 1, 8, 1);
f("global.glide", "Glide", 0, 1, 0, { unit: "s" });
b("global.legato", "Legato", 1);
i("global.bend", "Bend Range", 0, 24, 2, { unit: "st" });
f("global.volume", "Volume", 0, 1, 0.7);

b("fx.dist.on", "Dist On", 0);
c("fx.dist.type", "Dist Type", ["Tube", "Hard", "Fold", "Crush", "Rectify"], 0);
f("fx.dist.drive", "Dist Drive", 0, 1, 0.3);
f("fx.dist.mix", "Dist Mix", 0, 1, 1);
b("fx.comp.on", "Squash On", 0);
f("fx.comp.amount", "Squash", 0, 1, 0.5);
f("fx.comp.mix", "Squash Mix", 0, 1, 1);
b("fx.delay.on", "Delay On", 0);
c("fx.delay.div", "Delay Time", DIVS, 8);
f("fx.delay.fb", "Delay Feedback", 0, 0.95, 0.35);
f("fx.delay.mix", "Delay Mix", 0, 1, 0.25);
b("fx.delay.pingpong", "Ping-Pong", 1);
b("fx.reverb.on", "Reverb On", 0);
f("fx.reverb.size", "Reverb Size", 0, 1, 0.6);
f("fx.reverb.damp", "Reverb Damp", 0, 1, 0.5);
f("fx.reverb.mix", "Reverb Mix", 0, 1, 0.2);

export const PARAMS = P;
