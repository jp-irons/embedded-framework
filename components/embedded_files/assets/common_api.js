// TODO entries created in skeleton - suspect these are duplicates
export async function sendCredentials(ssid, password) {
  return fetch('/api/provision', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid, password })
  });
}

export async function fetchStatus() {
  const res = await fetch('/api/status');
  return res.json();
}

// common_api.js

// Generic GET helper
async function get(url) {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`GET ${url} failed`);
    return res.json();
}

// Generic POST helper
async function post(url, body = null) {
    const res = await fetch(url, {
        method: "POST",
        headers: body ? { "Content-Type": "application/json" } : undefined,
        body: body ? JSON.stringify(body) : null
    });
    if (!res.ok) throw new Error(`POST ${url} failed`);
    return res.json().catch(() => ({})); // allow empty JSON
}

// Generic DELETE helper
async function del(url) {
    const res = await fetch(url, { method: "DELETE" });
    if (!res.ok) throw new Error(`DELETE ${url} failed`);
    return res.json().catch(() => ({}));
}

// ---- Specific API wrappers ----

// WiFi scan
export function scanWifi() {
    return get("/framework/api/wifi/scan");
}

// WiFi status
export function wifiStatus() {
    return get("/framework/api/wifi/status");
}

// Credentials
export function listCredentials() {
    return get("/framework/api/credentials/list");
}

export function submitCredential(payload) {
    return post("/framework/api/credentials/submit", payload);
}

export function deleteCredential(ssid) {
    return del(`/framework/api/credentials/${encodeURIComponent(ssid)}`);
}

export function clearCredentials() {
    return post("/framework/api/credentials/clear");
}

export function clearNvs() {
    return post("/framework/api/credentials/clearNvs");
}

// Device
export function rebootDevice() {
    return post("/framework/api/device/reboot");
}
