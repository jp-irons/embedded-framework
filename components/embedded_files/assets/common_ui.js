export function $(selector) {
  return document.querySelector(selector);
}

export function show(el) {
  el.classList.remove('hidden');
}

export function hide(el) {
  el.classList.add('hidden');
}
