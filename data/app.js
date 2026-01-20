// Watch Winder Web Interface

let statusInterval = null;
let isAPMode = false;
let wifiScanned = false;

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    loadSettings();
    updateStatus();
    statusInterval = setInterval(updateStatus, 2000);

    // Add change listeners to recalculate on input change
    ['motor1', 'motor2'].forEach(motor => {
        ['tpd', 'activeHours', 'rotationTime', 'restTime'].forEach(field => {
            const el = document.getElementById(`${motor}-${field}`);
            if (el) {
                el.addEventListener('change', () => calculateSchedule(motor));
                el.addEventListener('input', () => calculateSchedule(motor));
            }
        });
    });
});

// API helper
async function api(endpoint, method = 'GET', data = null) {
    const options = {
        method,
        headers: { 'Content-Type': 'application/json' }
    };

    if (data) {
        options.body = JSON.stringify(data);
    }

    try {
        const response = await fetch(`/api${endpoint}`, options);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
    } catch (error) {
        console.error('API Error:', error);
        throw error;
    }
}

// Update status display
async function updateStatus() {
    try {
        const status = await api('/status');

        // Update connection status
        const badge = document.getElementById('connection-status');
        badge.textContent = 'Connected';
        badge.className = 'status-badge connected';

        // Check AP mode
        isAPMode = status.apMode;
        document.getElementById('wifi-setup').classList.toggle('hidden', !isAPMode);

        if (isAPMode && !wifiScanned) {
            wifiScanned = true;
            scanWiFi();
        }

        // Update system info
        document.getElementById('system-ip').textContent = status.ip;
        document.getElementById('system-uptime').textContent = formatUptime(status.uptime);

        // Update motor 1 status
        updateMotorStatus('motor1', status.motor1);

        // Update motor 2 status
        updateMotorStatus('motor2', status.motor2);

    } catch (error) {
        const badge = document.getElementById('connection-status');
        badge.textContent = 'Disconnected';
        badge.className = 'status-badge error';
    }
}

function updateMotorStatus(motorId, data) {
    const statusEl = document.getElementById(`${motorId}-status`);
    statusEl.textContent = data.running ? 'Running' : 'Stopped';
    statusEl.className = `status-indicator ${data.running ? 'running' : 'stopped'}`;

    document.getElementById(`${motorId}-cycles`).textContent =
        `${data.cycles}/${data.totalCycles}`;
    document.getElementById(`${motorId}-turns`).textContent =
        data.turns.toFixed(1);
    document.getElementById(`${motorId}-next`).textContent =
        data.running ? formatTime(data.nextCycle) : '--';
}

// Load settings from device
async function loadSettings() {
    try {
        const settings = await api('/settings');

        // Motor 1
        populateMotorSettings('motor1', settings.motor1);

        // Motor 2
        populateMotorSettings('motor2', settings.motor2);

    } catch (error) {
        showToast('Failed to load settings', 'error');
    }
}

function populateMotorSettings(motorId, data) {
    document.getElementById(`${motorId}-enabled`).checked = data.enabled;
    document.getElementById(`${motorId}-direction`).value = data.direction;
    document.getElementById(`${motorId}-tpd`).value = data.tpd;
    document.getElementById(`${motorId}-activeHours`).value = data.activeHours;
    document.getElementById(`${motorId}-rotationTime`).value = data.rotationTime;
    document.getElementById(`${motorId}-restTime`).value = data.restTime;

    // Update calculated values
    document.getElementById(`${motorId}-cycles-calc`).textContent = data.cyclesPerDay;
    document.getElementById(`${motorId}-turns-calc`).textContent =
        data.turnsPerCycle.toFixed(2);
}

function getMotorSettings(motorId) {
    return {
        enabled: document.getElementById(`${motorId}-enabled`).checked,
        direction: parseInt(document.getElementById(`${motorId}-direction`).value),
        tpd: parseInt(document.getElementById(`${motorId}-tpd`).value),
        activeHours: parseInt(document.getElementById(`${motorId}-activeHours`).value),
        rotationTime: parseInt(document.getElementById(`${motorId}-rotationTime`).value),
        restTime: parseInt(document.getElementById(`${motorId}-restTime`).value)
    };
}

