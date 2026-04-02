const API_BASE = import.meta.env.VITE_API_BASE ?? 'http://127.0.0.1:8080';
const WS_BASE = import.meta.env.VITE_WS_BASE ?? API_BASE.replace(/^http/, 'ws');

async function request(path, options = {}) {
  const response = await fetch(`${API_BASE}${path}`, {
    headers: {
      'Content-Type': 'application/json',
      ...(options.headers ?? {}),
    },
    ...options,
  });

  const data = await response.json();
  if (!response.ok) {
    throw new Error(data.detail ?? data.message ?? 'request failed');
  }
  return data;
}

export function createEventsSocket() {
  return new WebSocket(`${WS_BASE}/ws/events`);
}

export function getSummary() {
  return request('/api/summary', { method: 'GET' });
}

export function getServices() {
  return request('/api/services', { method: 'GET' });
}

export function login(userId) {
  return request('/api/login', { method: 'POST', body: JSON.stringify({ user_id: userId }) });
}

export function disconnectUser(userId) {
  return request('/api/disconnect', { method: 'POST', body: JSON.stringify({ user_id: userId }) });
}

export function sendMessage(payload) {
  return request('/api/send-message', { method: 'POST', body: JSON.stringify(payload) });
}

export function pullOffline(userId) {
  return request('/api/pull-offline', { method: 'POST', body: JSON.stringify({ user_id: userId }) });
}

export function createManifest(payload) {
  return request('/api/file/manifest', { method: 'POST', body: JSON.stringify(payload) });
}

export function uploadChunk(payload) {
  return request('/api/file/chunk', { method: 'POST', body: JSON.stringify(payload) });
}

export function queryResume(payload) {
  return request('/api/file/resume', { method: 'POST', body: JSON.stringify(payload) });
}

export function downloadTransfer(payload) {
  return request('/api/file/download', { method: 'POST', body: JSON.stringify(payload) });
}