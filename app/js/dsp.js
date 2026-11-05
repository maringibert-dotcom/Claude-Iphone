// MASALA — moteur DSP (AudioWorklet).
// Ce fichier est un script autonome : chargé comme AudioWorklet dans l'appli,
// ou via `vm` dans les tests Node. L'algorithme est porté à l'identique en C++
// dans plugin/Source/dsp/ pour que les presets sonnent pareil dans le VST3/AU.
(function (G) {
  "use strict";

  const TABLE_SIZE = 2048;
  const FRAMES = 8;
  const MIPS = 10; // harmoniques max : 1024 >> niveau
  const CONTROL_RATE = 32; // échantillons entre deux calculs de modulation
  const TWO_PI = Math.PI * 2;

  // ───────────────────── Wavetables (synthèse additive + IFFT) ─────────────────────
  function ifftImag(re, im) {
    // FFT inverse complexe radix-2 en place (sans 1/N). Résultat : parties im.
    const n = re.length;
    for (let i = 1, j = 0; i < n; i++) {
      let bit = n >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j) {
        let t = re[i]; re[i] = re[j]; re[j] = t;
        t = im[i]; im[i] = im[j]; im[j] = t;
      }
    }
    for (let len = 2; len <= n; len <<= 1) {
      const ang = TWO_PI / len;
      const wr = Math.cos(ang), wi = Math.sin(ang);
      for (let i = 0; i < n; i += len) {
        let cr = 1, ci = 0;
        for (let k = 0; k < len / 2; k++) {
          const a = i + k, b = a + len / 2;
          const xr = re[b] * cr - im[b] * ci;
          const xi = re[b] * ci + im[b] * cr;
          re[b] = re[a] - xr; im[b] = im[a] - xi;
          re[a] += xr; im[a] += xi;
          const t = cr * wr - ci * wi;
          ci = cr * wi + ci * wr; cr = t;
        }
      }
    }
    return im;
  }

  function besselJ(k, x) {
    // J_k(x) = 1/π ∫0^π cos(kτ − x sin τ) dτ  (trapèzes, 96 pas)
    const N = 96;
    let s = 0;
    for (let i = 0; i <= N; i++) {
      const t = (Math.PI * i) / N;
      const w = i === 0 || i === N ? 0.5 : 1;
      s += w * Math.cos(k * t - x * Math.sin(t));
    }
    return s / N;
  }

  const gauss = (n, c, w) => Math.exp(-((n - c) * (n - c)) / (2 * w * w));
  const lerp = (a, b, t) => a + (b - a) * t;
  const VOWELS = [[450, 800], [800, 1150], [400, 1600], [270, 2300], [325, 700]]; // o a e i u

  // Amplitude de l'harmonique n (1..1024) pour la table t à la position p (0..1).
  function spectrum(t, p, H) {
    const a = new Float64Array(H + 1);
    switch (t) {
      case 0: { // Basic : sinus → triangle → dent de scie → carré
        const seg = Math.min(2, Math.floor(p * 3));
        const u = p * 3 - seg;
        for (let n = 1; n <= H; n++) {
          const odd = n & 1;
          const sine = n === 1 ? 1 : 0;
          const tri = odd ? ((((n - 1) / 2) & 1) ? -1 : 1) * 8 / (Math.PI * Math.PI * n * n) : 0;
          const saw = ((n & 1) ? 1 : -1) * 2 / (Math.PI * n);
          const sq = odd ? 4 / (Math.PI * n) : 0;
          const ks = [sine, tri, saw, sq];
          a[n] = lerp(ks[seg], ks[seg + 1], u);
        }
        break;
      }
      case 1: { // Growl : formant résonant qui balaie les harmoniques
        const F = 2 + p * 38, w = 1.5 + p * 4;
        for (let n = 1; n <= H; n++)
          a[n] = ((n & 1) ? 1 : -1) * (1 / n) * (0.35 + 6 * gauss(n, F, w) + 2.5 * gauss(n, F * 2.1, w * 1.6));
        break;
      }
      case 2: { // Vowel : deux formants o → a → e → i → u (fondamentale 110 Hz)
        const x = p * (VOWELS.length - 1);
        const i0 = Math.min(VOWELS.length - 2, Math.floor(x)), u = x - i0;
        const f1 = lerp(VOWELS[i0][0], VOWELS[i0 + 1][0], u) / 110;
        const f2 = lerp(VOWELS[i0][1], VOWELS[i0 + 1][1], u) / 110;
        for (let n = 1; n <= H; n++)
          a[n] = (1 / Math.sqrt(n)) * (gauss(n, f1, 1.2) + 0.7 * gauss(n, f2, 1.8) + 0.04);
        break;
      }
      case 3: { // Metal : FM sin(φ + I·sin(3φ)), spectre via Bessel
        const I = p * 9, r = 3;
        for (let k = -40; k <= 40; k++) {
          const n = 1 + k * r;
          const J = besselJ(k, I);
          if (n > 0 && n <= H) a[n] += J;
          else if (n < 0 && -n <= H) a[-n] -= J; // sin(−x) = −sin(x)
        }
        break;
      }
      case 4: { // Tribal : impulsion à largeur variable, creuse et boisée
        const d = lerp(0.5, 0.04, p);
        for (let n = 1; n <= H; n++)
          a[n] = (2 / (n * Math.PI)) * Math.sin(n * Math.PI * d) * (1 + 0.6 * gauss(n, 5 + 10 * p, 2));
        break;
      }
      case 5: { // Digital : peignes harmoniques masqués
        const m = 2 + Math.floor(p * 9);
        for (let n = 1; n <= H; n++)
          a[n] = (1 / n) * (((n * m) % 7) < 3 ? 1 : 0.12) * ((n % m) === 1 ? 1.6 : 1);
        break;
      }
    }
    return a;
  }

  // tables[t][frame][mip] = Float32Array(TABLE_SIZE + 1) (point de garde)
  function buildTables(numTables) {
    const tables = [];
    for (let t = 0; t < numTables; t++) {
      const frames = [];
      for (let f = 0; f < FRAMES; f++) {
        const p = f / (FRAMES - 1);
        const amp = spectrum(t, p, 1024);
        const mips = [];
        let norm = 1;
        for (let m = 0; m < MIPS; m++) {
          const H = 1024 >> m;
          const re = new Float64Array(TABLE_SIZE), im = new Float64Array(TABLE_SIZE);
          for (let n = 1; n <= H; n++) re[n] = amp[n];
          const out = ifftImag(re, im);
          if (m === 0) {
            let peak = 0;
            for (let i = 0; i < TABLE_SIZE; i++) peak = Math.max(peak, Math.abs(out[i]));
            norm = peak > 1e-9 ? 1 / peak : 1;
          }
          const tab = new Float32Array(TABLE_SIZE + 1);
          for (let i = 0; i < TABLE_SIZE; i++) tab[i] = out[i] * norm;
          tab[TABLE_SIZE] = tab[0];
          mips.push(tab);
        }
        frames.push(mips);
      }
      tables.push(frames);
    }
    return tables;
  }

  // ───────────────────── Utilitaires ─────────────────────
  const clamp = (x, a, b) => (x < a ? a : x > b ? b : x);
  const mtof = (m) => 440 * Math.pow(2, (m - 69) / 12);

  // Générateur pseudo-aléatoire identique au C++ (xorshift32)
  function Rng(seed) { this.s = seed >>> 0 || 1; }
  Rng.prototype.next = function () {
    let x = this.s;
    x ^= x << 13; x >>>= 0;
    x ^= x >>> 17;
    x ^= x << 5; x >>>= 0;
    this.s = x;
    return x / 4294967296;
  };

  function readTable(tbl, frameF, mip, phase) {
    const fi = Math.min(FRAMES - 2, Math.floor(frameF));
    const fu = frameF - fi;
    const ta = tbl[fi][mip], tb = tbl[fi + 1][mip];
    const x = phase * TABLE_SIZE;
    const i = x | 0, u = x - i;
    const a = ta[i] + (ta[i + 1] - ta[i]) * u;
    const b = tb[i] + (tb[i + 1] - tb[i]) * u;
    return a + (b - a) * fu;
  }

  function warpPhase(p, mode, w) {
    if (w <= 0) return p;
    switch (mode) {
      case 0: return Math.pow(p, 1 + w * 3); // Bend
      case 1: { const q = p * (1 + w * 7); return q - Math.floor(q); } // Sync
      default: { const m = p < 0.5 ? 2 * p : 2 - 2 * p; return p + (m * 0.5 - p) * w; } // Mirror
    }
  }

  // ───────────────────── Enveloppe ADSR ─────────────────────
  function Env() { this.stage = 0; this.v = 0; } // 0 idle 1 att 2 dec 3 sus 4 rel
  Env.prototype.gate = function (on) { this.stage = on ? 1 : (this.stage ? 4 : 0); };
  Env.prototype.tick = function (a, d, s, r, sr, n) {
    // avance de n échantillons
    switch (this.stage) {
      case 1:
        this.v += n / (a * sr);
        if (this.v >= 1) { this.v = 1; this.stage = 2; }
        break;
      case 2: {
        const c = Math.exp(-n / (d * sr * 0.25));
        this.v = s + (this.v - s) * c;
        if (this.v - s < 1e-4) { this.v = s; this.stage = 3; }
        break;
      }
      case 3: this.v = s; break;
      case 4: {
        const c = Math.exp(-n / (r * sr * 0.25));
        this.v *= c;
        if (this.v < 1e-4) { this.v = 0; this.stage = 0; }
        break;
      }
    }
    return this.v;
  };

  // ───────────────────── LFO (unipolaire 0..1) ─────────────────────
  function lfoValue(shape, ph, held) {
    switch (shape) {
      case 0: return 0.5 - 0.5 * Math.cos(TWO_PI * ph);
      case 1: return ph < 0.5 ? ph * 2 : 2 - ph * 2;
      case 2: return ph;
      case 3: return 1 - ph;
      case 4: return ph < 0.5 ? 1 : 0;
      default: return held;
    }
  }

  // ───────────────────── Filtre SVF (TPT / Simper) ─────────────────────
  function Svf() { this.ic1 = 0; this.ic2 = 0; }
  Svf.prototype.reset = function () { this.ic1 = 0; this.ic2 = 0; };
  // renvoie [lp, bp, hp] via champs
  Svf.prototype.run = function (x, g, k) {
    const a1 = 1 / (1 + g * (g + k));
    const v3 = x - this.ic2;
    const v1 = a1 * this.ic1 + g * a1 * v3;
    const v2 = this.ic2 + g * v1;
    this.ic1 = 2 * v1 - this.ic1;
    this.ic2 = 2 * v2 - this.ic2;
    this.lp = v2; this.bp = v1; this.hp = x - k * v1 - v2;
  };

  // ───────────────────── Voix ─────────────────────
  const MAX_UNI = 7;
  function Osc() {
    this.ph = new Float64Array(MAX_UNI);
  }

  function Voice(eng) {
    this.eng = eng;
    this.note = -1; this.vel = 0; this.active = false; this.gate = false; this.age = 0;
    this.pitch = 60; this.targetPitch = 60;
    this.oscA = new Osc(); this.oscB = new Osc();
    this.subPh = 0;
    this.env1 = new Env(); this.env2 = new Env();
    this.lfoPh = [0, 0]; this.lfoHeld = [0, 0];
    this.fL = [new Svf(), new Svf()]; this.fR = [new Svf(), new Svf()];
    this.mod = new Float32Array(eng.numDests);
    this.ctl = 0;
    this.g = 0.5; this.k = 1.4;
    // valeurs de contrôle calculées au taux de contrôle
    this.c = {};
    this.ampPrev = 0; this.ampCur = 0;
  }

  Voice.prototype.start = function (note, vel, retrigger, glideFrom) {
    const e = this.eng;
    this.note = note; this.vel = vel; this.gate = true; this.active = true;
    this.targetPitch = note;
    if (glideFrom === undefined || glideFrom === null) this.pitch = note;
    else this.pitch = glideFrom;
    if (retrigger) {
      this.env1.gate(true); this.env2.gate(true);
      for (let l = 0; l < 2; l++) {
        if (e.p[e.ix["lfo" + (l + 1) + ".retrig"]] > 0.5) this.lfoPh[l] = 0;
        else this.lfoPh[l] = e.lfoFree[l];
        this.lfoHeld[l] = e.rng.next();
      }
      if (!this.wasActive) {
        for (let u = 0; u < MAX_UNI; u++) {
          this.oscA.ph[u] = u === 0 ? 0 : e.rng.next();
          this.oscB.ph[u] = u === 0 ? 0 : e.rng.next();
        }
        this.subPh = 0;
        this.fL[0].reset(); this.fL[1].reset(); this.fR[0].reset(); this.fR[1].reset();
      }
    }
    this.ctl = 0;
  };

  Voice.prototype.release = function () {
    this.gate = false;
    this.env1.gate(false); this.env2.gate(false);
  };

  Voice.prototype.control = function () {
    const e = this.eng, p = e.p, ix = e.ix, sr = e.sr, n = CONTROL_RATE;
    // glide
    const glide = p[ix["global.glide"]];
    if (glide > 0.0005) {
      const c = Math.exp(-n / (glide * sr * 0.25));
      this.pitch = this.targetPitch + (this.pitch - this.targetPitch) * c;
    } else this.pitch = this.targetPitch;

    const env2 = this.env2.tick(p[ix["env2.a"]], p[ix["env2.d"]], p[ix["env2.s"]], p[ix["env2.r"]], sr, n);

    // LFOs (la vitesse peut être modulée : on utilise la modulation du bloc précédent)
    const lfo = [0, 0];
    for (let l = 0; l < 2; l++) {
      const pre = "lfo" + (l + 1) + ".";
      let rate;
      if (p[ix[pre + "sync"]] > 0.5) rate = (e.bpm / 60) / e.divBeats[p[ix[pre + "div"]] | 0];
      else rate = p[ix[pre + "rate"]];
      rate *= Math.pow(2, this.mod[e.D.LFO1Rate + l]);
      let ph = this.lfoPh[l] + (rate * n) / sr;
      if (ph >= 1) { ph -= Math.floor(ph); this.lfoHeld[l] = e.rng.next(); }
      this.lfoPh[l] = ph;
      lfo[l] = lfoValue(p[ix[pre + "shape"]] | 0, ph, this.lfoHeld[l]);
    }

    // matrice de modulation
    const mod = this.mod;
    mod.fill(0);
    const srcVals = e.srcTmp;
    srcVals[0] = 0; srcVals[1] = lfo[0]; srcVals[2] = lfo[1]; srcVals[3] = env2; srcVals[4] = this.vel;
    srcVals[5] = p[ix.macro1]; srcVals[6] = p[ix.macro2]; srcVals[7] = p[ix.macro3]; srcVals[8] = p[ix.macro4];
    srcVals[9] = e.modWheel; srcVals[10] = clamp((this.note - 24) / 72, 0, 1);
    for (let s = 0; s < e.numSlots; s++) {
      const src = p[e.slotIx[s * 3]] | 0, dst = p[e.slotIx[s * 3 + 1]] | 0;
      if (src === 0 || dst === 0) continue;
      mod[dst] += p[e.slotIx[s * 3 + 2]] * srcVals[src] * e.destScale[dst];
    }

    // filtre
    const D = e.D;
    let cutSt = 12 * Math.log2(p[ix["filter.cutoff"]] / 440) + 69;
    cutSt += mod[D.Cutoff] + p[ix["filter.env"]] * env2 * 72 + p[ix["filter.keytrack"]] * (this.pitch - 60);
    const fc = clamp(mtof(cutSt), 16, sr * 0.45);
    this.g = Math.tan((Math.PI * fc) / sr);
    const res = clamp(p[ix["filter.res"]] + mod[D.Resonance], 0, 1);
    this.k = 2 - 1.96 * res;
    const c = this.c;
    c.drive = clamp(p[ix["filter.drive"]] + mod[D.FilterDrive], 0, 1);

    // oscillateurs
    const bend = e.bend * p[ix["global.bend"]];
    for (const o of ["A", "B"]) {
      const pre = "osc" + o + ".";
      const pitch = this.pitch + p[ix[pre + "semi"]] + p[ix[pre + "fine"]] / 100 + bend + mod[o === "A" ? D.OscAPitch : D.OscBPitch];
      const f = mtof(pitch);
      c[o + "inc"] = f / sr;
      const hmax = Math.max(1, Math.floor((0.5 * sr) / f));
      let mip = 0;
      while (mip < MIPS - 1 && (1024 >> mip) > hmax) mip++;
      c[o + "mip"] = mip;
      c[o + "pos"] = clamp(p[ix[pre + "pos"]] + mod[o === "A" ? D.OscAPos : D.OscBPos], 0, 1) * (FRAMES - 1);
      c[o + "warp"] = clamp(p[ix[pre + "warp"]] + mod[o === "A" ? D.OscAWarp : D.OscBWarp], 0, 1);
      c[o + "lvl"] = clamp(p[ix[pre + "level"]] + mod[o === "A" ? D.OscALevel : D.OscBLevel], 0, 1);
      c[o + "det"] = clamp(p[ix[pre + "detune"]] + mod[D.Detune], 0, 1);
    }
    c.fm = clamp(p[ix["fm.amount"]] + mod[D.FMAmount], 0, 1);
    c.sub = clamp(p[ix["sub.level"]] + mod[D.SubLevel], 0, 1);
    c.noise = clamp(p[ix["noise.level"]] + mod[D.NoiseLevel], 0, 1);
    c.subInc = mtof(this.pitch + bend - 12 * (1 + (p[ix["sub.oct"]] | 0))) / sr;
    c.amp = clamp(1 + mod[D.Amp], 0, 2);

    // l'enveloppe d'amplitude est interpolée linéairement sur le bloc
    this.ampPrev = this.ampCur;
    this.ampCur = this.env1.tick(p[ix["env1.a"]], p[ix["env1.d"]], p[ix["env1.s"]], p[ix["env1.r"]], sr, n);
    if (this.env1.stage === 0 && this.ampPrev < 1e-4) this.active = false;
  };

  Voice.prototype.render = function (outL, outR, start, len) {
    const e = this.eng, p = e.p, ix = e.ix, c = this.c;
    const onA = p[ix["oscA.on"]] > 0.5, onB = p[ix["oscB.on"]] > 0.5;
    const fm = c.fm;
    const runB = onB || fm > 0;
    const uniA = p[ix["oscA.unison"]] | 0, uniB = p[ix["oscB.unison"]] | 0;
    const tA = e.tables[p[ix["oscA.table"]] | 0], tB = e.tables[p[ix["oscB.table"]] | 0];
    const wmA = p[ix["oscA.warpMode"]] | 0, wmB = p[ix["oscB.warpMode"]] | 0;
    const sprA = p[ix["oscA.spread"]], sprB = p[ix["oscB.spread"]];
    const subOn = p[ix["sub.on"]] > 0.5, subShape = p[ix["sub.shape"]] | 0;
    const fOn = p[ix["filter.on"]] > 0.5, fType = p[ix["filter.type"]] | 0;
    const drive = c.drive, dGain = 1 + drive * 9, dNorm = 1 / Math.tanh(dGain) ;
    const gA = 1 / Math.sqrt(uniA), gB = 1 / Math.sqrt(uniB);

    // incréments et gains de pan par voix d'unisson (calculés une fois par bloc)
    const incA = e.tmpIncA, incB = e.tmpIncB, panLA = e.tmpPLA, panRA = e.tmpPRA, panLB = e.tmpPLB, panRB = e.tmpPRB;
    for (let u = 0; u < uniA; u++) {
      const sp = uniA > 1 ? (2 * u) / (uniA - 1) - 1 : 0;
      incA[u] = c.Ainc * Math.pow(2, (sp * c.Adet * 50) / 1200);
      const pan = sp * sprA;
      panLA[u] = Math.sqrt(0.5 * (1 - pan)); panRA[u] = Math.sqrt(0.5 * (1 + pan));
    }
    for (let u = 0; u < uniB; u++) {
      const sp = uniB > 1 ? (2 * u) / (uniB - 1) - 1 : 0;
      incB[u] = c.Binc * Math.pow(2, (sp * c.Bdet * 50) / 1200);
      const pan = sp * sprB;
      panLB[u] = Math.sqrt(0.5 * (1 - pan)); panRB[u] = Math.sqrt(0.5 * (1 + pan));
    }

    const phA = this.oscA.ph, phB = this.oscB.ph;
    const g = this.g, k = this.k;
    const fL0 = this.fL[0], fL1 = this.fL[1], fR0 = this.fR[0], fR1 = this.fR[1];
    const ampStep = (this.ampCur - this.ampPrev) / len;
    let amp = this.ampPrev;
    const vgain = c.amp * 0.5;

    for (let i = 0; i < len; i++) {
      let L = 0, R = 0;
      // Osc B (aussi modulateur FM)
      let modB = 0;
      if (runB) {
        for (let u = 0; u < uniB; u++) {
          const s = readTable(tB, c.Bpos, c.Bmip, warpPhase(phB[u], wmB, c.Bwarp));
          if (u === 0) modB = s;
          if (onB) { const v = s * c.Blvl * gB; L += v * panLB[u]; R += v * panRB[u]; }
          let ph = phB[u] + incB[u];
          phB[u] = ph - Math.floor(ph);
        }
      }
      if (onA) {
        for (let u = 0; u < uniA; u++) {
          let ph = phA[u] + fm * 2 * modB;
          ph -= Math.floor(ph);
          const s = readTable(tA, c.Apos, c.Amip, warpPhase(ph, wmA, c.Awarp));
          const v = s * c.Alvl * gA;
          L += v * panLA[u]; R += v * panRA[u];
          ph = phA[u] + incA[u];
          phA[u] = ph - Math.floor(ph);
        }
      }
      if (subOn) {
        const ph = this.subPh;
        let s;
        if (subShape === 0) s = Math.sin(TWO_PI * ph);
        else if (subShape === 1) s = ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph;
        else s = ph < 0.5 ? 0.8 : -0.8;
        s *= c.sub * 0.7071;
        L += s; R += s;
        const q = ph + c.subInc;
        this.subPh = q - Math.floor(q);
      }
      if (c.noise > 0) {
        L += (e.rng.next() * 2 - 1) * c.noise * 0.5;
        R += (e.rng.next() * 2 - 1) * c.noise * 0.5;
      }

      if (fOn) {
        if (drive > 0) { L = Math.tanh(L * dGain) * dNorm; R = Math.tanh(R * dGain) * dNorm; }
        fL0.run(L, g, k); fR0.run(R, g, k);
        switch (fType) {
          case 0: fL1.run(fL0.lp, g, k); fR1.run(fR0.lp, g, k); L = fL1.lp; R = fR1.lp; break;
          case 1: L = fL0.lp; R = fR0.lp; break;
          case 2: L = fL0.bp * k; R = fR0.bp * k; break;
          case 3: L = fL0.hp; R = fR0.hp; break;
          default: L = fL0.lp + fL0.hp; R = fR0.lp + fR0.hp; break;
        }
      }
      amp += ampStep;
      const a = amp * vgain;
      outL[start + i] += L * a;
      outR[start + i] += R * a;
    }
  };

  // ───────────────────── Effets ─────────────────────
  function DelayLine(n) { this.buf = new Float32Array(n); this.w = 0; }
  DelayLine.prototype.read = function (d) {
    const n = this.buf.length;
    let r = this.w - d;
    while (r < 0) r += n;
    const i = r | 0, u = r - i;
    const a = this.buf[i], b = this.buf[(i + 1) % n];
    return a + (b - a) * u;
  };
  DelayLine.prototype.write = function (x) { this.buf[this.w] = x; this.w = (this.w + 1) % this.buf.length; };

  // Freeverb
  const COMB = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617];
  const ALLP = [556, 441, 341, 225];
  function Reverb(sr) {
    const sc = sr / 44100;
    this.combL = COMB.map((d) => ({ b: new Float32Array(Math.round(d * sc)), i: 0, f: 0 }));
    this.combR = COMB.map((d) => ({ b: new Float32Array(Math.round((d + 23) * sc)), i: 0, f: 0 }));
    this.apL = ALLP.map((d) => ({ b: new Float32Array(Math.round(d * sc)), i: 0 }));
    this.apR = ALLP.map((d) => ({ b: new Float32Array(Math.round((d + 23) * sc)), i: 0 }));
  }
  function combRun(c, x, fb, damp) {
    const y = c.b[c.i];
    c.f = y * (1 - damp) + c.f * damp;
    c.b[c.i] = x + c.f * fb;
    if (++c.i >= c.b.length) c.i = 0;
    return y;
  }
  function apRun(a, x) {
    const y = a.b[a.i];
    a.b[a.i] = x + y * 0.5;
    if (++a.i >= a.b.length) a.i = 0;
    return y - x;
  }
  Reverb.prototype.run = function (l, r, size, damp) {
    const fb = 0.7 + size * 0.28, dm = damp * 0.4;
    const x = (l + r) * 0.015;
    let ol = 0, or = 0;
    for (let i = 0; i < 8; i++) { ol += combRun(this.combL[i], x, fb, dm); or += combRun(this.combR[i], x, fb, dm); }
    for (let i = 0; i < 4; i++) { ol = apRun(this.apL[i], ol); or = apRun(this.apR[i], or); }
    this.l = ol; this.r = or;
  };

  // ───────────────────── Moteur ─────────────────────
  function Engine(spec, sr, seed) {
    this.spec = spec;
    this.sr = sr;
    this.ix = {};
    spec.params.forEach((d, i) => (this.ix[d.id] = i));
    this.p = new Float32Array(spec.params.length);
    spec.params.forEach((d, i) => (this.p[i] = d.def));
    this.divBeats = spec.divBeats;
    this.destScale = spec.modDests.map((d) => d.scale);
    this.numDests = spec.modDests.length;
    this.D = {};
    spec.modDests.forEach((d, i) => (this.D[d.name.replace(/[^A-Za-z0-9]/g, "")] = i));
    this.numSlots = spec.params.filter((d) => /^mod\d+\.src$/.test(d.id)).length;
    this.slotIx = new Int32Array(this.numSlots * 3);
    for (let s = 0; s < this.numSlots; s++) {
      this.slotIx[s * 3] = this.ix[`mod${s + 1}.src`];
      this.slotIx[s * 3 + 1] = this.ix[`mod${s + 1}.dst`];
      this.slotIx[s * 3 + 2] = this.ix[`mod${s + 1}.amt`];
    }
    this.srcTmp = new Float32Array(spec.modSources.length);
    this.tables = buildTables(spec.tables.length);
    this.rng = new Rng(seed || 0x9e3779b9);
    this.voices = [];
    for (let v = 0; v < 8; v++) this.voices.push(new Voice(this));
    this.tmpIncA = new Float64Array(MAX_UNI); this.tmpIncB = new Float64Array(MAX_UNI);
    this.tmpPLA = new Float32Array(MAX_UNI); this.tmpPRA = new Float32Array(MAX_UNI);
    this.tmpPLB = new Float32Array(MAX_UNI); this.tmpPRB = new Float32Array(MAX_UNI);
    this.held = []; // pile de notes (mono)
    this.bpm = 140; this.bend = 0; this.modWheel = 0;
    this.lfoFree = [0, 0];
    this.counter = 0; this.lastVoice = null;
    this.bufL = new Float32Array(CONTROL_RATE); this.bufR = new Float32Array(CONTROL_RATE);
    // FX
    this.delayL = new DelayLine(Math.ceil(sr * 3)); this.delayR = new DelayLine(Math.ceil(sr * 3));
    this.dlyLP_L = 0; this.dlyLP_R = 0; this.dlyTime = sr * 0.25;
    this.reverb = new Reverb(sr);
    this.crushHold = [0, 0]; this.crushCnt = 0;
    this.dcX = [0, 0]; this.dcY = [0, 0];
    this.compEnv = 0;
  }

  Engine.prototype.setParam = function (id, value) {
    const i = typeof id === "number" ? id : this.ix[id];
    if (i !== undefined) this.p[i] = value;
  };

  Engine.prototype.allNotesOff = function () {
    this.held.length = 0;
    for (const v of this.voices) if (v.active) v.release();
  };

  Engine.prototype.noteOn = function (note, vel) {
    const poly = this.p[this.ix["global.poly"]] | 0;
    if (poly <= 1) {
      const v = this.voices[0];
      const legato = this.p[this.ix["global.legato"]] > 0.5;
      const wasHeld = this.held.length > 0 && v.gate;
      this.held = this.held.filter((n) => n !== note);
      this.held.push(note);
      const glide = this.p[this.ix["global.glide"]] > 0.0005;
      v.wasActive = v.active;
      if (wasHeld && legato) { v.targetPitch = note; v.note = note; if (!glide) v.pitch = note; }
      else v.start(note, vel, true, v.active && glide ? v.pitch : null);
      this.lastVoice = v;
      return;
    }
    let pick = null;
    for (let i = 0; i < poly; i++) if (!this.voices[i].active) { pick = this.voices[i]; break; }
    if (!pick) {
      pick = this.voices[0];
      for (let i = 1; i < poly; i++) if (this.voices[i].age < pick.age) pick = this.voices[i];
    }
    pick.wasActive = pick.active && pick.note === note;
    pick.age = ++this.counter;
    pick.start(note, vel, true, null);
    this.lastVoice = pick;
  };

  Engine.prototype.noteOff = function (note) {
    const poly = this.p[this.ix["global.poly"]] | 0;
    if (poly <= 1) {
      const v = this.voices[0];
      const wasTop = this.held[this.held.length - 1] === note;
      this.held = this.held.filter((n) => n !== note);
      if (!wasTop) return;
      if (this.held.length) {
        const n = this.held[this.held.length - 1];
        const legato = this.p[this.ix["global.legato"]] > 0.5;
        if (legato) { v.targetPitch = n; v.note = n; if (this.p[this.ix["global.glide"]] <= 0.0005) v.pitch = n; }
        else { v.wasActive = true; v.start(n, v.vel, true, this.p[this.ix["global.glide"]] > 0.0005 ? v.pitch : null); }
      } else v.release();
      return;
    }
    for (const v of this.voices) if (v.active && v.gate && v.note === note) v.release();
  };

  const DIST_TYPES = 5;
  Engine.prototype.process = function (outL, outR, n) {
    const p = this.p, ix = this.ix, sr = this.sr;
    let off = 0;
    while (off < n) {
      const len = Math.min(CONTROL_RATE, n - off);
      const bL = this.bufL, bR = this.bufR;
      bL.fill(0); bR.fill(0);
      // LFO libres (pour les LFO sans retrig)
      for (let l = 0; l < 2; l++) {
        const pre = "lfo" + (l + 1) + ".";
        const rate = p[ix[pre + "sync"]] > 0.5 ? (this.bpm / 60) / this.divBeats[p[ix[pre + "div"]] | 0] : p[ix[pre + "rate"]];
        const ph = this.lfoFree[l] + (rate * len) / sr;
        this.lfoFree[l] = ph - Math.floor(ph);
      }
      for (const v of this.voices) {
        if (!v.active) continue;
        v.control();
        v.render(bL, bR, 0, len);
      }
      // modulations globales : voix la plus récente
      const lv = this.lastVoice && this.lastVoice.active ? this.lastVoice.mod : null;
      const gm = (d) => (lv ? lv[d] : 0);
      this.processFx(bL, bR, len, gm(this.D.DistDrive), gm(this.D.DelayMix), gm(this.D.ReverbMix));
      const vol = p[ix["global.volume"]];
      for (let i = 0; i < len; i++) {
        outL[off + i] = clamp(bL[i] * vol, -1, 1);
        outR[off + i] = clamp(bR[i] * vol, -1, 1);
      }
      off += len;
    }
  };

  Engine.prototype.processFx = function (L, R, n, mDist, mDelay, mRev) {
    const p = this.p, ix = this.ix, sr = this.sr;
    // ── Distorsion ──
    if (p[ix["fx.dist.on"]] > 0.5) {
      const type = p[ix["fx.dist.type"]] | 0;
      const drive = clamp(p[ix["fx.dist.drive"]] + mDist, 0, 1);
      const mix = p[ix["fx.dist.mix"]];
      const gain = Math.pow(10, (drive * 36) / 20);
      const bits = 16 - drive * 13, q = Math.pow(2, bits - 1), hold = 1 + Math.floor(drive * 15);
      for (let i = 0; i < n; i++) {
        for (let ch = 0; ch < 2; ch++) {
          const buf = ch ? R : L;
          const x = buf[i];
          let y;
          switch (type) {
            case 0: y = Math.tanh(x * gain); break;
            case 1: y = clamp(x * gain, -1, 1); break;
            case 2: y = Math.sin(x * gain * 1.5707963); break;
            case 3: {
              if (ch === 0 && this.crushCnt++ % hold === 0) { this.crushHold[0] = L[i]; this.crushHold[1] = R[i]; }
              y = Math.round(clamp(this.crushHold[ch], -1, 1) * q) / q;
              break;
            }
            default: y = Math.tanh(Math.abs(x) * gain) * 2 - Math.tanh(gain * 0.5); break;
          }
          // bloqueur de DC
          const dy = y - this.dcX[ch] + 0.995 * this.dcY[ch];
          this.dcX[ch] = y; this.dcY[ch] = dy;
          buf[i] = x + (dy - x) * mix;
        }
      }
    }
    // ── Squash (compression haute + basse vers une cible, façon OTT) ──
    if (p[ix["fx.comp.on"]] > 0.5) {
      const amt = p[ix["fx.comp.amount"]], mix = p[ix["fx.comp.mix"]];
      const target = 0.25, att = Math.exp(-1 / (0.001 * sr)), rel = Math.exp(-1 / (0.08 * sr));
      const maxUp = 1 + amt * 7, expo = amt * 0.85;
      for (let i = 0; i < n; i++) {
        const lvl = Math.max(Math.abs(L[i]), Math.abs(R[i]));
        const c = lvl > this.compEnv ? att : rel;
        this.compEnv = lvl + (this.compEnv - lvl) * c;
        const env = Math.max(this.compEnv, 1e-5);
        const g = clamp(Math.pow(target / env, expo), 0.05, maxUp) * (1 + amt * 0.5);
        L[i] += (L[i] * g - L[i]) * mix;
        R[i] += (R[i] * g - R[i]) * mix;
      }
    }
    // ── Delay synchronisé ──
    if (p[ix["fx.delay.on"]] > 0.5) {
      const target = Math.min(sr * 2.9, (60 / this.bpm) * this.divBeats[p[ix["fx.delay.div"]] | 0] * sr);
      const fb = p[ix["fx.delay.fb"]], mix = clamp(p[ix["fx.delay.mix"]] + mDelay, 0, 1);
      const pp = p[ix["fx.delay.pingpong"]] > 0.5;
      const lpc = Math.exp((-2 * Math.PI * 6000) / sr);
      for (let i = 0; i < n; i++) {
        this.dlyTime += (target - this.dlyTime) * 0.0005;
        let dl = this.delayL.read(this.dlyTime), dr = this.delayR.read(this.dlyTime);
        this.dlyLP_L = dl + (this.dlyLP_L - dl) * lpc; dl = this.dlyLP_L;
        this.dlyLP_R = dr + (this.dlyLP_R - dr) * lpc; dr = this.dlyLP_R;
        if (pp) {
          this.delayL.write((L[i] + R[i]) * 0.5 + dr * fb);
          this.delayR.write(dl * fb);
        } else {
          this.delayL.write(L[i] + dl * fb);
          this.delayR.write(R[i] + dr * fb);
        }
        L[i] += dl * mix; R[i] += dr * mix;
      }
    }
    // ── Reverb ──
    if (p[ix["fx.reverb.on"]] > 0.5) {
      const size = p[ix["fx.reverb.size"]], damp = p[ix["fx.reverb.damp"]];
      const mix = clamp(p[ix["fx.reverb.mix"]] + mRev, 0, 1);
      const rv = this.reverb;
      for (let i = 0; i < n; i++) {
        rv.run(L[i], R[i], size, damp);
        L[i] = L[i] * (1 - mix * 0.5) + rv.l * mix;
        R[i] = R[i] * (1 - mix * 0.5) + rv.r * mix;
      }
    }
  };

  // Lecture d'une frame de wavetable pour l'affichage (mip 3 = 128 harmoniques)
  Engine.prototype.framePreview = function (table, pos, warpMode, warp, points) {
    const out = new Float32Array(points);
    const tbl = this.tables[table];
    for (let i = 0; i < points; i++) out[i] = readTable(tbl, pos * (FRAMES - 1), 2, warpPhase(i / points, warpMode, warp));
    return out;
  };

  G.MasalaDSP = { Engine, buildTables, TABLE_SIZE, FRAMES, MIPS, CONTROL_RATE };

  // ───────────────────── AudioWorkletProcessor ─────────────────────
  if (typeof registerProcessor === "function") {
    class MasalaProcessor extends AudioWorkletProcessor {
      constructor(options) {
        super();
        this.engine = new Engine(options.processorOptions.spec, sampleRate);
        this.meterAcc = 0; this.meterN = 0;
        this.port.onmessage = (ev) => {
          const m = ev.data, e = this.engine;
          switch (m.type) {
            case "param": e.setParam(m.id, m.value); break;
            case "params": for (let i = 0; i < m.values.length; i++) e.p[i] = m.values[i]; break;
            case "noteOn": e.noteOn(m.note, m.vel); break;
            case "noteOff": e.noteOff(m.note); break;
            case "allOff": e.allNotesOff(); break;
            case "bpm": e.bpm = m.value; break;
            case "bend": e.bend = m.value; break;
            case "wheel": e.modWheel = m.value; break;
          }
        };
        this.port.postMessage({ type: "ready" });
      }
      process(inputs, outputs) {
        const out = outputs[0];
        const L = out[0], R = out[1] || out[0];
        this.engine.process(L, R, L.length);
        let peak = 0;
        for (let i = 0; i < L.length; i++) peak = Math.max(peak, Math.abs(L[i]));
        this.meterAcc = Math.max(this.meterAcc, peak);
        if (++this.meterN >= 8) {
          const lv = this.engine.lastVoice;
          this.port.postMessage({ type: "meter", peak: this.meterAcc, mod: lv && lv.active ? Array.from(lv.mod) : null });
          this.meterAcc = 0; this.meterN = 0;
        }
        return true;
      }
    }
    registerProcessor("masala", MasalaProcessor);
  }
})(typeof globalThis !== "undefined" ? globalThis : this);
