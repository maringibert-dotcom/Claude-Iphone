// Serveur « compte épices » : comptes, presets dans le cloud, appairage du plugin.
// Aucune dépendance : Node ≥ 18.   node server/server.mjs   (PORT=8787 par défaut)
//
// Sert aussi l'appli web (dossier app/ et shared/) pour l'installer sur iPhone.
//
// API (JSON) :
//   POST   /api/register      {email, password}        → {token, email}
//   POST   /api/login         {email, password}        → {token, email}
//   GET    /api/me                                     → {email}
//   DELETE /api/me            {password}               → {ok}   (supprime le compte et ses données)
//   GET    /api/presets                                → {presets:[…]}
//   PUT    /api/presets/:id   preset                   → {ok, preset}
//   DELETE /api/presets/:id                            → {ok}
//   POST   /api/pair                                   → {code, expiresAt}   (appli connectée)
//   POST   /api/pair/claim    {code}                   → {token, email}       (plugin)
import http from "node:http";
import { readFile, writeFile, mkdir, rename } from "node:fs/promises";
import { existsSync, createReadStream, statSync } from "node:fs";
import { join, dirname, extname, normalize, sep } from "node:path";
import { fileURLToPath } from "node:url";
import crypto from "node:crypto";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..");
const DATA_DIR = process.env.DATA_DIR || join(ROOT, "server", "data");
const DB_FILE = join(DATA_DIR, "db.json");
const PORT = +(process.env.PORT || 8787);
const MAX_BODY = 512 * 1024;
const MAX_PRESETS = 2000;
const TOKEN_TTL = 1000 * 60 * 60 * 24 * 365;
const PAIR_TTL = 1000 * 60 * 10;

// ───────── stockage (fichier JSON, écriture atomique) ─────────
let db = { users: {}, sessions: {}, pairs: {} };
if (existsSync(DB_FILE)) db = JSON.parse(await readFile(DB_FILE, "utf8"));
let saving = Promise.resolve();
function persist() {
  saving = saving.then(async () => {
    await mkdir(DATA_DIR, { recursive: true });
    const tmp = DB_FILE + ".tmp";
    await writeFile(tmp, JSON.stringify(db));
    await rename(tmp, DB_FILE);
  }).catch((e) => console.error("écriture db :", e));
  return saving;
}

const sha = (s) => crypto.createHash("sha256").update(s).digest("hex");
function hashPassword(pw, salt = crypto.randomBytes(16).toString("hex")) {
  const h = crypto.scryptSync(pw, salt, 64).toString("hex");
  return { salt, hash: h };
}
function checkPassword(pw, user) {
  const h = crypto.scryptSync(pw, user.salt, 64);
  return crypto.timingSafeEqual(h, Buffer.from(user.hash, "hex"));
}
function newSession(email, label) {
  const token = crypto.randomBytes(32).toString("base64url");
  db.sessions[sha(token)] = { email, label, created: Date.now(), expires: Date.now() + TOKEN_TTL };
  return token;
}
function auth(req) {
  const m = /^Bearer (.+)$/.exec(req.headers.authorization || "");
  if (!m) return null;
  const s = db.sessions[sha(m[1])];
  if (!s || s.expires < Date.now()) return null;
  return db.users[s.email] ? s.email : null;
}

// ───────── limitation de débit simple (par IP) ─────────
const hits = new Map();
function limited(ip, max = 30) {
  const now = Date.now(), w = hits.get(ip) || [];
  const recent = w.filter((t) => now - t < 60_000);
  recent.push(now); hits.set(ip, recent);
  return recent.length > max;
}

// ───────── validation d'un preset ─────────
function cleanPreset(p, id) {
  if (!p || typeof p !== "object" || p.format !== "epices.masala.preset") throw new Error("format de preset invalide");
  if (typeof p.name !== "string" || !p.name.trim() || p.name.length > 60) throw new Error("nom invalide");
  if (!p.params || typeof p.params !== "object") throw new Error("paramètres manquants");
  const params = {};
  for (const [k, v] of Object.entries(p.params)) {
    if (!/^[a-zA-Z0-9.]{1,40}$/.test(k)) continue;
    if (typeof v === "number" && isFinite(v)) params[k] = v;
    else if (typeof v === "string" && v.length <= 40) params[k] = v;
    else if (typeof v === "boolean") params[k] = v ? 1 : 0;
  }
  const str = (s, n) => (typeof s === "string" ? s.slice(0, n) : "");
  return {
    format: "epices.masala.preset",
    version: 1,
    id,
    name: p.name.trim(),
    category: str(p.category, 30) || "Mes presets",
    author: str(p.author, 60),
    tags: Array.isArray(p.tags) ? p.tags.slice(0, 10).map((t) => str(t, 20)) : [],
    macros: Array.isArray(p.macros) ? p.macros.slice(0, 4).map((t) => str(t, 16)) : [],
    params,
    updatedAt: Number.isFinite(p.updatedAt) ? p.updatedAt : Date.now(),
  };
}

// ───────── HTTP ─────────
const send = (res, code, obj) => {
  res.writeHead(code, { "Content-Type": "application/json; charset=utf-8", ...CORS });
  res.end(JSON.stringify(obj));
};
const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
  "Access-Control-Allow-Methods": "GET, POST, PUT, DELETE, OPTIONS",
};
async function body(req) {
  let size = 0; const chunks = [];
  for await (const c of req) {
    size += c.length;
    if (size > MAX_BODY) throw Object.assign(new Error("requête trop volumineuse"), { code: 413 });
    chunks.push(c);
  }
  try { return chunks.length ? JSON.parse(Buffer.concat(chunks).toString("utf8")) : {}; }
  catch { throw Object.assign(new Error("JSON invalide"), { code: 400 }); }
}

