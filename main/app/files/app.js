(() => {
    'use strict';

    const TEMP_API   = '/app/api/temperature';
    const REFRESH_MS = 10_000;

    const tempValue  = document.getElementById('temp-value');
    const tempStatus = document.getElementById('temp-status');
    const refreshBtn = document.getElementById('refresh-btn');

    let refreshTimer = null;

    function setLoading() {
        refreshBtn.disabled = true;
        tempStatus.textContent = 'Updating…';
    }

    function setOk(celsius) {
        tempValue.textContent = celsius.toFixed(1);
        tempValue.classList.remove('error');
        tempStatus.textContent = 'Updated ' + new Date().toLocaleTimeString();
        refreshBtn.disabled = false;
    }

    function setError(msg) {
        tempValue.textContent = '—';
        tempValue.classList.add('error');
        tempStatus.textContent = msg;
        refreshBtn.disabled = false;
    }

    async function fetchTemperature() {
        setLoading();
        clearTimeout(refreshTimer);
        try {
            const res = await fetch(TEMP_API);
            if (!res.ok) {
                setError(`Error ${res.status}`);
                return;
            }
            const data = await res.json();
            if (typeof data.celsius !== 'number') {
                setError('Unexpected response');
                return;
            }
            setOk(data.celsius);
        } catch (e) {
            setError('Could not reach device');
        } finally {
            refreshTimer = setTimeout(fetchTemperature, REFRESH_MS);
        }
    }

    // Settings dropdown
    const navBtn      = document.getElementById('nav-settings-btn');
    const navDropdown = document.getElementById('nav-settings-dropdown');
    if (navBtn && navDropdown) {
        navBtn.addEventListener('click', e => {
            e.stopPropagation();
            const open = !navDropdown.classList.contains('hidden');
            navDropdown.classList.toggle('hidden', open);
            navBtn.setAttribute('aria-expanded', String(!open));
        });
        document.addEventListener('click', () => {
            navDropdown.classList.add('hidden');
            navBtn.setAttribute('aria-expanded', 'false');
        });
    }

    // Expose refresh so the inline onclick can reach it
    window.app = { refresh: fetchTemperature };

    // Kick off on load
    fetchTemperature();
})();
