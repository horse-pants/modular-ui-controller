<script>
  import { onDestroy } from 'svelte';

  // Grabs GET /screenshot.png off the device. Fetched into a blob rather than
  // pointed at with <img src>, because the device serves one screenshot at a
  // time (409 otherwise) — a blob means the preview and the Download button are
  // the same single request, and saving is instant.

  let url = null;          // object URL of the last capture
  let takenAt = null;
  let bytes = 0;
  let busy = false;
  let error = '';

  function release() {
    if (url) {
      URL.revokeObjectURL(url);
      url = null;
    }
  }

  async function capture() {
    busy = true;
    error = '';
    try {
      // The device sends no-store, but a proxy in between might not.
      const res = await fetch(`/screenshot.png?t=${Date.now()}`);
      if (!res.ok) {
        error = res.status === 409
          ? 'Another screenshot is still being sent — try again in a moment.'
          : res.status === 503
            ? 'The device is out of memory for the screenshot encoder.'
            : `Screenshot failed (HTTP ${res.status}).`;
        return;
      }
      const blob = await res.blob();
      release();
      url = URL.createObjectURL(blob);
      bytes = blob.size;
      takenAt = new Date();
    } catch {
      error = 'Could not reach the device.';
    } finally {
      busy = false;
    }
  }

  onDestroy(release);

  // Colon-free, so it is a valid filename on every OS.
  $: filename = takenAt
    ? `screen-${takenAt.toISOString().slice(0, 19).replace(/[:T]/g, '-')}.png`
    : 'screen.png';
</script>

<section class="card">
  <h3 class="section-title">Screenshot</h3>
  <p class="text-muted section-desc">
    Capture what's on the device screen right now as a PNG — for help docs and bug reports.
  </p>

  <div class="shot-actions">
    <button type="button" class="btn btn-primary" disabled={busy} on:click={capture}>
      {busy ? 'Capturing…' : url ? 'Retake' : 'Take screenshot'}
    </button>
    {#if url}
      <a class="btn" href={url} download={filename}>Download</a>
    {/if}
  </div>

  {#if error}
    <p class="shot-error">{error}</p>
  {/if}

  {#if url}
    <figure class="shot">
      <img src={url} alt="The device screen at the moment of capture" />
      <figcaption class="text-muted">
        {takenAt.toLocaleTimeString()} · {Math.round(bytes / 1024)} KB
      </figcaption>
    </figure>
  {/if}
</section>

<style>
  .shot-actions {
    display: flex;
    gap: 0.6rem;
    flex-wrap: wrap;
    align-items: center;
  }

  /* The <a> is a button by role here, so it needs the line-height a <button>
     gets for free. */
  .shot-actions a.btn {
    display: inline-flex;
    align-items: center;
    text-decoration: none;
  }

  .shot-error {
    margin: 0.75rem 0 0;
    color: var(--danger);
    font-size: 0.85rem;
  }

  .shot {
    margin: 1rem 0 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.5rem;
  }

  /* The panel is a tall 320x480, so cap the height rather than the width or it
     runs off the bottom of a phone. */
  .shot img {
    max-width: 100%;
    max-height: 60vh;
    border: 1px solid var(--border);
    border-radius: var(--radius-card);
    background: var(--bg-dark);
  }

  .shot figcaption {
    font-size: 0.8rem;
  }
</style>
