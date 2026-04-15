export function showModal({ title, message, onConfirm, onCancel }) {
  const modal = document.getElementById('modal');
  modal.querySelector('.title').textContent = title;
  modal.querySelector('.message').textContent = message;

  modal.querySelector('.confirm').onclick = () => {
    modal.classList.remove('open');
    onConfirm?.();
  };

  modal.querySelector('.cancel').onclick = () => {
    modal.classList.remove('open');
    onCancel?.();
  };

  modal.classList.add('open');
}
