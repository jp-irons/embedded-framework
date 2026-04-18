import {
    showConfirm,
    hideConfirmModal,
    wireConfirmButtons,
    showMessage,
    hideMessageModal,
} from "/common/modal.js";

import {
	rebootDevice
} from "/common/api.js";

// Global storage for scan results. 
window._scanResults = [];

let initialLoadDone = false;

document.addEventListener("DOMContentLoaded", () => {
    wireConfirmButtons();   // from modal.js
	
	// Wire message modal ok button
	document.getElementById("message-ok-btn").onclick = () => {
	    hideMessageModal();
	};

	// Wire refresh button
	document.getElementById("btn-refresh").onclick = () => {
	    loadScanResults();
	};
	// Wire reboot button
	document.getElementById("btn-reboot").onclick = () => {
	    showConfirm(
	        "danger",
	        "Reboot Device",
	        "Do you wish to reboot?",
	        async () => {
	            try {
	                await rebootDevice();
	                showMessage("success", "Rebooting", "Device is rebooting…");
	                setTimeout(() => location.reload(), 3000);
	            } catch (err) {
	                showMessage("error", "Reboot Failed", "Unable to reboot device");
	            }
	        }
	    );
	};	
	
	// Wire clear credentials
	document.getElementById("btn-clear-creds").onclick = requestClearCredentials;

	// Wire clear NVS
	document.getElementById("btn-clear-nvs").onclick = requestClearNvs;

	// Wire save provisioning
	document.getElementById("btn-save").onclick = submitProvisioning;

	fetchCredentials();     // existing
	loadScanResults();      // moved from HTML into module

});

// Fetch /scan and populate UI
async function loadScanResults() {
  try {
    const res = await fetch('/framework/api/wifi/scan');
    const networks = await res.json();

//	const networks = await api.scanWifi();
    window._scanResults = networks;

    const list = document.getElementById('network-list');
    list.innerHTML = '';

    networks.forEach((net, index) => {
      const item = document.createElement('div');
      item.className = 'network-item p-3 border rounded bg-gray-50';

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

  } catch (err) {
    console.error("Scan failed:", err);
  }
}

// Build JSON payload for /submit
function buildProvisioningPayload() {
  const checkboxes = document.querySelectorAll('.ssid-select');
  let selectedIndex = null;

  // Find the one selected SSID
  checkboxes.forEach(cb => {
    if (cb.checked) {
      selectedIndex = parseInt(cb.dataset.index, 10);
    }
  });

  if (selectedIndex === null) {
	showMessage('warning', 'Selection Required', 'Please select a network');
    return null;
  }

  const net = window._scanResults[selectedIndex];

  const lockBox = document.querySelector(`.bssid-lock[data-index="${selectedIndex}"]`);
  const bssidLocked = lockBox && lockBox.checked;

  return {
    ssid: net.ssid,
    password: document.getElementById('password').value,
    priority: 0,                     // or compute if needed
    bssid: net.bssid || null,
    bssidLocked: bssidLocked
  };
}

async function submitProvisioning() {
  const payload = buildProvisioningPayload();

  // If no payload, the modal has already been shown — stop here
  if (!payload) {
    return;
  }

  try {
    const res = await fetch('/framework/api/credentials/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    const result = await res.json();
    console.log("Provisioning submitted:", result);
	document.getElementById("password").value = "";
	showMessage("success", "Credential saved", `${payload.ssid} added.`);
	
	await fetchCredentials();
    // startStatusPolling();

  } catch (err) {
    console.error("Submit failed:", err);
	showMessage("error", "Save failed", "Unable to save credential.");
  }
}

// Poll /status every second
async function pollStatus() {
  try {
    const res = await fetch('/framework/api/wifi/status');
    const status = await res.json();

    const el = document.getElementById('status');
    el.textContent =
      `State: ${status.state}, SSID: ${status.ssid}, Error: ${status.lastErrorReason}`;

    if (status.state === 'RUNTIME_STA' && status.connected) {
      clearInterval(window._statusTimer);
      el.textContent = 'Connected!';
    }

  } catch (err) {
    console.error("Status poll failed:", err);
  }
}

function startStatusPolling() {
  window._statusTimer = setInterval(pollStatus, 1000);
}

async function fetchCredentials() {
  //const res = await fetch('/framework/api/credentials/list');
  const res = await fetch(`/framework/api/credentials/list?ts=${Date.now()}`)
  const creds = await res.json();
  renderCredList(creds);
}

function renderCredList(creds) {
  const ul = document.getElementById('cred-list');
  ul.innerHTML = '';
  creds.forEach((c, i) => {
    const li = document.createElement('li');

    const isLast = i === creds.length - 1;

    li.className =
      "flex justify-between items-center py-2 " +
      (isLast ? "" : "border-b border-gray-200");

    const name = document.createElement('span');
    name.textContent = c.ssid;
	
	// Right: button group
	const btnGroup = document.createElement('div');
	btnGroup.className = "flex gap-3";   // space between ^1st and Delete

	if (c.priority !== 0) {
	    const btnFirst = document.createElement("button");
	    btnFirst.textContent = "^1st";
	    btnFirst.className = "px-2 py-1 bg-gray-300 rounded hover:bg-gray-400";
	    btnFirst.onclick = () => makeFirst(c.ssid);
	    btnGroup.appendChild(btnFirst);
	}
	
	// Delete button (furthest right)
	const btnDelete = document.createElement('button');
	btnDelete.textContent = 'Delete';
	btnDelete.className = "px-3 py-1 bg-red-600 text-white rounded hover:bg-red-700";
	btnDelete.onclick = () => requestDeleteCredential(c.ssid);
	btnGroup.appendChild(btnDelete);
	
    li.appendChild(name);
	// ^1st button (only if not already top)
	li.appendChild(btnGroup);
    ul.appendChild(li);
  });
}

function requestDeleteCredential(ssid) {
  showConfirm(
    'danger',
    'Delete Credential',
    `Do you want to delete "${ssid}"?`,
    () => deleteCredential(ssid)
  );
}

async function makeFirst(ssid) {
  await fetch("/framework/api/credentials/makeFirst", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ ssid })
  });

  await fetchCredentials();  // refresh UI
}

