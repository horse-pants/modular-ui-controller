import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

// Device address for `npm run dev` — defaults to the mDNS hostname.
// Override with ESP32_HOST=192.168.x.x npm run dev if mDNS isn't resolving.
const DEVICE_HOST = process.env.ESP32_HOST || 'modular-ui.local';
const httpTarget = `http://${DEVICE_HOST}`;
const wsTarget = `ws://${DEVICE_HOST}`;

export default defineConfig({
  plugins: [svelte()],
  server: {
    port: 5173,
    proxy: {
      '/ws': {
        target: wsTarget,
        ws: true,
        changeOrigin: true
      },
      // API endpoints go to the device. /led-config itself is handled by the
      // Svelte router locally (and on the device by serving index.html).
      '/get-led-config':  { target: httpTarget, changeOrigin: true },
      '/save-led-config': { target: httpTarget, changeOrigin: true },
      '/clear-led-state': { target: httpTarget, changeOrigin: true },
      '/update':          { target: httpTarget, changeOrigin: true },
      '/setup':           { target: httpTarget, changeOrigin: true },
      '/logs':            { target: httpTarget, changeOrigin: true }
    }
  },
  build: {
    outDir: '../data/web',
    emptyOutDir: true,
    // Single JS/CSS bundles — no chunking. LittleFS images do better with fewer files.
    rollupOptions: {
      output: {
        entryFileNames: 'app.js',
        chunkFileNames: 'app.js',
        assetFileNames: (assetInfo) => {
          if (assetInfo.name.endsWith('.css')) return 'app.css';
          return assetInfo.name;
        }
      }
    }
  }
});
