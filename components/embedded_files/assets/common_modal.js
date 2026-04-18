// common_modal.js

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

export function showMessage(type, title, message) {
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

    modal.classList.remove('hidden');
}

export function hideMessageModal() {
    document.getElementById('message-modal').classList.add('hidden');
}
