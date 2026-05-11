//
// framework_ui.js
//
// Shared UI logic for both Provisioning Mode and Runtime Mode.
// Handles: scan UI, credential list UI, provisioning form, system actions,
// status polling, and DOM wiring.
//
// Requires:
//   ./api.js
//   ./modal.js
//   ./common_ui.js
//
// SPA entry points (called by app.js after each route mounts its HTML):
//   initWifiView()         — WiFi provisioning page
//   initDeviceInfoView()   — Device info page
//   initDeviceControlView()— Device control page
//

import {
    scanWifi,
    wifiStatus,
    listCredentials,
    submitCredential,
    makeFirst,
    deleteCredential as apiDeleteCredential,
    clearCredentials as apiClearCredentials,
    clearNvs as apiClearNvs,
    loadDeviceInfo as apiLoadDeviceInfo,
    loadFirmwareStatus as apiLoadFirmwareStatus,
    uploadFirmware as apiUploadFirmware,
    rollbackFirmware as apiRollbackFirmware,
    factoryResetFirmware as apiFactoryResetFirmware,
    rebootDevice,
    getAuthStatus,
    changePassword as apiChangePassword,
    login,
    isAuthenticated,
    startReconnectPolling,
    forceReauth
} from "./api.js";

import {
    showConfirm,
    showMessage,
    hideMessageModal,
    wireConfirmButtons
} from "./modal.js";

import {
    el,
    button,
    clear
} from "./common_ui.js";

// ------------------------------------------------------------
// Internal state
// ------------------------------------------------------------

let scanResults     = [];
let statusTimer     = null;
let fwUploadBusy    = false;   // prevents double-submit during upload


// ============================================================
// SPA view initialisers  (called by router after HTML is mounted)
// ============================================================

/**
 * Initialise the WiFi provisioning view.
 * Expects the DOM nodes from the #wifi route template to be present.
 */
export function initWifiView() {
    const btnRefresh = document.getElementById("btn-refresh");
    if (btnRefresh) btnRefresh.onclick = loadScanResults;

    const btnReboot = document.getElementById("btn-reboot");
    if (btnReboot) btnReboot.onclick = requestReboot;

    const btnClearCreds = document.getElementById("btn-clear-creds");
    if (btnClearCreds) btnClearCreds.onclick = requestClearCredentials;

    const btnSave = document.getElementById("btn-save");
    if (btnSave) btnSave.onclick = submitProvisioning;

    // Stop any previous polling cycle (e.g. user navigated away and back)
    stopStatusPolling();

    loadScanResults();
    refreshCredentials();
    startStatusPolling();
}

/**
 * Teardown the WiFi view — stops polling so it doesn't run in the background.
 * Called by the router before the next route mounts.
 */
export function teardownWifiView() {
    stopStatusPolling();
}

// ============================================================
// Firmware view
// ============================================================

let _lastPartitions = [];   // cache for rollback availability check

/**
 * Initialise the Firmware view — loads partition status and wires buttons.
 */
export function initFirmwareView() {
    fwUploadBusy = false;

    // Upload button → open hidden file picker
    const btnUpload = document.getElementById("btn-fw-upload");
    const fileInput = document.getElementById("fw-file-input");
    if (btnUpload && fileInput) {
        btnUpload.onclick = () => { if (!fwUploadBusy) fileInput.click(); };
        fileInput.onchange = (e) => {
            const file = e.target.files && e.target.files[0];
            if (file) requestFirmwareUpload(file);
        };
    }

    // Rollback button (enabled/disabled after partition data loads)
    const btnRollback = document.getElementById("btn-fw-rollback");
    if (btnRollback) btnRollback.onclick = requestRollback;

    // Factory Reset button
    const btnFactory = document.getElementById("btn-fw-factory");
    if (btnFactory) btnFactory.onclick = requestFactoryReset;

    loadFirmwareStatus();
}

/** Called by the router when navigating away — nothing to clean up for now. */
export function teardownFirmwareView() {
    fwUploadBusy = false;
}

