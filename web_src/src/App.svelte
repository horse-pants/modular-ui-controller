<script>
  import { onMount, onDestroy } from 'svelte';
  import { connectWebSocket, wsSend } from './api.js';
  import {
    colour, white, vu,
    animationEnabled, animationValue, animations,
    brightness,
    route, pathToRoute, navigate
  } from './stores.js';
  import ColorPicker from './components/ColorPicker.svelte';
  import ThemeToggle from './components/ThemeToggle.svelte';
  import LedConfig from './components/LedConfig.svelte';

  function onPopState() {
    route.set(pathToRoute(window.location.pathname));
  }

  onMount(() => {
    connectWebSocket();
    window.addEventListener('popstate', onPopState);
  });

  onDestroy(() => {
    if (typeof window !== 'undefined') {
      window.removeEventListener('popstate', onPopState);
    }
  });

  function toggleWhite() {
    const next = !$white;
    wsSend.white(next);
    if (next) {
      animationValue.set(-1);
      animationEnabled.set(false);
    }
  }

  function toggleVu() {
    wsSend.vu(!$vu);
  }

  function onColourChange(e) {
    wsSend.colour(e.detail);
    animationValue.set(-1);
    animationEnabled.set(false);
  }

  function onAnimationChange(e) {
    const v = parseInt(e.target.value);
    animationValue.set(v);
    if (v > -1) {
      animationEnabled.set(true);
      wsSend.animation(true, v);
    } else {
      animationEnabled.set(false);
      wsSend.animation(false);
    }
  }

  // Only send to the device on release (`change`), not during the drag (`input`).
  // The native <input type="range"> fires `input` at ~60Hz which flooded the
  // device with WS messages and NVS dirty-marks, crashing it on long drags.
  function onBrightnessInput(e) {
    // Track the thumb locally so the UI follows the finger without lag.
    brightness.set(parseInt(e.target.value));
  }

  function onBrightnessChange(e) {
    wsSend.brightness(parseInt(e.target.value));
  }

  function goToLedConfig(e) {
    e.preventDefault();
    navigate('/led-config');
  }
</script>

{#if $route === 'led-config'}
  <LedConfig>
    <ThemeToggle slot="theme-toggle" />
  </LedConfig>
{:else}
  <main>
    <header>
      <h1>LED Controller</h1>
      <ThemeToggle />
    </header>

    <section class="card">
      <div class="row">
        <div class="control">
          <label for="colour-picker">Colour</label>
          <ColorPicker id="colour-picker" value={$colour} on:change={onColourChange} />
        </div>

        <div class="control">
          <label for="white-toggle">White</label>
          <button type="button" id="white-toggle" class="btn toggle" class:active={$white} on:click={toggleWhite}>
            {$white ? 'On' : 'Off'}
          </button>
        </div>

        <div class="control">
          <label for="vu-toggle">VU</label>
          <button type="button" id="vu-toggle" class="btn toggle" class:active={$vu} on:click={toggleVu}>
            {$vu ? 'On' : 'Off'}
          </button>
        </div>
      </div>

      <div class="control full">
        <label for="animation">Animation</label>
        <select id="animation"
                class:active={$animationEnabled}
                value={$animationValue}
                on:change={onAnimationChange}>
          <option value={-1}>None</option>
          {#each $animations as anim}
            <option value={anim.value}>{anim.name}</option>
          {/each}
        </select>
      </div>

      <div class="control full">
        <label for="brightness">Brightness</label>
        <input id="brightness"
               type="range" min="0" max="255"
               value={$brightness}
               on:input={onBrightnessInput}
               on:change={onBrightnessChange}>
      </div>
    </section>

    <nav>
      <a href="/update">Update</a>
      <a href="/setup">WiFi</a>
      <a href="/led-config" on:click={goToLedConfig}>Settings</a>
      <a href="/logs">Logs</a>
    </nav>
  </main>
{/if}

<style>
  main {
    width: 100%;
    max-width: 440px;
    margin: 0 auto;
    padding: 2rem 1rem;
  }

  header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1.4rem;
  }

  header h1 {
    font-size: 1.4rem;
    font-weight: 700;
    letter-spacing: -0.02em;
    background: linear-gradient(135deg, var(--text-primary) 0%, var(--primary) 100%);
    -webkit-background-clip: text;
    background-clip: text;
    -webkit-text-fill-color: transparent;
  }

  .row {
    display: flex;
    gap: 0.85rem;
    margin-bottom: 1.25rem;
  }

  .control {
    display: flex;
    flex-direction: column;
    gap: 0.45rem;
  }

  .row .control {
    flex: 1;
    min-width: 0;
  }

  .control.full {
    width: 100%;
    margin-bottom: 1rem;
  }

  .control.full:last-child {
    margin-bottom: 0;
  }

  .toggle {
    width: 100%;
  }

  select.active {
    border-color: var(--primary);
    color: var(--primary);
    box-shadow: 0 0 0 1px var(--primary-glow);
  }

  nav {
    display: flex;
    justify-content: center;
    gap: 0.4rem;
    margin-top: 1.5rem;
    flex-wrap: wrap;
  }

  nav a {
    color: var(--text-muted);
    text-decoration: none;
    font-size: 0.82rem;
    padding: 0.5rem 0.85rem;
    border-radius: 999px;
    border: 1px solid var(--surface-border);
    background: var(--surface-sunken);
    transition: color 0.18s ease, border-color 0.18s ease, background 0.18s ease;
  }

  nav a:hover {
    color: var(--primary);
    border-color: var(--primary);
    background: var(--surface);
  }

  @media (max-width: 480px) {
    main { padding: 1rem 0.75rem; }
    .row { flex-wrap: wrap; }
    .row .control { min-width: calc(50% - 0.5rem); }
  }
</style>
