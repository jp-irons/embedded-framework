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
    getPullCheckStatus   as apiGetPullCheckStatus,
    checkUpdate          as apiCheckUpdate,
    savePullConfig       as apiSavePullConfig,
    setAutoUpdateEnabled as apiSetAutoUpdateEnabled,
    isAuthenticated,
    forceReauth,
    startReconnectPolling
} from "./api.js";

import {
    showConfirm,
    showMessage
} from "./modal.js";

// ── Module state ─────────────────────────────────────────────────────────────

let _lastPartitions   = [];   // cache for rollback availability check
let fwUploadBusy      = false; // prevents double-submit during upload
let _checkPollTimer   = null;  // setInterval handle for pull-check status polling
let _fwViewGeneration = 0;     // incremented on each view mount; stale async callbacks check this


// ============================================================
// View initialisers
// ============================================================

/**
 * Initialise the Firmware view — loads partition status and wires buttons.
 */
export function initFirmwareView() {
    fwUploadBusy = false;
    _fwViewGeneration++;

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

    // Pull OTA — Auto-update toggle (handler is attached inside loadPullStatus
    // once we know the current state and whether it's UI-settable)

    loadFirmwareStatus();
    loadPullStatus();
    syncCheckStatus();  // reflect any in-progress pull check (e.g. triggered by periodic task)
}

