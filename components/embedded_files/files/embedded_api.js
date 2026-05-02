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

// ---------- Firmware ----------

export function loadFirmwareStatus() {
    return get(`/framework/api/firmware/status?ts=${Date.now()}`);
}

/**
 * Upload a firmware .bin file.
 * Streams the ArrayBuffer directly as application/octet-stream so the device
 * can receive it in chunks without buffering the entire image in DRAM.
 * @param {File} file          - File object from an <input type="file">
 * @param {function} onProgress - Called with 0..100 percentage
 * @returns {Promise<object>}  - Resolves with the JSON response body
 */
export function uploadFirmware(file, onProgress) {
    return new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        xhr.open("POST", "/framework/api/firmware/upload");
        xhr.setRequestHeader("Content-Type", "application/octet-stream");

        if (onProgress) {
            xhr.upload.onprogress = (e) => {
                if (e.lengthComputable) onProgress(Math.round((e.loaded / e.total) * 100));
            };
        }

        xhr.onload = () => {
            if (xhr.status >= 200 && xhr.status < 300) {
                try { resolve(JSON.parse(xhr.responseText)); }
                catch { resolve({}); }
            } else {
                let msg = `HTTP ${xhr.status}`;
                try { msg = JSON.parse(xhr.responseText).error || msg; } catch {}
                reject(new Error(msg));
            }
        };
        xhr.onerror   = () => reject(new Error("Network error during upload"));
        xhr.ontimeout = () => reject(new Error("Upload timed out"));

        // Send the raw binary — no multipart wrapper needed
        xhr.send(file);
    });
}

export function rollbackFirmware() {
    return post("/framework/api/firmware/rollback");
}

export function factoryResetFirmware() {
    return post("/framework/api/firmware/factoryReset");
}


// ---------- Device ----------

export function clearNvs() {
    return post("/framework/api/device/clearNvs");
}


export function rebootDevice() {
    return post("/framework/api/device/reboot");
}

export function loadDeviceInfo() {
    return get(`/framework/api/device/info?ts=${Date.now()}`);
}