async function loadFirmwareStatus() {
    const container = document.getElementById("firmware-partitions");
    if (!container) return;

    try {
        const data = await apiLoadFirmwareStatus();
        _lastPartitions = data.partitions || [];
        renderPartitionCards(_lastPartitions);
        updateRollbackButton(_lastPartitions);
    } catch (err) {
        if (err.message === "network") return;
        console.warn("Firmware status load failed — triggering re-auth:", err);
        forceReauth();
    }
}

/** Enable the Rollback button only when a VALID non-running OTA partition exists. */
function updateRollbackButton(partitions) {
    const btn  = document.getElementById("btn-fw-rollback");
    const note = document.getElementById("fw-rollback-note");
    if (!btn) return;

    const canRollback = partitions.some(
        p => !p.isRunning && p.state === "valid" &&
             (p.label === "ota_0" || p.label === "ota_1")
    );

    if (canRollback) {
        btn.disabled = false;
        btn.classList.remove("opacity-50", "cursor-default");
        btn.classList.add("hover:bg-blue-700");
        if (note) note.classList.add("hidden");
    } else {
        btn.disabled = true;
        btn.classList.add("opacity-50", "cursor-default");
        btn.classList.remove("hover:bg-blue-700");
        if (note) note.classList.remove("hidden");
    }
}

function partitionBadges(p) {
    const badges = [];
    const stateMap = {
        valid:   "fw-badge-valid",
        pending: "fw-badge-pending",
        invalid: "fw-badge-invalid",
        aborted: "fw-badge-aborted",
        new:     "fw-badge-new",
        empty:   "fw-badge-empty",
        factory: "fw-badge-empty",
    };

    if (p.isRunning) {
        badges.push(`<span class="fw-badge fw-badge-running">Running</span>`);
        // Also show the actual OTA state for the running partition so the user
        // can confirm it has been validated (valid) vs. still pending.
        const otaCls   = stateMap[p.otaState] || "";
        const otaLabel = p.otaState ? p.otaState.charAt(0).toUpperCase() + p.otaState.slice(1) : "";
        if (otaLabel && p.otaState !== "empty") {
            badges.push(`<span class="fw-badge ${otaCls}">${otaLabel}</span>`);
        }
    }

    if (p.isNextBoot && !p.isRunning) {
        badges.push(`<span class="fw-badge fw-badge-next">Next Boot</span>`);
    }

    if (!p.isRunning) {
        const cls   = stateMap[p.state] || "fw-badge-empty";
        const label = p.state.charAt(0).toUpperCase() + p.state.slice(1);
        badges.push(`<span class="fw-badge ${cls}">${label}</span>`);
    }

    return badges.join(" ");
}

/** Always render as MB with 2 decimal places for consistent image-size display. */
function formatMB(n) {
    return (n / (1024 * 1024)).toFixed(2) + " MB";
}

/**
 * Tidy the combined build-date string that comes from the firmware descriptor.
 * ESP-IDF produces e.g. "May  3 2026 14:48:23" — collapse double spaces and
 * drop the seconds so it fits comfortably on one line.
 */
function formatBuildDate(dateStr) {
    return (dateStr || "")
        .replace(/\s+/g, " ")                       // "May  3" → "May 3"
        .replace(/(\d{2}:\d{2}):\d{2}$/, "$1");     // "14:48:23" → "14:48"
}

/** Truncate "v6.0-dev-3948-gabcdef" → "v6.0". */
function truncateIdfVer(ver) {
    const m = (ver || "").match(/^(v[\d.]+)/);
    return m ? m[1] : (ver || "");
}

