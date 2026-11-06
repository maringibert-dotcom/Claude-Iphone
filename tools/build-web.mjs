// Prépare le dossier www/ embarqué dans l'appli iOS (Capacitor).
//   node tools/build-web.mjs            (MASALA_SERVER=https://… pour le serveur du compte)
// - copie app/ et les données partagées (paramètres, presets d'usine)
// - remplace Google Fonts par la police Bricolage Grotesque embarquée (hors-ligne)
// - indique à l'appli où trouver ses données et le serveur du compte
import { cpSync, mkdirSync, readFileSync, rmSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const www = join(root, "www");
const server = (process.env.MASALA_SERVER || "").replace(/\/+$/, "");

rmSync(www, { recursive: true, force: true });
mkdirSync(www, { recursive: true });
cpSync(join(root, "app"), www, { recursive: true, filter: (src) => !src.endsWith("sw.js") });
mkdirSync(join(www, "shared/presets"), { recursive: true });
cpSync(join(root, "shared/params.json"), join(www, "shared/params.json"));
cpSync(join(root, "shared/presets/factory.json"), join(www, "shared/presets/factory.json"));

// police embarquée (licence OFL, fournie avec la DA)
const fontDir = join(root, "node_modules/@fontsource/bricolage-grotesque/files");
if (!existsSync(fontDir)) throw new Error("Lance d'abord `npm install` (police @fontsource/bricolage-grotesque manquante)");
mkdirSync(join(www, "fonts"), { recursive: true });
let css = "";
for (const w of [400, 500, 600, 700, 800]) {
  for (const subset of ["latin", "latin-ext"]) {
    const f = `bricolage-grotesque-${subset}-${w}-normal.woff2`;
    cpSync(join(fontDir, f), join(www, "fonts", f));
    const range = subset === "latin"
      ? "U+0000-00FF,U+0131,U+0152-0153,U+02BB-02BC,U+02C6,U+02DA,U+02DC,U+0304,U+0308,U+0329,U+2000-206F,U+20AC,U+2122,U+2191,U+2193,U+2212,U+2215,U+FEFF,U+FFFD"
      : "U+0100-02BA,U+02BD-02C5,U+02C7-02CC,U+02CE-02D7,U+02DD-02FF,U+0304,U+0308,U+0329,U+1D00-1DBF,U+1E00-1E9F,U+1EF2-1EFF,U+2020,U+20A0-20AB,U+20AD-20C0,U+2113,U+2C60-2C7F,U+A720-A7FF";
    css += `@font-face{font-family:"Bricolage Grotesque";font-style:normal;font-weight:${w};font-display:swap;src:url(${f}) format("woff2");unicode-range:${range}}\n`;
  }
}
writeFileSync(join(www, "fonts/fonts.css"), css);
cpSync(join(root, "app/brand/Police_Bricolage_Grotesque_licence_OFL.txt"), join(www, "fonts/OFL.txt"));

const config = `<script>window.MASALA_DATA = "shared/"; window.MASALA_SERVER = ${JSON.stringify(server)};</script>`;
for (const page of ["index.html", "privacy.html"]) {
  const p = join(www, page);
  let html = readFileSync(p, "utf8");
  html = html
    .replace(/\s*<link rel="preconnect"[^>]*>/g, "")
    .replace(/<link href="https:\/\/fonts\.googleapis\.com[^>]*>/, "")
    .replace(/\s*<link rel="manifest"[^>]*>/, "")
    .replace("</title>", `</title>\n  <link rel="stylesheet" href="fonts/fonts.css">`);
  if (page === "index.html") html = html.replace(/(\s*)<script src="js\/dsp\.js">/, `$1${config}$1<script src="js/dsp.js">`);
  writeFileSync(p, html);
}
console.log(`www/ prêt${server ? " (serveur : " + server + ")" : " (aucun serveur de compte intégré : MASALA_SERVER non défini)"}`);