function calculateSchedule(motorId) {
    const settings = getMotorSettings(motorId);

    // Calculate cycle duration in milliseconds
    const cycleDurationMs = (settings.rotationTime * 1000) +
                            (settings.restTime * 60 * 1000);

    // Calculate cycles per day
    const activeMs = settings.activeHours * 60 * 60 * 1000;
    const cyclesPerDay = Math.floor(activeMs / cycleDurationMs);

    // Calculate turns per cycle
    const turnsPerCycle = settings.tpd / cyclesPerDay;

    document.getElementById(`${motorId}-cycles-calc`).textContent = cyclesPerDay;
    document.getElementById(`${motorId}-turns-calc`).textContent =
        turnsPerCycle.toFixed(2);
}

// Save settings to device
async function saveSettings() {
    try {
        const data = {
            motor1: getMotorSettings('motor1'),
            motor2: getMotorSettings('motor2')
        };

        await api('/settings', 'POST', data);
        showToast('Settings saved', 'success');
    } catch (error) {
        showToast('Failed to save settings', 'error');
    }
}

// Motor controls
async function startAll() {
    try {
        await api('/start', 'POST', { motor: 0 });
        showToast('All motors started', 'success');
    } catch (error) {
        showToast('Failed to start motors', 'error');
    }
}

async function stopAll() {
    try {
        await api('/stop', 'POST', { motor: 0 });
        showToast('All motors stopped', 'success');
    } catch (error) {
        showToast('Failed to stop motors', 'error');
    }
}

async function startMotor(motor) {
    try {
        await api('/start', 'POST', { motor });
        showToast(`Motor ${motor} started`, 'success');
    } catch (error) {
        showToast(`Failed to start motor ${motor}`, 'error');
    }
}

async function stopMotor(motor) {
    try {
        await api('/stop', 'POST', { motor });
        showToast(`Motor ${motor} stopped`, 'success');
    } catch (error) {
        showToast(`Failed to stop motor ${motor}`, 'error');
    }
}

async function testMotor(motor) {
    const direction = parseInt(document.getElementById(`motor${motor}-direction`).value);
    try {
        showToast(`Testing motor ${motor}...`, 'success');
        await api('/test', 'POST', { motor, direction, duration: 3 });
    } catch (error) {
        showToast(`Failed to test motor ${motor}`, 'error');
    }
}

// WiFi functions
async function scanWiFi() {
    try {
        const select = document.getElementById('wifi-ssid');
        select.innerHTML = '<option value="">Scanning...</option>';

        const data = await api('/wifi/scan');

        select.innerHTML = '<option value="">Select network...</option>';
        data.networks.forEach(network => {
            const option = document.createElement('option');
            option.value = network.ssid;
            option.textContent = `${network.ssid} (${network.rssi} dBm)${network.secure ? ' *' : ''}`;
            select.appendChild(option);
        });
    } catch (error) {
        showToast('Failed to scan WiFi networks', 'error');
    }
}

async function connectWiFi() {
    const ssid = document.getElementById('wifi-ssid').value;
    const password = document.getElementById('wifi-password').value;

    if (!ssid) {
        showToast('Please select a network', 'error');
        return;
    }

    try {
        await api('/wifi/connect', 'POST', { ssid, password });
        showToast('Connecting... Device will reboot', 'success');
    } catch (error) {
        showToast('Failed to save WiFi credentials', 'error');
    }
}

// Utility functions
function formatUptime(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (hours > 0) {
        return `${hours}h ${minutes}m`;
    } else if (minutes > 0) {
        return `${minutes}m ${secs}s`;
    }
    return `${secs}s`;
}

function formatTime(seconds) {
    if (seconds <= 0) return 'Now';

    const minutes = Math.floor(seconds / 60);
    const secs = seconds % 60;

    if (minutes > 0) {
        return `${minutes}m ${secs}s`;
    }
    return `${secs}s`;
}

function showToast(message, type = '') {
    // Remove existing toast
    const existing = document.querySelector('.toast');
    if (existing) existing.remove();

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    document.body.appendChild(toast);

    setTimeout(() => toast.remove(), 3000);
}
