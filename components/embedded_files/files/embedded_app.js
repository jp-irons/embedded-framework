//
// embedded/app.js
//
// Tiny orchestrator for Provision Mode.
// All UI logic lives in /provision/ui.js
//

import { initUI } from "/embedded/ui.js";
import { wireConfirmButtons, hideMessageModal } from "/embedded/modal.js";

document.addEventListener("DOMContentLoaded", () => {
    // Modal system
    wireConfirmButtons();

    const msgOk = document.getElementById("message-ok-btn");
    if (msgOk) msgOk.onclick = hideMessageModal;

    // Initialise shared UI in runtime mode
    initUI({ mode: "runtime" });
});
