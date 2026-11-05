// Rend chaque preset d'usine hors-ligne avec le moteur de l'appli et vérifie
// l'absence de NaN, un niveau audible et l'absence de saturation permanente.
//   node tests/render.mjs [--wav]   (--wav écrit tests/out/*.wav)
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import vm from "node:vm";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const ctx = { Math, Float32Array, Float64Array, Int32Array, Array, Object, console };
ctx.globalThis = ctx;
vm.createContext(ctx);
vm.runInContext(readFileSync(join(root, "app/js/dsp.js"), "utf8"), ctx);
const { Engine } = ctx.MasalaDSP;
const spec = JSON.parse(readFileSync(join(root, "shared/params.json"), "utf8"));
const bank = JSON.parse(readFileSync(join(root, "shared/presets/factory.json"), "utf8"));
const wav = process.argv.includes("--wav");

const sr = 44100;
let t0 = Date.now();
const eng = new Engine(spec, sr, 1234);
console.log(`tables construites en ${Date.now() - t0} ms`);

function load(e, preset) {
  spec.params.forEach((d, i) => {
    let v = preset.params[d.id] ?? d.def;
    if (d.type === "choice" && typeof v === "string") v = d.choices.indexOf(v);
    if (typeof v === "boolean") v = v ? 1 : 0;
    e.p[i] = v;
  });
}

function writeWav(path, L, R) {
  const n = L.length, buf = Buffer.alloc(44 + n * 4);
  buf.write("RIFF", 0); buf.writeUInt32LE(36 + n * 4, 4); buf.write("WAVEfmt ", 8);
  buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22);
  buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34);
  buf.write("data", 36); buf.writeUInt32LE(n * 4, 40);
  for (let i = 0; i < n; i++) {
    buf.writeInt16LE(Math.round(Math.max(-1, Math.min(1, L[i])) * 32767), 44 + i * 4);
    buf.writeInt16LE(Math.round(Math.max(-1, Math.min(1, R[i])) * 32767), 46 + i * 4);
  }
  writeFileSync(path, buf);
}

let fail = 0;
if (wav) mkdirSync(join(root, "tests/out"), { recursive: true });
for (const pr of bank.presets) {
  const e = new Engine(spec, sr, 1234);
  load(e, pr);
  const secs = 3, N = sr * secs, L = new Float32Array(N), R = new Float32Array(N);
  const block = 128;
  const notes = [[0, 36], [0.75, 36], [1.5, 39], [2.0, 34]];
  t0 = Date.now();
  for (let off = 0, b = 0; off < N; off += block, b++) {
    const t = off / sr;
    for (const [nt, note] of notes) {
      if (Math.abs(t - nt) < block / sr / 2 + 1e-9 && t >= nt - 1e-9 && t < nt + block / sr) e.noteOn(note, 0.9);
      if (t >= nt + 0.6 && t < nt + 0.6 + block / sr) e.noteOff(note);
    }
    e.process(L.subarray(off, off + block), R.subarray(off, off + block), Math.min(block, N - off));
  }
  const ms = Date.now() - t0;
  let peak = 0, rms = 0, nan = false, clip = 0;
  for (let i = 0; i < N; i++) {
    const x = L[i];
    if (!Number.isFinite(x) || !Number.isFinite(R[i])) nan = true;
    peak = Math.max(peak, Math.abs(x));
    rms += x * x;
    if (Math.abs(x) >= 0.999) clip++;
  }
  rms = Math.sqrt(rms / N);
  const ok = !nan && rms > 0.01 && clip / N < 0.2;
  if (!ok) fail++;
  console.log(`${ok ? "ok " : "ERR"} ${pr.name.padEnd(20)} peak ${peak.toFixed(2)} rms ${rms.toFixed(3)} clip ${(100 * clip / N).toFixed(1)}%  cpu ${(ms / (secs * 10)).toFixed(1)}%`);
  if (wav) writeWav(join(root, "tests/out", pr.id + ".wav"), L, R);
}
process.exit(fail ? 1 : 0);
