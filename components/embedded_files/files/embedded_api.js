//
// embedded/api.js
//

// ---------- Generic helpers ----------

async function get(url) {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`GET ${url} failed`);
    return res.json();
}

async function post(url, body = null) {
    const res = await fetch(url, {
        method: "POST",
        headers: body ? { "Content-Type": "application/json" } : undefined,
        body: body ? JSON.stringify(body) : null
    });
    if (!res.ok) throw new Error(`POST ${url} failed`);
    return res.json().catch(() => ({}));   // allow empty JSON
}

async function del(url) {
    const res = await fetch(url, { method: "DELETE" });
    if (!res.ok) throw new Error(`DELETE ${url} failed`);
    return res.json().catch(() => ({}));
}


// ---------- WiFi Scan ----------

export function scanWifi() {
    return get(`/framework/api/wifi/scan?ts=${Date.now()}`);
}


// ---------- WiFi Status ----------

export function wifiStatus() {
    return get(`/framework/api/wifi/status?ts=${Date.now()}`);
}


// ---------- Credentials ----------

export function listCredentials() {
    return get(`/framework/api/credentials/list?ts=${Date.now()}`);
}

export function submitCredential(payload) {
    return post("/framework/api/credentials/submit", payload);
}

export function makeFirst(ssid) {
    return post("/framework/api/credentials/makeFirst", { ssid });
}

export function deleteCredential(ssid) {
    return del(`/framework/api/credentials/${encodeURIComponent(ssid)}`);
}

export function clearCredentials() {
    return post("/framework/api/credentials/clear");
}

// ---------- Device ----------

export function clearNvs() {
    return post("/framework/api/device/clearNvs");
}


export function rebootDevice() {
    return post("/framework/api/device/reboot");
}
