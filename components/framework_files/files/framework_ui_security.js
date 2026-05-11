//
// framework_ui_security.js
//
// Home view (default password warning banner) and Security view
// (auth status display, change-password form).
//
// Exports:
//   initHomeView()
//   initSecurityView()
//

import {
    getAuthStatus,
    changePassword as apiChangePassword,
    login,
    isAuthenticated
} from "./api.js";

import { showMessage } from "./modal.js";


// ============================================================
// Home view
// ============================================================

/**
 * Initialise the home view — checks auth status and shows a warning banner
 * if the password has never been changed from its default.
 */
export async function initHomeView() {
    try {
        const status = await getAuthStatus();
        if (!status.passwordChanged) {
            const banner = document.getElementById("auth-warning-banner");
            if (banner) banner.classList.remove("hidden");
        }
    } catch (err) {
        // Auth status unavailable — not fatal, silently ignore
        console.warn("Could not fetch auth status:", err);
    }
}


// ============================================================
// Security view
// ============================================================

/**
 * Initialise the Security view — loads auth status display and wires the
 * change-password form.
 *
 * The submit button is disabled until getAuthStatus() succeeds so the form
 * cannot be submitted before credentials have been confirmed.  This prevents
 * a race where the user fills in the form before the initial 401 response
 * returns and the login overlay appears.
 */
export async function initSecurityView() {
    // ── Status display ────────────────────────────────────────────────────
    const statusEl  = document.getElementById("auth-status-display");
    const submitBtn = document.querySelector("#change-password-form button[type='submit']");

    // Wire form early (so Enter key is intercepted) but keep submit disabled
    // until we've confirmed the user is authenticated.
    const form = document.getElementById("change-password-form");
    if (form) {
        form.onsubmit = async (e) => {
            e.preventDefault();
            await handleChangePassword();
        };
    }
    if (submitBtn) submitBtn.disabled = true;

    try {
        const status = await getAuthStatus();
        // Auth confirmed — enable the form
        if (submitBtn) submitBtn.disabled = false;
        if (statusEl) {
            if (status.passwordChanged) {
                statusEl.textContent = "Password has been changed from the default.";
                statusEl.classList.add("auth-status-ok");
            } else {
                statusEl.innerHTML =
                    "<strong>Default password is in use.</strong> " +
                    "Change it below before putting this device into service.";
                statusEl.classList.add("auth-status-warn");
            }
        }
    } catch (err) {
        // Auth failed — login overlay will appear; form stays disabled until
        // the user authenticates and the view re-mounts.
        if (statusEl) statusEl.textContent = "Authentication required.";
        console.warn("Auth status fetch failed:", err);
    }
}

async function handleChangePassword() {
    // Last-line guard: if credentials were never confirmed (e.g. the form was
    // somehow submitted before the login overlay flow completed), bail early
    // with a user-visible message rather than sending an unauthenticated request.
    if (!isAuthenticated()) {
        showMessage("error", "Not Logged In",
                    "Log in with the current device password before changing it.");
        return;
    }

    const newPwEl      = document.getElementById("new-password");
    const confirmPwEl  = document.getElementById("confirm-password");
    if (!newPwEl || !confirmPwEl) return;

    const newPw     = newPwEl.value;
    const confirmPw = confirmPwEl.value;

    if (newPw.length < 8) {
        showMessage("error", "Password Too Short", "Password must be at least 8 characters.");
        return;
    }
    if (newPw !== confirmPw) {
        showMessage("error", "Password Mismatch", "Passwords do not match.");
        return;
    }

    try {
        await apiChangePassword(newPw);
        // The server invalidated all sessions on password change.
        // Re-authenticate immediately with the new password so the user
        // doesn't see a login prompt on the next API call.
        await login(newPw);
        newPwEl.value     = "";
        confirmPwEl.value = "";
        showMessage("success", "Password Changed",
                    "Your API password has been updated successfully.");
        // Re-initialise the status display — will now show green "changed" state
        await initSecurityView();
    } catch (err) {
        console.error("Password change failed:", err);
        showMessage("error", "Change Failed", err.message || "Could not change password.");
    }
}
