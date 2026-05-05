//
// embedded/common_ui.js
//
// Generic UI helper utilities.
// These are intentionally backend‑agnostic and provisioning/runtime‑agnostic.
// They should contain NO device logic, NO API calls, and NO modal logic.
//

// ------------------------------------------------------------
// Element creation helpers
// ------------------------------------------------------------

/**
 * Create an element with optional class and text.
 */
export function el(tag, className = "", text = "") {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text) node.textContent = text;
    return node;
}

/**
 * Create a button with label, class, and click handler.
 */
export function button(label, className = "", onClick = null) {
    const btn = document.createElement("button");
    btn.textContent = label;
    if (className) btn.className = className;
    if (onClick) btn.onclick = onClick;
    return btn;
}


// ------------------------------------------------------------
// DOM helpers
// ------------------------------------------------------------

/**
 * Remove all children from a node.
 */
export function clear(node) {
    if (node) node.innerHTML = "";
}

/**
 * Show an element (remove hidden or add block).
 */
export function show(node, display = "block") {
    if (node) node.style.display = display;
}

/**
 * Hide an element.
 */
export function hide(node) {
    if (node) node.style.display = "none";
}

/**
 * Toggle visibility.
 */
export function toggle(node, showState) {
    if (!node) return;
    node.style.display = showState ? "block" : "none";
}


// ------------------------------------------------------------
// Class helpers
// ------------------------------------------------------------

export function addClass(node, cls) {
    if (node) node.classList.add(cls);
}

export function removeClass(node, cls) {
    if (node) node.classList.remove(cls);
}

export function toggleClass(node, cls, state) {
    if (!node) return;
    if (state) node.classList.add(cls);
    else node.classList.remove(cls);
}


// ------------------------------------------------------------
// Form helpers
// ------------------------------------------------------------

export function getInputValue(id) {
    const el = document.getElementById(id);
    return el ? el.value : "";
}

export function setInputValue(id, value) {
    const el = document.getElementById(id);
    if (el) el.value = value;
}

export function getChecked(id) {
    const el = document.getElementById(id);
    return el ? el.checked : false;
}

export function setChecked(id, value) {
    const el = document.getElementById(id);
    if (el) el.checked = value;
}
