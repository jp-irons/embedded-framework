//
// framework_ui_device.js
//
// Device view: hardware info display, NVS clear, and reboot.
// Reboot is also used by the WiFi view and is exported for that purpose.
//
// Exports:
//   initDeviceView()
//   requestReboot()   — shared with WiFi view
//

import {
    loadDeviceInfo    as apiLoadDeviceInfo,
    rebootDevice,
    clearNvs          as apiClearNvs,
    loadHostnameConfig as apiLoadHostnameConfig,
    saveHostnameConfig as apiSaveHostnameConfig,
    isAuthenticated,
    forceReauth,
    startReconnectPolling
} from "./api.js";

import {
    showConfirm,
    showMessage
} from "./modal.js";


// ============================================================
// View initialiser
// ============================================================

/**
 * Initialise the combined Device view (info + controls).
 */
export function initDeviceView() {
    refreshDeviceInfo();
    loadHostnameConfig();

    const btnRefreshInfo = document.getElementById("btn-refresh-device-info");
    if (btnRefreshInfo) btnRefreshInfo.onclick = refreshDeviceInfo;

    const btnClearNvs = document.getElementById("btn-clear-nvs");
    if (btnClearNvs) btnClearNvs.onclick = requestClearNvs;

    const btnReboot = document.getElementById("btn-reboot");
    if (btnReboot) btnReboot.onclick = requestReboot;

    const btnSaveIdentity = document.getElementById("btn-save-identity");
    if (btnSaveIdentity) btnSaveIdentity.onclick = requestSaveIdentity;
}


// ============================================================
// Device info
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
    const flashMB       = fmt(info.flashSize,    1024 * 1024, 0) + " MB";
    const psramMB       = info.psramSize ? fmt(info.psramSize, 1024 * 1024, 0) + " MB" : "—";
    const freeHeapKB    = fmt(info.freeHeap,     1024,        0) + " KB";
    const minFreeHeapKB = fmt(info.minFreeHeap,  1024,        0) + " KB";
    const tempStr       = (info.temperature != null && isFinite(info.temperature))
        ? info.temperature.toFixed(1) + "°C"
        : "—";

    // CSS grid: max-content labels (sized to widest across all rows) + 1fr values.
    // This gives perfectly aligned columns without any per-row width negotiation.
    container.innerHTML = `
        <div style="display:grid; grid-template-columns:max-content 1fr max-content 1fr;
                    gap:0.3rem 0.75rem; align-items:baseline;">
            <span style="font-weight:600; white-space:nowrap;">Chip Model:</span>
            <span>${info.chipModel} rev ${info.revision}</span>
            <span style="font-weight:600; white-space:nowrap;">MAC Address:</span>
            <span>${info.mac}</span>

            <span style="font-weight:600; white-space:nowrap;">Flash Size:</span>
            <span>${flashMB}</span>
            <span style="font-weight:600; white-space:nowrap;">PSRAM Size:</span>
            <span>${psramMB}</span>

            <span style="font-weight:600; white-space:nowrap;">Free Heap:</span>
            <span>${freeHeapKB}</span>
            <span style="font-weight:600; white-space:nowrap;">Min Free Heap:</span>
            <span>${minFreeHeapKB}</span>

            <span style="font-weight:600; white-space:nowrap;">CPU Frequency:</span>
            <span>${info.cpuFreqMhz} MHz</span>
            <span style="font-weight:600; white-space:nowrap;">ESP-IDF Version:</span>
            <span>${info.idfVersion}</span>

            <span style="font-weight:600; white-space:nowrap;">Uptime:</span>
            <span>${info.uptime}</span>
            <span style="font-weight:600; white-space:nowrap;">Temperature:</span>
            <span>${tempStr}</span>

            <span style="font-weight:600; white-space:nowrap;">Last Reset:</span>
            <span>${info.lastReset}</span>
            <span style="font-weight:600; white-space:nowrap;">OTA Partition:</span>
            <span>${info.otaPartition}</span>
        </div>
    `;
}


// ============================================================
// Device identity (hostname / AP SSID prefix)
// ============================================================

async function loadHostnameConfig() {
    const inputHostname = document.getElementById("input-hostname-prefix");
    const inputApSsid   = document.getElementById("input-ap-ssid-prefix");
    const spanHostname  = document.getElementById("hostname-effective");
    const spanApSsid    = document.getElementById("ap-ssid-effective");

    try {
        const data = await apiLoadHostnameConfig();
        if (inputHostname) inputHostname.value       = data.hostnamePrefix  || "";
        if (inputApSsid)   inputApSsid.value         = data.apSsidPrefix    || "";
        if (spanHostname)  spanHostname.textContent   = data.effectiveHostname
                                                          ? "→ " + data.effectiveHostname + ".local"
                                                          : "";
        if (spanApSsid)    spanApSsid.textContent     = data.effectiveApSsid
                                                          ? "→ " + data.effectiveApSsid
                                                          : "";
    } catch (err) {
        if (err.message === "network") return;
        console.warn("Hostname config load failed:", err);
    }
}

function requestSaveIdentity() {
    const inputHostname  = document.getElementById("input-hostname-prefix");
    const inputApSsid    = document.getElementById("input-ap-ssid-prefix");
    const hostnamePrefix = inputHostname?.value.trim() ?? "";
    const apSsidPrefix   = inputApSsid?.value.trim()   ?? "";
    // Empty string is valid — it signals "clear this override and revert to default".

    showConfirm(
        "danger",
        "Save & Reboot",
        "Save device identity and reboot? The device will reconnect automatically " +
        "and you will be prompted to log in again.",
        () => handleSaveIdentity(hostnamePrefix, apSsidPrefix)
    );
}

async function handleSaveIdentity(hostnamePrefix, apSsidPrefix) {
    const btn = document.getElementById("btn-save-identity");
    if (btn) { btn.disabled = true; btn.textContent = "Saving…"; }

    try {
        await apiSaveHostnameConfig(hostnamePrefix, apSsidPrefix);
        await rebootDevice();
        showMessage("success", "Saved — Rebooting",
                    "Device identity saved. Device is rebooting. " +
                    "Reconnecting automatically — you will be prompted to log in again shortly.");
        startReconnectPolling();
    } catch (err) {
        if (btn) { btn.disabled = false; btn.textContent = "Save & Reboot"; }
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Save Failed", err.message || "Unable to save device identity.");
    }
}


// ============================================================
// NVS clear
// ============================================================

function requestClearNvs() {
    showConfirm(
        "danger",
        "Clear NVS",
        "This will erase ALL stored data: saved Wi-Fi networks, the API password, " +
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
        showMessage("error", "Clear Failed", err.message || "Unable to clear NVS.");
    }
}


// ============================================================
// Reboot  (exported — also used by the WiFi view)
// ============================================================

export function requestReboot() {
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
        showMessage("error", "Reboot Failed", err.message || "Unable to reboot device.");
    }
}
