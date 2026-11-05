// MASALA — client du compte épices (synchronisation des presets).
(function (G) {
  "use strict";
  const KEY = "masala.cloud";

  function load() {
    try { return JSON.parse(localStorage.getItem(KEY)) || {}; } catch { return {}; }
  }
  function save(s) { try { localStorage.setItem(KEY, JSON.stringify(s)); } catch {} }

  const Cloud = {
    state: load(),
    get loggedIn() { return !!this.state.token; },
    get server() {
      return (this.state.server || (location.protocol.startsWith("http") ? location.origin : "")).replace(/\/+$/, "");
    },
    setServer(url) { this.state.server = url.trim(); save(this.state); },

    async api(method, path, body) {
      if (!this.server) throw new Error("Aucun serveur configuré");
      const res = await fetch(this.server + "/api" + path, {
        method,
        headers: {
          "Content-Type": "application/json",
          ...(this.state.token ? { Authorization: "Bearer " + this.state.token } : {}),
        },
        body: body ? JSON.stringify(body) : undefined,
      });
      let data = null;
      try { data = await res.json(); } catch {}
      if (!res.ok) {
        if (res.status === 401 && this.state.token) { this.logout(); }
        throw new Error((data && data.error) || `Erreur ${res.status}`);
      }
      return data;
    },

    async register(email, password) {
      const r = await this.api("POST", "/register", { email, password });
      Object.assign(this.state, { token: r.token, email: r.email }); save(this.state);
      return r;
    },
    async login(email, password) {
      const r = await this.api("POST", "/login", { email, password });
      Object.assign(this.state, { token: r.token, email: r.email }); save(this.state);
      return r;
    },
    logout() { delete this.state.token; delete this.state.email; save(this.state); },

    list() { return this.api("GET", "/presets"); },
    put(preset) { return this.api("PUT", "/presets/" + encodeURIComponent(preset.id), preset); },
    remove(id) { return this.api("DELETE", "/presets/" + encodeURIComponent(id)); },
    pairCode() { return this.api("POST", "/pair"); },

    // Fusion bidirectionnelle : le plus récent (updatedAt) gagne.
    async sync(localPresets) {
      const remote = (await this.list()).presets;
      const byId = new Map(remote.map((p) => [p.id, p]));
      let pushed = 0, pulled = 0;
      const merged = new Map(localPresets.map((p) => [p.id, p]));
      for (const lp of localPresets) {
        const rp = byId.get(lp.id);
        if (!rp || (lp.updatedAt || 0) > (rp.updatedAt || 0)) { await this.put(lp); pushed++; }
      }
      for (const rp of remote) {
        const lp = merged.get(rp.id);
        if (!lp || (rp.updatedAt || 0) > (lp.updatedAt || 0)) { merged.set(rp.id, rp); pulled++; }
      }
      return { presets: [...merged.values()], pushed, pulled };
    },
  };

  G.MasalaCloud = Cloud;
})(window);