async function deleteCredential(ssid) {
  try {
    await fetch(`/framework/api/credentials/${encodeURIComponent(ssid)}`, {
      method: 'DELETE'
    });
    await fetchCredentials();
    showMessage('success', 'Deleted', `Credential "${ssid}" has been removed.`);
  } catch (err) {
    console.error(err);
    showMessage('error', 'Delete Failed', `Unable to delete "${ssid}".`);
  }
}

function requestClearCredentials() {
  showConfirm(
    'warning',
    'Clear All Credentials',
    'Do you want to remove all saved WiFi credentials?',
    clearCredentials
  );
}

async function clearCredentials() {
  try {
    await fetch('/framework/api/credentials/clear', { method: 'POST' });
    await fetchCredentials();
    showMessage('success', 'Cleared', 'All credentials have been removed.');
  } catch (err) {
    console.error(err);
    showMessage('error', 'Clear Failed', 'Unable to clear credentials.');
  }
}

function requestClearNvs() {
  showConfirm(
    'danger',
    'Clear NVS',
    'This will erase all stored WiFi data. Continue?',
    clearNvs
  );
}

async function clearNvs() {
  try {
    await fetch('/framework/api/credentials/clearNvs', { method: 'POST' });
    showMessage('success', 'NVS Cleared', 'Non-volatile storage has been erased.');
  } catch (err) {
    console.error(err);
    showMessage('error', 'Clear Failed', 'Unable to clear NVS.');
  }
}

async function confirmReboot() {
  try {
    const res = await fetch('/framework/api/device/reboot', { method: 'POST' });

    if (!res.ok) {
      showMessage('error', 'Reboot Failed', 'Unable to reboot device');
      return;
    }

    showMessage('success', 'Rebooting', 'Device is rebooting…');

    setTimeout(() => {
      location.reload();
    }, 3000);

  } catch (err) {
    console.error(err);
    showMessage('error', 'Reboot Failed', 'Unable to reboot device');
  }
}