async function api(req, res, path) {
  const ip = req.socket.remoteAddress;
  const m = req.method;

  if (path === "/health") return send(res, 200, { ok: true, service: "epices-masala" });

  if ((path === "/register" || path === "/login") && m === "POST") {
    if (limited(ip, 20)) return send(res, 429, { error: "Trop de tentatives, réessaie dans une minute." });
    const { email, password } = await body(req);
    const e = String(email || "").trim().toLowerCase();
    if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(e)) return send(res, 400, { error: "E-mail invalide." });
    if (typeof password !== "string" || password.length < 8) return send(res, 400, { error: "Mot de passe : 8 caractères minimum." });
    if (path === "/register") {
      if (db.users[e]) return send(res, 409, { error: "Ce compte existe déjà." });
      db.users[e] = { ...hashPassword(password), created: Date.now(), presets: {} };
    } else if (!db.users[e] || !checkPassword(password, db.users[e])) {
      return send(res, 401, { error: "E-mail ou mot de passe incorrect." });
    }
    const token = newSession(e, "app");
    await persist();
    return send(res, 200, { token, email: e });
  }

  if (path === "/pair/claim" && m === "POST") {
    if (limited(ip, 10)) return send(res, 429, { error: "Trop de tentatives." });
    const { code } = await body(req);
    const key = String(code || "").replace(/\D/g, "");
    const p = db.pairs[key];
    if (!p || p.expires < Date.now()) return send(res, 404, { error: "Code inconnu ou expiré." });
    delete db.pairs[key];
    const token = newSession(p.email, "plugin");
    await persist();
    return send(res, 200, { token, email: p.email });
  }

  const email = auth(req);
  if (!email) return send(res, 401, { error: "Non connecté." });
  const user = db.users[email];

  if (path === "/me" && m === "GET") return send(res, 200, { email, presets: Object.keys(user.presets).length });

  // Suppression du compte (exigée par l'App Store) : mot de passe requis,
  // efface l'utilisateur, ses presets, ses sessions et ses codes d'appairage.
  if (path === "/me" && m === "DELETE") {
    if (limited(ip, 10)) return send(res, 429, { error: "Trop de tentatives." });
    const { password } = await body(req);
    if (typeof password !== "string" || !checkPassword(password, user)) return send(res, 403, { error: "Mot de passe incorrect." });
    delete db.users[email];
    for (const [k, s] of Object.entries(db.sessions)) if (s.email === email) delete db.sessions[k];
    for (const [k, p] of Object.entries(db.pairs)) if (p.email === email) delete db.pairs[k];
    await persist();
    return send(res, 200, { ok: true });
  }

  if (path === "/pair" && m === "POST") {
    for (const [k, v] of Object.entries(db.pairs)) if (v.expires < Date.now()) delete db.pairs[k];
    let code;
    do code = String(crypto.randomInt(0, 1_000_000)).padStart(6, "0"); while (db.pairs[code]);
    db.pairs[code] = { email, expires: Date.now() + PAIR_TTL };
    await persist();
    return send(res, 200, { code, expiresAt: db.pairs[code].expires });
  }

  if (path === "/presets" && m === "GET") {
    return send(res, 200, { presets: Object.values(user.presets).sort((a, b) => a.name.localeCompare(b.name)) });
  }

  const pm = /^\/presets\/([A-Za-z0-9_\-]{1,64})$/.exec(path);
  if (pm) {
    const id = pm[1];
    if (m === "PUT") {
      if (!user.presets[id] && Object.keys(user.presets).length >= MAX_PRESETS) return send(res, 413, { error: "Limite de presets atteinte." });
      let p;
      try { p = cleanPreset(await body(req), id); } catch (e) { return send(res, e.code || 400, { error: e.message }); }
      user.presets[id] = p;
      await persist();
      return send(res, 200, { ok: true, preset: p });
    }
    if (m === "DELETE") {
      delete user.presets[id];
      await persist();
      return send(res, 200, { ok: true });
    }
    if (m === "GET") return user.presets[id] ? send(res, 200, user.presets[id]) : send(res, 404, { error: "Introuvable." });
  }
  return send(res, 404, { error: "Route inconnue." });
}

const MIME = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8", ".webmanifest": "application/manifest+json", ".svg": "image/svg+xml",
  ".png": "image/png", ".masala": "application/json",
};
function staticFile(req, res, path) {
  if (path === "/") { res.writeHead(302, { Location: "/app/" }); return res.end(); }
  if (!path.startsWith("/app/") && !path.startsWith("/shared/")) { res.writeHead(404); return res.end(); }
  let file = normalize(join(ROOT, decodeURIComponent(path)));
  if (!file.startsWith(ROOT + sep)) { res.writeHead(403); return res.end(); }
  if (path.endsWith("/")) file = join(file, "index.html");
  if (!existsSync(file) || !statSync(file).isFile()) { res.writeHead(404); return res.end("introuvable"); }
  res.writeHead(200, { "Content-Type": MIME[extname(file)] || "application/octet-stream", "Cache-Control": "no-cache" });
  createReadStream(file).pipe(res);
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, "http://x");
  try {
    if (req.method === "OPTIONS") { res.writeHead(204, CORS); return res.end(); }
    if (url.pathname.startsWith("/api/")) return await api(req, res, url.pathname.slice(4));
    return staticFile(req, res, url.pathname);
  } catch (e) {
    console.error(e);
    send(res, e.code && e.code < 600 ? e.code : 500, { error: e.message || "Erreur serveur" });
  }
});
server.listen(PORT, () => console.log(`épices · MASALA → http://localhost:${PORT}/app/`));
