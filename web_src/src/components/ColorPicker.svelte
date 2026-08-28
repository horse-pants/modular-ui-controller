<script>
  import { createEventDispatcher, onMount, onDestroy } from 'svelte';

  export let value = '#ff0000';
  export let id = undefined;   // so a <label for=...> can point at the trigger

  const dispatch = createEventDispatcher();

  let open = false;
  let hexInput = value;
  let containerEl;

  const presets = [
    '#ff0000', '#ff8000', '#ffff00', '#00ff00',
    '#00ffff', '#0080ff', '#0000ff', '#8000ff',
    '#ff00ff', '#ff0080', '#ffffff', '#000000'
  ];

  // Stops matching the CSS gradient below — used to map an x-position to a hex.
  const gradientStops = [
    { p: 0.0000, c: '#ff0000' },
    { p: 0.1666, c: '#ff8000' },
    { p: 0.3333, c: '#ffff00' },
    { p: 0.5000, c: '#00ff00' },
    { p: 0.6666, c: '#00ffff' },
    { p: 0.8333, c: '#0000ff' },
    { p: 1.0000, c: '#ff00ff' }
  ];

  $: hexInput = value;

  function pick(hex) {
    value = hex;
    dispatch('change', hex);
  }

  function gradientClick(e) {
    // The gradient is a button so it's focusable and announced, but activating
    // it from the keyboard carries no pointer position — the presets and the hex
    // field are the keyboard path, so do nothing rather than snap to red.
    if (e.detail === 0) return;

    const rect = e.currentTarget.getBoundingClientRect();
    const pct = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));

    let lo = gradientStops[0], hi = gradientStops[gradientStops.length - 1];
    for (let i = 0; i < gradientStops.length - 1; i++) {
      if (pct >= gradientStops[i].p && pct <= gradientStops[i + 1].p) {
        lo = gradientStops[i];
        hi = gradientStops[i + 1];
        break;
      }
    }
    const f = (pct - lo.p) / (hi.p - lo.p || 1);
    pick(interpolate(lo.c, hi.c, f));
  }

  function interpolate(a, b, f) {
    const ah = a.replace('#', ''), bh = b.replace('#', '');
    const ar = parseInt(ah.substr(0, 2), 16), ag = parseInt(ah.substr(2, 2), 16), ab = parseInt(ah.substr(4, 2), 16);
    const br = parseInt(bh.substr(0, 2), 16), bg = parseInt(bh.substr(2, 2), 16), bb = parseInt(bh.substr(4, 2), 16);
    const r = Math.round(ar + (br - ar) * f);
    const g = Math.round(ag + (bg - ag) * f);
    const bl = Math.round(ab + (bb - ab) * f);
    return '#' + [r, g, bl].map(v => v.toString(16).padStart(2, '0')).join('');
  }

  function onHexInput(e) {
    const v = e.target.value;
    if (/^#([A-Fa-f0-9]{6})$/.test(v)) pick(v);
  }

  function onDocClick(e) {
    if (open && containerEl && !containerEl.contains(e.target)) open = false;
  }

  onMount(() => document.addEventListener('click', onDocClick));
  onDestroy(() => document.removeEventListener('click', onDocClick));
</script>

<div class="color-picker-container" bind:this={containerEl}>
  <button type="button"
          {id}
          class="btn color-display"
          style="background:{value}"
          aria-label="Open colour picker"
          on:click={() => open = !open}>&nbsp;</button>

  {#if open}
    <div class="color-picker">
      <button type="button"
              class="color-gradient"
              aria-label="Hue gradient - click to pick a colour"
              on:click={gradientClick}></button>

      <div class="color-presets">
        {#each presets as p}
          <button type="button"
                  class="preset"
                  style="background:{p}"
                  aria-label={`Preset ${p}`}
                  on:click={() => pick(p)}></button>
        {/each}
      </div>

      <input type="text"
             class="hex-input"
             maxlength="7"
             placeholder="#ff0000"
             bind:value={hexInput}
             on:input={onHexInput}>
    </div>
  {/if}
</div>

<style>
  .color-picker-container {
    position: relative;
  }

  /* Inherits .btn's full box model (padding, font, line-height, border) so the
     swatch matches the White/VU buttons pixel-for-pixel. Only the visuals are
     overridden here. Inline style="background:..." beats .btn:hover's background
     via specificity, so the chosen colour stays visible on hover. */
  .color-display {
    width: 100%;
    color: transparent;
    box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.25), 0 2px 10px rgba(0, 0, 0, 0.25);
  }

  .color-display:hover {
    color: transparent;
    border-color: var(--primary);
    box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.25), 0 0 14px var(--primary-glow);
  }

  .color-picker {
    position: absolute;
    top: 60px;
    left: 0;
    width: 280px;
    background: var(--surface-elevated);
    border: 1px solid var(--surface-border);
    border-radius: var(--radius-card);
    padding: 1rem;
    z-index: 100;
    box-shadow: var(--shadow-modal);
  }

  @supports (backdrop-filter: blur(1px)) {
    .color-picker {
      backdrop-filter: blur(20px) saturate(160%);
      -webkit-backdrop-filter: blur(20px) saturate(160%);
    }
  }

  .color-gradient {
    display: block;
    appearance: none;
    padding: 0;
    width: 100%;
    height: 100px;
    border-radius: var(--radius-button);
    background: linear-gradient(to right, #ff0000, #ff8000, #ffff00, #00ff00, #00ffff, #0000ff, #ff00ff);
    cursor: crosshair;
    margin-bottom: 0.75rem;
    border: 1px solid var(--surface-border);
  }

  .color-presets {
    display: grid;
    grid-template-columns: repeat(6, 1fr);
    gap: 8px;
    margin-bottom: 0.75rem;
  }

  .preset {
    aspect-ratio: 1;
    border-radius: 6px;
    cursor: pointer;
    border: 1px solid var(--surface-border);
    padding: 0;
    transition: transform 0.15s ease, border-color 0.15s ease, box-shadow 0.15s ease;
    min-height: 32px;
  }

  .preset:hover {
    border-color: var(--primary);
    transform: scale(1.08);
    box-shadow: 0 0 12px var(--primary-glow);
  }

  .hex-input {
    font-family: 'JetBrains Mono', ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    text-align: center;
    letter-spacing: 0.05em;
  }
</style>
