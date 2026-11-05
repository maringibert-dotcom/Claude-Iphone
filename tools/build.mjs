// Génère les fichiers partagés entre l'appli et le plugin.
//   node tools/build.mjs
import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { PARAMS, MOD_DESTS, MOD_SOURCES, DIV_BEATS, DIVS, TABLES } from "./params.mjs";
import { FACTORY, CATEGORIES } from "./presets.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const out = (rel, text) => {
  const p = join(root, rel);
  mkdirSync(dirname(p), { recursive: true });
  writeFileSync(p, text);
  console.log("écrit", rel);
};

// ── validation des presets ──
const byId = Object.fromEntries(PARAMS.map((p) => [p.id, p]));
let errors = 0;
for (const pr of FACTORY) {
  for (const [k, v] of Object.entries(pr.params)) {
    const d = byId[k];
    if (!d) { console.error(`${pr.name}: paramètre inconnu ${k}`); errors++; continue; }
    if (d.type === "choice") {
      if (!d.choices.includes(v)) { console.error(`${pr.name}: ${k}=${v} n'est pas un choix valide`); errors++; }
    } else if (typeof v !== "number" || v < d.min || v > d.max) {
      console.error(`${pr.name}: ${k}=${v} hors bornes [${d.min}, ${d.max}]`); errors++;
    }
  }
  if (pr.macros.length !== 4) { console.error(`${pr.name}: 4 noms de macros attendus`); errors++; }
}
const ids = new Set(FACTORY.map((p) => p.id));
if (ids.size !== FACTORY.length) { console.error("ids de presets dupliqués"); errors++; }
if (errors) process.exit(1);

const spec = {
  format: "epices.masala.params",
  version: 1,
  params: PARAMS,
  modSources: MOD_SOURCES,
  modDests: MOD_DESTS.map(([name, scale]) => ({ name, scale })),
  divs: DIVS,
  divBeats: DIV_BEATS,
  tables: TABLES,
  categories: CATEGORIES,
};
out("shared/params.json", JSON.stringify(spec, null, 1) + "\n");
out("shared/presets/factory.json", JSON.stringify({ format: "epices.masala.bank", version: 1, name: "Usine", presets: FACTORY }, null, 1) + "\n");

// ── en-tête C++ pour le plugin ──
const cid = (id) => id.replace(/\./g, "_");
const typeNum = { float: 0, int: 1, bool: 2, choice: 3 };
const esc = (s) => JSON.stringify(s);
const num = (x) => (Number.isInteger(x) ? x.toFixed(1) : String(x)) + "f";
let h = `// FICHIER GÉNÉRÉ par tools/build.mjs — ne pas éditer à la main.
#pragma once

namespace masala
{
enum class ParamType { Float = 0, Int = 1, Bool = 2, Choice = 3 };

struct ParamDef
{
    const char* id;
    const char* name;
    ParamType type;
    float min, max, def;
    bool logSkew;
    const char* choices; // séparés par '|'
};

namespace PID
{
enum : int
{
${PARAMS.map((p) => `    ${cid(p.id)},`).join("\n")}
    COUNT
};
}

inline const ParamDef kParams[PID::COUNT] = {
${PARAMS.map((p) => `    { ${esc(p.id)}, ${esc(p.name)}, ParamType(${typeNum[p.type]}), ${num(p.min)}, ${num(p.max)}, ${num(p.def)}, ${p.skew === "log"}, ${esc((p.choices || []).join("|"))} },`).join("\n")}
};

constexpr int kNumModSlots = ${PARAMS.filter((p) => /^mod\d+\.src$/.test(p.id)).length};
constexpr int kNumModSources = ${MOD_SOURCES.length};
constexpr int kNumModDests = ${MOD_DESTS.length};
inline const float kModDestScale[kNumModDests] = { ${MOD_DESTS.map((d) => num(d[1])).join(", ")} };

namespace Dest
{
enum : int
{
${MOD_DESTS.map(([n]) => `    ${n.replace(/[^A-Za-z0-9]/g, "")},`).join("\n")}
};
}
namespace Src
{
enum : int
{
${MOD_SOURCES.map((n) => `    ${n.replace(/[^A-Za-z0-9]/g, "")},`).join("\n")}
};
}

constexpr int kNumDivs = ${DIVS.length};
inline const float kDivBeats[kNumDivs] = { ${DIV_BEATS.map((x) => num(+x.toFixed(6))).join(", ")} };
constexpr int kNumTables = ${TABLES.length};
} // namespace masala
`;
out("plugin/Source/ParamDefs.h", h);
console.log(`${PARAMS.length} paramètres, ${FACTORY.length} presets`);
