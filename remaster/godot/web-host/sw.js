/* BOTF Remastered service worker (F1 audit, 17 Nov 2026). A private fan project: nothing here leaves the player's browser.
 *
 * What it does, with no CDN and no special headers from the host:
 *  - Hashed packs (packs/<name>.<12 hex>.pck): cache-first, forever. The name is the content hash, so a pack never changes under
 *    its name; a changed build has new names, which simply miss the cache and are fetched. After each packs/manifest.json the
 *    packs the manifest no longer lists are dropped from the cache (that is the invalidation).
 *  - The engine (index.js, index.wasm, index.side.wasm, index.pck, the audio worklets, the core wasm): cache-first inside a bucket
 *    named for the build. The build id is the <meta name="botf-build"> that tools/verify/hash_web_packs.py writes into index.html;
 *    index.html is always fetched fresh (network first), so a new upload switches the bucket on the next visit and the old
 *    bucket is deleted. No file of one build is ever served with the page of another.
 *  - packs/manifest.json, sizes.json and sw.js itself always go to the network (the manifest falls back to the cached copy offline).
 * The first visit downloads as usual; the page sends {type:'warm'} once the title is up and the worker then stores the engine
 * files. From the second visit the engine and every pack seen before come from the cache.
 */
'use strict';

const META = 'botf-meta';
const ENGINE_PREFIX = 'botf-engine-';
const PACKS = 'botf-packs';
const SCOPE = self.registration.scope;
const BUILD_KEY = SCOPE + '__build';
const MANIFEST_KEY = SCOPE + '__manifest';
const ENGINE_FILES = ['index.js', 'index.wasm', 'index.side.wasm', 'index.pck', 'index.audio.worklet.js',
  'index.audio.position.worklet.js', 'libbotf_core.web.template_release.wasm32.wasm', 'index.png', 'index.icon.png',
  'index.apple-touch-icon.png'];
const HASHED_PACK = /^packs\/[^/]+\.[0-9a-f]{12}\.pck$/;

let build = null;   // the build id of the page now being served ('' = unknown)

self.addEventListener('install', () => { self.skipWaiting(); });
self.addEventListener('activate', (e) => { e.waitUntil(self.clients.claim()); });

function rel(url) {
  const u = new URL(url);
  return u.href.startsWith(SCOPE) ? u.pathname.slice(new URL(SCOPE).pathname.length) + '' : null;
}

async function currentBuild() {
  if (build !== null) return build;
  try {
    const r = await (await caches.open(META)).match(BUILD_KEY);
    build = r ? (await r.text()) : '';
  } catch (_) { build = ''; }
  return build;
}

async function setBuild(id) {
  if (!id) return;
  const old = await currentBuild();
  build = id;
  if (old === id) return;
  try {
    await (await caches.open(META)).put(BUILD_KEY, new Response(id));
    for (const k of await caches.keys()) {
      if (k.startsWith(ENGINE_PREFIX) && k !== ENGINE_PREFIX + id) await caches.delete(k);   // the old build's engine files
    }
  } catch (_) { /* storage trouble: the game still runs from the network */ }
}

function buildOf(html) {
  const m = /<meta[^>]+name=["']botf-build["'][^>]+content=["']([0-9a-f]+)["']/i.exec(html);
  return m ? m[1] : '';
}

async function putSafe(cacheName, key, resp) {
  try { await (await caches.open(cacheName)).put(key, resp); } catch (_) { /* quota: skip, never fail the request */ }
}

async function pruneManifest(text) {
  try {
    const man = JSON.parse(text);
    const keep = new Set(Object.values(man.files || {}).map((f) => new URL('packs/' + f, SCOPE).href));
    const c = await caches.open(PACKS);
    for (const req of await c.keys()) {
      if (!keep.has(req.url)) await c.delete(req);   // a pack from an older build
    }
  } catch (_) { /* a malformed manifest prunes nothing */ }
}

async function onNavigate(req) {
  try {
    const net = await fetch(req.url, { cache: 'no-cache', credentials: 'same-origin' });
    if (net.ok) {
      const copy = net.clone();
      const id = buildOf(await copy.text());
      await setBuild(id);
      if (id) await putSafe(ENGINE_PREFIX + id, new URL('index.html', SCOPE).href, net.clone());
    }
    return net;
  } catch (err) {
    const id = await currentBuild();
    const hit = id ? await (await caches.open(ENGINE_PREFIX + id)).match(new URL('index.html', SCOPE).href) : null;
    if (hit) return hit;
    throw err;
  }
}

async function onManifest(req) {
  try {
    const net = await fetch(req.url, { cache: 'no-store' });
    if (net.ok) {
      const text = await net.clone().text();
      await putSafe(META, MANIFEST_KEY, new Response(text, { headers: { 'content-type': 'application/json' } }));
      pruneManifest(text);
    }
    return net;
  } catch (err) {
    const hit = await (await caches.open(META)).match(MANIFEST_KEY);
    if (hit) return hit;
    throw err;
  }
}

async function cacheFirst(cacheName, req) {
  const c = await caches.open(cacheName);
  const hit = await c.match(req.url);
  if (hit) return hit;
  const net = await fetch(req);
  if (net.ok && net.status === 200) await putSafe(cacheName, req.url, net.clone());
  return net;
}

self.addEventListener('fetch', (e) => {
  const req = e.request;
  if (req.method !== 'GET' || req.headers.has('range')) return;
  const p = rel(req.url);
  if (p === null || new URL(req.url).search.includes('nocache')) return;
  if (req.mode === 'navigate' || p === '' || p === 'index.html') { e.respondWith(onNavigate(req)); return; }
  if (p === 'packs/manifest.json') { e.respondWith(onManifest(req)); return; }
  if (HASHED_PACK.test(p)) { e.respondWith(cacheFirst(PACKS, req)); return; }
  if (ENGINE_FILES.includes(p)) {
    e.respondWith((async () => {
      const id = await currentBuild();
      if (!id) return fetch(req);   // build not known yet (first visit, before the page was seen): plain network
      return cacheFirst(ENGINE_PREFIX + id, req);
    })());
  }
});

// The page says the title is up: store the engine files for the next visit (they are in the browser's own cache by now).
self.addEventListener('message', (e) => {
  if (!e.data || e.data.type !== 'warm') return;
  e.waitUntil((async () => {
    let id = await currentBuild();
    if (!id) {   // first visit: the page was loaded before this worker existed, so it never saw index.html. Read the build id now.
      try {
        const page = new URL('index.html', SCOPE).href;
        const r = await fetch(page, { cache: 'no-cache' });
        if (r.ok) {
          id = buildOf(await r.clone().text());
          await setBuild(id);
          if (id) await putSafe(ENGINE_PREFIX + id, page, r);
        }
      } catch (_) { /* offline: nothing to warm */ }
    }
    if (!id) return;
    const c = await caches.open(ENGINE_PREFIX + id);
    for (const f of ENGINE_FILES) {
      const url = new URL(f, SCOPE).href;
      if (await c.match(url)) continue;
      try {
        const r = await fetch(url);
        if (r.ok && r.status === 200) await c.put(url, r);
      } catch (_) { /* one file missing is fine: it is fetched when asked for */ }
    }
  })());
});
