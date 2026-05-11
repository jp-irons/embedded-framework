//
// framework_app.js
//
// SPA entry point.
// - Wires modals (once, they live in the shell)
// - Defines routes
// - Each route renders its view HTML then calls the appropriate initXxx() from its view module
//

import { initRouter }         from "./router.js";
import { wireConfirmButtons, hideMessageModal, clearMessageCallback } from "./modal.js";
import { initWifiView, teardownWifiView }          from "./ui_wifi.js";
import { initDeviceView }                          from "./ui_device.js";
import { initFirmwareView, teardownFirmwareView }  from "./ui_firmware.js";
import { initHomeView, initSecurityView }          from "./ui_security.js";
import { login, clearToken, onAuthRequired, getAuthStatus, isAuthenticated,
         startReconnectPolling, stopReconnectPolling } from "./api.js";

document.addEventListener("DOMContentLoaded", () => {

    // Modals live in the shell — wire them once
    wireConfirmButtons();
    document.getElementById("message-ok-btn").onclick = hideMessageModal;

    // ----- Dropdown nav -----
    const navBtn      = document.getElementById("emb-nav-btn");
    const navDropdown = document.getElementById("emb-nav-dropdown");

    if (navBtn && navDropdown) {
        // Toggle open/close on button click
        navBtn.addEventListener("click", (e) => {
            e.stopPropagation();
            const isOpen = !navDropdown.classList.contains("hidden");
            navDropdown.classList.toggle("hidden", isOpen);
            navBtn.setAttribute("aria-expanded", String(!isOpen));
        });

        // Close immediately when any link inside is activated
        navDropdown.addEventListener("click", () => {
            navDropdown.classList.add("hidden");
            navBtn.setAttribute("aria-expanded", "false");
        });

        // Close when clicking anywhere outside the menu
        document.addEventListener("click", () => {
            navDropdown.classList.add("hidden");
            navBtn.setAttribute("aria-expanded", "false");
        });
    }

    // ----- Login overlay -----
    // fetch() never triggers the browser's built-in Basic Auth dialog on a 401,
    // so we manage credentials ourselves.  api.js calls onAuthRequired() whenever
    // a 401 is received; we respond by showing this overlay.  On successful login
    // we re-dispatch a hashchange so the current route re-mounts cleanly.
    const loginOverlay  = document.getElementById("login-overlay");
    const loginForm     = document.getElementById("login-form");
    const loginPassword = document.getElementById("login-password");
    const loginError    = document.getElementById("login-error");
    // The page shell sits outside the overlay in the DOM — marking it inert
    // while the overlay is visible traps keyboard focus inside the overlay and
    // prevents Tab from reaching form elements on the page behind it.
    const pageShell     = document.querySelector(".max-w-xl");

    function showLoginOverlay() {
        // Guard: if already visible, don't reset the password field or steal focus.
        // Without this, repeated 401s from polling (e.g. WiFi status every second)
        // would call this function once per second, wiping whatever the user has
        // typed in the password field.
        const alreadyVisible = loginOverlay && !loginOverlay.classList.contains("hidden");

        // Discard any pending message-modal onOk callback (e.g. the forceReauth()
        // queued by a firmware-upload success message).  If the heartbeat or any
        // other mechanism triggered re-auth first, the OK button must become a
        // plain dismiss rather than firing forceReauth() a second time after the
        // user has already logged back in.
        clearMessageCallback();

        if (pageShell)    pageShell.inert = true;
        if (loginOverlay) loginOverlay.classList.remove("hidden");

        if (!alreadyVisible) {
            if (loginPassword) loginPassword.value = "";
            if (loginError)    loginError.classList.add("hidden");
            // Delay focus so the element is visible before the browser scrolls to it
            setTimeout(() => loginPassword?.focus(), 50);
        }
    }

    function hideLoginOverlay() {
        if (loginOverlay)  loginOverlay.classList.add("hidden");
        if (pageShell)     pageShell.inert = false;
        // Clear the password field value AFTER hiding.  Chrome (and other
        // credential managers) use "password field value goes empty after a
        // form submit" as the signal that credentials were successfully consumed
        // and should be offered for saving.  Without this the SPA never gives
        // Chrome the confirmation it waits for, so the save prompt never appears.
        if (loginPassword) loginPassword.value = "";
    }

    // Register for 401s that occur during normal app operation (e.g. after a
    // password change or session expiry).  On re-login the current route
    // re-mounts via hashchange.
    onAuthRequired(showLoginOverlay);

    // ── Route-navigation auth check ───────────────────────────────────────
    // Belt-and-suspenders: whenever the user navigates to a new hash route,
    // explicitly verify the session token *before* the router mounts the
    // new view.  This fires independently of the background heartbeat and
    // provides an immediate auth check on any navigation that happens after
    // a device reboot.
    // Registered here (before initRouter is called) so this listener fires
    // first in the event queue.
    window.addEventListener("hashchange", () => {
        if (appStarted && isAuthenticated()) {
            console.debug("[nav] hashchange → auth check");
            getAuthStatus().catch(() => {});
        }
    });

    // Proactive token check when the user returns to this tab.
    // Without this, pages that have no background polling (Home, Device,
    // Firmware, Security) would never detect a stale token unless the user
    // clicked something.  A device reboot while the tab is open would leave
    // the UI silent and stuck.  This fires as soon as the tab is visible
    // again; a stale token gets a 401 which flows through handle401() →
    // onAuthRequired() → showLoginOverlay() as normal.
    document.addEventListener("visibilitychange", () => {
        if (document.visibilityState === "visible" && appStarted && isAuthenticated()) {
            console.debug("[visibility] tab visible → auth check");
            getAuthStatus().catch(() => {}); // 401 handled inside api.js handle401()
        }
    });

    // Track whether the router has been started so the login handler knows
    // whether to launch the app or just re-render the current route.
    let appStarted = false;

    // ---------- Background auth heartbeat ----------
    //
    // Sessions are RAM-only on the device — every reboot (planned or not)
    // invalidates all tokens.  The #wifi route has a 1-second wifiStatus()
    // poll that catches a 401 the moment the device comes back up.  Every
    // other route (#home, #device, #firmware, #security) makes exactly one
    // API call on mount; if that call lands during the reboot window it gets
    // a network error that is silently ignored, the device comes back, and
    // nothing triggers again — leaving the user on a stale page indefinitely.
    //
    // The heartbeat fills that gap.  It fires getAuthStatus() every 3 seconds
    // (matching the reconnect-poll cadence) while the session token is held.
    // A 401 flows through handle401() → onAuthRequired() → showLoginOverlay()
    // exactly as any other auth failure would.  Network errors (device still
    // booting / TLS not yet stable) are silently ignored — the next tick
    // retries.  Once a 401 is received the heartbeat stops itself — the
    // reconnect poll or a successful re-login will restart it.
    let _heartbeatTimer = null;

    function startAuthHeartbeat() {
        if (_heartbeatTimer) return; // already running
        console.debug("[heartbeat] starting (3 s interval)");
        _heartbeatTimer = setInterval(async () => {
            if (!appStarted) return;
            if (!isAuthenticated()) {
                // Token was already cleared (e.g. a prior 401 from another
                // code path showed the overlay).  Stop the heartbeat so we
                // don't fire unnecessary unauthenticated requests.
                console.debug("[heartbeat] token gone — stopping heartbeat");
                stopAuthHeartbeat();
                return;
            }
            // Yield if the user is being asked to acknowledge a message (e.g. "device
            // is rebooting — click OK to log in again").  The message's onOk callback
            // will call forceReauth() which shows the login overlay at the right moment.
            // Racing with it here causes double-overlay.
            const msgModal = document.getElementById("message-modal");
            if (msgModal && !msgModal.classList.contains("hidden")) {
                console.debug("[heartbeat] message modal open — skipping tick");
                return;
            }

            console.debug("[heartbeat] tick — calling getAuthStatus");
            try {
                await getAuthStatus();
                console.debug("[heartbeat] auth ok");
            } catch (err) {
                console.debug("[heartbeat] error:", err.message);
                if (err.message !== "network") {
                    // 401 (or other non-network failure) — handle401() inside
                    // api.js has already cleared the token and called
                    // showLoginOverlay().  Stop the heartbeat; it will be
                    // restarted by startAuthHeartbeat() on the next successful
                    // login.
                    console.debug("[heartbeat] non-network error — stopping heartbeat");
                    stopAuthHeartbeat();
                }
                // "network" → device still rebooting / TLS not yet stable;
                // keep the heartbeat running so we retry on the next tick.
            }
        }, 3_000);
    }

    function stopAuthHeartbeat() {
        if (_heartbeatTimer) {
            clearInterval(_heartbeatTimer);
            _heartbeatTimer = null;
            console.debug("[heartbeat] stopped");
        }
    }

    if (loginForm) {
        loginForm.onsubmit = async (e) => {
            e.preventDefault();
            const pw = loginPassword?.value ?? "";

            // Disable submit while in-flight so the user can't double-submit.
            const submitBtn = loginForm.querySelector('button[type="submit"]');
            if (submitBtn) submitBtn.disabled = true;
            if (loginError) loginError.classList.add("hidden");

            // After a device reboot the browser may still have stale TLS
            // session-resumption tickets.  The reconnect poller confirms the
            // device is up (it received a 401), but the *login* POST may land
            // on a different TCP connection that is still failing the TLS
            // handshake (-0x7780).  Rather than surfacing "Cannot reach device"
            // immediately and forcing the user to navigate away (which causes a
            // full page reload that is itself unreliable in this window), we
            // retry transparently up to MAX_RETRIES times with a short delay.
            // Wrong-password errors (401) are never retried.
            const MAX_RETRIES   = 5;
            const RETRY_DELAY   = 2000; // ms between attempts
            let lastErr = null;

            for (let attempt = 0; attempt <= MAX_RETRIES; attempt++) {
                try {
                    // POST /auth/login with Basic Auth → stores returned Bearer token
                    await login(pw);

                    // ── Success ──────────────────────────────────────────────
                    // Re-render in-place rather than doing a full page reload.
                    // location.replace() is unsafe in the ~60 s after a device
                    // reboot because TLS session-resumption tickets are
                    // invalidated; the browser tries to resume the old session,
                    // the ESP32 rejects it with a fatal alert (-0x7780), and
                    // the reload silently fails — leaving the user staring at a
                    // blank page with no feedback.  In-place re-render avoids
                    // the HTTPS round-trip entirely.
                    //
                    // Two cases:
                    //  • appStarted=false  — first login ever in this tab; the
                    //    router has not been initialised yet.
                    //  • appStarted=true   — re-authentication after a 401
                    //    (device rebooted while tab was open); dispatch
                    //    hashchange to re-mount the current route.
                    console.debug("[auth] login ok — re-rendering in place");
                    stopReconnectPolling();
                    if (submitBtn) submitBtn.disabled = false;
                    if (!appStarted) {
                        appStarted = true;
                        startAuthHeartbeat();
                        hideLoginOverlay();
                        initRouter({ routes, fallback: "#home" });
                    } else {
                        // Re-authentication after a 401 — the heartbeat was
                        // stopped when the 401 fired; restart it now that we
                        // have a fresh session token.
                        startAuthHeartbeat();
                        hideLoginOverlay();
                        window.dispatchEvent(new Event("hashchange"));
                    }
                    return; // done — exit the submit handler

                } catch (err) {
                    lastErr = err;

                    if (err.message === "network" && attempt < MAX_RETRIES) {
                        // TLS still stabilising — pause and try again
                        if (loginError) {
                            loginError.textContent = "Reconnecting…";
                            loginError.classList.remove("hidden");
                        }
                        await new Promise(r => setTimeout(r, RETRY_DELAY));
                        continue;
                    }

                    // Wrong password or retries exhausted — stop looping
                    break;
                }
            }

            // ── All attempts failed ───────────────────────────────────────
            if (submitBtn) submitBtn.disabled = false;
            clearToken();
            if (loginError) {
                // Distinguish network errors (device unreachable / TLS cert
                // changed) from auth errors (wrong password) so the user
                // knows whether to check the connection or their password.
                loginError.textContent = (lastErr?.message === "network")
                    ? "Cannot reach device — check connection or try again."
                    : "Incorrect password — please try again.";
                loginError.classList.remove("hidden");
            }
            // If we exhausted network-error retries, restart reconnect
            // polling so the overlay remains responsive: the next 401 from
            // the poller will re-arm the form and the user can try again
            // once the connection has fully stabilised.
            if (lastErr?.message === "network") {
                startReconnectPolling();
            }
        };
    }

    // On page load, choose between two paths:
    //
    //  • No stored credential → show the login overlay immediately.
    //
    //  • Stored credential → attempt silent validation first; only show the
    //    overlay if the credential turns out to be stale.  Showing it
    //    unconditionally then hiding it once the round-trip completes causes a
    //    visible flash on every navigation between pages.
    if (isAuthenticated()) {
        getAuthStatus()
            .then(() => {
                // Credential is still valid — start the app without ever
                // showing the overlay.
                appStarted = true;
                startAuthHeartbeat();
                initRouter({ routes, fallback: "#home" });
            })
            .catch((err) => {
                // Credential is stale or device unreachable — show the overlay.
                showLoginOverlay();
                // If the failure was a network error (device mid-reboot at
                // page-load time), start reconnect polling immediately so the
                // overlay auto-recovers when the device comes back.  A 401
                // ("unauthorized") means the token is simply expired — the
                // user just needs to type the password; no polling required.
                if (err?.message === "network") {
                    startReconnectPolling();
                }
            });
    } else {
        showLoginOverlay();
    }

    // Route table
    const routes = [
        {
            hash: "#home",
            mount(app) {
                app.innerHTML = `
                    <div id="auth-warning-banner" class="auth-warning-banner hidden">
                        <strong>Default password in use.</strong>
                        Consider <a href="#security" class="auth-warning-link">changing your password</a>
                        before putting this device into service.
                    </div>
                    <h1 class="text-2xl font-semibold mb-4">Device Management</h1>
                    <p class="text-gray-700 mb-6">
                        This interface supports configuration of device features
                        including WiFi, Firmware, and Reboot.
                    </p>
                    <p class="text-sm text-gray-500 mt-6">
                        This interface is embedded in firmware and always available.
                    </p>
                `;
                initHomeView();
            }
        },
        {
            hash: "#wifi",
            mount(app) {
                app.innerHTML = `
                    <h1 class="text-2xl font-semibold mb-4">WiFi Provisioning</h1>

                    <div class="mt-6 pt-6 border-t border-gray-300">
                        <div class="flex items-center gap-3">
                            <label class="font-medium whitespace-nowrap">Wi-Fi Status</label>
                            <div id="status"
                                 class="flex-grow p-3 bg-gray-50 border rounded text-sm text-gray-700">
                                Waiting for status…
                            </div>
                        </div>
                    </div>

                    <div class="mt-6 pt-6 border-t border-gray-300">
                        <label class="block font-medium mb-1">Saved Credentials</label>
                        <ul id="cred-list" class="space-y-2"></ul>

                        <div class="flex justify-end gap-3 mt-4 pt-4">
                            <button id="btn-clear-creds"
                                    class="px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700">
                                Clear credentials
                            </button>
                            <button id="btn-reboot"
                                    class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700">
                                Reboot
                            </button>
                        </div>
                    </div>

                    <div class="mt-6 pt-6 border-t border-gray-300">
                        <div class="flex items-center gap-3">
                            <label class="font-medium whitespace-nowrap">Password</label>
                            <input id="password" type="password"
                                   class="flex-grow px-3 py-2 border rounded"
                                   placeholder="Network password" />
                            <button id="btn-save"
                                    class="px-4 py-2 bg-green-600 text-white rounded hover:bg-green-700">
                                Save
                            </button>
                        </div>
                    </div>

                    <div class="mt-6 pt-6">
                        <div class="flex justify-between items-center mb-2">
                            <label class="block font-medium mb-1">Available Networks</label>
                            <button id="btn-refresh"
                                    class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700">
                                Refresh
                            </button>
                        </div>
                        <div id="network-list" class="space-y-3"></div>
                    </div>
                `;
                initWifiView();
            },
            teardown() {
                teardownWifiView();
            }
        },
        {
            hash: "#firmware",
            mount(app) {
                app.innerHTML = `
                    <h1 class="text-2xl font-semibold mb-4">Firmware</h1>

                    <div id="firmware-partitions" class="space-y-3">
                        <div class="text-gray-500 text-sm">Loading partition info…</div>
                    </div>
					
                    <!-- Upload progress (hidden until an upload is in progress) -->
                    <div id="fw-upload-progress" 
							class="hidden mt-4 border rounded p-4 bg-gray-300 shadow-sm text-sm">
                        <div class="flex items-center gap-3 ">
                            <span>Uploading…</span>
                            <div class="flex-grow bg-gray-200 rounded-full h-3">
                                <div id="fw-progress-bar" class="bg-blue-600 rounded-full h-3" style="width:0%"></div>
                            </div>
                            <span id="fw-progress-pct" class="w-10 text-right">0%</span>
                        </div>
					</div>


                    <!-- Hidden file input — triggered by the Upload button -->
                    <input id="fw-file-input" type="file" accept=".bin" class="hidden" />

                    <div class="mt-6 pt-4 border-t border-gray-300">
                        <p id="fw-rollback-note"
                           class="hidden text-xs text-gray-500 mb-3 text-right">
                            Rollback is available after a second OTA upgrade, when a
                            previous valid OTA image exists. To return to factory firmware
                            now, use Factory Reset.
                        </p>
                        <div class="flex justify-end gap-3">
                            <button id="btn-fw-upload"
                                    class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700">
                                Upload Firmware
                            </button>
                            <button id="btn-fw-rollback"
                                    class="px-4 py-2 bg-blue-600 text-white rounded
                                           opacity-50 cursor-default"
                                    disabled>
                                Rollback
                            </button>
                            <button id="btn-fw-factory"
                                    class="px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700">
                                Factory Reset
                            </button>
                        </div>
                    </div>
                `;
                initFirmwareView();
            },
            teardown() {
                teardownFirmwareView();
            }
        },
        {
            hash: "#device",
            mount(app) {
                app.innerHTML = `
                    <h1 class="text-2xl font-semibold mb-4">Device</h1>

                    <div id="device-info-container"
                         class="border rounded p-4 bg-white shadow-sm text-sm">
                        Loading…
                    </div>

                    <div class="flex justify-between items-center mt-4 pt-4 border-t border-gray-300">
                        <button id="btn-refresh-device-info"
                                class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700">
                            Refresh
                        </button>
                        <div class="flex gap-3">
                            <button id="btn-clear-nvs"
                                    class="px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700">
                                Clear NVS
                            </button>
                            <button id="btn-reboot"
                                    class="px-4 py-2 bg-red-600 text-white rounded hover:bg-red-700">
                                Reboot
                            </button>
                        </div>
                    </div>
                `;
                initDeviceView();
            }
        },
        {
            hash: "#security",
            mount(app) {
                app.innerHTML = `
                    <h1 class="text-2xl font-semibold mb-4">Security</h1>

                    <div id="auth-status-display" class="mb-6 p-3 bg-gray-50 border rounded text-sm text-gray-700">
                        Checking status…
                    </div>

                    <div class="mt-6 pt-6 border-t border-gray-300">
                        <h2 class="font-medium mb-4">Change API Password</h2>

                        <form id="change-password-form">
                            <div class="mb-4">
                                <label class="block font-medium mb-1">New Password</label>
                                <input id="new-password" type="password"
                                       class="input-field"
                                       placeholder="Min. 8 characters"
                                       autocomplete="new-password" />
                            </div>
                            <div class="mb-4">
                                <label class="block font-medium mb-1">Confirm Password</label>
                                <input id="confirm-password" type="password"
                                       class="input-field"
                                       placeholder="Re-enter new password"
                                       autocomplete="new-password" />
                            </div>
                            <div class="flex justify-end">
                                <button type="submit"
                                        class="px-4 py-2 bg-blue-600 text-white rounded hover:bg-blue-700">
                                    Change Password
                                </button>
                            </div>
                            <p class="text-sm text-gray-500 mt-3">
                                Passwords must be 8–64 characters.
                                You will be prompted to log in again after changing.
                            </p>
                        </form>
                    </div>
                `;
                initSecurityView();
            }
        }
    ];

    // Note: initRouter() is called inside the login handler above,
    // after the first successful authentication.
});
