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

/**
 * Force re-authentication from anywhere in the app.
 * Clears the current token and fires the login overlay, exactly as a 401 does.
 * Safe to call when already unauthenticated — clearToken() and
 * showLoginOverlay() are both idempotent.
 *
 * Use this instead of showing an error popup when an API call fails during
 * an automatic data load (e.g. credentials list on mount).  The root cause
 * is almost always a stale session, not a genuine server error.
 */
export function forceReauth() {
    handle401();
}

// ---------- Generic helpers ----------

async function get(url) {
    // Snapshot the token before the request so we can detect stale responses.
    // After a reboot, timed-out in-flight requests (ERR_TIMED_OUT, ~30 s) can
    // resolve with a 401 long after a fresh login has stored a new token.
    // Without this guard each such stale 401 would wipe the new token and
    // re-show the login overlay.
    const tokenSnapshot = _token;
    let res;
    try { res = await fetch(url, { headers: authHeaders() }); }
    catch { throw new Error("network"); }
    if (res.status === 401) {
        if (_token === tokenSnapshot) handle401(); // only act if token is unchanged
        throw new Error("unauthorized");
    }
    if (!res.ok) throw new Error(`GET ${url} failed: ${res.status}`);
    return res.json();
}

async function post(url, body = null) {
    const tokenSnapshot = _token;
    let res;
    try {
        res = await fetch(url, {
            method: "POST",
            headers: authHeaders(body ? { "Content-Type": "application/json" } : {}),
            body: body ? JSON.stringify(body) : null
        });
    } catch { throw new Error("network"); }
    if (res.status === 401) {
        if (_token === tokenSnapshot) handle401();
        throw new Error("unauthorized");
    }
    if (!res.ok) throw new Error(`POST ${url} failed: ${res.status}`);
    return res.json().catch(() => ({}));
}

async function del(url) {
    const tokenSnapshot = _token;
    let res;
    try { res = await fetch(url, { method: "DELETE", headers: authHeaders() }); }
    catch { throw new Error("network"); }
    if (res.status === 401) {
        if (_token === tokenSnapshot) handle401();
        throw new Error("unauthorized");
    }
    if (!res.ok) throw new Error(`DELETE ${url} failed: ${res.status}`);
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
        // Snapshot the token before the request — same stale-401 guard as
        // get/post/del.  An upload can take many seconds; if the device reboots
        // during that window the XHR will eventually resolve with a 401.  Without
        // the snapshot, handle401() would wipe the new token and re-show the
        // login overlay even if the user has already logged back in.
        const tokenSnapshot = _token;
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
                if (_token === tokenSnapshot) handle401();
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
        // If the token has changed since the upload started, the device rebooted
        // and the user has already re-authenticated — use "network" so the
        // catch block in handleFirmwareUpload treats this as a stale callback
        // and discards it silently.  If the token is unchanged, surface the
        // real message so a genuine connectivity failure is shown to the user.
        xhr.onerror   = () => reject(new Error(
            _token !== tokenSnapshot ? "network" : "Network error during upload"));
        xhr.ontimeout = () => reject(new Error(
            _token !== tokenSnapshot ? "network" : "Upload timed out"));

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
    let res;
    try {
        res = await fetch("/framework/api/auth/login", {
            method: "POST",
            headers: { "Authorization": "Basic " + btoa("admin:" + password) }
        });
    } catch {
        // Network error — device unreachable (rebooting, TLS cert changed, etc.)
        throw new Error("network");
    }
    if (res.status === 401) throw new Error("unauthorized");
    if (!res.ok) throw new Error("server");
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


// ---------- Reconnect polling ----------
//
// After an intentional reboot (user clicked Reboot / Rollback / Factory Reset)
// we need the UI to recover automatically without a full page reload.
// location.reload() is unreliable in the ~60 s after a device reboot because
// the ESP32's TLS session cache is cleared on reboot; the browser tries to
// resume the old session, gets a fatal alert (-0x7780), and the reload fails
// silently.
//
// Instead, we poll getAuthStatus() every few seconds.  Three outcomes:
//   • Network error  → device still rebooting; keep polling.
//   • 401            → device up, new session required; handle401() fires the
//                      login overlay automatically; we stop polling.
//   • 200            → device up, session unexpectedly still valid; stop polling.

let _reconnectTimer = null;

/**
 * Start polling for device availability after an intentional reboot.
 * Safe to call multiple times — cancels any existing poll before starting.
 */
export function startReconnectPolling(intervalMs = 3000) {
    stopReconnectPolling();
    console.debug("[reconnect] polling started");
    _reconnectTimer = setInterval(async () => {
        try {
            await getAuthStatus();
            // Device back up and old session still valid (rare after reboot)
            console.debug("[reconnect] device back — session valid, stopping poll");
            stopReconnectPolling();
        } catch (err) {
            if (err.message !== "network") {
                // 401 received — handle401() already fired the login overlay
                console.debug("[reconnect] device back — 401 received, stopping poll");
                stopReconnectPolling();
            }
            // "network" → device still rebooting, keep polling
        }
    }, intervalMs);
}

/** Cancel any in-progress reconnect polling. */
export function stopReconnectPolling() {
    if (_reconnectTimer) {
        clearInterval(_reconnectTimer);
        _reconnectTimer = null;
    }
}