function renderPartitionCards(partitions) {
    const container = document.getElementById("firmware-partitions");
    if (!container) return;

    if (partitions.length === 0) {
        container.innerHTML =
            `<div class="text-gray-500 text-sm">No partition data available.</div>`;
        return;
    }

    container.innerHTML = partitions.map(p => {
        const hasFirmware = !!p.version;
        const cardBg      = hasFirmware ? "bg-white" : "bg-gray-50";

        // ── Version row (firmware only) ──────────────────────────────────
        const versionRow = hasFirmware ? `
            <tr>
                <td class="pr-6 text-gray-500 align-top">Version</td>
                <td>${p.version}<span class="text-gray-400 ml-2">· ${formatBuildDate(p.buildDate)}</span></td>
            </tr>` : "";

        // ── Image row (always shown) ──────────────────────────────────────
        const slotLabel  = p.partitionSize ? formatMB(p.partitionSize) : "—";
        const imageCell  = p.firmwareSize
            ? `<span>${formatMB(p.firmwareSize)} / ${slotLabel}</span>`
            : `<span class="text-gray-400">- / ${slotLabel}</span>`;

        const imageRow = `
            <tr>
                <td class="pr-6 text-gray-500 align-middle">Image</td>
                <td>${imageCell}</td>
            </tr>`;

        // ── Project + IDF rows (firmware only) ───────────────────────────
        const projectRow = hasFirmware
            ? `<tr><td class="pr-6 text-gray-500">Project</td><td>${p.project}</td></tr>` : "";
        const idfRow = hasFirmware
            ? `<tr><td class="pr-6 text-gray-500">IDF</td><td>${truncateIdfVer(p.idfVersion)}</td></tr>` : "";

        return `
            <div class="border rounded p-4 ${cardBg} shadow-sm">
                <div class="flex justify-between items-center">
                    <span class="font-semibold">${p.label}</span>
                    <div class="flex gap-2">${partitionBadges(p)}</div>
                </div>
                <table class="text-sm mt-3">
                    ${versionRow}
                    ${imageRow}
                    ${projectRow}
                    ${idfRow}
                </table>
            </div>`;
    }).join("");
}

// ── Upload ────────────────────────────────────────────────────────────────

function requestFirmwareUpload(file) {
    showConfirm(
        "warning",
        "Upload Firmware",
        `Upload "${file.name}" (${(file.size / 1024).toFixed(0)} KB) and reboot?`,
        () => handleFirmwareUpload(file)
    );
    // Clear the file input so the same file can be re-selected if needed
    const fi = document.getElementById("fw-file-input");
    if (fi) fi.value = "";
}

async function handleFirmwareUpload(file) {
    if (fwUploadBusy) return;
    fwUploadBusy = true;

    // Disable all firmware buttons during upload
    ["btn-fw-upload", "btn-fw-rollback", "btn-fw-factory"].forEach(id => {
        const b = document.getElementById(id);
        if (b) { b.disabled = true; b.classList.add("opacity-50", "cursor-default"); }
    });

    // Show progress bar
    const progressWrapper = document.getElementById("fw-upload-progress");
    const progressBar     = document.getElementById("fw-progress-bar");
    const progressPct     = document.getElementById("fw-progress-pct");
    if (progressWrapper) progressWrapper.classList.remove("hidden");

    try {
        await apiUploadFirmware(file, (pct) => {
            if (progressBar) progressBar.style.width = pct + "%";
            if (progressPct) progressPct.textContent = pct + "%";
        });
        // Device reboots after a successful upload.  Show the success message
        // and trigger the login overlay only once the user clicks OK —
        // forceReauth() in the callback handles that.  We do NOT call
        // startReconnectPolling() here: it would race with the onOk callback
        // and show the overlay a second time if the device comes back and
        // delivers a 401 before OK is clicked.  The login form's own retry
        // logic (5 attempts × 2 s) handles reconnecting, and its failure
        // path calls startReconnectPolling() if all retries are exhausted.
        showMessage("success", "Upload Complete",
                    "Firmware uploaded successfully. The device is rebooting — " +
                    "click OK, then log in again to continue.",
                    () => forceReauth());
    } catch (err) {
        // Discard stale callbacks that arrive after a device reboot + re-auth:
        // • "network"       — api.js detected the token changed (onerror/ontimeout)
        // • "unauthorized"  — stale XHR got a 401 but we correctly skipped handle401()
        //                     because the token had already been replaced
        // • !isAuthenticated() — any other path that cleared the token first
        if (!isAuthenticated() || err.message === "network" || err.message === "unauthorized") return;
        console.error("Firmware upload failed:", err);
        showMessage("error", "Upload Failed", err.message || "Could not upload firmware.");
        // Re-enable buttons and hide progress on failure
        fwUploadBusy = false;
        if (progressWrapper) progressWrapper.classList.add("hidden");
        ["btn-fw-upload", "btn-fw-factory"].forEach(id => {
            const b = document.getElementById(id);
            if (b) { b.disabled = false; b.classList.remove("opacity-50", "cursor-default"); }
        });
        // Re-evaluate rollback availability
        updateRollbackButton(_lastPartitions);
    }
}

