/* BOTF Remastered: Desktop | Mobile mode for the host page (onboarding-and-mobile plan 2.1, stage M-A).
   One build serves both modes. This script decides the mode before the engine starts and gives the player a switch.
   Rule, in order: ?mode=mobile or ?mode=desktop in the address (not remembered); else the choice the player made with the
   switch (localStorage 'botf-mode', try/catch: private tabs may refuse it); else touch + a narrow screen (shorter side of the
   screen <= 640 CSS px) is Mobile and everything else is Desktop.
   The game reads globalThis.BOTF_MODE ('desktop' | 'mobile') and BOTF_MODE_SRC ('query' | 'stored' | 'auto') in
   scripts/platform_mode.gd; the same rule is there as PlatformMode.decide() for a page that did not set them.
   Mobile always runs the WebGL build: on a webgpu/ page this script sends the player to webgl/. The loader (index.html) uses
   the same mode (it picks webgl for Mobile).
   This file is the source. tools/web/embed_mode_js.py copies it, minified to one line, into the head_include of the Web
   and WebGPU export presets and into the loader page; test_platform_mode_js.py fails if a copy has drifted.
   Keep it plain ES5, every statement ending in a semicolon, no // comments (the copy is one line). */
(function () {
  var NARROW_MAX = 640;
  function decide(query, stored, touch, shortSide) {
    var q = String(query || '').trim().toLowerCase();
    if (q === 'mobile' || q === 'desktop') { return { mode: q, source: 'query' }; }
    var s = String(stored || '').trim().toLowerCase();
    if (s === 'mobile' || s === 'desktop') { return { mode: s, source: 'stored' }; }
    if (touch && shortSide > 0 && shortSide <= NARROW_MAX) { return { mode: 'mobile', source: 'auto' }; }
    return { mode: 'desktop', source: 'auto' };
  }
  function read() {
    var q = '', s = '';
    try { q = new URLSearchParams(location.search).get('mode') || ''; } catch (e) {}
    try { s = localStorage.getItem('botf-mode') || ''; } catch (e) {}
    var touch = ((navigator.maxTouchPoints || 0) > 0) || ('ontouchstart' in window);
    var shortSide = 0;
    try { shortSide = Math.min(screen.width, screen.height) || 0; } catch (e) {}
    return decide(q, s, touch, shortSide);
  }
  var d = read();
  globalThis.BOTF_MODE = d.mode;
  globalThis.BOTF_MODE_SRC = d.source;
  globalThis.botfDecideMode = decide;
  var inGpu = /\/webgpu\/[^\/]*$/.test(location.pathname);
  var inGl = /\/webgl\/[^\/]*$/.test(location.pathname);
  if (d.mode === 'mobile' && inGpu) {
    location.replace(location.pathname.replace('/webgpu/', '/webgl/') + location.search + location.hash);
    return;
  }
  function setMode(m) {
    var kept = true;
    try { localStorage.setItem('botf-mode', m); } catch (e) { kept = false; }
    try { sessionStorage.removeItem('botf-build'); } catch (e) {}
    var u = new URL(location.href);
    u.searchParams.delete('mode');
    if (!kept) { u.searchParams.set('mode', m); }
    if (inGl || inGpu) { u.pathname = u.pathname.replace(/\/(webgl|webgpu)\/[^\/]*$/, '/'); }
    location.href = u.toString();
  }
  globalThis.botfSetMode = setMode;
  function build() {
    if (document.getElementById('botf-mode')) { return; }
    var st = document.createElement('style');
    st.textContent = '#botf-mode{position:fixed;z-index:60;font:14px sans-serif;color:#cfe0ff;background:rgba(5,7,15,.92);border:1px solid #2a4064;border-radius:999px;display:flex;overflow:hidden;top:10px;left:50%;transform:translateX(-50%)}#botf-mode button{all:unset;cursor:pointer;padding:8px 16px;min-width:64px;text-align:center}#botf-mode button.on{background:#2a4064;color:#fff;font-weight:600}#botf-mode.small,#botf-mode.fold{top:auto;bottom:6px;left:6px;right:auto;transform:none;opacity:.45}#botf-mode.small{font-size:12px;opacity:.9}#botf-mode.small button{padding:6px 10px;min-width:0}#botf-mode.fold{width:32px;height:32px;border-radius:16px;justify-content:center;align-items:center;cursor:pointer}#botf-mode.fold:hover{opacity:1}#botf-mode.fold button{display:none}#botf-mode.fold:after{content:"\\21C4";font-size:16px}';
    document.head.appendChild(st);
    var box = document.createElement('div');
    box.id = 'botf-mode';
    box.setAttribute('role', 'group');
    box.setAttribute('aria-label', 'Display mode');
    [['desktop', 'Desktop'], ['mobile', 'Mobile']].forEach(function (p) {
      var b = document.createElement('button');
      b.type = 'button';
      b.textContent = p[1];
      b.dataset.mode = p[0];
      if (p[0] === d.mode) { b.className = 'on'; b.setAttribute('aria-pressed', 'true'); }
      b.addEventListener('click', function (ev) {
        ev.stopPropagation();
        if (box.className.indexOf('fold') >= 0) { box.className = 'small'; return; }
        if (p[0] !== d.mode) { setMode(p[0]); }
      });
      box.appendChild(b);
    });
    document.body.appendChild(box);
    globalThis.botfModeBox = box;
  }
  if (document.readyState === 'loading') { document.addEventListener('DOMContentLoaded', build); } else { build(); }
  var done = globalThis.botfBootDone;
  globalThis.botfBootDone = function () {
    if (typeof done === 'function') { done(); }
    var box = document.getElementById('botf-mode');
    if (box) { box.className = 'fold'; box.addEventListener('click', function (ev) { if (box.className === 'fold') { box.className = 'small'; setTimeout(function () { if (box.className === 'small') { box.className = 'fold'; } }, 5000); } }); }
  };
}());
