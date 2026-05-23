import { animations, colour, white, vu, animationEnabled, animationValue, brightness } from './stores.js';

let ws = null;
let reconnectTimer = null;

export function connectWebSocket() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const url = `${protocol}//${window.location.host}/ws`;

  ws = new WebSocket(url);

  ws.onopen = () => {
    send({ message: 'connect' });
  };

  ws.onmessage = (event) => {
    let data;
    try { data = JSON.parse(event.data); } catch { return; }

    // Logger messages are forwarded over the same socket — ignore them here.
    if (data.type === 'log') return;

    if (data.message === 'states' && Array.isArray(data.controls)) {
      for (const ctrl of data.controls) {
        switch (ctrl.name) {
          case 'vu':         vu.set(!!ctrl.state); break;
          case 'white':      white.set(!!ctrl.state); break;
          case 'animation':
            animationEnabled.set(!!ctrl.state);
            if (typeof ctrl.animation === 'number') animationValue.set(ctrl.animation);
            break;
          case 'colour':     if (typeof ctrl.state === 'string') colour.set(ctrl.state); break;
          case 'brightness': if (typeof ctrl.state === 'number') brightness.set(ctrl.state); break;
        }
      }
    } else if (Array.isArray(data.animations)) {
      animations.set(data.animations);
    }
  };

  ws.onclose = () => {
    reconnectTimer = setTimeout(connectWebSocket, 2000);
  };

  ws.onerror = () => {
    // onclose will fire next — let it handle the reconnect.
  };
}

function send(payload) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(payload));
  }
}

// Outbound message helpers — names match the device-side handlers in WebUIManager.cpp.
export const wsSend = {
  vu(value)         { send({ message: 'vu', value }); },
  white(value)      { send({ message: 'white', value }); },
  brightness(value) { send({ message: 'brightness', value }); },
  colour(value)     { send({ message: 'colour', value }); },
  animation(enabled, animation = -1) {
    if (enabled) send({ message: 'animation', value: true, animation });
    else         send({ message: 'animation', value: false });
  }
};
