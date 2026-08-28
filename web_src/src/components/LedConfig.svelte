<script>
  import { onMount } from 'svelte';
  import { navigate } from '../stores.js';
  import ScreenshotPanel from './ScreenshotPanel.svelte';

  let numStrips = '';
  let ledsPerStrip = '';
  let loaded = false;

  // 'idle' | 'saving' | 'saved' | 'clearing' | 'cleared' | 'error'
  let status = 'idle';
  let errorMessage = '';
  let restartCountdown = 0;

  // Screensaver settings (applied live, no restart).
  let screensaverEnabled = true;
  let screensaverTimeout = 30;
  let settingsLoaded = false;
  let settingsStatus = 'idle';   // 'idle' | 'saving' | 'saved' | 'error'
  let settingsError = '';

  // Home Assistant / MQTT broker settings (applied live, no restart).
  let mqttEnabled = false;
  let mqttHost = 'homeassistant.local';
  let mqttPort = 1883;
  let mqttUser = '';
  let mqttPass = '';          // write-only: blank = keep existing on save
  let mqttHasPass = false;    // whether the device already has a stored password
  let mqttPrefix = 'modular-ui';
  let mqttStatus = 'idle';       // 'idle' | 'saving' | 'saved' | 'error'
  let mqttError = '';

  $: total = (parseInt(numStrips) || 0) * (parseInt(ledsPerStrip) || 0);

  onMount(async () => {
    try {
      const r = await fetch('/get-led-config');
      const data = await r.json();
      if (data.hasLedSettings) {
        numStrips = data.numStrips || '';
        ledsPerStrip = data.ledsPerStrip || '';
      }
    } catch (e) {
      console.error('Failed to load LED config:', e);
    } finally {
      loaded = true;
    }

    try {
      const r = await fetch('/get-settings');
      const data = await r.json();
      screensaverEnabled = !!data.screensaverEnabled;
      if (typeof data.screensaverTimeoutSec === 'number') {
        screensaverTimeout = data.screensaverTimeoutSec;
      }
      mqttEnabled = !!data.mqttEnabled;
      if (typeof data.mqttHost === 'string') mqttHost = data.mqttHost;
      if (typeof data.mqttPort === 'number') mqttPort = data.mqttPort;
      if (typeof data.mqttUser === 'string') mqttUser = data.mqttUser;
      mqttHasPass = !!data.mqttHasPass;
      if (typeof data.mqttPrefix === 'string') mqttPrefix = data.mqttPrefix;
    } catch (e) {
      console.error('Failed to load settings:', e);
    } finally {
      settingsLoaded = true;
    }
  });

  async function saveMqtt() {
    mqttStatus = 'saving';
    mqttError = '';
    try {
      const body = new URLSearchParams();
      body.set('mqtt_enabled', mqttEnabled ? 'true' : 'false');
      body.set('mqtt_host', mqttHost || '');
      body.set('mqtt_port', String(parseInt(mqttPort) || 1883));
      body.set('mqtt_user', mqttUser || '');
      body.set('mqtt_pass', mqttPass || '');
      body.set('mqtt_prefix', mqttPrefix || 'modular-ui');

      const r = await fetch('/save-settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body
      });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);

      // Password is write-only — if one was just entered, it's now stored; clear the
      // field so it isn't resubmitted and reflect that a password exists.
      if (mqttPass.length > 0) {
        mqttHasPass = true;
        mqttPass = '';
      }

      mqttStatus = 'saved';
      setTimeout(() => { if (mqttStatus === 'saved') mqttStatus = 'idle'; }, 2000);
    } catch (e) {
      mqttStatus = 'error';
      mqttError = String(e.message || e);
    }
  }

  async function saveSettings() {
    settingsStatus = 'saving';
    settingsError = '';
    try {
      const body = new URLSearchParams();
      body.set('screensaver_enabled', screensaverEnabled ? 'true' : 'false');
      body.set('screensaver_timeout_sec', String(parseInt(screensaverTimeout) || 30));

      const r = await fetch('/save-settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body
      });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);

      settingsStatus = 'saved';
      setTimeout(() => { if (settingsStatus === 'saved') settingsStatus = 'idle'; }, 2000);
    } catch (e) {
      settingsStatus = 'error';
      settingsError = String(e.message || e);
    }
  }

  async function save() {
    if (!numStrips || !ledsPerStrip) return;
    status = 'saving';
    errorMessage = '';

    try {
      const body = new URLSearchParams();
      body.set('num_strips', String(numStrips));
      body.set('leds_per_strip', String(ledsPerStrip));

      const r = await fetch('/save-led-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body
      });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);

      status = 'saved';
      // The device reboots ~2s after the POST. Count down then hop back to /.
      restartCountdown = 5;
      const tick = setInterval(() => {
        restartCountdown -= 1;
        if (restartCountdown <= 0) {
          clearInterval(tick);
          navigate('/');
        }
      }, 1000);
    } catch (e) {
      status = 'error';
      errorMessage = String(e.message || e);
    }
  }

  async function clearState() {
    if (!confirm('Clear all saved LED state preferences?\n\nThis will reset brightness, color, animation, and mode settings. The device will show the default red fade-in on next boot.')) return;
    status = 'clearing';
    errorMessage = '';

    try {
      const r = await fetch('/clear-led-state', { method: 'POST' });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      status = 'cleared';
    } catch (e) {
      status = 'error';
      errorMessage = String(e.message || e);
    }
  }

  function backToMain(e) {
    e.preventDefault();
    navigate('/');
  }
