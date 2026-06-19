(function () {
  'use strict';

  var isLinked = false;
  var presetOpen = false;
  var phase = 0;
  var lastConfidence = 0.35;
  var demoMeterInterval = null;

  function sendToPlugin() {
    /* Demo mode on Shopify storefront */
  }

  function setParameter() {
    /* Demo mode on Shopify storefront */
  }

  function clamp(v, min, max) {
    return Math.max(min, Math.min(max, v));
  }

  function setToggle(id, active, cls) {
    var el = document.getElementById(id);
    if (el) el.classList.toggle(cls, !!active);
  }

  window.toggleBypass = function () {
    var b = document.getElementById('bypassBtn');
    var active = !b.classList.contains('bypass-active');
    setToggle('bypassBtn', active, 'bypass-active');
    setParameter('bypass', active ? 1 : 0);
  };

  window.toggleFormant = function () {
    var b = document.getElementById('formantBtn');
    var active = !b.classList.contains('formant-active');
    setToggle('formantBtn', active, 'formant-active');
    document.getElementById('formantSection').classList.toggle('active', active);
  };

  window.toggleLowLat = function () {
    var b = document.getElementById('lowLatBtn');
    var active = !b.classList.contains('active');
    b.classList.toggle('active', active);
    setParameter('low_latency_mode', active ? 1 : 0);
  };

  window.toggleLink = function () {
    var b = document.getElementById('linkBtn');
    isLinked = !isLinked;
    b.classList.toggle('active', isLinked);
    if (isLinked) syncLink(parseFloat(document.getElementById('pitchK').dataset.v));
  };

  window.togglePresets = function (e) {
    e.stopPropagation();
    presetOpen = !presetOpen;
    document.getElementById('presetDropdown').classList.toggle('open', presetOpen);
  };

  function getPresetItems() {
    return Array.from(document.querySelectorAll('.preset-item'));
  }

  window.selectPreset = function (el) {
    document.querySelectorAll('.preset-item').forEach(function (i) {
      i.classList.remove('active');
    });
    el.classList.add('active');
    document.getElementById('presetName').textContent = el.textContent;
    presetOpen = false;
    document.getElementById('presetDropdown').classList.remove('open');
    applyPreset(el.textContent);
  };

  window.prevPreset = function (e) {
    e.stopPropagation();
    var items = getPresetItems();
    var idx = items.findIndex(function (i) {
      return i.classList.contains('active');
    });
    selectPreset(items[(idx - 1 + items.length) % items.length]);
  };

  window.nextPreset = function (e) {
    e.stopPropagation();
    var items = getPresetItems();
    var idx = items.findIndex(function (i) {
      return i.classList.contains('active');
    });
    selectPreset(items[(idx + 1) % items.length]);
  };

  document.addEventListener('click', function () {
    presetOpen = false;
    var dropdown = document.getElementById('presetDropdown');
    if (dropdown) dropdown.classList.remove('open');
  });

  function applyPreset(name) {
    var items = getPresetItems();
    var el = items.find(function (item) {
      return item.textContent.trim() === name.trim();
    });

    if (el && el.dataset.speed !== undefined) {
      speedKnob._upd(parseFloat(el.dataset.speed), true);
      humanizeKnob._upd(parseFloat(el.dataset.humanize), true);
      mixKnob._upd(parseFloat(el.dataset.mix), true);
      return;
    }

    var presets = {
      Natural: [20, 0, 100],
      'Hard Tune': [0, 0, 100],
      'Soft Correction': [55, 30, 65],
      'Pop Vocal': [18, 10, 100],
      'R&B Smooth': [35, 35, 85],
      'Trap Vocal': [2, 0, 100],
      'Hip Hop Clean': [24, 12, 90],
      Robot: [0, 0, 100],
      'Deep Voice': [12, 5, 100],
      Chipmunk: [8, 0, 100],
      'Lo-Fi': [70, 50, 55],
      'Live Mode': [0, 0, 100],
      'Studio Vocal': [22, 18, 95]
    };
    var p = presets[name] || [20, 0, 100];
    speedKnob._upd(p[0], true);
    humanizeKnob._upd(p[1], true);
    mixKnob._upd(p[2], true);
  }

  window.triggerAI = function () {
    var btn = document.getElementById('aiKeyBtn');
    var ind = document.getElementById('aiIndicator');
    var dk = document.getElementById('detectedKey');
    var keys = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    btn.classList.add('detecting');
    ind.classList.add('active');
    dk.classList.remove('show');
    setTimeout(function () {
      var key = Math.floor(Math.random() * 12);
      var minor = Math.random() > 0.5;
      btn.classList.remove('detecting');
      ind.classList.remove('active');
      dk.textContent = keys[key] + ' ' + (minor ? 'Minor' : 'Major');
      dk.classList.add('show');
      document.getElementById('keySelect').value = key;
      document.getElementById('scaleSelect').value = minor ? 1 : 0;
      setParameter('key', key);
      setParameter('scale', minor ? 1 : 0);
    }, 1200);
  };

  var canvas = document.getElementById('waveform');
  if (!canvas) return;

  var ctx = canvas.getContext('2d');

  function rc() {
    canvas.width = canvas.offsetWidth * 2;
    canvas.height = canvas.offsetHeight * 2;
    ctx.setTransform(2, 0, 0, 2, 0, 0);
  }

  rc();
  window.addEventListener('resize', rc);

  function dw() {
    var w = canvas.offsetWidth;
    var h = canvas.offsetHeight;
    var amp = 0.15 + lastConfidence * 0.25;
    ctx.clearRect(0, 0, w, h);
    ctx.shadowBlur = 15;
    ctx.shadowColor = '#00e5ff';
    ctx.beginPath();
    ctx.strokeStyle = '#00e5ff';
    ctx.lineWidth = 2;
    for (var x = 0; x < w; x++) {
      var y = h / 2 + Math.sin(x * 0.02 + phase) * (h * amp) + Math.sin(x * 0.1 + phase * 2) * 5;
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.beginPath();
    ctx.strokeStyle = 'rgba(255,45,146,.3)';
    ctx.lineWidth = 1;
    ctx.shadowColor = '#ff2d92';
    for (var x2 = 0; x2 < w; x2++) {
      var y2 = h / 2 + Math.sin(x2 * 0.015 + phase * 1.5) * (h * (amp * 0.7));
      if (x2 === 0) ctx.moveTo(x2, y2);
      else ctx.lineTo(x2, y2);
    }
    ctx.stroke();
    phase += 0.03;
    requestAnimationFrame(dw);
  }

  dw();

  function makeKnob(wrId, valId, min, max, fmt) {
    var wr = document.getElementById(wrId);
    if (!wr) return null;
    var valEl = valId ? document.getElementById(valId) : null;
    var ptr = wr.querySelector('[class$="pointer"]');
    var drag = false;
    var sy = 0;
    var sv = 0;

    function upd(v, notify) {
      v = clamp(Number(v), min, max);
      wr.dataset.v = v;
      var norm = (v - min) / (max - min);
      var rot = -135 + norm * 270;
      ptr.style.transform = 'translateX(-50%) rotate(' + rot + 'deg)';
      wr.style.setProperty('--ka', norm * 270);
      if (valEl && fmt) valEl.textContent = fmt(v);
      if (notify && wr._onChange) wr._onChange(v);
      return v;
    }

    upd(parseFloat(wr.dataset.v), false);

    wr.addEventListener('mousedown', function (e) {
      drag = true;
      sy = e.clientY;
      sv = parseFloat(wr.dataset.v);
      document.body.style.cursor = 'ns-resize';
      e.preventDefault();
    });

    wr.addEventListener(
      'touchstart',
      function (e) {
        drag = true;
        sy = e.touches[0].clientY;
        sv = parseFloat(wr.dataset.v);
        e.preventDefault();
      },
      { passive: false }
    );

    document.addEventListener('mousemove', function (e) {
      if (!drag) return;
      var sens = max - min > 50 ? 0.5 : 0.03;
      upd(sv + (sy - e.clientY) * sens, true);
    });

    document.addEventListener(
      'touchmove',
      function (e) {
        if (!drag) return;
        var sens = max - min > 50 ? 0.5 : 0.03;
        upd(sv + (sy - e.touches[0].clientY) * sens, true);
      },
      { passive: false }
    );

    document.addEventListener('mouseup', function () {
      if (drag) {
        drag = false;
        document.body.style.cursor = 'default';
      }
    });

    document.addEventListener('touchend', function () {
      drag = false;
    });

    wr.addEventListener('dblclick', function () {
      upd((min + max) / 2, true);
    });

    wr._upd = upd;
    return wr;
  }

  function pctFmt(v) {
    return Math.round(v) + '%';
  }

  function stFmt(v) {
    return v.toFixed(1);
  }

  var speedKnob = makeKnob('speedK', 'speedV', 0, 100, pctFmt);
  var humanizeKnob = makeKnob('humanizeK', 'humanizeV', 0, 100, pctFmt);
  var mixKnob = makeKnob('mixK', 'mixV', 0, 100, pctFmt);
  var pitchKnob = makeKnob('pitchK', 'pitchV', -2, 2, stFmt);
  var formantKnob = makeKnob('formantK', 'formantV', -2, 2, stFmt);
  makeKnob('fMixK', null, 0, 100, null);

  function bindKnob(knob, id, type) {
    if (!knob) return;
    knob._onChange = function (v) {
      if (type === 'inverse') setParameter(id, 100 - v);
      else setParameter(id, v);
      if (id === 'pitch_offset' && isLinked) syncLink(v);
      if (id === 'formant_shift' && isLinked) {
        pitchKnob._upd(v, false);
        setParameter('pitch_offset', v);
      }
    };
  }

  bindKnob(speedKnob, 'retune_speed', 'direct');
  bindKnob(humanizeKnob, 'humanize', 'direct');
  bindKnob(mixKnob, 'flex_tune', 'inverse');
  bindKnob(pitchKnob, 'pitch_offset', 'direct');
  bindKnob(formantKnob, 'formant_shift', 'direct');

  function syncLink(v) {
    formantKnob._upd(v, false);
    setParameter('formant_shift', v);
  }

  var keySelect = document.getElementById('keySelect');
  var scaleSelect = document.getElementById('scaleSelect');
  if (keySelect) keySelect.addEventListener('change', function () { setParameter('key', this.value); });
  if (scaleSelect) scaleSelect.addEventListener('change', function () { setParameter('scale', this.value); });

  function runDemoMeters() {
    var notes = ['C4', 'D4', 'E4', 'F4', 'G4', 'A4', 'B4', 'C5'];
    var freqs = [262, 294, 330, 349, 392, 440, 494, 523];
    var i = 0;
    demoMeterInterval = setInterval(function () {
      lastConfidence = 0.25 + Math.random() * 0.55;
      var level = Math.round(clamp(lastConfidence, 0, 1) * 100);
      var noteEl = document.getElementById('currentNote');
      var freqEl = document.getElementById('currentFreq');
      var mIn = document.getElementById('mIn');
      var mOut = document.getElementById('mOut');
      if (noteEl) noteEl.textContent = notes[i % notes.length];
      if (freqEl) freqEl.textContent = freqs[i % freqs.length] + ' Hz';
      if (mIn) mIn.style.height = level + '%';
      if (mOut) mOut.style.height = Math.max(0, level - 5) + '%';
      i++;
    }, 900);
  }

  runDemoMeters();

  document.addEventListener('keydown', function (e) {
    var k = e.key.toLowerCase();
    if (k === 'b') toggleBypass();
    if (k === 'f') toggleFormant();
    if (k === 'a') triggerAI();
    if (k === 'l') toggleLink();
  });

  window.addEventListener('beforeunload', function () {
    if (demoMeterInterval) clearInterval(demoMeterInterval);
  });
})();