// ── Rollback ──────────────────────────────────────────────────────────────

function requestRollback() {
    showConfirm(
        "warning",
        "Rollback Firmware",
        "Revert to the previous firmware version and reboot?",
        handleRollback
    );
}

async function handleRollback() {
    try {
        await apiRollbackFirmware();
        showMessage("success", "Rolling Back",
                    "Rolling back to previous firmware. The device is rebooting — " +
                    "click OK, then log in again to continue.",
                    () => forceReauth());
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error("Rollback failed:", err);
        showMessage("error", "Rollback Failed",
                    err.message || "No valid firmware to roll back to.");
    }
}

// ── Factory Reset ─────────────────────────────────────────────────────────

function requestFactoryReset() {
    showConfirm(
        "danger",
        "Factory Reset",
        "This will erase all OTA updates and reboot to the factory firmware. Continue?",
        handleFactoryReset
    );
}

async function handleFactoryReset() {
    try {
        await apiFactoryResetFirmware();
        showMessage("success", "Factory Reset",
                    "Factory reset complete. The device is rebooting — " +
                    "click OK, then log in again to continue.",
                    () => forceReauth());
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error("Factory reset failed:", err);
        showMessage("error", "Factory Reset Failed",
                    err.message || "Could not perform factory reset.");
    }
}

/**
 * Initialise the combined Device view (info + controls).
 */
export function initDeviceView() {
    refreshDeviceInfo();

    const btnRefreshInfo = document.getElementById("btn-refresh-device-info");
    if (btnRefreshInfo) btnRefreshInfo.onclick = refreshDeviceInfo;

    const btnClearNvs = document.getElementById("btn-clear-nvs");
    if (btnClearNvs) btnClearNvs.onclick = requestClearNvs;

    const btnReboot = document.getElementById("btn-reboot");
    if (btnReboot) btnReboot.onclick = requestReboot;
}


// ============================================================
// Device Info
// ============================================================

async function refreshDeviceInfo() {
    const container = document.getElementById("device-info-container");
    if (!container) return;

    container.innerHTML = `<div class="text-gray-500">Loading…</div>`;

    let info;
    try {
        info = await apiLoadDeviceInfo();
    } catch (err) {
        if (err.message === "network") return;
        console.warn("Device info load failed — triggering re-auth:", err);
        forceReauth();
        return;
    }

    const fmt  = (v, divisor, dp) =>
        (v != null && isFinite(v)) ? (v / divisor).toFixed(dp) : "—";
    const flashMB       = fmt(info.flashSize,    1024 * 1024, 0);
    const freeHeapKB    = fmt(info.freeHeap,     1024,        0);
    const minFreeHeapKB = fmt(info.minFreeHeap,  1024,        0);
    const tempStr       = (info.temperature != null && isFinite(info.temperature))
        ? info.temperature.toFixed(1) + "°C"
        : "—";
    const psramRow = info.psramSize
        ? `<tr><td class="pr-4 font-semibold text-right">PSRAM Size:</td><td>${fmt(info.psramSize, 1024 * 1024, 0)} MB</td></tr>`
        : "";

    container.innerHTML = `
        <table class="text-sm">
            <tr>
                <td class="pr-4 font-semibold text-right">Chip Model:</td>
                <td>${info.chipModel} rev ${info.revision}</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">MAC Address:</td>
                <td>${info.mac}</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">Flash Size:</td>
                <td>${flashMB} MB</td>
            </tr>
            ${psramRow}
            <tr>
                <td class="pr-4 font-semibold text-right">Free Heap:</td>
                <td>${freeHeapKB} KB</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">Min Free Heap:</td>
                <td>${minFreeHeapKB} KB</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">CPU Frequency:</td>
                <td>${info.cpuFreqMhz} MHz</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">ESP-IDF Version:</td>
                <td>${info.idfVersion}</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">Last Reset:</td>
                <td>${info.lastReset}</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">Temperature:</td>
                <td>${tempStr}</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">Uptime:</td>
                <td>${info.uptime}</td>
            </tr>
            <tr>
                <td class="pr-4 font-semibold text-right">OTA Partition:</td>
                <td>${info.otaPartition}</td>
            </tr>
        </table>
    `;
}


