// MASALA — composants d'interface (potards, sélecteurs, interrupteurs…)
(function (G) {
  "use strict";

  // ─── Store des paramètres ───
  class Store {
    constructor(spec) {
      this.spec = spec;
      this.defs = {};
      this.index = {};
      spec.params.forEach((d, i) => { this.defs[d.id] = d; this.index[d.id] = i; });
      this.values = spec.params.map((d) => d.def);
      this.listeners = {};
      this.any = [];
    }
    get(id) { return this.values[this.index[id]]; }
    set(id, v, silent) {
      const d = this.defs[id];
      if (!d) return;
      v = Math.min(d.max, Math.max(d.min, v));
      if (d.type !== "float") v = Math.round(v);
      this.values[this.index[id]] = v;
      (this.listeners[id] || []).forEach((f) => f(v));
      if (!silent) this.any.forEach((f) => f(id, v));
    }
    on(id, f) { (this.listeners[id] ||= []).push(f); }
    onAny(f) { this.any.push(f); }
    refreshAll() { this.spec.params.forEach((d) => (this.listeners[d.id] || []).forEach((f) => f(this.get(d.id)))); }
  }

  const toNorm = (d, v) => d.skew === "log" ? Math.log(v / d.min) / Math.log(d.max / d.min) : (v - d.min) / (d.max - d.min);
  const fromNorm = (d, n) => {
    n = Math.min(1, Math.max(0, n));
    const v = d.skew === "log" ? d.min * Math.pow(d.max / d.min, n) : d.min + n * (d.max - d.min);
    return d.type === "float" ? v : Math.round(v);
  };

  function fmt(d, v) {
    if (d.type === "choice") return d.choices[v];
    if (d.type === "bool") return v ? "On" : "Off";
    switch (d.unit) {
      case "Hz": return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + "k" : v < 10 ? v.toFixed(2) : Math.round(v) + "";
      case "s": return v < 1 ? Math.round(v * 1000) + "ms" : v.toFixed(2) + "s";
      case "st": return (v > 0 ? "+" : "") + v;
      case "ct": return (v > 0 ? "+" : "") + Math.round(v) + "c";
    }
    if (d.type === "int") return String(v);
    if (d.min < 0) return (v > 0 ? "+" : "") + Math.round(v * 100);
    return Math.round(v * 100) + "%";
  }

  const el = (tag, cls, html) => {
    const e = document.createElement(tag);
    if (cls) e.className = cls;
    if (html !== undefined) e.innerHTML = html;
    return e;
  };

  // ─── arc SVG ───
  const A0 = -135, A1 = 135;
  function polar(cx, cy, r, deg) {
    const a = ((deg - 90) * Math.PI) / 180;
    return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
  }
  function arc(cx, cy, r, a, b) {
    if (Math.abs(b - a) < 0.01) b = a + 0.01;
    const [x0, y0] = polar(cx, cy, r, a), [x1, y1] = polar(cx, cy, r, b);
    const large = Math.abs(b - a) > 180 ? 1 : 0;
    const sweep = b > a ? 1 : 0;
    return `M${x0.toFixed(2)} ${y0.toFixed(2)}A${r} ${r} 0 ${large} ${sweep} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
  }

  let defsInjected = false;
  function injectDefs() {
    if (defsInjected) return;
    defsInjected = true;
    const s = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    s.setAttribute("width", "0"); s.setAttribute("height", "0"); s.style.position = "absolute";
    s.innerHTML = "";
    document.body.appendChild(s);
  }

  // ─── Potard ───
  function Knob(store, id, opts = {}) {
    injectDefs();
    const d = store.defs[id];
    const root = el("div", "knob");
    const bipolar = d.min < 0 && d.max > 0;
    root.innerHTML = `<svg viewBox="0 0 60 60"><path class="track"/><path class="arc"/><path class="modarc"/><circle class="cap" cx="30" cy="30" r="15"/><circle class="dot" cx="30" cy="22" r="3.6"/></svg><div class="val"></div><div class="lbl"></div>`;
    const [track, arcEl, modEl] = root.querySelectorAll("path");
    const dot = root.querySelector(".dot");
    const valEl = root.querySelector(".val"), lblEl = root.querySelector(".lbl");
    track.setAttribute("d", arc(30, 30, 25, A0, A1));
    lblEl.textContent = opts.label || d.name;
    const zero = bipolar ? toNorm(d, 0) : 0;
    let norm = 0;
    function draw(v) {
      norm = toNorm(d, v);
      const a = A0 + norm * (A1 - A0), z = A0 + zero * (A1 - A0);
      arcEl.setAttribute("d", arc(30, 30, 25, Math.min(a, z), Math.max(a, z)));
      const [x, y] = polar(30, 30, 7.5, a);
      dot.setAttribute("cx", x.toFixed(2)); dot.setAttribute("cy", y.toFixed(2));
      valEl.textContent = opts.format ? opts.format(v) : fmt(d, v);
    }
    store.on(id, draw);
    draw(store.get(id));

    let startY = 0, startN = 0, lastTap = 0, pid = null;
    root.addEventListener("pointerdown", (e) => {
      const now = performance.now();
      if (now - lastTap < 300) { store.set(id, d.def); lastTap = 0; return; }
      lastTap = now;
      pid = e.pointerId; root.setPointerCapture(pid);
      startY = e.clientY; startN = norm;
      root.classList.add("active");
      e.preventDefault();
    });
    root.addEventListener("pointermove", (e) => {
      if (e.pointerId !== pid) return;
      const range = d.type === "float" ? 200 : Math.max(120, (d.max - d.min) * 10);
      store.set(id, fromNorm(d, startN + (startY - e.clientY) / range));
    });
    const end = (e) => { if (e.pointerId === pid) { pid = null; root.classList.remove("active"); } };
    root.addEventListener("pointerup", end);
    root.addEventListener("pointercancel", end);
    root.setLabel = (t) => (lblEl.textContent = t);
    root.setMod = (n) => {
      if (n === null || n === undefined || Math.abs(n - norm) < 0.004) { modEl.setAttribute("d", ""); return; }
      n = Math.min(1, Math.max(0, n));
      const a = A0 + norm * (A1 - A0), b = A0 + n * (A1 - A0);
      modEl.setAttribute("d", arc(30, 30, 29.5, Math.min(a, b), Math.max(a, b)));
    };
    root.paramId = id;
    return root;
  }

  // ─── Segmenté ───
  function Seg(store, id, labels) {
    const d = store.defs[id];
    const root = el("div", "seg");
    const names = labels || d.choices || ["Off", "On"];
    const btns = names.map((n, i) => {
      const b = el("button", "", n);
      b.addEventListener("click", () => store.set(id, i));
      root.appendChild(b);
      return b;
    });
    const draw = (v) => btns.forEach((b, i) => b.classList.toggle("on", i === v));
    store.on(id, draw); draw(store.get(id));
    return root;
  }

  // ─── Liste déroulante ───
  function Pick(store, id, opts = {}) {
    const d = store.defs[id];
    const s = el("select", "pick");
    (d.choices || []).forEach((c, i) => {
      const o = el("option", "", opts.labels ? opts.labels[i] : c);
      o.value = i;
      s.appendChild(o);
    });
    s.addEventListener("change", () => store.set(id, +s.value));
    const draw = (v) => { s.value = v; if (opts.onChange) opts.onChange(v); };
    store.on(id, draw); draw(store.get(id));
    return s;
  }

  // ─── Interrupteur ───
  function Toggle(store, id) {
    const b = el("button", "toggle-sw");
    b.setAttribute("aria-label", store.defs[id].name);
    b.addEventListener("click", () => store.set(id, store.get(id) ? 0 : 1));
    const draw = (v) => b.classList.toggle("on", !!v);
    store.on(id, draw); draw(store.get(id));
    return b;
  }

  // ─── Curseur bipolaire (quantité de modulation) ───
  function BiSlider(store, id) {
    const d = store.defs[id];
    const root = el("div", "bislider", `<div class="mid"></div><div class="fill"></div><span></span>`);
    const fill = root.querySelector(".fill"), lab = root.querySelector("span");
    const draw = (v) => {
      const x = (v - d.min) / (d.max - d.min);
      fill.style.left = Math.min(x, 0.5) * 100 + "%";
      fill.style.width = Math.abs(x - 0.5) * 100 + "%";
      lab.textContent = fmt(d, v);
    };
    store.on(id, draw); draw(store.get(id));
    let pid = null, lastTap = 0;
    const setFrom = (e) => {
      const r = root.getBoundingClientRect();
      let v = d.min + ((e.clientX - r.left) / r.width) * (d.max - d.min);
      if (Math.abs(v) < 0.03) v = 0;
      store.set(id, v);
    };
    root.addEventListener("pointerdown", (e) => {
      const now = performance.now();
      if (now - lastTap < 300) { store.set(id, 0); lastTap = 0; return; }
      lastTap = now;
      pid = e.pointerId; root.setPointerCapture(pid); setFrom(e);
    });
    root.addEventListener("pointermove", (e) => { if (e.pointerId === pid) setFrom(e); });
    root.addEventListener("pointerup", () => (pid = null));
    root.addEventListener("pointercancel", () => (pid = null));
    return root;
  }

  G.MasalaUI = { Store, Knob, Seg, Pick, Toggle, BiSlider, el, fmt, toNorm, fromNorm };
})(window);
