(function () {
  'use strict';

  /* zvýraznenie kódu */
  if (window.hljs) {
    document.querySelectorAll('.code pre code').forEach(function (el) { hljs.highlightElement(el); });
  }

  /* kopírovanie kódu */
  document.querySelectorAll('[data-copy]').forEach(function (btn) {
    var label = btn.textContent;
    btn.addEventListener('click', function () {
      var code = document.getElementById(btn.getAttribute('data-copy'));
      function flash(text) {
        btn.textContent = text;
        setTimeout(function () { btn.textContent = label; }, 1800);
      }
      function select() {
        var range = document.createRange();
        range.selectNodeContents(code);
        var sel = window.getSelection();
        sel.removeAllRanges();
        sel.addRange(range);
        flash('Označené, stlač Ctrl+C');
      }
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(code.textContent).then(function () { flash('Skopírované'); }, select);
      } else {
        select();
      }
    });
  });

  /* záložky Arduino / Python */
  var tabs = document.querySelectorAll('.tabs [role="tab"]');
  var panels = document.querySelectorAll('[role="tabpanel"]');
  function show(id) {
    tabs.forEach(function (t) { t.setAttribute('aria-selected', String(t.getAttribute('aria-controls') === id)); });
    panels.forEach(function (p) { p.hidden = p.id !== id; });
  }
  tabs.forEach(function (t) { t.addEventListener('click', function () { show(t.getAttribute('aria-controls')); }); });
  if (tabs.length) show(tabs[0].getAttribute('aria-controls'));

  /* fotky a video: ukážu sa, len keď súbor existuje, inak ostane zástupný panel */
  document.querySelectorAll('.slot').forEach(function (slot) {
    var media = slot.querySelector('img, video');
    if (!media) return;
    function ok() { slot.classList.add('ok'); }
    if (media.tagName === 'IMG') {
      if (media.complete && media.naturalWidth > 0) ok();
      media.addEventListener('load', ok);
    } else {
      if (media.readyState >= 1) ok();
      media.addEventListener('loadedmetadata', ok);
    }
  });

  /* ukážka displeja */
  var reduce = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  if (reduce) return;

  var text = 'One Kiss (Extended Mix) - Calvin Harris';
  var gap = 4, cols = 16, pos = 0, hold = 4;
  var topCells = document.querySelectorAll('#lcd-top span');
  var barCells = document.querySelectorAll('#lcd-bars i');
  var levels = [];
  for (var i = 0; i < barCells.length; i++) levels.push(Number(barCells[i].style.getPropertyValue('--h')) || 0);

  function drawTitle() {
    for (var i = 0; i < cols; i++) {
      var k = (pos + i) % (text.length + gap);
      topCells[i].textContent = k < text.length ? text.charAt(k) : '';
    }
  }
  setInterval(function () {
    if (document.hidden) return;
    if (hold > 0) { hold--; return; }
    pos = (pos + 1) % (text.length + gap);
    if (pos === 0) hold = 4;
    drawTitle();
  }, 350);

  setInterval(function () {
    if (document.hidden) return;
    for (var i = 0; i < barCells.length; i++) {
      var weight = 1 - i * 0.045;
      var target = Math.random() < 0.55 ? Math.round(Math.random() * 8 * weight) : 0;
      levels[i] = target > levels[i] ? target : Math.max(0, levels[i] - 1);
      barCells[i].style.setProperty('--h', levels[i]);
    }
  }, 140);
})();
