import { writable } from 'svelte/store';

// LED state, mirrored from the device over WebSocket
export const colour = writable('#ff0000');
export const white = writable(false);
export const vu = writable(false);
export const animationEnabled = writable(false);
export const animationValue = writable(-1);   // -1 = no animation selected
export const brightness = writable(128);

// Animation list — populated when the device sends the "animations" message
export const animations = writable([]);       // [{ name, value }]

// Path-based view routing. Kept as a store so any component can subscribe.
// Values match window.location.pathname so deep links and the LVGL device's
// /led-config link both land on the right view.
export const route = writable(pathToRoute(typeof window !== 'undefined' ? window.location.pathname : '/'));

export function pathToRoute(pathname) {
  if (pathname.startsWith('/led-config')) return 'led-config';
  return 'main';
}

export function navigate(pathname) {
  if (typeof window === 'undefined') return;
  if (window.location.pathname !== pathname) {
    window.history.pushState({}, '', pathname);
  }
  route.set(pathToRoute(pathname));
}
