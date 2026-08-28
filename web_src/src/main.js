import { mount } from 'svelte';
import App from './App.svelte';
import './styles/global.css';

// Svelte 5 mounts through mount() — `new App({ target })` was removed in v5.
const app = mount(App, {
  target: document.getElementById('app')
});

export default app;
