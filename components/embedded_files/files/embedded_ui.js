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
//   /embedded/common_ui.js   (optional helpers)
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


// ------------------------------------------------------------
// Public entry point
// ------------------------------------------------------------

export function initUI({ mode }) {
    // mode = "provision" or "runtime"

    wireConfirmButtons();

    // Message modal OK button
    const msgOk = document.getElementById("message-ok-btn");
    if (msgOk) msgOk.onclick = hideMessageModal;

    // Scan refresh
    const btnRefresh = document.getElementById("btn-refresh");
    if (btnRefresh) btnRefresh.onclick = loadScanResults;

    // Reboot
    const btnReboot = document.getElementById("btn-reboot");
    if (btnReboot) btnReboot.onclick = requestReboot;

    // Clear NVS
    const btnClearNvs = document.getElementById("btn-clear-nvs");
    if (btnClearNvs) btnClearNvs.onclick = requestClearNvs;

    // Clear credentials
    const btnClearCreds = document.getElementById("btn-clear-creds");
    if (btnClearCreds) btnClearCreds.onclick = requestClearCredentials;

    // Provisioning save
    const btnSave = document.getElementById("btn-save");
    if (btnSave) btnSave.onclick = submitProvisioning;

    // Initial loads
    loadScanResults();
    refreshCredentials();
	
	// Device Info page
	const deviceInfoContainer = document.getElementById("device-info-container");
	if (deviceInfoContainer) {
	    refreshDeviceInfo();
	    const btnRefreshInfo = document.getElementById("btn-refresh-device-info");
	    if (btnRefreshInfo) btnRefreshInfo.onclick = refreshDeviceInfo;
	}
	
    if (mode === "runtime") {
        startStatusPolling();
    }
}

async function refreshDeviceInfo() {
    const container = document.getElementById("device-info-container");
    if (!container) return;

    container.innerHTML = `<div class="text-gray-500">Loading…</div>`;

    const res = await apiLoadDeviceInfo();
    //if (!res.ok) {
     //   container.innerHTML = `<div class="text-red-600">Failed to load device info</div>`;
     //   return;
    //}

    //const info = res.data;
	const info = res;

	container.innerHTML = `
	    <table class="text-sm">
	        <tr>
	            <td class="pr-4 font-semibold text-right">Chip Model:</td>
	            <td>${info.chipModel}</td>
	        </tr>
	        <tr>
	            <td class="pr-4 font-semibold text-right">Revision:</td>
	            <td>${info.revision}</td>
	        </tr>
	        <tr>
	            <td class="pr-4 font-semibold text-right">MAC Address:</td>
	            <td>${info.mac}</td>
	        </tr>
	        <tr>
	            <td class="pr-4 font-semibold text-right">Flash Size:</td>
	            <td>${info.flashSize} bytes</td>
	        </tr>
	        <tr>
	            <td class="pr-4 font-semibold text-right">Free Heap:</td>
	            <td>${info.freeHeap} bytes</td>
	        </tr>
	    </table>
	`;

}


// ------------------------------------------------------------
// Device Info Page
// ------------------------------------------------------------
function initDeviceInfoPage() {
    const container = document.getElementById("device-info-container");
    if (!container) return; // Not on this page

    async function refresh() {
        clear(container);
        container.append(el("div", { class: "text-gray-500" }, "Loading…"));

        const res = await loadDeviceInfo();
        if (!res.ok) {
            clear(container);
            container.append(
                el("div", { class: "text-red-600" }, "Failed to load device info")
            );
            return;
        }

        const info = res.data;

        clear(container);
        container.append(
            el("div", { class: "grid grid-cols-2 gap-2 text-sm" },
                el("div", { class: "font-medium" }, "Chip Model"),
                el("div", {}, info.chipModel),

                el("div", { class: "font-medium" }, "Revision"),
                el("div", {}, info.revision),

                el("div", { class: "font-medium" }, "MAC Address"),
                el("div", {}, info.mac),

                el("div", { class: "font-medium" }, "Flash Size"),
                el("div", {}, info.flashSize + " bytes"),

                el("div", { class: "font-medium" }, "Free Heap"),
                el("div", {}, info.freeHeap + " bytes")
            )
        );
    }

    // Wire refresh button
    const refreshBtn = document.getElementById("device-info-refresh");
    if (refreshBtn) refreshBtn.onclick = refresh;

    refresh();
}

// ------------------------------------------------------------
// Scan UI
// ------------------------------------------------------------

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


// ------------------------------------------------------------
// Provisioning form
// ------------------------------------------------------------

function buildProvisioningPayload() {
    const checkboxes = document.querySelectorAll(".ssid-select");
    let selectedIndex = null;

    checkboxes.forEach(cb => {
        if (cb.checked) {
            selectedIndex = parseInt(cb.dataset.index, 10);
        }
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
		// Clear all selected SSIDs
		document.querySelectorAll('.ssid-select').forEach(cb => cb.checked = false);
		// Clear all BSSID lock checkboxes too
		document.querySelectorAll('.bssid-lock').forEach(cb => cb.checked = false);
        showMessage("success", "Credential Saved", `${payload.ssid} added.`);
        await refreshCredentials();
    } catch (err) {
        console.error("Submit failed:", err);
        showMessage("error", "Save Failed", "Unable to save credential.");
    }
}


// ------------------------------------------------------------
// Credential list UI
// ------------------------------------------------------------

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
        const isLast = i === creds.length - 1;

        li.className =
            "flex justify-between items-center py-2 ";
			// +
            //(isLast ? "border-b border-gray-200" : "");

        const name = document.createElement("span");
        name.textContent = c.ssid;

        const btnGroup = document.createElement("div");
        btnGroup.className = "flex gap-3";

        if (c.priority !== 0) {
            const btnFirst = document.createElement("button");
            btnFirst.textContent = "^1st";
            btnFirst.className = "px-2 py-1 bg-gray-300 rounded hover:bg-gray-400";
            btnFirst.onclick = () => handleMakeFirst(c.ssid);
            btnGroup.appendChild(btnFirst);
        }

        const btnDelete = document.createElement("button");
        btnDelete.textContent = "Delete";
        btnDelete.className =
            "px-3 py-1 bg-red-600 text-white rounded hover:bg-red-700";
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


// ------------------------------------------------------------
// System actions
// ------------------------------------------------------------

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


// ------------------------------------------------------------
// Status polling (runtime mode)
// ------------------------------------------------------------

async function pollStatus() {
    try {
        const status = await wifiStatus();
        const el = document.getElementById("status");
        if (el) {
            el.textContent =
                `State: ${status.state}, SSID: ${status.ssid}, Error: ${status.lastErrorReason}`;
        }

		if (status.state === "STA Connected") {
		    stopStatusPolling();
		    if (el) el.textContent = "Connected";
		}
		if (status.state === "AP Mode") {
            stopStatusPolling();
            if (el) el.textContent = "AP Mode";
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
