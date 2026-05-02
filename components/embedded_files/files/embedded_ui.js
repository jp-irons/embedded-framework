//
// embedded/ui.js
//
// Shared UI logic for both Provisioning Mode and Runtime Mode.
// Handles: scan UI, credential list UI, provisioning form, system actions,
// status polling, and DOM wiring.
//
// Requires:
//   /embedded/api.js
//   /embedded/modal.js
//   /embedded/common_ui.js
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
    rebootDevice
} from "/embedded/api.js";

import {
    showConfirm,
    showMessage,
    hideMessageModal,
    wireConfirmButtons
} from "/embedded/modal.js";

import {
    el,
    button,
    clear
} from "/embedded/common_ui.js";

// ------------------------------------------------------------
// Internal state
// ------------------------------------------------------------

let scanResults = [];
let statusTimer = null;


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

    const info = await apiLoadDeviceInfo();

    const flashMB = (info.flashSize / (1024 * 1024)).toFixed(0);
    const psramRow = info.psramSize
        ? `<tr><td class="pr-4 font-semibold text-right">PSRAM Size:</td><td>${(info.psramSize / (1024 * 1024)).toFixed(0)} MB</td></tr>`
        : "";
    const freeHeapKB = (info.freeHeap / 1024).toFixed(0);
    const minFreeHeapKB = (info.minFreeHeap / 1024).toFixed(0);

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
                <td>${info.temperature.toFixed(1)}°C</td>
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
        console.error("Scan failed:", err);
        showMessage("error", "Scan Failed", "Unable to scan for networks.");
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
        console.error("Failed to load credentials:", err);
        showMessage("error", "Load Failed", "Unable to load saved credentials.");
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
        console.error(err);
        showMessage("error", "Delete Failed", `Unable to delete "${ssid}".`);
    }
}

async function handleMakeFirst(ssid) {
    try {
        await makeFirst(ssid);
        await refreshCredentials();
    } catch (err) {
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
        console.error(err);
        showMessage("error", "Clear Failed", "Unable to clear credentials.");
    }
}

function requestClearNvs() {
    showConfirm(
        "danger",
        "Clear NVS",
        "This will erase all stored WiFi data. Continue?",
        handleClearNvs
    );
}

async function handleClearNvs() {
    try {
        await apiClearNvs();
        showMessage("success", "NVS Cleared", "Non-volatile storage has been erased.");
    } catch (err) {
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
        showMessage("success", "Rebooting", "Device is rebooting…");
        setTimeout(() => location.reload(), 3000);
    } catch (err) {
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
