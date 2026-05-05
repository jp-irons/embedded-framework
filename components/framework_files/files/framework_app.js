//
// embedded/app.js
//
// SPA entry point.
// - Wires modals (once, they live in the shell)
// - Defines routes
// - Each route renders its view HTML then calls the appropriate initXxx() from ui.js
//

import { initRouter }         from "./router.js";
import { wireConfirmButtons, hideMessageModal } from "./modal.js";
import { initWifiView, teardownWifiView, initDeviceView, initFirmwareView, teardownFirmwareView,
         initHomeView, initSecurityView } from "./ui.js";
import { setPassword, clearPassword, onAuthRequired, getAuthStatus, isAuthenticated } from "./api.js";

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
        if (pageShell)     pageShell.inert = true;
        if (loginPassword) loginPassword.value = "";
        if (loginError)    loginError.classList.add("hidden");
        if (loginOverlay)  loginOverlay.classList.remove("hidden");
        // Delay focus so the element is visible before the browser scrolls to it
        setTimeout(() => loginPassword?.focus(), 50);
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
                if (!appStarted) {
                    // Chrome's native password manager needs:
                    //   form submitted  →  navigation to a DIFFERENT url  →  login form gone
                    //
                    // Reloading to the exact same pathname is treated as a
                    // "same-page reload" — Chrome doesn't consider that a
                    // post-login navigation and won't offer to save.  Adding
                    // a harmless ?auth query string makes the destination URL
                    // distinct.  ESP-IDF's httpd matches handlers on path
                    // only, so the server serves the same file either way.
                    // The auto-login path strips ?auth via history.replaceState
                    // before the router renders, so the user never sees it.
                    //
                    // credentials.store() is fired fire-and-forget as a
                    // belt-and-suspenders hint for browsers that support it;
                    // it is NOT awaited because the user-gesture context is
                    // consumed by the getAuthStatus() await above.
                    if (navigator.credentials && window.PasswordCredential) {
                        navigator.credentials
                            .store(new PasswordCredential({ id: "admin", password: pw }))
                            .catch(err => console.debug("[auth] credential store:", err));
                    }
                    console.debug("[auth] login ok — reloading for credential save");
                    location.replace(location.pathname);
                } else {
                    // Re-authentication after session expiry — no reload needed,
                    // just dismiss the overlay and re-render the current route.
                    hideLoginOverlay();
                    window.dispatchEvent(new Event("hashchange"));
                }
            } catch {
                // 401 — wrong password; clear stored credential and show error
                clearPassword();
                if (loginError) loginError.classList.remove("hidden");
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
                initRouter({ routes, fallback: "#home" });
            })
            .catch(() => {
                // Credential is stale (password changed, device rebooted, etc.)
                // — show the overlay now so the user can re-authenticate.
                showLoginOverlay();
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
