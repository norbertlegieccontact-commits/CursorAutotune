(function () {
  var CFG = window.VOXTUNE_CONFIG || {};
  var WAVE_CYAN = CFG.cyan || '#00e5ff';
  var WAVE_MAGENTA = CFG.magenta || '#ff2d92';

  var PRESET_MAP = {};
  if (Array.isArray(CFG.presets)) {
    CFG.presets.forEach(function (p) { if (p && p.name) PRESET_MAP[p.name] = p.v; });
  }
  if (!Object.keys(PRESET_MAP).length) {
    PRESET_MAP = { Natural: [20, 0, 100] };
  }

  var isLinked = false, presetOpen = false, pluginReady = false, phase = 0, lastConfidence = 0;
  var speedKnob, humanizeKnob, mixKnob, pitchKnob, formantKnob;

  function $(id) { return document.getElementById(id); }
  function clamp(v, min, max) { return Math.max(min, Math.min(max, v)); }
  function setToggle(id, active, cls) { var el = $(id); if (el) el.classList.toggle(cls, !!active); }

  function sendToPlugin(eventId, payload) {
    var message = { eventId: eventId, payload: payload || {} };
    if (window.__JUCE__ && window.__JUCE__.backend && window.__JUCE__.backend.emitEvent) {
      window.__JUCE__.backend.emitEvent('juce_event', message); pluginReady = true; return;
    }
    if (window.__JUCE__ && window.__JUCE__.postMessage) window.__JUCE__.postMessage(JSON.stringify(message));
  }
  function setParameter(name, value) { sendToPlugin('set_parameter', { name: name, value: Number(value) }); }

  window.toggleBypass = function () { var b = $('bypassBtn'); var active = !b.classList.contains('bypass-active'); setToggle('bypassBtn', active, 'bypass-active'); setParameter('bypass', active ? 1 : 0); };
  window.toggleFormant = function () { var b = $('formantBtn'); var active = !b.classList.contains('formant-active'); setToggle('formantBtn', active, 'formant-active'); $('formantSection').classList.toggle('active', active); };
  window.toggleLowLat = function () { var b = $('lowLatBtn'); var active = !b.classList.contains('active'); b.classList.toggle('active', active); setParameter('low_latency_mode', active ? 1 : 0); };
  window.toggleLink = function () { var b = $('linkBtn'); isLinked = !isLinked; b.classList.toggle('active', isLinked); if (isLinked) syncLink(parseFloat($('pitchK').dataset.v)); };
  window.togglePresets = function (e) { e.stopPropagation(); presetOpen = !presetOpen; $('presetDropdown').classList.toggle('open', presetOpen); };

  function getPresetItems() { return Array.prototype.slice.call(document.querySelectorAll('.preset-item')); }
  window.selectPreset = function (el) {
    document.querySelectorAll('.preset-item').forEach(function (i) { i.classList.remove('active'); });
    el.classList.add('active'); $('presetName').textContent = el.textContent; presetOpen = false;
    $('presetDropdown').classList.remove('open'); applyPreset(el.textContent.trim());
  };
  window.prevPreset = function (e) { e.stopPropagation(); var items = getPresetItems(); var idx = items.findIndex(function (i) { return i.classList.contains('active'); }); selectPreset(items[(idx - 1 + items.length) % items.length]); };
  window.nextPreset = function (e) { e.stopPropagation(); var items = getPresetItems(); var idx = items.findIndex(function (i) { return i.classList.contains('active'); }); selectPreset(items[(idx + 1) % items.length]); };
  document.addEventListener('click', function () { presetOpen = false; var d = $('presetDropdown'); if (d) d.classList.remove('open'); });

  function applyPreset(name) {
    var p = PRESET_MAP[name] || [20, 0, 100];
    speedKnob._upd(p[0], true); humanizeKnob._upd(p[1], true); mixKnob._upd(p[2], true);
  }

  window.triggerAI = function () {
    var btn = $('aiKeyBtn'), ind = $('aiIndicator'), dk = $('detectedKey');
    var keys = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    btn.classList.add('detecting'); ind.classList.add('active'); dk.classList.remove('show');
    setTimeout(function () {
      var key = Math.floor(Math.random() * 12), minor = Math.random() > 0.5;
      btn.classList.remove('detecting'); ind.classList.remove('active');
      dk.textContent = keys[key] + ' ' + (minor ? 'Minor' : 'Major'); dk.classList.add('show');
      $('keySelect').value = key; $('scaleSelect').value = minor ? 1 : 0;
      setParameter('key', key); setParameter('scale', minor ? 1 : 0);
    }, 1200);
  };

  function makeKnob(wrId, valId, min, max, fmt) {
    var wr = $(wrId); if (!wr) return null;
    var valEl = valId ? $(valId) : null, ptr = wr.querySelector('[class$="pointer"]'), drag = false, sy = 0, sv = 0;
    function upd(v, notify) {
      v = clamp(Number(v), min, max); wr.dataset.v = v;
      var norm = (v - min) / (max - min), rot = -135 + norm * 270;
      ptr.style.transform = 'translateX(-50%) rotate(' + rot + 'deg)';
      wr.style.setProperty('--ka', norm * 270);
      if (valEl && fmt) valEl.textContent = fmt(v);
      if (notify && wr._onChange) wr._onChange(v);
      return v;
    }
    upd(parseFloat(wr.dataset.v), false);
    wr.addEventListener('mousedown', function (e) { drag = true; sy = e.clientY; sv = parseFloat(wr.dataset.v); document.body.style.cursor = 'ns-resize'; e.preventDefault(); });
    document.addEventListener('mousemove', function (e) { if (!drag) return; var sens = (max - min) > 50 ? 0.5 : 0.03; upd(sv + (sy - e.clientY) * sens, true); });
    document.addEventListener('mouseup', function () { if (drag) { drag = false; document.body.style.cursor = 'default'; } });
    wr.addEventListener('dblclick', function () { upd((min + max) / 2, true); });
    wr._upd = upd; return wr;
  }
  function pctFmt(v) { return Math.round(v) + '%'; }
  function stFmt(v) { return v.toFixed(1); }

  function bindKnob(knob, id, type) {
    if (!knob) return;
    knob._onChange = function (v) {
      if (type === 'inverse') setParameter(id, 100 - v); else setParameter(id, v);
      if (id === 'pitch_offset' && isLinked) syncLink(v);
      if (id === 'formant_shift' && isLinked) { pitchKnob._upd(v, false); setParameter('pitch_offset', v); }
    };
  }
  function syncLink(v) { formantKnob._upd(v, false); setParameter('formant_shift', v); }

  function applyParameter(name, value, notify) {
    if (name === 'bypass') setToggle('bypassBtn', value > 0.5, 'bypass-active');
    if (name === 'low_latency_mode') $('lowLatBtn').classList.toggle('active', value > 0.5);
    if (name === 'key') $('keySelect').value = String(Math.round(value));
    if (name === 'scale') $('scaleSelect').value = String(Math.round(value));
    if (name === 'retune_speed') speedKnob._upd(value, notify);
    if (name === 'humanize') humanizeKnob._upd(value, notify);
    if (name === 'flex_tune') mixKnob._upd(100 - value, notify);
    if (name === 'pitch_offset') pitchKnob._upd(value, notify);
    if (name === 'formant_shift') formantKnob._upd(value, notify);
  }

  function init() {
    var canvas = $('waveform'); if (!canvas) return;
    var ctx = canvas.getContext('2d');
    function rc() { canvas.width = canvas.offsetWidth * 2; canvas.height = canvas.offsetHeight * 2; ctx.setTransform(2, 0, 0, 2, 0, 0); }
    rc(); window.addEventListener('resize', rc);
    function dw() {
      var w = canvas.offsetWidth, h = canvas.offsetHeight, amp = 0.15 + lastConfidence * 0.25;
      ctx.clearRect(0, 0, w, h); ctx.shadowBlur = 15; ctx.shadowColor = WAVE_CYAN;
      ctx.beginPath(); ctx.strokeStyle = WAVE_CYAN; ctx.lineWidth = 2;
      for (var x = 0; x < w; x++) { var y = h / 2 + Math.sin(x * 0.02 + phase) * (h * amp) + Math.sin(x * 0.1 + phase * 2) * 5; (x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)); }
      ctx.stroke();
      ctx.beginPath(); ctx.strokeStyle = 'rgba(255,45,146,.3)'; ctx.lineWidth = 1; ctx.shadowColor = WAVE_MAGENTA;
      for (var x2 = 0; x2 < w; x2++) { var y2 = h / 2 + Math.sin(x2 * 0.015 + phase * 1.5) * (h * (amp * 0.7)); (x2 === 0 ? ctx.moveTo(x2, y2) : ctx.lineTo(x2, y2)); }
      ctx.stroke(); phase += 0.03; requestAnimationFrame(dw);
    }
    dw();

    speedKnob = makeKnob('speedK', 'speedV', 0, 100, pctFmt);
    humanizeKnob = makeKnob('humanizeK', 'humanizeV', 0, 100, pctFmt);
    mixKnob = makeKnob('mixK', 'mixV', 0, 100, pctFmt);
    pitchKnob = makeKnob('pitchK', 'pitchV', -2, 2, stFmt);
    formantKnob = makeKnob('formantK', 'formantV', -2, 2, stFmt);
    makeKnob('fMixK', null, 0, 100, null);

    bindKnob(speedKnob, 'retune_speed', 'direct');
    bindKnob(humanizeKnob, 'humanize', 'direct');
    bindKnob(mixKnob, 'flex_tune', 'inverse');
    bindKnob(pitchKnob, 'pitch_offset', 'direct');
    bindKnob(formantKnob, 'formant_shift', 'direct');

    $('keySelect').addEventListener('change', function () { setParameter('key', this.value); });
    $('scaleSelect').addEventListener('change', function () { setParameter('scale', this.value); });

    window.addEventListener('pitch_data', function (e) {
      var d = e.detail || {}; lastConfidence = Number(d.confidence || 0);
      $('currentNote').textContent = d.note || '--';
      $('currentFreq').textContent = (d.detectedHz && d.detectedHz > 0 ? Math.round(d.detectedHz) : 0) + ' Hz';
      var level = Math.round(clamp(lastConfidence, 0, 1) * 100);
      $('mIn').style.height = level + '%'; $('mOut').style.height = Math.max(0, level - 5) + '%';
    });
    window.addEventListener('parameter_changed', function (e) { var p = e.detail || {}; applyParameter(p.name, Number(p.value), false); });

    document.addEventListener('keydown', function (e) {
      var tag = (e.target && e.target.tagName) || ''; if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
      var k = e.key.toLowerCase();
      if (k === 'b') toggleBypass(); if (k === 'f') toggleFormant(); if (k === 'a') triggerAI(); if (k === 'l') toggleLink();
    });

    setTimeout(function () { sendToPlugin('request_state', {}); }, 100);
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