</script>

{#if status === 'saved'}
  <main class="centered">
    <div class="card status-card">
      <div class="icon">&#10003;</div>
      <h1>Configuration Saved</h1>
      <p>Your LED configuration has been saved.</p>
      <p class="status-line">Restarting device… ({restartCountdown}s)</p>
    </div>
  </main>
{:else if status === 'cleared'}
  <main class="centered">
    <div class="card status-card">
      <div class="icon">&#8635;</div>
      <h1>State Cleared</h1>
      <p>Saved LED preferences have been deleted.</p>
      <p>On next boot the device will fade in to red (first-boot behaviour).</p>
      <a class="back-pill" href="/led-config" on:click|preventDefault={() => { status = 'idle'; }}>
        &larr; Back to Settings
      </a>
    </div>
  </main>
{:else}
  <main>
    <header>
      <h1>Settings</h1>
      <slot name="theme-toggle" />
    </header>

    <section class="card">
      <h3 class="section-title">Idle Screensaver</h3>
      <p class="text-muted section-desc">
        Show the full-screen VU meter on the device when the screen has been idle.
      </p>

      {#if !settingsLoaded}
        <p class="text-muted">Loading…</p>
      {:else}
        <div class="control row-control">
          <label for="screensaver_enabled">Enabled</label>
          <button id="screensaver_enabled"
                  type="button"
                  class="btn toggle"
                  class:active={screensaverEnabled}
                  on:click={() => (screensaverEnabled = !screensaverEnabled)}>
            {screensaverEnabled ? 'On' : 'Off'}
          </button>
        </div>

        {#if screensaverEnabled}
          <div class="control">
            <label for="screensaver_timeout">Idle Timeout (seconds)</label>
            <input id="screensaver_timeout" type="number" min="5" max="600"
                   bind:value={screensaverTimeout}>
          </div>
        {/if}

        <button type="button"
                class="btn btn-primary"
                disabled={settingsStatus === 'saving'}
                on:click={saveSettings}>
          {settingsStatus === 'saving' ? 'Saving…' : 'Save'}
        </button>

        {#if settingsStatus === 'saved'}
          <p class="ok">Saved &#10003;</p>
        {:else if settingsStatus === 'error'}
          <p class="error">Error: {settingsError}</p>
        {/if}
      {/if}
    </section>

    <section class="card">
      <h3 class="section-title">Home Assistant (MQTT)</h3>
      <p class="text-muted section-desc">
        Expose the LEDs to Home Assistant via MQTT Discovery — no add-on or custom
        integration needed. Point this at your broker; entities appear automatically.
      </p>

      {#if !settingsLoaded}
        <p class="text-muted">Loading…</p>
      {:else}
        <div class="control row-control">
          <label for="mqtt_enabled">Enabled</label>
          <button id="mqtt_enabled"
                  type="button"
                  class="btn toggle"
                  class:active={mqttEnabled}
                  on:click={() => (mqttEnabled = !mqttEnabled)}>
            {mqttEnabled ? 'On' : 'Off'}
          </button>
        </div>

        {#if mqttEnabled}
          <div class="control">
            <label for="mqtt_host">Broker Host</label>
            <input id="mqtt_host" type="text" bind:value={mqttHost}
                   placeholder="homeassistant.local" autocomplete="off">
          </div>
          <div class="control">
            <label for="mqtt_port">Port</label>
            <input id="mqtt_port" type="number" min="1" max="65535" bind:value={mqttPort}>
          </div>
          <div class="control">
            <label for="mqtt_user">Username</label>
            <input id="mqtt_user" type="text" bind:value={mqttUser} autocomplete="off">
          </div>
          <div class="control">
            <label for="mqtt_pass">Password</label>
            <input id="mqtt_pass" type="password" bind:value={mqttPass}
                   autocomplete="new-password"
                   placeholder={mqttHasPass ? '•••••••• (leave blank to keep)' : 'Set a password'}>
          </div>
          <div class="control">
            <label for="mqtt_prefix">Topic Prefix</label>
            <input id="mqtt_prefix" type="text" bind:value={mqttPrefix}
                   placeholder="modular-ui" autocomplete="off">
          </div>
        {/if}

        <button type="button"
                class="btn btn-primary"
                disabled={mqttStatus === 'saving'}
                on:click={saveMqtt}>
          {mqttStatus === 'saving' ? 'Saving…' : 'Save'}
        </button>

        {#if mqttStatus === 'saved'}
          <p class="ok">Saved &#10003;</p>
        {:else if mqttStatus === 'error'}
          <p class="error">Error: {mqttError}</p>
        {/if}
      {/if}
    </section>

    <ScreenshotPanel />

    <section class="card">
      <h3 class="section-title">LED Strips</h3>
      {#if !loaded}
        <p class="text-muted">Loading…</p>
      {:else}
        <div class="control">
          <label for="num_strips">Number of Strips</label>
          <input id="num_strips" type="number" min="1" max="20" bind:value={numStrips} required>
        </div>

        <div class="control">
          <label for="leds_per_strip">LEDs per Strip</label>
          <input id="leds_per_strip" type="number" min="1" max="300" bind:value={ledsPerStrip} required>
        </div>

        <p class="text-muted total">Total LEDs: <strong>{total}</strong></p>

        <button type="button"
                class="btn btn-primary"
                disabled={status === 'saving' || !numStrips || !ledsPerStrip}
                on:click={save}>
          {status === 'saving' ? 'Saving…' : 'Save & Restart'}
        </button>

        {#if status === 'error'}
          <p class="error">Error: {errorMessage}</p>
        {/if}

        <hr>

        <div class="danger-zone">
          <h3>Reset LED State</h3>
          <p class="text-muted">
            Clear saved brightness, colour, animation, and mode settings.<br>
            Device will boot with default red fade-in after restart.
          </p>
          <button type="button"
                  class="btn btn-danger"
                  disabled={status === 'clearing'}
                  on:click={clearState}>
            {status === 'clearing' ? 'Clearing…' : 'Clear Saved State'}
          </button>
        </div>

        <a class="back" href="/" on:click={backToMain}>&larr; Back</a>
      {/if}
    </section>
  </main>
{/if}

<style>
  main {
    width: 100%;
    max-width: 440px;
    margin: 0 auto;
    padding: 2rem 1rem;
  }

  main.centered {
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
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

  .control {
    display: flex;
    flex-direction: column;
    gap: 0.45rem;
    margin-bottom: 1rem;
  }

  .section-title {
    font-size: 1rem;
    font-weight: 600;
    margin-bottom: 0.35rem;
  }

  .section-desc {
    font-size: 0.85rem;
    margin-bottom: 1.1rem;
  }

  /* Label beside its control (used for the on/off toggle row). */
  .row-control {
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
  }

  .row-control label {
    margin-bottom: 0;
  }

  /* Override the component-wide `.btn { width: 100% }` so the toggle stays
     compact beside its label. */
  .btn.toggle {
    width: auto;
    min-width: 5.5rem;
  }

  .ok {
    color: var(--success);
    margin-top: 0.75rem;
    font-size: 0.85rem;
  }

  .total {
    margin-bottom: 1rem;
  }

  .total strong {
    color: var(--text-primary);
    font-weight: 600;
  }

  .btn {
    width: 100%;
  }

  .btn-primary {
    margin-top: 0.5rem;
  }

  .btn-primary:disabled,
  .btn-danger:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .error {
    color: var(--danger);
    margin-top: 0.75rem;
    font-size: 0.85rem;
  }

  hr {
    border: none;
    border-top: 1px solid var(--surface-border);
    margin: 1.75rem 0;
  }

  .danger-zone {
    text-align: center;
  }

  .danger-zone h3 {
    color: var(--danger);
    font-size: 1rem;
    font-weight: 600;
    margin-bottom: 0.5rem;
  }

  .danger-zone p {
    margin-bottom: 1rem;
  }

  .back {
    display: block;
    text-align: center;
    margin-top: 1rem;
    color: var(--primary);
    text-decoration: none;
    font-size: 0.85rem;
  }

  .back:hover {
    text-decoration: underline;
  }

  /* Saved / Cleared status cards */
  .status-card {
    max-width: 380px;
    width: 100%;
    text-align: center;
    padding: 2rem 1.5rem;
  }

  .status-card .icon {
    font-size: 3rem;
    color: var(--primary);
    margin-bottom: 1rem;
    text-shadow: 0 0 24px var(--primary-glow);
  }

  .status-card h1 {
    font-size: 1.2rem;
    font-weight: 700;
    margin-bottom: 0.75rem;
    background: linear-gradient(135deg, var(--text-primary) 0%, var(--primary) 100%);
    -webkit-background-clip: text;
    background-clip: text;
    -webkit-text-fill-color: transparent;
  }

  .status-card p {
    color: var(--text-muted);
    font-size: 0.9rem;
    margin-bottom: 0.5rem;
  }

  .status-line {
    margin-top: 1rem;
    font-size: 0.85rem;
  }

  .back-pill {
    display: inline-block;
    margin-top: 1.25rem;
    color: var(--primary);
    text-decoration: none;
    font-size: 0.9rem;
    padding: 0.5rem 1rem;
    border: 1px solid var(--surface-border);
    border-radius: 999px;
    background: var(--surface-sunken);
    transition: border-color 0.18s, color 0.18s;
  }

  .back-pill:hover {
    border-color: var(--primary);
  }
</style>