// ============================================================
// Scan UI
// ============================================================

async function loadScanResults() {
    try {
        scanResults = await scanWifi();
        renderScanList(scanResults);
    } catch (err) {
        if (err.message === "network") return;
        console.warn("Scan failed — triggering re-auth:", err);
        forceReauth();
    }
}

function renderScanList(networks) {
    const list = document.getElementById("network-list");
    if (!list) return;

    list.innerHTML = "";

    networks.forEach((net, index) => {
        const item = document.createElement("div");
        item.className = "network-item p-3 border rounded bg-gray-50";

        item.innerHTML = `
            <label class="flex items-center space-x-2">
                <input type="checkbox" class="ssid-select" data-index="${index}">
                <span class="font-medium">${net.ssid}</span>
                <span class="text-sm text-gray-500">(${net.rssi} dBm)</span>
            </label>
            <div class="ml-6 mt-1 text-sm text-gray-600">
                BSSID: ${net.bssid}
                <label class="ml-3">
                    <input type="checkbox" class="bssid-lock" data-index="${index}">
                    Lock to BSSID
                </label>
            </div>
        `;

        list.appendChild(item);
    });
}


// ============================================================
// Provisioning form
// ============================================================

function buildProvisioningPayload() {
    const checkboxes = document.querySelectorAll(".ssid-select");
    let selectedIndex = null;

    checkboxes.forEach(cb => {
        if (cb.checked) selectedIndex = parseInt(cb.dataset.index, 10);
    });

    if (selectedIndex === null) {
        showMessage("warning", "Selection Required", "Please select a network.");
        return null;
    }

    const net = scanResults[selectedIndex];
    const lockBox = document.querySelector(`.bssid-lock[data-index="${selectedIndex}"]`);
    const bssidLocked = lockBox && lockBox.checked;

    return {
        ssid: net.ssid,
        password: document.getElementById("password").value,
        priority: 0,
        bssid: net.bssid || null,
        bssidLocked
    };
}

async function submitProvisioning() {
    const payload = buildProvisioningPayload();
    if (!payload) return;

    try {
        await submitCredential(payload);
        document.getElementById("password").value = "";
        document.querySelectorAll(".ssid-select").forEach(cb => cb.checked = false);
        document.querySelectorAll(".bssid-lock").forEach(cb => cb.checked = false);
        showMessage("success", "Credential Saved", `${payload.ssid} added.`);
        await refreshCredentials();
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error("Submit failed:", err);
        showMessage("error", "Save Failed", "Unable to save credential.");
    }
}


// ============================================================
// Credential list UI
// ============================================================

async function refreshCredentials() {
    try {
        const creds = await listCredentials();
        renderCredList(creds);
    } catch (err) {
        // Network error → device is rebooting; keep quiet and let the polling
        // or reconnect mechanism handle recovery.
        if (err.message === "network") return;
        // Any other failure (401, truncated response, unexpected status) means
        // the session is likely dead.  Trigger re-login immediately rather than
        // showing a confusing "Load Failed" popup that the user has to dismiss.
        console.warn("Credentials load failed — triggering re-auth:", err);
        forceReauth();
    }
}

