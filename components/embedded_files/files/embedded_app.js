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
import { initWifiView, teardownWifiView, initDeviceView } from "/embedded/ui.js";

document.addEventListener("DOMContentLoaded", () => {

    // Modals live in the shell — wire them once
    wireConfirmButtons();
    document.getElementById("message-ok-btn").onclick = hideMessageModal;

    // Route table
    const routes = [
        {
            hash: "#home",
            mount(app) {
                app.innerHTML = `
                    <h1 class="text-2xl font-semibold mb-4">Device Management</h1>
                    <p class="text-gray-700 mb-6">
                        This interface supports configuration of device features
                        including WiFi, Firmware, and Reboot.
                    </p>
                    <p class="text-sm text-gray-500 mt-6">
                        This interface is embedded in firmware and always available.
                    </p>
                `;
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
                                    class="px-3 py-1 bg-red-600 text-white rounded hover:bg-red-700">
                                Clear credentials
                            </button>
                            <button id="btn-reboot"
                                    class="px-3 py-1 bg-blue-600 text-white rounded hover:bg-blue-700">
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
                                    class="px-3 py-1 bg-blue-600 text-white rounded hover:bg-blue-700">
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
                                class="px-3 py-1 bg-blue-600 text-white rounded hover:bg-blue-700">
                            Refresh
                        </button>
                        <div class="flex gap-3">
                            <button id="btn-clear-nvs"
                                    class="px-3 py-1 bg-red-600 text-white rounded hover:bg-red-700">
                                Clear NVS
                            </button>
                            <button id="btn-reboot"
                                    class="px-3 py-1 bg-red-600 text-white rounded hover:bg-red-700">
                                Reboot
                            </button>
                        </div>
                    </div>
                `;
                initDeviceView();
            }
        }
    ];

    initRouter({ routes, fallback: "#home" });
});
