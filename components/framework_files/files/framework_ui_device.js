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
    loadDeviceInfo as apiLoadDeviceInfo,
    rebootDevice,
    clearNvs      as apiClearNvs,
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

    const btnRefreshInfo = document.getElementById("btn-refresh-device-info");
    if (btnRefreshInfo) btnRefreshInfo.onclick = refreshDeviceInfo;

    const btnClearNvs = document.getElementById("btn-clear-nvs");
    if (btnClearNvs) btnClearNvs.onclick = requestClearNvs;

    const btnReboot = document.getElementById("btn-reboot");
    if (btnReboot) btnReboot.onclick = requestReboot;
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