function renderCredList(creds) {
    const ul = document.getElementById("cred-list");
    if (!ul) return;

    ul.innerHTML = "";

    creds.forEach((c, i) => {
        const li = document.createElement("li");
        li.className = "flex justify-between items-center py-2";

        const name = document.createElement("span");
        name.textContent = c.ssid;

        const btnGroup = document.createElement("div");
        btnGroup.className = "flex gap-3";

        if (c.priority !== 0) {
            const btnFirst = document.createElement("button");
            btnFirst.textContent = "^1st";
            btnFirst.className = "px-4 py-2 bg-gray-300 rounded hover:bg-gray-400";
            btnFirst.onclick = () => handleMakeFirst(c.ssid);
            btnGroup.appendChild(btnFirst);
        }

        const btnDelete = document.createElement("button");
        btnDelete.textContent = "Delete";
        btnDelete.className = "px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700";
        btnDelete.onclick = () => requestDeleteCredential(c.ssid);
        btnGroup.appendChild(btnDelete);

        li.appendChild(name);
        li.appendChild(btnGroup);
        ul.appendChild(li);
    });
}

function requestDeleteCredential(ssid) {
    showConfirm(
        "danger",
        "Delete Credential",
        `Do you want to delete "${ssid}"?`,
        () => handleDeleteCredential(ssid)
    );
}

async function handleDeleteCredential(ssid) {
    try {
        await apiDeleteCredential(ssid);
        await refreshCredentials();
        showMessage("success", "Deleted", `Credential "${ssid}" has been removed.`);
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Delete Failed", `Unable to delete "${ssid}".`);
    }
}

async function handleMakeFirst(ssid) {
    try {
        await makeFirst(ssid);
        await refreshCredentials();
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Reorder Failed", "Unable to reorder credentials.");
    }
}


// ============================================================
// System actions
// ============================================================

function requestClearCredentials() {
    showConfirm(
        "warning",
        "Clear All Credentials",
        "Do you want to remove all saved WiFi credentials?",
        handleClearCredentials
    );
}

async function handleClearCredentials() {
    try {
        await apiClearCredentials();
        await refreshCredentials();
        showMessage("success", "Cleared", "All credentials have been removed.");
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Clear Failed", "Unable to clear credentials.");
    }
}

function requestClearNvs() {
    showConfirm(
        "danger",
        "Clear NVS",
        "This will erase ALL stored data: WiFi credentials, the API password, " +
        "API keys, and the device TLS certificate. A new certificate will be " +
        "generated on the next reboot — your browser will show a new security " +
        "warning and you will need to accept it before reconnecting. Continue?",
        handleClearNvs
    );
}

async function handleClearNvs() {
    try {
        await apiClearNvs();
        showMessage("success", "NVS Cleared", "Non-volatile storage has been erased.");
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Clear Failed", "Unable to clear NVS.");
    }
}

function requestReboot() {
    showConfirm(
        "danger",
        "Reboot Device",
        "Do you wish to reboot?",
        handleReboot
    );
}

async function handleReboot() {
    try {
        await rebootDevice();
        showMessage("success", "Rebooting",
                    "Device is rebooting. Reconnecting automatically — " +
                    "you will be prompted to log in again shortly.");
        // Do NOT call location.reload() here.  The device's TLS session cache
        // is cleared on reboot; the browser tries to resume the old TLS session
        // and gets a fatal alert for ~60 s, which silently kills a page reload.
        // Instead, poll getAuthStatus() every 3 s.  When the device comes back
        // up it returns 401 → handle401() fires the login overlay automatically.
        startReconnectPolling();
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Reboot Failed", "Unable to reboot device.");
    }
}


// ============================================================
// Status polling
// ============================================================

async function pollStatus() {
    try {
        const status = await wifiStatus();
        const statusEl = document.getElementById("status");
        if (statusEl) {
            statusEl.textContent =
                `State: ${status.state}, SSID: ${status.ssid}, Error: ${status.lastErrorReason}`;
        }

        if (status.state === "STA Connected") {
            stopStatusPolling();
            if (statusEl) statusEl.textContent = "Connected";
        }
        if (status.state === "AP Mode") {
            stopStatusPolling();
            if (statusEl) statusEl.textContent = "AP Mode";
        }
    } catch (err) {
        if (!isAuthenticated()) {
            // 401 — session expired.  Stop polling so we don't hammer the device
            // with repeated unauthorised requests (each one would call
            // showLoginOverlay() and clear any password the user is typing).
            stopStatusPolling();
            return;
        }
        console.error("Status poll failed:", err);
    }
}

