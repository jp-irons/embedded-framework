//
// framework_ui_firmware.js
//
// Firmware view: OTA upload, rollback, factory reset, partition status display.
//
// Exports:
//   initFirmwareView()
//   teardownFirmwareView()
//

import {
    loadFirmwareStatus as apiLoadFirmwareStatus,
    uploadFirmware     as apiUploadFirmware,
    rollbackFirmware   as apiRollbackFirmware,
    factoryResetFirmware as apiFactoryResetFirmware,
    isAuthenticated,
    forceReauth
} from "./api.js";

import {
    showConfirm,
    showMessage
} from "./modal.js";

// ── Module state ─────────────────────────────────────────────────────────────

let _lastPartitions = [];   // cache for rollback availability check
let fwUploadBusy    = false; // prevents double-submit during upload


// ============================================================
// View initialisers
// ============================================================

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


// ============================================================
// Partition status
// ============================================================

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


// ============================================================
// Upload
// ============================================================

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


// ============================================================
// Rollback
// ============================================================

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


// ============================================================
// Factory Reset
// ============================================================

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
