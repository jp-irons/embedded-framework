//
// framework_ui_wifi.js
//
// WiFi view: network scan, saved networks list, provisioning form,
// and connection status polling.
//
// Exports:
//   initWifiView()
//   teardownWifiView()
//

import {
    scanWifi,
    wifiStatus,
    listNetworks,
    submitNetwork,
    makeFirst,
    deleteNetwork   as apiDeleteNetwork,
    clearNetworks   as apiClearNetworks,
    isAuthenticated,
    forceReauth
} from "./api.js";

import {
    showConfirm,
    showMessage
} from "./modal.js";

import { requestReboot } from "./ui_device.js";


// ── Module state ─────────────────────────────────────────────────────────────

let scanResults = [];
let statusTimer = null;


// ============================================================
// View initialisers
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
    if (btnClearCreds) btnClearCreds.onclick = requestClearNetworks;

    const btnSave = document.getElementById("btn-save");
    if (btnSave) btnSave.onclick = submitProvisioning;

    // Stop any previous polling cycle (e.g. user navigated away and back)
    stopStatusPolling();

    loadScanResults();
    refreshNetworks();
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
        await submitNetwork(payload);
        document.getElementById("password").value = "";
        document.querySelectorAll(".ssid-select").forEach(cb => cb.checked = false);
        document.querySelectorAll(".bssid-lock").forEach(cb => cb.checked = false);
        showMessage("success", "Network Saved", `${payload.ssid} added.`);
        await refreshNetworks();
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error("Submit failed:", err);
        showMessage("error", "Save Failed", err.message || "Unable to save network.");
    }
}


// ============================================================
// Credential list UI
// ============================================================

async function refreshNetworks() {
    try {
        const creds = await listNetworks();
        renderCredList(creds);
    } catch (err) {
        // Network error → device is rebooting; keep quiet and let the polling
        // or reconnect mechanism handle recovery.
        if (err.message === "network") return;
        // Any other failure (401, truncated response, unexpected status) means
        // the session is likely dead.  Trigger re-login immediately rather than
        // showing a confusing "Load Failed" popup that the user has to dismiss.
        console.warn("Networks load failed — triggering re-auth:", err);
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
            btnFirst.className = "px-3 py-1 bg-gray-300 rounded hover:bg-gray-400";
            btnFirst.onclick = () => handleMakeFirst(c.ssid);
            btnGroup.appendChild(btnFirst);
        }

        const btnDelete = document.createElement("button");
        btnDelete.textContent = "Delete";
        btnDelete.className = "px-3 py-1 bg-red-600 text-white rounded hover:bg-red-700";
        btnDelete.onclick = () => requestDeleteNetwork(c.ssid);
        btnGroup.appendChild(btnDelete);

        li.appendChild(name);
        li.appendChild(btnGroup);
        ul.appendChild(li);
    });
}

function requestDeleteNetwork(ssid) {
    showConfirm(
        "danger",
        "Delete Network",
        `Do you want to delete "${ssid}"?`,
        () => handleDeleteNetwork(ssid)
    );
}

async function handleDeleteNetwork(ssid) {
    try {
        await apiDeleteNetwork(ssid);
        await refreshNetworks();
        showMessage("success", "Deleted", `Network "${ssid}" has been removed.`);
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Delete Failed", err.message || `Unable to delete "${ssid}".`);
    }
}

async function handleMakeFirst(ssid) {
    try {
        await makeFirst(ssid);
        await refreshNetworks();
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Reorder Failed", err.message || "Unable to reorder networks.");
    }
}


// ============================================================
// Clear networks
// ============================================================

function requestClearNetworks() {
    showConfirm(
        "warning",
        "Clear All Networks",
        "Do you want to remove all saved Wi-Fi networks?",
        handleClearNetworks
    );
}

async function handleClearNetworks() {
    try {
        await apiClearNetworks();
        await refreshNetworks();
        showMessage("success", "Cleared", "All networks have been removed.");
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error(err);
        showMessage("error", "Clear Failed", err.message || "Unable to clear networks.");
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