function startStatusPolling() {
    stopStatusPolling();
    statusTimer = setInterval(pollStatus, 1000);
}

function stopStatusPolling() {
    if (statusTimer) {
        clearInterval(statusTimer);
        statusTimer = null;
    }
}


// ============================================================
// Home view
// ============================================================

/**
 * Initialise the home view — checks auth status and shows a warning banner
 * if the password has never been changed from its default.
 */
export async function initHomeView() {
    try {
        const status = await getAuthStatus();
        if (!status.passwordChanged) {
            const banner = document.getElementById("auth-warning-banner");
            if (banner) banner.classList.remove("hidden");
        }
    } catch (err) {
        // Auth status unavailable — not fatal, silently ignore
        console.warn("Could not fetch auth status:", err);
    }
}


// ============================================================
// Security view
// ============================================================

/**
 * Initialise the Security view — loads auth status display and wires the
 * change-password form.
 *
 * The submit button is disabled until getAuthStatus() succeeds so the form
 * cannot be submitted before credentials have been confirmed.  This prevents
 * a race where the user fills in the form before the initial 401 response
 * returns and the login overlay appears.
 */
export async function initSecurityView() {
    // ── Status display ────────────────────────────────────────────────────
    const statusEl  = document.getElementById("auth-status-display");
    const submitBtn = document.querySelector("#change-password-form button[type='submit']");

    // Wire form early (so Enter key is intercepted) but keep submit disabled
    // until we've confirmed the user is authenticated.
    const form = document.getElementById("change-password-form");
    if (form) {
        form.onsubmit = async (e) => {
            e.preventDefault();
            await handleChangePassword();
        };
    }
    if (submitBtn) submitBtn.disabled = true;

    try {
        const status = await getAuthStatus();
        // Auth confirmed — enable the form
        if (submitBtn) submitBtn.disabled = false;
        if (statusEl) {
            if (status.passwordChanged) {
                statusEl.textContent = "Password has been changed from the default.";
                statusEl.classList.add("auth-status-ok");
            } else {
                statusEl.innerHTML =
                    "<strong>Default password is in use.</strong> " +
                    "Change it below before putting this device into service.";
                statusEl.classList.add("auth-status-warn");
            }
        }
    } catch (err) {
        // Auth failed — login overlay will appear; form stays disabled until
        // the user authenticates and the view re-mounts.
        if (statusEl) statusEl.textContent = "Authentication required.";
        console.warn("Auth status fetch failed:", err);
    }
}

async function handleChangePassword() {
    // Last-line guard: if credentials were never confirmed (e.g. the form was
    // somehow submitted before the login overlay flow completed), bail early
    // with a user-visible message rather than sending an unauthenticated request.
    if (!isAuthenticated()) {
        showMessage("error", "Not Logged In",
                    "Log in with the current device password before changing it.");
        return;
    }

    const newPwEl      = document.getElementById("new-password");
    const confirmPwEl  = document.getElementById("confirm-password");
    if (!newPwEl || !confirmPwEl) return;

    const newPw     = newPwEl.value;
    const confirmPw = confirmPwEl.value;

    if (newPw.length < 8) {
        showMessage("error", "Password Too Short", "Password must be at least 8 characters.");
        return;
    }
    if (newPw !== confirmPw) {
        showMessage("error", "Password Mismatch", "Passwords do not match.");
        return;
    }

    try {
        await apiChangePassword(newPw);
        // The server invalidated all sessions on password change.
        // Re-authenticate immediately with the new password so the user
        // doesn't see a login prompt on the next API call.
        await login(newPw);
        newPwEl.value     = "";
        confirmPwEl.value = "";
        showMessage("success", "Password Changed",
                    "Your API password has been updated successfully.");
        // Re-initialise the status display — will now show green "changed" state
        await initSecurityView();
    } catch (err) {
        console.error("Password change failed:", err);
        showMessage("error", "Change Failed", err.message || "Could not change password.");
    }
}
