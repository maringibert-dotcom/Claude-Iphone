// Compare le moteur JS (appli) et le moteur C++ (plugin) sur tous les presets d'usine.
//   node tests/parity.mjs   (compile tests/parity.cpp avec g++)
import { readFileSync, mkdirSync } from "node:fs";
import { execFileSync } from "node:child_process";
import vm from "node:vm";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const bin = join(root, "tests/out/parity");
mkdirSync(join(root, "tests/out"), { recursive: true });
execFileSync("g++", ["-O2", "-std=c++17", "-o", bin, join(root, "tests/parity.cpp"), join(root, "plugin/Source/dsp/Engine.cpp")], { stdio: "inherit" });

const ctx = { Math, Float32Array, Float64Array, Int32Array, Array, Object, console };
ctx.globalThis = ctx;
vm.createContext(ctx);
vm.runInContext(readFileSync(join(root, "app/js/dsp.js"), "utf8"), ctx);
const { Engine } = ctx.MasalaDSP;
const spec = JSON.parse(readFileSync(join(root, "shared/params.json"), "utf8"));
const bank = JSON.parse(readFileSync(join(root, "shared/presets/factory.json"), "utf8"));

const sr = 48000, N = sr * 2;
const events = [[0, "on", 36, 0.9], [28800, "off", 36, 0], [38400, "on", 41, 0.7], [57600, "on", 43, 0.8], [76800, "off", 41, 0], [86400, "off", 43, 0]];
let worst = 0, fails = 0;
for (const pr of bank.presets) {
  const e = new Engine(spec, sr, 1234);
  const vals = spec.params.map((d) => {
    let v = pr.params[d.id] ?? d.def;
    if (d.type === "choice" && typeof v === "string") v = d.choices.indexOf(v);
    return Math.fround(v);
  });
  vals.forEach((v, i) => (e.p[i] = v));
  const L = new Float32Array(N), R = new Float32Array(N);
  let k = 0;
  for (let off = 0; off < N; off += 128) {
    while (k < events.length && events[k][0] <= off) { const [, w, n, v] = events[k++]; w === "on" ? e.noteOn(n, v) : e.noteOff(n); }
    e.process(L.subarray(off, off + 128), R.subarray(off, off + 128), Math.min(128, N - off));
  }
  const input = `${sr} ${N}\n${vals.join(" ")}\n${events.map((x) => x.join(" ")).join("\n")}\n`;
  const out = execFileSync(bin, { input, maxBuffer: N * 8 + 1024 });
  const f = new Float32Array(out.buffer, out.byteOffset, N * 2);
  let maxDiff = 0, energy = 0;
  for (let i = 0; i < N; i++) {
    maxDiff = Math.max(maxDiff, Math.abs(f[2 * i] - L[i]), Math.abs(f[2 * i + 1] - R[i]));
    energy = Math.max(energy, Math.abs(L[i]));
  }
  const ok = maxDiff < 1e-3;
  if (!ok) fails++;
  worst = Math.max(worst, maxDiff);
  console.log(`${ok ? "ok " : "ERR"} ${pr.name.padEnd(20)} écart max ${maxDiff.toExponential(2)} (crête ${energy.toFixed(2)})`);
}
console.log(`écart max global : ${worst.toExponential(2)}`);
process.exit(fails ? 1 : 0);
