// framework_modal.js

let confirmCallback = null;

// ----- Confirm Modal -----

export function showConfirm(type, title, message, onConfirm) {
    const modal = document.getElementById('confirm-modal');
    const titleEl = document.getElementById('confirm-modal-title');
    const msgEl = document.getElementById('confirm-modal-message');

    titleEl.textContent = title;
    msgEl.textContent = message;

    // Color coding
    if (type === 'danger') {
        titleEl.style.color = '#dc2626';
    } else if (type === 'warning') {
        titleEl.style.color = '#d97706';
    } else {
        titleEl.style.color = '#111827';
    }

    confirmCallback = onConfirm;
    modal.classList.remove('hidden');
}

export function hideConfirmModal() {
    document.getElementById('confirm-modal').classList.add('hidden');
    confirmCallback = null;
}

export function wireConfirmButtons() {
    document.getElementById('confirm-cancel-btn').onclick = () => {
        hideConfirmModal();
    };

    document.getElementById('confirm-ok-btn').onclick = () => {
        if (confirmCallback) confirmCallback();
        hideConfirmModal();
    };
}

// ----- Message Modal -----

let messageOkCallback = null;

/**
 * Show the message modal.
 * @param {string}        type    - 'success' | 'error' | 'warning' | 'info'
 * @param {string}        title
 * @param {string}        message
 * @param {Function|null} onOk    - Optional callback fired when OK is clicked.
 *                                  Use this to sequence actions that should only
 *                                  happen after the user has acknowledged the
 *                                  message (e.g. showing the login overlay after
 *                                  a firmware upload confirmation).
 */
export function showMessage(type, title, message, onOk = null) {
    const modal = document.getElementById('message-modal');
    const titleEl = document.getElementById('message-modal-title');
    const msgEl = document.getElementById('message-modal-message');

    titleEl.textContent = title;
    msgEl.textContent = message;

    if (type === 'error') {
        titleEl.style.color = '#dc2626';
    } else if (type === 'success') {
        titleEl.style.color = '#16a34a';
    } else if (type === 'warning') {
        titleEl.style.color = '#d97706';
    } else {
        titleEl.style.color = '#111827';
    }

    messageOkCallback = onOk;
    modal.classList.remove('hidden');
}

export function hideMessageModal() {
    document.getElementById('message-modal').classList.add('hidden');
    const cb = messageOkCallback;
    messageOkCallback = null;
    if (cb) cb();
}

/**
 * Discard any pending onOk callback without calling it or hiding the modal.
 * Call this before showing the login overlay so that if the heartbeat or
 * reconnect poll triggers re-auth while a reboot message is displayed, the
 * message's OK button becomes a plain dismiss rather than re-triggering
 * forceReauth() after the user has already logged back in.
 */
export function clearMessageCallback() {
    messageOkCallback = null;
}
