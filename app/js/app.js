// MASALA · epices audio — application principale.
(async function () {
  "use strict";
  const { Store, Knob, Seg, Pick, Toggle, BiSlider, el, toNorm } = window.MasalaUI;
  const Cloud = window.MasalaCloud;
  const $ = (s) => document.querySelector(s);

  // catégories : déclinaisons de l'orange de la marque + le papier
  const CAT_COLORS = {
    "Dubstep": "var(--orange)", "Riddim": "var(--orange-vif)", "Tribe": "var(--peche)",
    "Deep / Dub": "var(--orange-sombre)", "Halftime": "#FFB38A", "Leads & Pads": "var(--papier)",
    "FX": "var(--sable)", "Mes presets": "var(--papier)",
  };

  // ───────── Chargement des données partagées ─────────
  const [spec, bank] = await Promise.all([
    fetch((window.MASALA_DATA || "../shared/") + "params.json").then((r) => r.json()),
    fetch((window.MASALA_DATA || "../shared/") + "presets/factory.json").then((r) => r.json()),
  ]);
  const store = new Store(spec);
  const D = {};
  spec.modDests.forEach((d, i) => (D[d.name] = i));

  // ───────── Audio ─────────
  let ctx = null, node = null;
  const post = (m) => node && node.port.postMessage(m);
  let liveMod = null;
  let dirty = true, modDirty = false; // redessin des écrans

  async function startAudio() {
    if (ctx) { if (ctx.state !== "running") await ctx.resume(); return; }
    try { if (navigator.audioSession) navigator.audioSession.type = "playback"; } catch {} // son même en mode silencieux
    ctx = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: "interactive" });
    // iOS : un buffer silencieux joué pendant le geste débloque la sortie
    const b = ctx.createBuffer(1, 1, ctx.sampleRate), s = ctx.createBufferSource();
    s.buffer = b; s.connect(ctx.destination); s.start();
    await ctx.audioWorklet.addModule("js/dsp.js");
    node = new AudioWorkletNode(ctx, "masala", { numberOfInputs: 0, outputChannelCount: [2], processorOptions: { spec } });
    node.connect(ctx.destination);
    node.port.onmessage = (e) => { if (e.data.type === "meter") { liveMod = e.data.mod; modDirty = true; } };
    post({ type: "params", values: store.values.slice() });
    post({ type: "bpm", value: bpm });
    await ctx.resume();
  }
  document.addEventListener("visibilitychange", () => {
    if (!ctx) return;
    if (document.hidden) post({ type: "allOff" });
    else ctx.resume();
  });

  store.onAny((id, v) => post({ type: "param", id: store.index[id], value: v }));

  // ───────── Tempo ─────────
  let bpm = +(localStorage.getItem("masala.bpm") || 140);
  const bpmEl = $("#bpm-val");
  const setBpm = (v) => {
    bpm = Math.max(60, Math.min(200, v));
    bpmEl.textContent = bpm;
    localStorage.setItem("masala.bpm", bpm);
    post({ type: "bpm", value: bpm });
  };
  setBpm(bpm);
  document.querySelectorAll(".bpm-btn").forEach((b) => {
    let timer = null;
    const step = () => setBpm(bpm + +b.dataset.bpm);
    b.addEventListener("pointerdown", () => { step(); timer = setInterval(step, 120); });
    ["pointerup", "pointerleave", "pointercancel"].forEach((ev) => b.addEventListener(ev, () => clearInterval(timer)));
  });

  // ───────── Toast ─────────
  let toastT = 0;
  function toast(msg) {
    const t = $("#toast");
    t.textContent = msg; t.classList.add("show");
    clearTimeout(toastT); toastT = setTimeout(() => t.classList.remove("show"), 2200);
  }

  // ───────── Construction des pages ─────────
  const knobs = {}; // paramId → knob (pour la visualisation des modulations)
  const K = (id, label, opts) => { const k = Knob(store, id, { label, ...opts }); (knobs[id] ||= []).push(k); return k; };
  const knobRow = (cls, ...ks) => { const r = el("div", "knobs" + (cls ? " " + cls : "")); ks.forEach((k) => r.appendChild(k)); return r; };
  function card(cls, title, toggleId, extra) {
    const c = el("div", "card " + cls);
    const h = el("div", "card-head");
    if (toggleId) {
      h.appendChild(Toggle(store, toggleId));
      store.on(toggleId, (v) => c.classList.toggle("off", !v));
      c.classList.toggle("off", !store.get(toggleId));
    }
    h.appendChild(el("h3", "", title));
    if (extra) h.appendChild(extra);
    c.appendChild(h);
    return c;
  }
  const row = (...items) => { const r = el("div", "row"); items.forEach((i) => r.appendChild(i)); return r; };
  const mini = (t) => el("label", "mini", t);
  const page = (name) => document.querySelector(`.page[data-page="${name}"]`);

  // ── OSC ──
  const waveCanvases = {};
  for (const o of ["A", "B"]) {
    const p = `osc${o}.`;
    const c = card(`osc-${o.toLowerCase()}`, `Osc <em>${o}</em>`, p + "on", Pick(store, p + "table"));
    const cv = el("canvas", "display");
    waveCanvases[o] = cv;
    c.appendChild(cv);
    // glisser sur l'écran = position de la table
    let pid = null, sx = 0, sp = 0;
    cv.addEventListener("pointerdown", (e) => { pid = e.pointerId; cv.setPointerCapture(pid); sx = e.clientX; sp = store.get(p + "pos"); });
    cv.addEventListener("pointermove", (e) => { if (e.pointerId === pid) store.set(p + "pos", sp + (e.clientX - sx) / (cv.clientWidth * 2)); });
    cv.addEventListener("pointerup", () => (pid = null));
    c.appendChild(row(mini("Warp"), Seg(store, p + "warpMode")));
    c.appendChild(knobRow("", K(p + "pos", "Position"), K(p + "level", "Niveau"), K(p + "semi", "Demi-ton"), K(p + "fine", "Fin"),
      K(p + "unison", "Unisson"), K(p + "detune", "Detune"), K(p + "spread", "Stéréo"), K(p + "warp", "Warp")));
    page("osc").appendChild(c);
  }
  {
    const c = card("sub-card", "Sub · Bruit · FM", "sub.on");
    c.appendChild(row(mini("Oct"), Seg(store, "sub.oct"), Seg(store, "sub.shape", ["Sin", "Tri", "Carré"])));
    c.appendChild(knobRow("", K("sub.level", "Sub"), K("noise.level", "Bruit"), K("fm.amount", "FM B→A")));
    page("osc").appendChild(c);
  }

  // ── FILTRE ──
  let filterCanvas, envCanvases = {};
  {
    const c = card("filter-card", "Filtre", "filter.on");
    c.appendChild(row(Seg(store, "filter.type")));
    filterCanvas = el("canvas", "display");
    c.appendChild(filterCanvas);
    let pid = null, sx = 0, sy = 0, sc = 0, sr = 0;
    filterCanvas.addEventListener("pointerdown", (e) => {
      pid = e.pointerId; filterCanvas.setPointerCapture(pid);
      sx = e.clientX; sy = e.clientY; sc = toNorm(store.defs["filter.cutoff"], store.get("filter.cutoff")); sr = store.get("filter.res");
    });
    filterCanvas.addEventListener("pointermove", (e) => {
      if (e.pointerId !== pid) return;
      const d = store.defs["filter.cutoff"];
      const n = Math.min(1, Math.max(0, sc + (e.clientX - sx) / (filterCanvas.clientWidth * 2)));
      store.set("filter.cutoff", d.min * Math.pow(d.max / d.min, n));
      store.set("filter.res", sr + (sy - e.clientY) / 300);
    });
    filterCanvas.addEventListener("pointerup", () => (pid = null));
    c.appendChild(knobRow("five", K("filter.cutoff", "Cutoff"), K("filter.res", "Réso"), K("filter.drive", "Drive"), K("filter.env", "Env 2"), K("filter.keytrack", "Key")));
    page("filter").appendChild(c);
  }
  for (const [e, title, cls] of [["env1", "Env 1 · <em>Amp</em>", "env1-card"], ["env2", "Env 2 · <em>Mod</em>", "env2-card"]]) {
    const c = card(cls, title);
    const cv = el("canvas", "display small");
    envCanvases[e] = cv;
    c.appendChild(cv);
    c.appendChild(knobRow("", K(e + ".a", "Attaque"), K(e + ".d", "Decay"), K(e + ".s", "Sustain"), K(e + ".r", "Release")));
    page("filter").appendChild(c);
  }

  // ── MOD ──
  const lfoCanvases = {};
  for (const l of ["lfo1", "lfo2"]) {
    const n = l === "lfo1" ? "1" : "2";
    const c = card(`${l}-card`, `LFO <em>${n}</em>`);
    const cv = el("canvas", "display small");
    lfoCanvases[l] = cv;
    c.appendChild(cv);
    c.appendChild(row(Seg(store, l + ".shape", ["∿", "△", "⟋", "⟍", "⊓", "S&H"])));
    const rate = K(l + ".rate", "Vitesse"), div = K(l + ".div", "Division");
    const sync = Toggle(store, l + ".sync"), retrig = Toggle(store, l + ".retrig");
    const ks = knobRow("", rate, div);
    const wrapS = el("div", "knob"); wrapS.append(sync, el("div", "lbl", "Sync"));
    const wrapR = el("div", "knob"); wrapR.append(retrig, el("div", "lbl", "Retrig"));
    wrapS.style.justifyContent = wrapR.style.justifyContent = "center";
    ks.append(wrapS, wrapR);
    store.on(l + ".sync", (v) => { rate.style.display = v ? "none" : ""; div.style.display = v ? "" : "none"; });
    rate.style.display = store.get(l + ".sync") ? "none" : ""; div.style.display = store.get(l + ".sync") ? "" : "none";
    c.appendChild(ks);
    page("mod").appendChild(c);
  }
  const macroNames = ["Macro 1", "Macro 2", "Macro 3", "Macro 4"];
  const srcLabels = () => spec.modSources.map((s, i) => (i >= 5 && i <= 8 ? `M${i - 4} · ${macroNames[i - 5]}` : s));
  const srcPicks = [];
  {
    const c = card("matrix-card", "Matrice de <em>modulation</em>");
    c.appendChild(el("p", "hint", "Source → destination. Double-tape un curseur pour le remettre à zéro."));
    for (let s = 1; s <= 8; s++) {
      const r = el("div", "mrow");
      const sp = Pick(store, `mod${s}.src`, { labels: srcLabels() });
      srcPicks.push(sp);
      r.append(el("span", "n", String(s)), sp, Pick(store, `mod${s}.dst`), el("span"));
      const amt = BiSlider(store, `mod${s}.amt`);
      amt.classList.add("amt");
      r.appendChild(amt);
      const idle = () => r.classList.toggle("idle", !store.get(`mod${s}.src`) || !store.get(`mod${s}.dst`));
      store.on(`mod${s}.src`, idle); store.on(`mod${s}.dst`, idle); idle();
      c.appendChild(r);
    }
    page("mod").appendChild(c);
  }

  // ── FX ──
  {
    const c = card("fx-dist", "Distorsion", "fx.dist.on");
    c.appendChild(row(Seg(store, "fx.dist.type", ["Tube", "Hard", "Fold", "Crush", "Rect"])));
    c.appendChild(knobRow("", K("fx.dist.drive", "Drive"), K("fx.dist.mix", "Mix")));
    page("fx").appendChild(c);
    const c2 = card("fx-comp", "Squash <em>multi</em>", "fx.comp.on");
    c2.appendChild(el("p", "hint", "Compression haute + basse façon « OTT » : rapproche tout vers l'avant."));
    c2.appendChild(knobRow("", K("fx.comp.amount", "Squash"), K("fx.comp.mix", "Mix")));
    page("fx").appendChild(c2);
    const c3 = card("fx-delay", "Delay", "fx.delay.on");
    const pp = el("div", "knob"); pp.append(Toggle(store, "fx.delay.pingpong"), el("div", "lbl", "Ping-pong"));
    pp.style.justifyContent = "center";
    c3.appendChild(knobRow("", K("fx.delay.div", "Temps"), K("fx.delay.fb", "Feedback"), K("fx.delay.mix", "Mix"), pp));
    page("fx").appendChild(c3);
    const c4 = card("fx-reverb", "Reverb", "fx.reverb.on");
    c4.appendChild(knobRow("", K("fx.reverb.size", "Taille"), K("fx.reverb.damp", "Amorti"), K("fx.reverb.mix", "Mix")));
    page("fx").appendChild(c4);
  }

  // ── JOUER ──
  const macroKnobs = [], bigMacroKnobs = [];
  {
    const xy = el("div", "xy", `<div class="dot"></div><span class="ax y">↑ <b id="xy-y">Macro 2</b></span><span class="ax x"><b id="xy-x">Macro 1</b> →</span>`);
    const setXY = () => {
      xy.style.setProperty("--x", store.get("macro1") * 100 + "%");
      xy.style.setProperty("--y", (1 - store.get("macro2")) * 100 + "%");
    };
    store.on("macro1", setXY); store.on("macro2", setXY); setXY();
    let pid = null;
    const mv = (e) => {
      const r = xy.getBoundingClientRect();
      store.set("macro1", (e.clientX - r.left) / r.width);
      store.set("macro2", 1 - (e.clientY - r.top) / r.height);
    };
    xy.addEventListener("pointerdown", (e) => { pid = e.pointerId; xy.setPointerCapture(pid); mv(e); });
    xy.addEventListener("pointermove", (e) => { if (e.pointerId === pid) mv(e); });
    xy.addEventListener("pointerup", () => (pid = null));
    page("play").appendChild(xy);
    const c = card("matrix-card big-macros", "Macros");
    const ks = [1, 2, 3, 4].map((m) => { const k = K("macro" + m, "Macro " + m); bigMacroKnobs.push(k); return k; });
    c.appendChild(knobRow("", ...ks));
    page("play").appendChild(c);
    const c2 = card("osc-a", "Jeu");
    const lg = el("div", "knob"); lg.append(Toggle(store, "global.legato"), el("div", "lbl", "Legato"));
    lg.style.justifyContent = "center";
    c2.appendChild(knobRow("", K("global.poly", "Voix", { format: (v) => (v === 1 ? "Mono" : v) }), K("global.glide", "Glide"), K("global.bend", "Bend"), K("global.volume", "Volume")));
    c2.appendChild(knobRow("", lg));
    page("play").appendChild(c2);
  }
  // bande de macros
  for (let m = 1; m <= 4; m++) {
    const k = K("macro" + m, "Macro " + m);
    macroKnobs.push(k);
    $("#macro-strip").appendChild(k);
  }
  function setMacroNames(names) {
    names.forEach((n, i) => {
      macroNames[i] = n || `Macro ${i + 1}`;
      macroKnobs[i].setLabel(macroNames[i]);
      bigMacroKnobs[i].setLabel(macroNames[i]);
    });
    $("#xy-x").textContent = macroNames[0];
    $("#xy-y").textContent = macroNames[1];
    const labels = srcLabels();
    srcPicks.forEach((s) => [...s.options].forEach((o, i) => (o.textContent = labels[i])));
  }

  // ───────── Onglets ─────────
  function showTab(name) {
    document.querySelectorAll(".tabs button").forEach((b) => b.classList.toggle("active", b.dataset.tab === name));
    document.querySelectorAll(".page").forEach((p) => p.classList.toggle("active", p.dataset.page === name));
    localStorage.setItem("masala.tab", name);
    dirty = true;
  }
  document.querySelectorAll(".tabs button").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.tab)));
  showTab(localStorage.getItem("masala.tab") || "osc");

  // ───────── Dessins (wavetable, filtre, enveloppes, LFO) ─────────
  let preview = null; // moteur local utilisé uniquement pour lire les tables
  store.onAny(() => (dirty = true));

  function fit(cv) {
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const w = cv.clientWidth, h = cv.clientHeight;
    if (!w) return null;
    if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
    const g = cv.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    return { g, w, h };
  }
  const css = (v) => getComputedStyle(document.documentElement).getPropertyValue(v).trim();
  const COL = {};
  ["--orange", "--papier", "--line", "--muted", "--noir-3"].forEach((v) => (COL[v] = css(v)));

  const HAZE = [[10, 0.018], [6, 0.025], [3, 0.03]]; // [épaisseur, opacité] des couches de brume
  const mval = (dest) => (liveMod ? liveMod[D[dest]] || 0 : 0);

  function drawWave(o) {
    const cv = waveCanvases[o];
    const f = fit(cv); if (!f) return;
    const { g, w, h } = f;
    const p = `osc${o}.`;
    const color = o === "A" ? COL["--orange"] : COL["--papier"];
    if (!preview) preview = new window.MasalaDSP.Engine(spec, 44100);
    const table = store.get(p + "table"), wm = store.get(p + "warpMode");
    const pos = Math.min(1, Math.max(0, store.get(p + "pos") + mval(`Osc ${o} Pos`)));
    const warp = Math.min(1, Math.max(0, store.get(p + "warp") + mval(`Osc ${o} Warp`)));
    const N = 160;
    // frames empilées en perspective, en brume : traits larges et très transparents,
    // sans contour net, qui s'estompent avec la profondeur
    g.lineJoin = "round"; g.lineCap = "round"; g.strokeStyle = COL["--papier"];
    for (let fr = 7; fr >= 0; fr--) {
      const d = preview.framePreview(table, fr / 7, wm, warp, N);
      const ox = fr * 5, oy = -fr * 4;
      g.beginPath();
      for (let i = 0; i < N; i++) {
        const x = 14 + ox + (i / (N - 1)) * (w - 64), y = h * 0.58 + oy - d[i] * h * 0.3;
        i ? g.lineTo(x, y) : g.moveTo(x, y);
      }
      const depth = 1 - fr / 9;
      for (const [lw, a] of HAZE) { g.lineWidth = lw; g.globalAlpha = a * depth; g.stroke(); }
    }
    g.globalAlpha = 1;
    const d = preview.framePreview(table, pos, wm, warp, N);
    const ox = pos * 35, oy = -pos * 28;
    g.beginPath();
    for (let i = 0; i < N; i++) {
      const x = 14 + ox + (i / (N - 1)) * (w - 64), y = h * 0.58 + oy - d[i] * h * 0.3;
      i ? g.lineTo(x, y) : g.moveTo(x, y);
    }
    g.strokeStyle = color; g.lineWidth = 3; g.lineJoin = "round"; g.stroke();
    g.fillStyle = COL["--muted"]; g.font = "600 11px 'Bricolage Grotesque', sans-serif";
    g.fillText(spec.tables[table].toUpperCase() + "  ·  " + Math.round(pos * 100) + "%", 10, h - 8);
  }

  function drawFilter() {
    const f = fit(filterCanvas); if (!f) return;
    const { g, w, h } = f;
    const type = store.get("filter.type");
    const fc = Math.min(20000, store.get("filter.cutoff") * Math.pow(2, mval("Cutoff") / 12));
    const res = Math.min(1, Math.max(0, store.get("filter.res") + mval("Resonance")));
    const k = 2 - 1.96 * res;
    const on = store.get("filter.on");
    g.strokeStyle = "rgba(255,255,255,.05)"; g.lineWidth = 1;
    for (const fq of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
      const x = (Math.log(fq / 20) / Math.log(1000)) * w;
      g.beginPath(); g.moveTo(x, 0); g.lineTo(x, h); g.stroke();
    }
    const dbTop = 18, dbBot = -42;
    const yOf = (db) => ((dbTop - Math.max(dbBot, Math.min(dbTop, db))) / (dbTop - dbBot)) * h;
    g.beginPath();
    for (let x = 0; x <= w; x += 2) {
      const fq = 20 * Math.pow(1000, x / w);
      const s = fq / fc; // s = jω/ωc
      const re = 1 - s * s, im = k * s, den = re * re + im * im;
      let mag;
      if (!on) mag = 1;
      else switch (type) {
        case 0: mag = 1 / den; break;
        case 1: mag = 1 / Math.sqrt(den); break;
        case 2: mag = (k * s) * k / Math.sqrt(den); break;
        case 3: mag = (s * s) / Math.sqrt(den); break;
        default: mag = Math.abs(1 - s * s) / Math.sqrt(den);
      }
      const y = yOf(20 * Math.log10(Math.max(1e-6, mag)));
      x ? g.lineTo(x, y) : g.moveTo(x, y);
    }
    const col = COL["--orange"];
    g.strokeStyle = col; g.lineWidth = 3; g.stroke();
    g.lineTo(w, h); g.lineTo(0, h); g.closePath();
    const grad = g.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0, "rgba(242,84,27,.45)"); grad.addColorStop(1, "rgba(242,84,27,.05)");
    g.fillStyle = grad; g.fill();
    g.fillStyle = COL["--muted"]; g.font = "600 11px 'Bricolage Grotesque', sans-serif";
    g.fillText(spec.params[store.index["filter.type"]].choices[type] + "  ·  " + (fc >= 1000 ? (fc / 1000).toFixed(2) + " kHz" : Math.round(fc) + " Hz"), 10, h - 8);
  }

  function drawEnv(e) {
    const f = fit(envCanvases[e]); if (!f) return;
    const { g, w, h } = f;
    const a = store.get(e + ".a"), d = store.get(e + ".d"), s = store.get(e + ".s"), r = store.get(e + ".r");
    const sc = (t) => Math.log(1 + t * 20) / Math.log(1 + 10 * 20); // échelle comprimée
    const wa = sc(a) * w * 0.3, wd = sc(d) * w * 0.3, ws = w * 0.15, wr = sc(r) * w * 0.3;
    const top = 10, bot = h - 8, yS = bot - s * (bot - top);
    let x = 8;
    g.beginPath(); g.moveTo(x, bot);
    x += wa; g.lineTo(x, top);
    for (let i = 1; i <= 20; i++) { const t = i / 20; g.lineTo(x + wd * t, yS + (top - yS) * Math.exp(-t * 4)); }
    x += wd; g.lineTo(x + ws, yS); x += ws;
    for (let i = 1; i <= 20; i++) { const t = i / 20; g.lineTo(x + wr * t, bot - (bot - yS) * Math.exp(-t * 4)); }
    const col = e === "env1" ? COL["--orange"] : COL["--papier"];
    g.strokeStyle = col; g.lineWidth = 3; g.lineJoin = "round"; g.stroke();
  }

  const lfoT0 = performance.now();
  function drawLfo(l) {
    const f = fit(lfoCanvases[l]); if (!f) return;
    const { g, w, h } = f;
    const shape = store.get(l + ".shape");
    const sync = store.get(l + ".sync");
    const rate = sync ? (bpm / 60) / spec.divBeats[store.get(l + ".div")] : store.get(l + ".rate");
    const val = (ph) => {
      switch (shape) {
        case 0: return 0.5 - 0.5 * Math.cos(2 * Math.PI * ph);
        case 1: return ph < 0.5 ? ph * 2 : 2 - ph * 2;
        case 2: return ph;
        case 3: return 1 - ph;
        case 4: return ph < 0.5 ? 1 : 0;
        default: { const s = Math.floor(ph * 8); return ((Math.sin(s * 12.9898) * 43758.5453) % 1 + 1) % 1; }
      }
    };
    const col = l === "lfo1" ? COL["--orange"] : COL["--papier"];
    const top = 8, bot = h - 8;
    g.beginPath();
    for (let x = 0; x <= w; x += 2) {
      const ph = ((x / w) * 2) % 1;
      const y = bot - val(shape === 5 ? (x / w) : ph) * (bot - top);
      x ? g.lineTo(x, y) : g.moveTo(x, y);
    }
    g.strokeStyle = col; g.lineWidth = 3; g.stroke();
    const ph = (((performance.now() - lfoT0) / 1000) * rate) % 1;
    const x = (ph / 2) * w, y = bot - val(shape === 5 ? ph / 2 : ph) * (bot - top);
    g.fillStyle = "#1E1B16"; g.strokeStyle = col; g.lineWidth = 3;
    g.beginPath(); g.arc(x, y, 6, 0, Math.PI * 2); g.fill(); g.stroke();
    g.fillStyle = COL["--muted"]; g.font = "600 11px 'Bricolage Grotesque', sans-serif";
    g.fillText(sync ? spec.divs[store.get(l + ".div")] : rate.toFixed(2) + " Hz", w - 60, 14);
  }

  // arcs de modulation sur les potards
  const MOD_VIEW = [
    ["filter.cutoff", "Cutoff", (d, v, m) => toNorm(d, Math.min(d.max, Math.max(d.min, v * Math.pow(2, m / 12))))],
    ["filter.res", "Resonance"], ["oscA.pos", "Osc A Pos"], ["oscB.pos", "Osc B Pos"], ["fm.amount", "FM Amount"],
    ["filter.drive", "Filter Drive"], ["oscA.warp", "Osc A Warp"], ["oscB.warp", "Osc B Warp"], ["oscA.level", "Osc A Level"],
    ["oscB.level", "Osc B Level"], ["sub.level", "Sub Level"], ["noise.level", "Noise Level"], ["fx.dist.drive", "Dist Drive"],
    ["fx.delay.mix", "Delay Mix"], ["fx.reverb.mix", "Reverb Mix"], ["oscA.detune", "Detune"], ["oscB.detune", "Detune"],
  ];
  function drawModArcs() {
    for (const [id, dest, conv] of MOD_VIEW) {
      const ks = knobs[id]; if (!ks) continue;
      const d = store.defs[id], v = store.get(id), m = mval(dest);
      const n = liveMod && m !== 0 ? (conv ? conv(d, v, m) : toNorm(d, Math.min(d.max, Math.max(d.min, v + m)))) : null;
      ks.forEach((k) => k.setMod(n));
    }
  }

  function frame() {
    const tab = localStorage.getItem("masala.tab") || "osc";
    if (dirty || modDirty) {
      if (tab === "osc") { drawWave("A"); drawWave("B"); }
      if (tab === "filter") { drawFilter(); drawEnv("env1"); drawEnv("env2"); }
      drawModArcs();
      dirty = false; modDirty = false;
    }
    if (tab === "mod") { drawLfo("lfo1"); drawLfo("lfo2"); }
    requestAnimationFrame(frame);
  }
  window.addEventListener("resize", () => (dirty = true));

  // ───────── Presets ─────────
  const USER_KEY = "masala.user";
  let userPresets = [];
  try { userPresets = JSON.parse(localStorage.getItem(USER_KEY)) || []; } catch {}
  const saveUser = () => localStorage.setItem(USER_KEY, JSON.stringify(userPresets));
  const allPresets = () => [...bank.presets, ...userPresets.filter((p) => !p.deleted)];
  let current = null;

  function applyPreset(pr) {
    for (const d of spec.params) {
      let v = pr.params && pr.params[d.id] !== undefined ? pr.params[d.id] : d.def;
      if (d.type === "choice" && typeof v === "string") { const i = d.choices.indexOf(v); v = i >= 0 ? i : d.def; }
      if (typeof v === "boolean") v = v ? 1 : 0;
      if (typeof v !== "number" || !isFinite(v)) v = d.def;
      store.set(d.id, v, true);
    }
    post({ type: "allOff" });
    post({ type: "params", values: store.values.slice() });
    current = pr;
    $("#preset-title").textContent = pr.name;
    const cat = pr.category || "Mes presets";
    $("#preset-cat").textContent = cat;
    $("#preset-cat").style.setProperty("--cat", CAT_COLORS[cat] || "var(--papier)");
    setMacroNames(pr.macros || []);
    localStorage.setItem("masala.current", pr.id);
    dirty = true;
  }

  function serialize(meta) {
    const params = {};
    for (const d of spec.params) {
      const v = store.get(d.id);
      params[d.id] = d.type === "choice" ? d.choices[v] : v;
    }
    return { format: "epices.masala.preset", version: 1, author: Cloud.state.email || "moi", tags: [], ...meta, macros: macroNames.slice(), params, updatedAt: Date.now() };
  }

  function step(dir) {
    const list = allPresets();
    let i = list.findIndex((p) => p.id === (current && current.id));
    i = (i + dir + list.length) % list.length;
    applyPreset(list[i]);
  }
  $("#preset-prev").onclick = () => step(-1);
  $("#preset-next").onclick = () => step(1);

  // feuilles
  const backdrop = $("#sheet-backdrop");
  let openSheet = null;
  function show(sheet) { openSheet = sheet; sheet.classList.add("show"); backdrop.classList.add("show"); }
  function hide() { if (openSheet) openSheet.classList.remove("show"); backdrop.classList.remove("show"); openSheet = null; }
  backdrop.onclick = hide;
  document.querySelectorAll(".sheet-grab").forEach((g) => (g.onclick = hide));

  let filter = "Tous";
  function renderList() {
    const q = $("#preset-search").value.trim().toLowerCase();
    const cats = ["Tous", ...spec.categories, "Mes presets"];
    const fl = $("#preset-filters");
    fl.innerHTML = "";
    cats.forEach((c) => {
      const b = el("button", c === filter ? "on" : "", c);
      b.style.setProperty("--cat", CAT_COLORS[c] || "var(--orange)");
      b.onclick = () => { filter = c; renderList(); };
      fl.appendChild(b);
    });
    const list = $("#preset-list");
    list.innerHTML = "";
    const items = allPresets().filter((p) => {
      const isUser = !p.id.startsWith("factory-");
      if (filter === "Mes presets" && !isUser) return false;
      if (filter !== "Tous" && filter !== "Mes presets" && p.category !== filter) return false;
      if (q && !(p.name + " " + (p.tags || []).join(" ") + " " + p.category).toLowerCase().includes(q)) return false;
      return true;
    });
    for (const p of items) {
      const isUser = !p.id.startsWith("factory-");
      const it = el("div", "preset-item" + (current && current.id === p.id ? " current" : ""));
      it.style.setProperty("--cat", CAT_COLORS[p.category] || "var(--papier)");
      it.innerHTML = `<span class="sw"></span><span class="t"><b></b><small></small></span>`;
      it.querySelector("b").textContent = p.name;
      it.querySelector("small").textContent = `${p.category} · ${(p.tags || []).slice(0, 3).join(", ")}`;
      if (isUser) {
        const tag = el("span", "tag", Cloud.loggedIn ? "☁︎ moi" : "moi");
        const del = el("button", "del", "×");
        del.onclick = async (e) => {
          e.stopPropagation();
          // deux appuis pour confirmer (les boîtes de dialogue sont bloquées dans certaines vues)
          if (!del.classList.contains("armed")) { del.classList.add("armed"); del.textContent = "Supprimer ?"; setTimeout(() => { del.classList.remove("armed"); del.textContent = "×"; }, 2500); return; }
          p.deleted = true; p.updatedAt = Date.now();
          if (Cloud.loggedIn) { try { await Cloud.remove(p.id); } catch {} }
          userPresets = userPresets.filter((x) => x.id !== p.id);
          saveUser(); renderList();
        };
        it.append(tag, del);
      }
      it.onclick = () => { applyPreset(p); renderList(); };
      list.appendChild(it);
    }
    if (!items.length) list.appendChild(el("p", "hint", "Aucun preset ici pour l'instant."));
  }
  $("#preset-open").onclick = () => { renderList(); show($("#sheet-presets")); };
  $("#preset-search").oninput = renderList;

  // enregistrement
  const catSel = $("#save-cat");
  [...spec.categories, "Mes presets"].forEach((c) => { const o = el("option", "", c); o.value = c; catSel.appendChild(o); });
  $("#preset-save").onclick = () => {
    const isUser = current && !current.id.startsWith("factory-");
    $("#save-name").value = isUser ? current.name : (current ? current.name + " (moi)" : "Mon preset");
    catSel.value = (current && current.category) || "Mes presets";
    macroNames.forEach((n, i) => ($("#save-m" + (i + 1)).value = n));
    $("#save-hint").textContent = Cloud.loggedIn ? "Sera aussi envoyé sur ton compte epices." : "Connecte-toi pour le retrouver dans le plugin.";
    show($("#sheet-save"));
  };
  function readMacroInputs() { [1, 2, 3, 4].forEach((i) => (macroNames[i - 1] = $("#save-m" + i).value.trim() || `Macro ${i}`)); }
  $("#save-confirm").onclick = async () => {
    const name = $("#save-name").value.trim() || "Sans titre";
    readMacroInputs();
    const existing = userPresets.find((p) => p.name === name && !p.deleted);
    const id = existing ? existing.id : "user-" + Date.now().toString(36) + Math.random().toString(36).slice(2, 6);
    const pr = serialize({ id, name, category: catSel.value });
    if (existing) Object.assign(existing, pr); else userPresets.push(pr);
    saveUser();
    applyPreset(existing || pr);
    hide();
    toast("Preset enregistré");
    if (Cloud.loggedIn) {
      try { await Cloud.put(existing || pr); toast("Preset enregistré et synchronisé ☁︎"); } catch (e) { toast("Enregistré localement (sync : " + e.message + ")"); }
    }
  };
  $("#save-export").onclick = async () => {
    readMacroInputs();
    const name = $("#save-name").value.trim() || "Sans titre";
    const pr = serialize({ id: (current && !current.id.startsWith("factory-") ? current.id : "user-" + Date.now().toString(36)), name, category: catSel.value });
    const text = JSON.stringify(pr, null, 1);
    const fname = name.replace(/[^\w\- ]+/g, "_") + ".masala";
    const native = window.Capacitor && window.Capacitor.isNativePlatform && window.Capacitor.isNativePlatform();
    if (native) {
      // appli iOS : écrit le fichier puis ouvre la feuille de partage (AirDrop, Fichiers, Mail…)
      try {
        const { Filesystem, Share } = window.Capacitor.Plugins;
        const w = await Filesystem.writeFile({ path: fname, data: text, directory: "CACHE", encoding: "utf8" });
        await Share.share({ title: name, files: [w.uri] });
      } catch (err) { if (!/cancel/i.test(String(err && err.message))) toast("Export impossible : " + err.message); }
      return;
    }
    const file = new File([text], fname, { type: "application/json" });
    if (navigator.canShare && navigator.canShare({ files: [file] })) {
      try { await navigator.share({ files: [file], title: name }); return; } catch {}
    }
    const a = el("a"); a.href = URL.createObjectURL(file); a.download = file.name; a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 2000);
  };
  // import
  $("#btn-import").onclick = () => $("#file-import").click();
  $("#file-import").onchange = async (e) => {
    let n = 0;
    for (const f of e.target.files) {
      try {
        const data = JSON.parse(await f.text());
        const list = data.presets || [data];
        for (const p of list) {
          if (!p.params || !p.name) continue;
          if (!p.id || p.id.startsWith("factory-")) p.id = "user-" + Date.now().toString(36) + Math.random().toString(36).slice(2, 6);
          p.updatedAt = Date.now();
          userPresets = userPresets.filter((x) => x.id !== p.id);
          userPresets.push(p); n++;
        }
      } catch { toast("Fichier illisible : " + f.name); }
    }
    saveUser(); renderList();
    if (n) toast(`${n} preset(s) importé(s)`);
    e.target.value = "";
  };

  // ───────── Compte ─────────
  function renderAccount() {
    $("#account-dot").classList.toggle("on", Cloud.loggedIn);
    $("#account-in").hidden = !Cloud.loggedIn;
    $("#account-out").hidden = Cloud.loggedIn;
    $("#acc-who").textContent = Cloud.state.email || "";
    $("#acc-server").value = Cloud.state.server || Cloud.server;
  }
  const status = (t) => ($("#acc-status").textContent = t);
  $("#btn-account").onclick = () => { renderAccount(); status(""); show($("#sheet-account")); };
  const authAction = (fn) => async () => {
    Cloud.setServer($("#acc-server").value);
    status("…");
    try {
      await fn($("#acc-email").value.trim(), $("#acc-pass").value);
      $("#acc-pass").value = "";
      status("");
      renderAccount();
      await doSync();
    } catch (e) { status(e.message); }
  };
  $("#acc-login").onclick = authAction((e, p) => Cloud.login(e, p));
  $("#acc-register").onclick = authAction((e, p) => Cloud.register(e, p));
  $("#acc-logout").onclick = () => { Cloud.logout(); renderAccount(); status("Déconnecté."); };
  async function doSync() {
    status("Synchronisation…");
    try {
      const r = await Cloud.sync(userPresets);
      userPresets = r.presets.filter((p) => !p.deleted);
      saveUser();
      status(`Synchro OK — ${r.pushed} envoyé(s), ${r.pulled} reçu(s).`);
      toast("Presets synchronisés ☁︎");
    } catch (e) { status("Échec : " + e.message); }
  }
  $("#acc-sync").onclick = doSync;
  $("#acc-delete").onclick = async (e) => {
    const b = e.currentTarget;
    if (!b.classList.contains("armed")) {
      b.classList.add("armed"); b.textContent = "Confirmer la suppression";
      setTimeout(() => { b.classList.remove("armed"); b.textContent = "Supprimer définitivement"; }, 4000);
      return;
    }
    status("Suppression…");
    try {
      await Cloud.deleteAccount($("#del-pass").value);
      $("#del-pass").value = "";
      renderAccount();
      status("Compte supprimé. Tes presets restent sur ce téléphone.");
    } catch (err) { status(err.message); }
  };
  $("#acc-pair").onclick = async () => {
    try {
      const r = await Cloud.pairCode();
      $("#pair-code").textContent = r.code;
    } catch (e) { status(e.message); }
  };
  renderAccount();
  if (window.MASALA_DEMO) $("#btn-account").hidden = true; // version d'essai sans serveur

  // ───────── Clavier ─────────
  let octave = +(localStorage.getItem("masala.oct") || 2);
  let hold = false;
  const keysEl = $("#keys");
  const WHITE = [0, 2, 4, 5, 7, 9, 11, 12, 14, 16];
  const BLACK = [[1, 0], [3, 1], [6, 3], [8, 4], [10, 5], [13, 7], [15, 8]]; // [demi-ton, index de la blanche à gauche]
  const NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const keyEls = {};
  function buildKeys() {
    keysEl.innerHTML = "";
    keysEl.style.setProperty("--whites", WHITE.length);
    const base = 12 * (octave + 1);
    WHITE.forEach((s) => {
      const k = el("div", "key");
      k.dataset.note = base + s;
      if (s % 12 === 0) k.appendChild(el("span", "kl", "C" + (octave + Math.floor(s / 12))));
      keysEl.appendChild(k);
      keyEls[base + s] = k;
    });
    BLACK.forEach(([s, wi]) => {
      const k = el("div", "key black");
      k.dataset.note = base + s;
      k.style.left = `calc(${((wi + 1) / WHITE.length) * 100}% - (100% / ${WHITE.length} * .31))`;
      keysEl.appendChild(k);
      keyEls[base + s] = k;
    });
    $("#oct-label").textContent = "C" + octave;
  }
  buildKeys();
  const down = new Set();
  function noteOn(n, vel = 0.85) {
    if (!ctx) return;
    down.add(n);
    post({ type: "noteOn", note: n, vel });
    keysEl.querySelector(`[data-note="${n}"]`)?.classList.add("down");
  }
  function noteOff(n) {
    down.delete(n);
    post({ type: "noteOff", note: n });
    keysEl.querySelector(`[data-note="${n}"]`)?.classList.remove("down");
  }
  const ptrNote = new Map();
  const keyAt = (x, y) => { const t = document.elementFromPoint(x, y); return t && t.closest ? t.closest("[data-note]") : null; };
  const noteAt = (x, y) => { const k = keyAt(x, y); return k ? +k.dataset.note : null; };
  keysEl.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    const n = noteAt(e.clientX, e.clientY);
    if (n === null) return;
    if (hold) {
      if (down.has(n)) { noteOff(n); return; }
      [...down].forEach(noteOff);
      noteOn(n); return;
    }
    keysEl.setPointerCapture(e.pointerId);
    // vélocité selon la hauteur de frappe sur la touche
    const r = keyAt(e.clientX, e.clientY).getBoundingClientRect();
    const vel = 0.55 + 0.45 * Math.min(1, (e.clientY - r.top) / r.height);
    ptrNote.set(e.pointerId, n);
    noteOn(n, vel);
  });
  keysEl.addEventListener("pointermove", (e) => {
    if (!ptrNote.has(e.pointerId)) return;
    const n = noteAt(e.clientX, e.clientY), old = ptrNote.get(e.pointerId);
    if (n !== null && n !== old) { noteOn(n); noteOff(old); ptrNote.set(e.pointerId, n); }
  });
  const ptrEnd = (e) => { const n = ptrNote.get(e.pointerId); if (n !== undefined) { noteOff(n); ptrNote.delete(e.pointerId); } };
  keysEl.addEventListener("pointerup", ptrEnd);
  keysEl.addEventListener("pointercancel", ptrEnd);
  $("#oct-down").onclick = () => { [...down].forEach(noteOff); octave = Math.max(0, octave - 1); localStorage.setItem("masala.oct", octave); buildKeys(); };
  $("#oct-up").onclick = () => { [...down].forEach(noteOff); octave = Math.min(6, octave + 1); localStorage.setItem("masala.oct", octave); buildKeys(); };
  $("#btn-hold").onclick = (e) => { hold = !hold; e.currentTarget.classList.toggle("on", hold); if (!hold) [...down].forEach(noteOff); };

  // clavier d'ordinateur (pratique sur ordi / iPad avec clavier)
  const QW = "awsedftgyhujkolp";
  document.addEventListener("keydown", (e) => {
    if (e.repeat || e.target.tagName === "INPUT") return;
    const i = QW.indexOf(e.key.toLowerCase());
    if (i >= 0) noteOn(12 * (octave + 1) + i);
    if (e.key === "z") $("#oct-down").click();
    if (e.key === "x") $("#oct-up").click();
  });
  document.addEventListener("keyup", (e) => {
    const i = QW.indexOf(e.key.toLowerCase());
    if (i >= 0) noteOff(12 * (octave + 1) + i);
  });

  // MIDI (Chrome/Android, macOS…)
  if (navigator.requestMIDIAccess) {
    navigator.requestMIDIAccess().then((m) => {
      const bind = (inp) => (inp.onmidimessage = (ev) => {
        const [st, a, b] = ev.data, t = st & 0xf0;
        if (t === 0x90 && b > 0) noteOn(a, b / 127);
        else if (t === 0x80 || (t === 0x90 && b === 0)) noteOff(a);
        else if (t === 0xe0) post({ type: "bend", value: ((b << 7 | a) - 8192) / 8192 });
        else if (t === 0xb0 && a === 1) post({ type: "wheel", value: b / 127 });
        else if (t === 0xb0 && a >= 21 && a <= 24) store.set("macro" + (a - 20), b / 127);
      });
      m.inputs.forEach(bind);
      m.onstatechange = () => m.inputs.forEach(bind);
    }).catch(() => {});
  }

  // ───────── Motifs (riffs pour auditionner) ─────────
  // [pas de départ (double-croche), longueur en pas, demi-tons]
  const RIFFS = {
    "Wobble": [[0, 6, 0], [6, 2, 0], [8, 4, 3], [12, 2, 0], [14, 2, -2], [16, 6, 0], [22, 2, 0], [24, 4, 5], [28, 4, 3]],
    "Riddim": [[0, 1, 0], [2, 1, 0], [3, 1, 0], [6, 2, 0], [8, 1, 0], [10, 1, 0], [11, 1, 0], [14, 2, 1], [16, 1, 0], [18, 1, 0], [19, 1, 0], [22, 2, 0], [24, 3, -2], [28, 3, -5]],
    "Tribe": [0, 0, 12, 0, 0, 7, 0, 0, 0, 0, 12, 0, 0, 10, 0, 7, 0, 0, 12, 0, 0, 7, 0, 0, 3, 3, 15, 3, 0, 10, 0, 7].map((s, i) => [i, 0.7, s]),
    "Deep 140": [[0, 3, 0], [4, 2, 0], [7, 1, 0], [10, 4, -2], [16, 3, 0], [20, 2, 3], [23, 1, 0], [26, 6, -5]],
    "Halftime": [[0, 8, 0], [12, 2, 0], [16, 4, 1], [24, 6, -2], [30, 2, 0]],
  };
  const riffSel = $("#riff-select");
  Object.keys(RIFFS).forEach((k) => { const o = el("option", "", k); o.value = k; riffSel.appendChild(o); });
  riffSel.value = localStorage.getItem("masala.riff") || "Wobble";
  riffSel.onchange = () => localStorage.setItem("masala.riff", riffSel.value);
  let riffTimer = null, riffEvents = [], riffStart = 0, riffLoop = 0;
  function riffSchedule() {
    const stepDur = 60 / bpm / 4;
    const base = 12 * (octave + 1);
    const t0 = riffStart + riffLoop * 32 * stepDur;
    for (const [st, len, semi] of RIFFS[riffSel.value]) {
      riffEvents.push({ t: t0 + st * stepDur, on: true, n: base + semi });
      riffEvents.push({ t: t0 + (st + len) * stepDur - 0.005, on: false, n: base + semi });
    }
    riffEvents.sort((a, b) => a.t - b.t || (a.on ? 1 : -1));
    riffLoop++;
  }
  function riffTick() {
    const now = ctx.currentTime;
    const stepDur = 60 / bpm / 4;
    if (riffEvents.length === 0 || riffEvents[riffEvents.length - 1].t - now < 32 * stepDur * 0.5) riffSchedule();
    while (riffEvents.length && riffEvents[0].t <= now) {
      const ev = riffEvents.shift();
      ev.on ? noteOn(ev.n, 0.9) : noteOff(ev.n);
    }
  }
  $("#btn-riff").onclick = (e) => {
    if (!ctx) return;
    const b = e.currentTarget;
    if (riffTimer) {
      clearInterval(riffTimer); riffTimer = null; riffEvents = [];
      [...down].forEach(noteOff); post({ type: "allOff" });
      b.classList.remove("on"); b.textContent = "▶ Riff";
    } else {
      riffStart = ctx.currentTime + 0.05; riffLoop = 0; riffEvents = [];
      riffTimer = setInterval(riffTick, 5);
      b.classList.add("on"); b.textContent = "■ Riff";
    }
  };

  // ───────── Démarrage ─────────
  if (window.Capacitor && window.Capacitor.isNativePlatform && window.Capacitor.isNativePlatform())
    document.querySelectorAll(".web-only").forEach((e) => (e.hidden = true));
  const startId = localStorage.getItem("masala.current");
  applyPreset(allPresets().find((p) => p.id === startId) || bank.presets.find((p) => p.name === "Harissa Wobble") || bank.presets[0]);
  requestAnimationFrame(frame);

  $("#start").onclick = async () => {
    try {
      await startAudio();
      $("#splash").classList.add("hide");
    } catch (e) {
      alert("Impossible de démarrer l'audio : " + e.message);
    }
  };

  if ("serviceWorker" in navigator && location.protocol.startsWith("http") && !window.Capacitor && !window.MASALA_DATA) {
    navigator.serviceWorker.register("sw.js").catch(() => {});
  }
})();
