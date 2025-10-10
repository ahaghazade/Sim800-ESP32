var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onLoad);

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) {
    console.log('Connection opened');
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}
function onMessage(event) {
    // Handle messages if needed
}

function onLoad(event) {
    initWebSocket();
}

let settings = [];
let existingSettings = [];

function combineSettings(existingSettings, newSettings) {
    if (newSettings.length === 0) return existingSettings;
    newSettings.forEach(newSetting => {
        const index = existingSettings.findIndex(existingSetting =>
            arraysEqual(existingSetting.days, newSetting.days) &&
            existingSetting.onTime === newSetting.onTime &&
            existingSetting.offTime === newSetting.offTime
        );
        if (index !== -1) {
            existingSettings[index] = newSetting;
        } else {
            existingSettings.push(newSetting);
        }
    });
    return existingSettings;
}

function arraysEqual(arr1, arr2) {
    return JSON.stringify(arr1) === JSON.stringify(arr2);
}

const days = document.querySelectorAll('.day');
days.forEach(day => {
    day.addEventListener('click', () => {
        day.classList.toggle('selected');
    });
});

document.getElementById('add-btn').addEventListener('click', () => {
    const selectedDays = [...document.querySelectorAll('.day.selected')].map(day => day.textContent);
    const onTime = document.getElementById('on-time').value;
    const offTime = document.getElementById('off-time').value;

    if (selectedDays.length > 0 && onTime && offTime) {
        const settingId = Date.now(); // Use timestamp as unique ID

        const newSetting = {
            id: settingId,
            days: selectedDays,
            onTime: onTime,
            offTime: offTime,
            isActive: true
        };

        existingSettings.push(newSetting);

        const settingElement = document.createElement('div');
        settingElement.id = `setting-${settingId}`;
        settingElement.classList.add('narrow-box');
        settingElement.innerHTML = `
            <div class="time-info">
                On: ${onTime} - Off: ${offTime}
                <div class="days-info">${selectedDays.join(', ')}</div>
            </div>
            <label class="toggle-switch">
                <input type="checkbox" id="toggle-${settingId}" checked>
                <span class="slider"></span>
            </label>
            <button class="delete-btn" data-id="${settingId}"><i class="fas fa-times"></i></button>
        `;
        document.getElementById('settings-list').appendChild(settingElement);

        document.getElementById(`toggle-${settingId}`).addEventListener('change', function () {
            const isChecked = this.checked;
            const setting = existingSettings.find(s => s.id === settingId);
            if (setting) setting.isActive = isChecked;
        });

        settingElement.querySelector('.delete-btn').addEventListener('click', function () {
            document.getElementById(`setting-${settingId}`).remove();
            existingSettings = existingSettings.filter(s => s.id !== settingId);
        });

    } else {
        alert('Please select days and set on/off times!');
    }
});

document.getElementById('save-btn').addEventListener('click', () => {
    const activeSettings = existingSettings.filter(setting => setting.isActive);
    const combinedSettings = combineSettings(existingSettings, activeSettings);

    if (combinedSettings.length > 0) {
        websocket.send(JSON.stringify({ "RelayTime": combinedSettings }));
        alert('Settings saved successfully.');
    } else {
        alert('No active settings to save.');
    }
});

document.addEventListener("DOMContentLoaded", function () {
    document.getElementById('log-btn').addEventListener('click', () => {
        window.location.href = '/log';
    });
    document.getElementById('setting-btn').addEventListener('click', () => {
        window.location.href = '/setting';
    });
    document.getElementById('clock-btn').addEventListener('click', () => {
        window.location.href = '/clock';
    });
    document.getElementById('sensors-btn').addEventListener('click', () => {
        window.location.href = '/sensors';
    });

    fetch('/GetClock')
        .then(response => response.json())
        .then(data => {
            populateTimeSettings(data);
            existingSettings = data.RelayTime;
        })
        .catch(error => {
            console.error("Error fetching data from ESP32:", error);
        });
});

function populateTimeSettings(data) {
    document.getElementById('settings-list').innerHTML = '';
    const relayTimes = data.RelayTime;

    relayTimes.forEach((setting, index) => {
        const id = Date.now() + index;  // Ensure uniqueness
        setting.id = id;

        const settingElement = document.createElement('div');
        settingElement.id = `setting-${id}`;
        settingElement.classList.add('narrow-box');

        settingElement.innerHTML = `
            <div class="time-info">
                On: ${setting.onTime} - Off: ${setting.offTime}
                <div class="days-info">${setting.days.join(', ')}</div>
            </div>
            <label class="toggle-switch">
                <input type="checkbox" id="toggle-${id}" ${setting.isActive ? 'checked' : ''}>
                <span class="slider"></span>
            </label>
            <button class="delete-btn" data-id="${id}"><i class="fas fa-times"></i></button>
        `;
        document.getElementById('settings-list').appendChild(settingElement);

        document.getElementById(`toggle-${id}`).addEventListener('change', function () {
            setting.isActive = this.checked;
        });

        settingElement.querySelector('.delete-btn').addEventListener('click', function () {
            document.getElementById(`setting-${id}`).remove();
            existingSettings = existingSettings.filter(s => s.id !== id);
        });
    });
}

//---------------AUTO LOGOUT-------------------
let logoutTimer;
function resetTimer() {
    clearTimeout(logoutTimer);
    startLogoutTimer();
}
function startLogoutTimer() {
    logoutTimer = setTimeout(function () {
        autoLogout();
    }, 300000);
}
function autoLogout() {
    fetch('/logout', {
        method: 'GET'
    })
        .then(response => response.text())
        .then(data => {
            window.location.href = '/';
        })
        .catch(error => console.error('Error:', error));
}
startLogoutTimer();
document.addEventListener('mousemove', resetTimer);
document.addEventListener('keypress', resetTimer);
document.addEventListener('mousedown', resetTimer);
document.addEventListener('touchstart', resetTimer);
document.getElementById('logoutBtn').addEventListener('click', function () {
    autoLogout();
});
