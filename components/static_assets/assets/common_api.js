export async function sendCredentials(ssid, password) {
  return fetch('/api/provision', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid, password })
  });
}

export async function fetchStatus() {
  const res = await fetch('/api/status');
  return res.json();
}
