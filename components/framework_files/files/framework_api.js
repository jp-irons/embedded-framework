//
// framework/api.js
//

// ---------- Auth state ----------
//
// The device now uses session tokens for browser access.
//
// Login flow:
//   1. POST /framework/api/auth/login  with  Authorization: Basic <base64(admin:password)>
//   2. Server returns  {"token":"<64-hex>"}
//   3. All subsequent requests include  Authorization: Bearer <token>
//
// The token is kept in sessionStorage so it survives full page loads within
// the same browser tab without forcing a re-login.  sessionStorage is
// tab-scoped and cleared automatically when the tab is closed, which is the
// appropriate lifetime for a device management session.
//
// On a 401 response the token is cleared and the onAuthRequired callback fires,
// which re-shows the login overlay.  The user logs in again and receives a new
// token; the current route re-mounts without a page reload.
//
// Machine-to-machine clients use a persistent API key (generated on the
// Security page) presented identically as a Bearer token.  The server accepts
// both session tokens and API keys on every framework API endpoint.
//
// WHY NOT the browser's built-in password manager?
//
// Chrome will not offer to save credentials for origins with a certificate
// error — including self-signed HTTPS certs.  The ESP32's embedded server uses
// a self-signed cert by default, so Chrome's password manager is silently
// suppressed.  The Credential Management API (navigator.credentials.store) is
// also unavailable for the same reason.  Session tokens in sessionStorage give
// equivalent UX: one login per browser session, transparent re-auth on device
// reboot, credential cleared on tab close.
const SESSION_KEY = "fw_auth_token";

let _token          = (() => { try { return sessionStorage.getItem(SESSION_KEY); } catch { return null; } })();
let _authRequiredCb = null;

export function setToken(token) {
    _token = token;
    try { sessionStorage.setItem(SESSION_KEY, token); } catch {}
}
export function clearToken() {
    _token = null;
    try { sessionStorage.removeItem(SESSION_KEY); } catch {}
}
export function onAuthRequired(fn) { _authRequiredCb = fn; }
export function isAuthenticated()  { return _token !== null; }

function authHeaders(extra = {}) {
    if (!_token) return extra;
    return { ...extra, "Authorization": "Bearer " + _token };
}

function handle401() {
    clearToken();
    if (_authRequiredCb) _authRequiredCb();
}

// ---------- Generic helpers ----------

async function get(url) {
    const res = await fetch(url, { headers: authHeaders() });
    if (res.status === 401) { handle401(); throw new Error("Unauthorized"); }
    if (!res.ok) throw new Error(`GET ${url} failed`);
    return res.json();
}

async function post(url, body = null) {
    const res = await fetch(url, {
        method: "POST",
        headers: authHeaders(body ? { "Content-Type": "application/json" } : {}),
        body: body ? JSON.stringify(body) : null
    });
    if (res.status === 401) { handle401(); throw new Error("Unauthorized"); }
    if (!res.ok) throw new Error(`POST ${url} failed`);
    return res.json().catch(() => ({}));
}

async function del(url) {
    const res = await fetch(url, { method: "DELETE", headers: authHeaders() });
    if (res.status === 401) { handle401(); throw new Error("Unauthorized"); }
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
        if (_token) {
            xhr.setRequestHeader("Authorization", "Bearer " + _token);
        }

        if (onProgress) {
            xhr.upload.onprogress = (e) => {
                if (e.lengthComputable) onProgress(Math.round((e.loaded / e.total) * 100));
            };
        }

        xhr.onload = () => {
            if (xhr.status === 401) {
                handle401();
                reject(new Error("Unauthorized"));
                return;
            }
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


// ---------- Auth ----------

/**
 * Exchange a password for a session token.
 * Sends Basic Auth credentials to POST /auth/login and stores the returned
 * token.  Throws on wrong password (401) or network error.
 */
export async function login(password) {
    const res = await fetch("/framework/api/auth/login", {
        method: "POST",
        headers: { "Authorization": "Basic " + btoa("admin:" + password) }
    });
    if (res.status === 401) throw new Error("Unauthorized");
    if (!res.ok) throw new Error("Login request failed");
    const data = await res.json();
    setToken(data.token);
}

/**
 * Invalidate the current session token on the server and clear it locally.
 */
export async function logout() {
    if (!_token) return;
    try {
        await post("/framework/api/auth/logout");
    } finally {
        clearToken();
    }
}

export function getAuthStatus() {
    return get(`/framework/api/auth/status?ts=${Date.now()}`);
}

export function changePassword(newPassword) {
    return post("/framework/api/auth/password", { newPassword });
}


// ---------- API key (M2M) ----------

export function getApiKeyStatus() {
    return get(`/framework/api/auth/apikey?ts=${Date.now()}`);
}

/** Generate (or rotate) the device API key.  Returns {"key": "<64-hex>"}. */
export function generateApiKey() {
    return post("/framework/api/auth/apikey");
}

/** Revoke the current API key. */
export function revokeApiKey() {
    return del("/framework/api/auth/apikey");
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