/** Called by the router when navigating away. */
export function teardownFirmwareView() {
    fwUploadBusy = false;
    stopCheckPolling();
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

    // Date without time — strip the HH:MM that formatBuildDate leaves
    const dateOnly = formatBuildDate(running.buildDate || "").replace(/\s+\d{2}:\d{2}$/, "");

    el.innerHTML = `
        <div style="display:flex; justify-content:space-between; align-items:center;">
            <div style="display:flex; align-items:baseline; gap:0.5rem;">
                <span style="font-weight:600; font-size:1rem;">${running.label}</span>
                ${running.version
                    ? `<span style="color:#6b7280;">${running.version}</span>`
                    : ""}
                ${dateOnly
                    ? `<span style="color:#9ca3af; font-size:0.8rem;">${dateOnly}</span>`
                    : ""}
            </div>
            <div style="display:flex; gap:0.375rem; flex-wrap:wrap;">${badges.join("")}</div>
        </div>
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
        `Upload "${file.name}" (${Math.round(file.size / 1024)} kB) and reboot?`,
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

    // Show upload progress strip
    const progressWrapper = document.getElementById("fw-upload-progress");
    const progressBytes   = document.getElementById("fw-progress-bytes");
    if (progressWrapper) progressWrapper.classList.remove("hidden");

    const fmtKB = n => `${Math.round(n / 1024)} kB`;

    try {
        await apiUploadFirmware(file, (loaded, total) => {
            if (progressBytes) progressBytes.textContent = total > 0
                ? `Uploading… ${fmtKB(loaded)} of ${fmtKB(total)}`
                : `Uploading… ${fmtKB(loaded)}`;
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
        applyAutoUpdateState(data.autoUpdateEnabled !== false, data.uiSettable !== false);
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.warn("Pull status load failed:", err);
        if (urlDisplay) urlDisplay.textContent = "Unavailable";
    }
}

/**
 * Show or hide the auto-update toggle row and wire its button.
 * Called once after loadPullStatus() resolves so we know both the current
 * state and whether the setting is UI-settable.
 *
 * @param {boolean} enabled     - Current auto-update state
 * @param {boolean} uiSettable  - Whether the user can change it
 */
function applyAutoUpdateState(enabled, uiSettable) {
    const row   = document.getElementById("fw-auto-update-row");
    const label = document.getElementById("fw-auto-update-label");
    const btn   = document.getElementById("btn-fw-auto-update");
    if (!row || !label || !btn) return;

    if (!uiSettable) {
        row.classList.add("hidden");
        return;
    }

    // Show the row and set current state
    row.classList.remove("hidden");
    updateAutoUpdateButton(enabled, label, btn);

    // Re-attach onclick each time (avoids stale closures across view remounts)
    btn.onclick = () => requestToggleAutoUpdate(label, btn);
}

/** Render the toggle button to reflect the current enabled state. */
function updateAutoUpdateButton(enabled, label, btn) {
    label.textContent = enabled ? "Automatic updates are enabled" : "Automatic updates are disabled";
    btn.textContent   = enabled ? "Disable" : "Enable";
    btn.className     = "px-3 py-1 rounded whitespace-nowrap text-white bg-blue-600 hover:bg-blue-700";
}

async function requestToggleAutoUpdate(label, btn) {
    const currentlyEnabled = btn.textContent === "Disable";
    const newEnabled       = !currentlyEnabled;

    btn.disabled    = true;
    btn.textContent = "Saving…";

    try {
        await apiSetAutoUpdateEnabled(newEnabled);
        updateAutoUpdateButton(newEnabled, label, btn);
    } catch (err) {
        if (!isAuthenticated() || err.message === "network") return;
        console.error("Auto-update toggle failed:", err);
        showMessage("error", "Save Failed", err.message || "Could not update auto-update setting.");
        // Restore previous button state
        updateAutoUpdateButton(currentlyEnabled, label, btn);
    } finally {
        btn.disabled = false;
    }
}

// ── Pull-check status polling ─────────────────────────────────────────────

/**
 * Update the inline status strip to reflect a given pull-check state.
 *
 * States and their meaning:
 *   idle        — hide the strip (no check running)
 *   checking    — fetching version.txt from GitHub
 *   up_to_date  — remote version matched local; message = remote version string
 *   downloading — update found, esp_https_ota() in progress; message = remote version
 *   rebooting   — device dropped connection after download (inferred from network error)
 *   error       — any failure; message = short description
 *
 * Terminal states (up_to_date, error) re-enable the button and auto-hide after 6 s.
 * The downloading/rebooting states leave the button disabled until the view remounts
 * after reconnect.
 */
function applyCheckStatus(state, message, downloaded = 0, total = 0) {
    const strip = document.getElementById("fw-pull-status");
    const text  = document.getElementById("fw-pull-status-text");
    if (!strip || !text) return;

    const fmtKB = n => `${Math.round(n / 1024)} kB`;
    const progress = downloaded > 0
        ? ` · ${total > 0 ? `${fmtKB(downloaded)} of ${fmtKB(total)}` : fmtKB(downloaded)}`
        : "";

    const map = {
        idle:        null,
        checking:    { bg: "#eff6ff", color: "#1e40af", txt: "Checking for updates…" },
        up_to_date:  { bg: "#f0fdf4", color: "#166534",
                       txt: message ? `Already up to date (${message})` : "Already up to date" },
        downloading: { bg: "#fffbeb", color: "#92400e",
                       txt: (message ? `Update found (${message}) — downloading` : "Update found — downloading") + progress + "…" },
        rebooting:   { bg: "#fffbeb", color: "#92400e",
                       txt: "Update downloaded — device is rebooting…" },
        error:       { bg: "#fef2f2", color: "#991b1b", txt: message || "Check failed" },
    };

    const cfg = map[state];
    if (!cfg) {
        strip.classList.add("hidden");
        return;
    }

    strip.style.background = cfg.bg;
    strip.style.color      = cfg.color;
    text.textContent       = cfg.txt;
    strip.classList.remove("hidden");

    // Terminal states: re-enable the button and auto-hide the strip after a delay
    if (state === "up_to_date" || state === "error") {
        stopCheckPolling();
        const btn = document.getElementById("btn-fw-check-update");
        if (btn) { btn.disabled = false; btn.textContent = "Check Now"; }
        setTimeout(() => strip.classList.add("hidden"), 6000);
    }
}

function startCheckPolling() {
    stopCheckPolling();
    _checkPollTimer = setInterval(pollCheckStatus, 2000);
}

function stopCheckPolling() {
    if (_checkPollTimer) { clearInterval(_checkPollTimer); _checkPollTimer = null; }
}

async function pollCheckStatus() {
    const gen = _fwViewGeneration;
    try {
        const data = await apiGetPullCheckStatus();
        if (_fwViewGeneration !== gen) return; // view remounted while fetch was in flight
        // If the server reports "idle" while we're actively polling, the check finished
        // and the device cleared its state (e.g. after a reboot mid-check).  Treat it
        // as a terminal state so the button is re-enabled rather than left disabled.
        if (data.state === "idle") {
            stopCheckPolling();
            const btn = document.getElementById("btn-fw-check-update");
            if (btn) { btn.disabled = false; btn.textContent = "Check Now"; }
            applyCheckStatus("idle", ""); // hides the strip
            return;
        }
        applyCheckStatus(data.state, data.message || "", data.downloaded || 0, data.total || 0);
    } catch (err) {
        if (_fwViewGeneration !== gen) return; // stale — discard silently
        if (err.message === "network") {
            // Device dropped — a downloaded update is being flashed
            applyCheckStatus("rebooting", "");
            stopCheckPolling();
            startReconnectPolling();
        } else if (err.message === "unauthorized") {
            // Auth overlay will handle this; just stop polling
            stopCheckPolling();
        } else {
            // HTTP 404 means the endpoint isn't present (older firmware); anything
            // else is a genuine but non-fatal comms error — show the code, not a
            // generic "lost contact" message that implies a network failure.
            const msg = err.message?.startsWith("HTTP 404")
                ? "Status unavailable"
                : (err.message || "Check failed");
            applyCheckStatus("error", msg);
            stopCheckPolling();
        }
    }
}

/**
 * On view mount, check whether a pull check is already in progress
 * (e.g. triggered by the periodic background task) and show the strip if so.
 * Starts polling if the check is still running.
 */
async function syncCheckStatus() {
    const gen = _fwViewGeneration;
    try {
        const data = await apiGetPullCheckStatus();
        if (_fwViewGeneration !== gen) return; // stale
        if (data.state && data.state !== "idle") {
            applyCheckStatus(data.state, data.message || "", data.downloaded || 0, data.total || 0);
            if (data.state === "checking" || data.state === "downloading") {
                const btn = document.getElementById("btn-fw-check-update");
                if (btn) { btn.disabled = true; btn.textContent = "Checking…"; }
                startCheckPolling();
            }
        }
    } catch {
        // Best-effort — silently ignore if the endpoint isn't reachable on mount
    }
}

async function requestCheckUpdate() {
    const gen = _fwViewGeneration;
    const btn = document.getElementById("btn-fw-check-update");
    if (btn) { btn.disabled = true; btn.textContent = "Checking…"; }
    applyCheckStatus("checking", "");
    try {
        await apiCheckUpdate();
        if (_fwViewGeneration !== gen) return; // stale
        startCheckPolling();
    } catch (err) {
        if (_fwViewGeneration !== gen) return; // stale
        if (!isAuthenticated()) {
            // Login overlay will appear — just reset the strip silently
            applyCheckStatus("idle", "");
            if (btn) { btn.disabled = false; btn.textContent = "Check Now"; }
            return;
        }
        if (err.message === "network") {
            // Momentary connectivity loss — show a brief error so the user knows
            // what happened, then auto-hide and re-enable via applyCheckStatus
            applyCheckStatus("error", "Connection lost — try again");
            return;
        }
        console.error("Check update failed:", err);
        applyCheckStatus("error", err.message || "Could not trigger update check");
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
