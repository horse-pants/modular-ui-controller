<script>
  import { onMount } from 'svelte';

  let theme = 'dark';

  onMount(() => {
    theme = localStorage.getItem('theme') === 'light' ? 'light' : 'dark';
    apply();
  });

  function apply() {
    document.body.classList.toggle('light', theme === 'light');
  }

  function toggle() {
    theme = theme === 'light' ? 'dark' : 'light';
    localStorage.setItem('theme', theme);
    apply();
  }
</script>

<button type="button"
        class="theme-toggle"
        on:click={toggle}
        aria-label={theme === 'light' ? 'Switch to dark mode' : 'Switch to light mode'}>
  {#if theme === 'light'}
    <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>
    </svg>
  {:else}
    <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="4"/>
      <path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41"/>
    </svg>
  {/if}
</button>

<style>
  .theme-toggle {
    width: 40px;
    height: 40px;
    border-radius: 50%;
    background: var(--surface);
    border: 1px solid var(--surface-border);
    color: var(--text-muted);
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 0;
    transition: color 0.18s ease, border-color 0.18s ease, transform 0.12s ease, box-shadow 0.18s ease;
  }

  .theme-toggle:hover {
    color: var(--primary);
    border-color: var(--primary);
    box-shadow: 0 0 14px var(--primary-glow);
  }

  .theme-toggle:active {
    transform: translateY(1px);
  }
</style>
