import { sendCredentials } from '/common/api.js';
import { validateSSID, validatePassword } from '/common/validation.js';
import { showModal } from '/common/modal.js';
import { $, hide, show } from '/common/ui.js';

$('#submit').onclick = async () => {
  const ssid = $('#ssid').value.trim();
  const password = $('#password').value;

  if (!validateSSID(ssid)) {
    showModal({ title: 'Error', message: 'SSID required' });
    return;
  }

  if (!validatePassword(password)) {
    showModal({ title: 'Error', message: 'Password too short' });
    return;
  }

  await sendCredentials(ssid, password);

  showModal({
    title: 'Success',
    message: 'Credentials sent. Device will reboot.'
  });
};
