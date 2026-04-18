export function validateSSID(ssid) {
  return ssid && ssid.length > 0;
}

export function validatePassword(password) {
  return password.length >= 8;
}
