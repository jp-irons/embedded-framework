import { fetchStatus } from '/common/api.js';
import { $ } from '/common/ui.js';

async function update() {
  const status = await fetchStatus();
  $('#status').textContent = JSON.stringify(status, null, 2);
}

setInterval(update, 2000);
update();
