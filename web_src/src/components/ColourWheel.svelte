<script>
  import { createEventDispatcher } from 'svelte';

  // The same HSV disc the device paints: angle = hue, distance from centre =
  // saturation, value pinned at full (brightness is the fader's job). Mirroring
  // the panel means a colour picked here lands the knob in the same place there.
  //
  // The gradient starts at 3 o'clock and runs clockwise because that is what
  // atan2(dy, dx) does with y pointing down — see ColourWheel.cpp.

  export let value = '#ff0000';
  export let id = undefined;

  const dispatch = createEventDispatcher();

  let el;
  let dragging = false;
  let hue = 0;
  let sat = 100;

  // Follow an external change (device, MQTT) unless the finger is down, so a
  // state echo can't fight the drag.
  $: if (!dragging) ({ hue, sat } = hexToHs(value));

  function hexToHs(hex) {
    const m = /^#?([0-9a-f]{6})$/i.exec(hex || '');
    if (!m) return { hue: 0, sat: 0 };
    const n = parseInt(m[1], 16);
    const r = ((n >> 16) & 255) / 255, g = ((n >> 8) & 255) / 255, b = (n & 255) / 255;
    const max = Math.max(r, g, b), min = Math.min(r, g, b), d = max - min;
    let h = 0;
    if (d !== 0) {
      if (max === r) h = ((g - b) / d) % 6;
      else if (max === g) h = (b - r) / d + 2;
      else h = (r - g) / d + 4;
      h *= 60;
      if (h < 0) h += 360;
    }
    return { hue: h, sat: max === 0 ? 0 : (d / max) * 100 };
  }

  function hsToHex(h, s) {
    const c = s / 100, x = c * (1 - Math.abs(((h / 60) % 2) - 1)), m = 1 - c;
    let rgb;
    if (h < 60) rgb = [c, x, 0];
    else if (h < 120) rgb = [x, c, 0];
    else if (h < 180) rgb = [0, c, x];
    else if (h < 240) rgb = [0, x, c];
    else if (h < 300) rgb = [x, 0, c];
    else rgb = [c, 0, x];
    return '#' + rgb
      .map((v) => Math.round((v + m) * 255).toString(16).padStart(2, '0'))
      .join('');
  }

  function pick(event) {
    if (!el) return;
    const r = el.getBoundingClientRect();
    const radius = r.width / 2;
    const dx = event.clientX - (r.left + radius);
    const dy = event.clientY - (r.top + radius);

    let angle = (Math.atan2(dy, dx) * 180) / Math.PI;
    if (angle < 0) angle += 360;

    // Outside the disc still picks, clamped to the rim, so a drag that
    // overshoots keeps tracking hue instead of freezing.
    hue = angle;
    sat = Math.min(100, (Math.hypot(dx, dy) / radius) * 100);

    dispatch('change', hsToHex(hue, sat));
  }

  function onPointerDown(event) {
    dragging = true;
    el.setPointerCapture(event.pointerId);
    pick(event);
  }

  function onPointerMove(event) {
    if (dragging) pick(event);
  }

  function onPointerUp(event) {
    if (!dragging) return;
    dragging = false;
    el.releasePointerCapture(event.pointerId);
  }

  // Arrow keys step the hue, so the wheel is operable without a pointer.
  function onKeyDown(event) {
    const step = event.shiftKey ? 15 : 5;
    if (event.key === 'ArrowRight' || event.key === 'ArrowUp') hue = (hue + step) % 360;
    else if (event.key === 'ArrowLeft' || event.key === 'ArrowDown') hue = (hue + 360 - step) % 360;
    else return;
    event.preventDefault();
    if (sat === 0) sat = 100;
    dispatch('change', hsToHex(hue, sat));
  }

  $: knobX = 50 + Math.cos((hue * Math.PI) / 180) * (sat / 2);
  $: knobY = 50 + Math.sin((hue * Math.PI) / 180) * (sat / 2);
</script>

<div
  {id}
  class="wheel"
  class:dragging
  bind:this={el}
  role="slider"
  tabindex="0"
  aria-label="Colour"
  aria-valuetext={value}
  aria-valuenow={Math.round(hue)}
  aria-valuemin="0"
  aria-valuemax="360"
  on:pointerdown={onPointerDown}
  on:pointermove={onPointerMove}
  on:pointerup={onPointerUp}
  on:pointercancel={onPointerUp}
  on:keydown={onKeyDown}
>
  <span class="knob" style="left:{knobX}%; top:{knobY}%; background:{value}"></span>
</div>

<style>
  .wheel {
    position: relative;
    width: 100%;
    max-width: 240px;
    aspect-ratio: 1;
    margin: 0 auto;
    border-radius: 50%;
    touch-action: none;
    cursor: crosshair;
    background:
      radial-gradient(circle at 50% 50%, #fff 0%, rgba(255, 255, 255, 0) 100%),
      conic-gradient(from 90deg, #f00, #ff0, #0f0, #0ff, #00f, #f0f, #f00);
  }

  .wheel:focus-visible {
    outline: 2px solid var(--border-focus);
    outline-offset: 4px;
  }

  .knob {
    position: absolute;
    width: 26px;
    height: 26px;
    margin: -13px 0 0 -13px;
    border-radius: 50%;
    border: 3px solid #fff;
    box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.45);
    pointer-events: none;
  }

  .wheel.dragging .knob {
    transform: scale(1.12);
  }

  @media (prefers-reduced-motion: reduce) {
    .wheel.dragging .knob { transform: none; }
  }
</style>
