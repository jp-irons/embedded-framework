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
    loadFirmwareStatus   as apiLoadFirmwareStatus,
    uploadFirmware       as apiUploadFirmware,
    rollbackFirmware     as apiRollbackFirmware,
    factoryResetFirmware as apiFactoryResetFirmware,
    loadPullStatus       as apiLoadPullStatus,
    checkUpdate          as apiCheckUpdate,
    savePullConfig       as apiSavePullConfig,
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

    // Pull OTA — Check Now button
    const btnCheck = document.getElementById("btn-fw-check-update");
    if (btnCheck) btnCheck.onclick = requestCheckUpdate;

    // Pull OTA — Save URL button
    const btnSaveUrl = document.getElementById("btn-fw-save-url");
    if (btnSaveUrl) btnSaveUrl.onclick = requestSavePullUrl;

    loadFirmwareStatus();
    loadPullStatus();
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
        renderSummary(_lastPartitions);
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

/** Truncate "v6.0-dev-3948-gabcdef" → "IDF v6.0". */
function truncateIdfVer(ver) {
    const m = (ver || "").match(/^(v[\d.]+)/);
    return "IDF " + (m ? m[1] : (ver || ""));
}

// ── Accent color for the card's top border, keyed on partition state ────────
function accentColor(p) {
    if (p.isRunning)                                    return "#16a34a"; // green
    if (p.state === "valid")                            return "#16a34a"; // green
    if (p.state === "pending")                          return "#d97706"; // amber
    if (p.state === "invalid" || p.state === "aborted") return "#dc2626"; // red
    if (p.state === "new")                              return "#7c3aed"; // purple
    return "#d1d5db"; // gray — empty / factory
}

/**
 * Render the summary pane at the top of the Firmware page.
 * Shows the currently-running partition's label, version, build date,
 * and a "next boot" notice if a different partition is queued.
 */
function renderSummary(partitions) {
    const el = document.getElementById("fw-summary");
    if (!el) return;

    const running = partitions.find(p => p.isRunning);
    if (!running) {
        el.innerHTML = `<span class="text-gray-500">No running partition found.</span>`;
        return;
    }

    // Badges for the running partition
    const badges = [`<span class="fw-badge fw-badge-running">Running</span>`];
    const stateMap = {
        valid:   "fw-badge-valid",
        pending: "fw-badge-pending",
        invalid: "fw-badge-invalid",
        aborted: "fw-badge-aborted",
    };
    if (running.otaState && running.otaState !== "empty" && stateMap[running.otaState]) {
        const lbl = running.otaState.charAt(0).toUpperCase() + running.otaState.slice(1);
        badges.push(`<span class="fw-badge ${stateMap[running.otaState]}">${lbl}</span>`);
    }

    // Optional "next boot" notice when a different partition is queued
    const nextBoot = partitions.find(p => p.isNextBoot && !p.isRunning);
    const nextBootHtml = nextBoot ? `
        <div style="margin-top:0.5rem; font-size:0.75rem; color:#92400e;
                    background:#fffbeb; border:1px solid #fcd34d;
                    border-radius:0.25rem; padding:0.375rem 0.625rem;">
            Next boot: <strong>${nextBoot.label}</strong>${nextBoot.version ? ` · ${nextBoot.version}` : ""} — reboot to apply
        </div>` : "";

    el.innerHTML = `
        <div style="display:flex; justify-content:space-between; align-items:flex-start;">
            <div>
                <span style="font-weight:600; font-size:1rem;">${running.label}</span>
                ${running.version
                    ? `<span style="margin-left:0.5rem; color:#6b7280;">${running.version}</span>`
                    : ""}
            </div>
            <div style="display:flex; gap:0.375rem;">${badges.join("")}</div>
        </div>
        ${running.buildDate
            ? `<div style="margin-top:0.25rem; font-size:0.75rem; color:#6b7280;">
                   Built ${formatBuildDate(running.buildDate)}
               </div>`
            : ""}
        ${nextBootHtml}
    `;
}

