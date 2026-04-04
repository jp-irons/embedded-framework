// Global storage for scan results
window._scanResults = [];

let initialLoadDone = false;

document.addEventListener('DOMContentLoaded', () => {
    if (!initialLoadDone) {
        initialLoadDone = true;
        fetchCredentials();
    }
});

// Fetch /scan and populate UI
async function loadScanResults() {
  try {
    const res = await fetch('/api/wifi/scan');
    const networks = await res.json();

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
    alert("Please select a network");
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
// POST /submit
async function submitProvisioning() {
  const payload = buildProvisioningPayload();

  try {
    const res = await fetch('/api/credentials/submit', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    const result = await res.json();
    console.log("Provisioning submitted:", result);

    startStatusPolling();

  } catch (err) {
    console.error("Submit failed:", err);
  }
}

// Poll /status every second
async function pollStatus() {
  try {
    const res = await fetch('/api/provision/status');
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
  const res = await fetch('/api/credentials/list');
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

    const btn = document.createElement('button');
    btn.textContent = 'Delete';
    btn.className = "px-3 py-1 bg-red-600 text-white rounded hover:bg-red-700";
    btn.onclick = () => deleteCredential(c.ssid);

    li.appendChild(name);
    li.appendChild(btn);
    ul.appendChild(li);
  });
}

async function deleteCredential(ssid) {
  await fetch(`/api/credentials/${encodeURIComponent(ssid)}`, { method: 'DELETE' });
  await fetchCredentials();
}

async function clearCredentials() {
  await fetch('/api/credentials/clear', { method: 'POST' });
  await fetchCredentials();
}

async function clearNvs() {
  await fetch('/api/nvs/clear', { method: 'POST' });
}

document.getElementById('btn-clear-creds').onclick = clearCredentials;
document.getElementById('btn-clear-nvs').onclick = clearNvs;

window.addEventListener('load', fetchCredentials);