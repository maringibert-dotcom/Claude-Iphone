// Service worker : MASALA fonctionne hors-ligne une fois chargé.
const CACHE = "masala-v1";
const CORE = [
  "./", "./index.html", "./css/style.css", "./js/dsp.js", "./js/ui.js", "./js/cloud.js", "./js/app.js",
  "./manifest.webmanifest", "./icons/icon.svg", "./icons/icon-180.png", "./icons/icon-192.png",
  "../shared/params.json", "../shared/presets/factory.json",
];
self.addEventListener("install", (e) => e.waitUntil(caches.open(CACHE).then((c) => c.addAll(CORE)).then(() => self.skipWaiting())));
self.addEventListener("activate", (e) => e.waitUntil(
  caches.keys().then((ks) => Promise.all(ks.filter((k) => k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim())
));
self.addEventListener("fetch", (e) => {
  const url = new URL(e.request.url);
  if (e.request.method !== "GET" || url.pathname.startsWith("/api/")) return;
  // réseau d'abord (mises à jour), cache en secours
  e.respondWith(
    fetch(e.request).then((r) => {
      if (r.ok && (url.origin === location.origin || url.hostname.includes("fonts."))) {
        const copy = r.clone();
        caches.open(CACHE).then((c) => c.put(e.request, copy));
      }
      return r;
    }).catch(() => caches.match(e.request))
  );
});