/**
 * Render the 3-column partition grid.
 * Each card has a colored top border indicating its state, with version,
 * build date (on its own row), image size, project, and IDF version.
 */
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
        const accent      = accentColor(p);

        // Badges — Running and/or Next Boot first, then OTA/partition state
        const badges = [];
        if (p.isRunning) {
            badges.push(`<span class="fw-badge fw-badge-running">Running</span>`);
        }
        if (p.isNextBoot && !p.isRunning) {
            badges.push(`<span class="fw-badge fw-badge-next">Next Boot</span>`);
        }
        const stateMap = {
            valid:   "fw-badge-valid",
            pending: "fw-badge-pending",
            invalid: "fw-badge-invalid",
            aborted: "fw-badge-aborted",
            new:     "fw-badge-new",
            empty:   "fw-badge-empty",
            factory: "fw-badge-empty",
        };
        // For running partitions show the OTA state; for others show partition state
        const displayState = p.isRunning ? (p.otaState || p.state) : p.state;
        if (displayState && displayState !== "empty") {
            const cls = stateMap[displayState] || "";
            const lbl = displayState.charAt(0).toUpperCase() + displayState.slice(1);
            badges.push(`<span class="fw-badge ${cls}">${lbl}</span>`);
        }

        // Image size cell
        const slotLabel = p.partitionSize ? formatMB(p.partitionSize) : "—";
        const imageLabel = p.firmwareSize
            ? `${formatMB(p.firmwareSize)} / ${slotLabel}`
            : `— / ${slotLabel}`;

        // Value-only rows — no label column, context makes each value self-evident
        const tdVal = `style="padding-bottom:0.2rem; color:#374151;"`;
        const rows = hasFirmware ? `
            <tr><td ${tdVal}>${p.version}</td></tr>
            <tr><td ${tdVal}>${formatBuildDate(p.buildDate)}</td></tr>
            <tr><td ${tdVal}>${imageLabel}</td></tr>
            <tr><td ${tdVal}>${truncateIdfVer(p.idfVersion)}</td></tr>
        ` : `
            <tr><td ${tdVal}>${imageLabel}</td></tr>
        `;

        // Project name as a muted subtitle — part of the card identity, not a data row
        const projectSubtitle = hasFirmware && p.project
            ? `<div style="font-size:0.7rem; color:#9ca3af; margin-bottom:0.375rem;
                           white-space:nowrap; overflow:hidden; text-overflow:ellipsis;"
                    title="${p.project}">${p.project}</div>`
            : "";

        return `
            <div class="fw-partition-card" style="border-top:4px solid ${accent};">
                <div class="fw-partition-card-body">
                    <div style="font-weight:600; margin-bottom:0.1rem;">${p.label}</div>
                    ${projectSubtitle}
                    <div style="display:flex; flex-wrap:wrap; gap:0.25rem; margin-bottom:0.5rem;">
                        ${badges.join("")}
                    </div>
                    <table style="width:100%;">${rows}</table>
                </div>
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


// ============================================================
// Pull-based OTA
// ============================================================

async function loadPullStatus() {
    const urlDisplay = document.getElementById("fw-pull-url");
    const urlInput   = document.getElementById("fw-pull-url-input");
    try {
        const data = await apiLoadPullStatus();
        const url  = data.url || "";
        if (urlDisplay) urlDisplay.textContent = url || "Not configured";
        if (urlInput && !urlInput.value) urlInput.value = url;
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.warn("Pull status load failed:", err);
        if (urlDisplay) urlDisplay.textContent = "Unavailable";
    }
}

async function requestCheckUpdate() {
    const btn = document.getElementById("btn-fw-check-update");
    if (btn) { btn.disabled = true; btn.textContent = "Checking…"; }
    try {
        await apiCheckUpdate();
        showMessage("success", "Update Check Initiated",
                    "The device is checking for a firmware update in the background. " +
                    "Watch the serial log — the device will reboot automatically if an update is found.");
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error("Check update failed:", err);
        showMessage("error", "Check Failed", err.message || "Could not trigger update check.");
    } finally {
        if (btn) { btn.disabled = false; btn.textContent = "Check Now"; }
    }
}

async function requestSavePullUrl() {
    const urlInput = document.getElementById("fw-pull-url-input");
    const url = urlInput ? urlInput.value.trim() : "";
    if (!url) {
        showMessage("error", "Invalid URL", "Please enter a URL before saving.");
        return;
    }
    const btn = document.getElementById("btn-fw-save-url");
    if (btn) { btn.disabled = true; btn.textContent = "Saving…"; }
    try {
        await apiSavePullConfig(url);
        const urlDisplay = document.getElementById("fw-pull-url");
        if (urlDisplay) urlDisplay.textContent = url;
        showMessage("success", "URL Saved", "Auto-update URL saved successfully.");
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error("Save pull URL failed:", err);
        showMessage("error", "Save Failed", err.message || "Could not save URL.");
    } finally {
        if (btn) { btn.disabled = false; btn.textContent = "Save URL"; }
    }
}
