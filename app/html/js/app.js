const DATA_TYPES = ['cpu','memory','network','temperature','uptime','storage','thermometry','spot_temperatures','air_quality'];

document.addEventListener('DOMContentLoaded', () => {
    loadSettings();
    document.getElementById('saveBtn').addEventListener('click', saveSettings);
    document.getElementById('testBtn').addEventListener('click', testConnection);
});

function loadSettings() {
    fetch('settings')
        .then(r => r.json())
        .then(data => {
            const db = data.influxdb || {};
            document.getElementById('influxdbUrl').value    = db.url    || '';
            document.getElementById('influxdbOrg').value    = db.org    || '';
            document.getElementById('influxdbBucket').value = db.bucket || '';
            document.getElementById('influxdbToken').value  = db.token  || '';
            document.getElementById('influxdbEnabled').checked = !!db.enabled;

            const dc = data.dataCollection || {};
            document.getElementById('pollInterval').value = dc.pollIntervalSeconds || 30;

            const types = dc.selectedDataTypes || {};
            DATA_TYPES.forEach(t => {
                const el = document.querySelector(`input[name="${t}"]`);
                if (el) el.checked = !!types[t];
            });
        })
        .catch(() => showToast('Failed to load settings', 'error'));
}

function saveSettings() {
    const types = {};
    DATA_TYPES.forEach(t => {
        const el = document.querySelector(`input[name="${t}"]`);
        types[t] = el ? el.checked : false;
    });

    const settings = {
        influxdb: {
            url:     document.getElementById('influxdbUrl').value.trim(),
            org:     document.getElementById('influxdbOrg').value.trim(),
            bucket:  document.getElementById('influxdbBucket').value.trim(),
            token:   document.getElementById('influxdbToken').value.trim(),
            enabled: document.getElementById('influxdbEnabled').checked
        },
        dataCollection: {
            pollIntervalSeconds: parseInt(document.getElementById('pollInterval').value) || 30,
            selectedDataTypes: types
        },
        measurement: { name: 'device_metrics' }
    };

    fetch('settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    })
    .then(r => {
        if (r.ok) showToast('Settings saved', 'success');
        else showToast('Failed to save settings', 'error');
    })
    .catch(() => showToast('Failed to save settings', 'error'));
}

function testConnection() {
    showToast('Testing connection…', 'info');
    fetch('test')
        .then(r => r.ok
            ? showToast('Connected successfully', 'success')
            : r.text().then(t => showToast('Failed: ' + t, 'error')))
        .catch(() => showToast('Connection error', 'error'));
}

function showToast(msg, type) {
    const el = document.getElementById('toast');
    el.textContent = msg;
    el.className = 'toast ' + type + ' show';
    clearTimeout(el._t);
    el._t = setTimeout(() => el.classList.remove('show'), 4000);
}
