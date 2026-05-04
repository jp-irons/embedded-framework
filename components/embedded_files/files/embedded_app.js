//
// embedded/app.js
//
// SPA entry point.
// - Wires modals (once, they live in the shell)
// - Defines routes
// - Each route renders its view HTML then calls the appropriate initXxx() from ui.js
//

import { initRouter }         from "/embedded/router.js";
import { wireConfirmButtons, hideMessageModal } from "/embedded/modal.js";
import { initWifiView, teardownWifiView, initDeviceView, initFirmwareView, teardownFirmwareView,
         initHomeView, initSecurityView } from "/embedded/ui.js";
import { setPassword, clearPassword, onAuthRequired, getAuthStatus } from "/embedded/api.js";

document.addEventListener("DOMContentLoaded", () => {

    // Modals live in the shell — wire them once
    wireConfirmButtons();
    document.getElementById("message-ok-btn").onclick = hideMessageModal;

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
        if (pageShell)     pageShell.inert = true;
        if (loginPassword) loginPassword.value = "";
        if (loginError)    loginError.classList.add("hidden");
        if (loginOverlay)  loginOverlay.classList.remove("hidden");
        // Delay focus so the element is visible before the browser scrolls to it
        setTimeout(() => loginPassword?.focus(), 50);
    }

    function hideLoginOverlay() {
        if (loginOverlay) loginOverlay.classList.add("hidden");
        if (pageShell)    pageShell.inert = false;
    }

    // Register for 401s that occur during normal app operation (e.g. after a
    // password change or session expiry).  On re-login the current route
    // re-mounts via hashchange.
    onAuthRequired(showLoginOverlay);

    // Track whether the router has been started so the login handler knows
    // whether to launch the app or just re-render the current route.
    let appStarted = false;

    if (loginForm) {
        loginForm.onsubmit = async (e) => {
            e.preventDefault();
            const pw = loginPassword?.value ?? "";
            setPassword(pw);
            try {
                await getAuthStatus();
                hideLoginOverlay();
                if (!appStarted) {
                    // First successful login — start the router now.
                    // Doing this here (rather than unconditionally at startup)
                    // guarantees _password is set before any view calls the API.
                    appStarted = true;
                    initRouter({ routes, fallback: "#home" });
                } else {
                    // Re-authentication after session expiry — re-render the
                    // current route so it reloads its data with new credentials.
                    window.dispatchEvent(new Event("hashchange"));
                }
            } catch {
                // 401 — wrong password; clear stored credential and show error
                clearPassword();
                if (loginError) loginError.classList.remove("hidden");
            }
        };
    }

    // Show the login overlay immediately on page load.  Credentials are
    // required before any API call will succeed, so there is no point
    // rendering any view until the user has authenticated.  This also
    // eliminates the reactive 401-then-show race condition entirely.
    showLoginOverlay();

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
                    <div id="fw-upload-progress" class="hidden mt-4">
                        <div class="flex items-center gap-3 text-sm text-gray-700">
                            <span>Uploading…</span>
                            <div class="flex-grow bg-gray-200 rounded-full h-3">
                                <div id="fw-progress-bar"
                                     class="bg-blue-500 h-3 rounded-full transition-all"
                                     style="width:0%"></div>
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
