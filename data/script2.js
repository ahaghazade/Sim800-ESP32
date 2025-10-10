var counter = 0
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onLoad);

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line

    // Event listeners for toggle switches
    const toggles = [
        { id: 'temperature-toggle', key: 'TempSensorEn' },
        { id: 'humidity-toggle', key: 'HumiditySensorEn' },
        { id: 'fire-toggle', key: 'FireSensorEn' },
        { id: 'co-toggle', key: 'CoSensorEn' }
    ];

    toggles.forEach(toggle => {
        const element = document.getElementById(toggle.id);
        element.addEventListener('change', function() {
            const data = {
                [toggle.key]: element.checked ? 1 : 0 // Send 1 for checked, 0 for unchecked
            };

            // Send data through WebSocket
            websocket.send(JSON.stringify(data));
            console.log("Data sent:", data);
        });
    });
    
    // Event listener for the RGB slider
    const rgbSlider = document.getElementById('rgb-slider');
    rgbSlider.addEventListener('input', function() {
        const value = rgbSlider.value;
        document.getElementById('rgb-slider-value').textContent = "RGB Brightness (" + value + ")";
        
        // Create data object
        const data = {
            "RgbRingVariable": value
        };

        // Send data through WebSocket
        websocket.send(JSON.stringify(data));
        console.log("Data sent:", data);
    });

    // Event listener for the Radar slider
    const radarSlider = document.getElementById('radar-slider');
    radarSlider.addEventListener('input', function() {
        const value = radarSlider.value;
        document.getElementById('radar-slider-value').textContent = "Detection Distance (" + value + ")";
        const data = {
            "RadarDistanceVariable": value
        };
        websocket.send(JSON.stringify(data));
        console.log("Data sent:", data);
    });

    // Handle mouse up and touch end for RGB slider
    rgbSlider.addEventListener('mouseup', function() {
        sendFinishedMessage('RgbRing', rgbSlider.value);
    });
    rgbSlider.addEventListener('touchend', function() {
        sendFinishedMessage('RgbRing', rgbSlider.value);
    });

    // Handle mouse up and touch end for Radar slider
    radarSlider.addEventListener('mouseup', function() {
        sendFinishedMessage('RadarDistance', radarSlider.value);
    });
    radarSlider.addEventListener('touchend', function() {
        sendFinishedMessage('RadarDistance', radarSlider.value);
    });

    // Function to send finished message
    function sendFinishedMessage(key, value) {
        const finishedData = { [key]: value };
        websocket.send(JSON.stringify(finishedData));
        console.log("Finished message sent:", finishedData);
    }

}
function onOpen(event) {
    console.log('Connection opened');
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
} 
function onMessage(event) {
    console.log("new_readings[Page2]", event.data);
    const data = JSON.parse(event.data);
    if (data.hasOwnProperty('RadarDistance')  || data.hasOwnProperty('RgbRing')|| data.hasOwnProperty('TempSensorEn') ||
     data.hasOwnProperty('HumiditySensorEn') || data.hasOwnProperty('FireSensorEn') || data.hasOwnProperty('CoSensorEn') ||
     data.hasOwnProperty('LogCount') || data.hasOwnProperty('AlarmDelay') || data.hasOwnProperty('Firelimit') ||
     data.hasOwnProperty('Templimit') || data.hasOwnProperty('Creditlimit') || data.hasOwnProperty('Colimit') || data.hasOwnProperty('AlarmDuration'))
    {
        if (data.hasOwnProperty('RadarDistance'))
        {
            document.getElementById('radar-slider').value = data.RadarDistance;
            updateSliderRadarValue(data.RadarDistance)
        }
            
        if (data.hasOwnProperty('RgbRing'))
        {
            document.getElementById('rgb-slider').value = data.RgbRing;
            updateSliderRGBValue(data.RgbRing);
        }

        
        if (data.hasOwnProperty('TempSensorEn'))
            document.getElementById('temperature-toggle').checked = data.TempSensorEn === 1;
        if (data.hasOwnProperty('HumiditySensorEn'))
            document.getElementById('humidity-toggle').checked = data.HumiditySensorEn === 1;
        if (data.hasOwnProperty('FireSensorEn'))
            document.getElementById('fire-toggle').checked = data.FireSensorEn === 1;
        if (data.hasOwnProperty('CoSensorEn'))
            document.getElementById('co-toggle').checked = data.CoSensorEn === 1;
        if (data.hasOwnProperty('LogCount'))
            document.getElementById('log-count').placeholder = "Last Set: " + data.LogCount;
        if (data.hasOwnProperty('AlarmDelay'))
            document.getElementById('alarm-delay').placeholder = "Last Set: " + data.AlarmDelay + "(S)";
        if (data.hasOwnProperty('Templimit'))
            document.getElementById('temp-limit').placeholder = "Last Set: " + data.Templimit + "°C";
        if (data.hasOwnProperty('Creditlimit'))
            document.getElementById('credit-limit').placeholder = data.Creditlimit + " Rials";
        if (data.hasOwnProperty('Colimit'))
            document.getElementById('co-limit').placeholder = "Last Set: " + data.Colimit;
        if (data.hasOwnProperty('Firelimit'))
            document.getElementById('fire-limit').placeholder = "Last Set: " + data.Firelimit;
        if (data.hasOwnProperty('AlarmDuration'))
            document.getElementById('alarm-duration').placeholder = "Last Set: " + data.AlarmDuration;
    }
    else if (data.hasOwnProperty('PhoneNumbers')) 
    {
        const phoneNumbers = data.PhoneNumbers;

        // Loop through the received phone numbers and update the inputs
        phoneNumbers.forEach((entry, index) => {
            // Map index (0-3) to phone number inputs (1-4)
            const phoneIndex = index + 1;
            const phoneInput = document.getElementById(`admin-phone-${phoneIndex}`);
            const adminCheckbox = document.getElementById(`admin-check-${phoneIndex}`);

            // Get the phone number key (like Number1, Number2, etc.)
            const numberKey = `Number${phoneIndex}`;

            // Update the phone number field if it exists
            if (entry.hasOwnProperty(numberKey)) {
                phoneInput.value = entry[numberKey];
            }

            // Update the admin checkbox based on isAdmin value
            if (entry.hasOwnProperty('isAdmin')) {
                adminCheckbox.checked = entry.isAdmin;
            }
        });
    }
}
function onLoad(event) {
    initWebSocket();
}

function openColorPicker(event, item) {
    const picker = document.getElementById(item + 'Picker');
    picker.click();
}

function setColor(color, item) {
    const button = document.getElementById(item + 'Button');
    button.innerText = color; // Update button text with color code
    button.style.color = getContrastingColor(color); // Change text color based on background
    button.style.backgroundColor = color; // Update button background color

    // Send the updated color code via WebSocket
    sendColorCode(item, color);
}

function setInitialColor(color, item) {
    const button = document.getElementById(item + 'Button');
    button.innerText = color; // Update button text with color code
    button.style.color = getContrastingColor(color); // Change text color based on background
    button.style.backgroundColor = color; // Update button background color
}

function sendColorCode(item, color) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        const data = {};
        data[item + 'Color'] = color.replace('#', ''); // Remove '#' for hex code
        websocket.send(JSON.stringify(data));
    }
}

function getContrastingColor(hex) {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    
    const luminance = (0.299 * r + 0.587 * g + 0.114 * b);
    
    return luminance > 186 ? '#000000' : '#FFFFFF';
}


// Function to handle the submission
function handleSubmit() {
    // Collect values from inputs
    const radarDistance = document.getElementById('radar-slider').value;
    const rgbBrighness  = document.getElementById('rgb-slider').value;
    // const rgbToggle     = document.getElementById('rgb-toggle').checked;
    const temperatureToggle = document.getElementById('temperature-toggle').checked;
    const humidityToggle = document.getElementById('humidity-toggle').checked;
    const fireToggle    = document.getElementById('fire-toggle').checked;
    const coToggle      = document.getElementById('co-toggle').checked;
    const alarmDelay    = document.getElementById('alarm-delay').value;
    const logCount      = document.getElementById('log-count').value;
    const templimit     = document.getElementById('temp-limit').value;
    const creditlimit   = document.getElementById('credit-limit').value;
    const colimit       = document.getElementById('co-limit').value;
    const firelimit     = document.getElementById('fire-limit').value;
    const alarmduraion  = document.getElementById('alarm-duration').value;
    
    const phone1 = document.getElementById('admin-phone-1').value;
    const admin1 = document.getElementById('admin-check-1').checked;
    
    const phone2 = document.getElementById('admin-phone-2').value;
    const admin2 = document.getElementById('admin-check-2').checked;

    const phone3 = document.getElementById('admin-phone-3').value;
    const admin3 = document.getElementById('admin-check-3').checked;

    const phone4 = document.getElementById('admin-phone-4').value;
    const admin4 = document.getElementById('admin-check-4').checked;

    const newPassword = document.getElementById('new-password').value;
    const confirmPassword = document.getElementById('confirm-password').value;

    // Create a data object to send as JSON
    const data = {
        "RadarDistance"        : radarDistance,
        // "RgbRing"       : rgbToggle,
        "RgbRing"       : rgbBrighness,
        "TempSensorEn"    : temperatureToggle,
        "HumiditySensorEn": humidityToggle,
        "FireSensorEn"    : fireToggle,
        "CoSensorEn"      : coToggle,
        "AlarmDelay"    : alarmDelay,
        "LogCount"      : logCount,
        "Templimit"     : templimit,
        "Creditlimit"   : creditlimit,
        "Colimit"       : colimit,
        "Firelimit"     : firelimit,
        "AlarmDuration": alarmduraion,
        "PhoneNumbers"  : 
        [
            { "Number1": phone1, "isAdmin": admin1 },
            { "Number2": phone2, "isAdmin": admin2 },
            { "Number3": phone3, "isAdmin": admin3 },
            { "Number4": phone4, "isAdmin": admin4 }
        ],
        "NewPassword"   : newPassword,
        "ConfirmPassword": confirmPassword
    };

    // Convert the data object to JSON and send it through WebSocket
    websocket.send(JSON.stringify(data));
    console.log("Data sent: ", data);
}
// Event listener for the submit button
document.addEventListener("DOMContentLoaded", function() {
    const submitButton = document.getElementById('submit');
    
    submitButton.addEventListener('click', function(event) {
        event.preventDefault(); // Prevent the default form submission behavior
        handleSubmit(); // Call the function to handle submission
        alert('Changes applied');
    });

    // Get the setting button element
    const sensorsButton = document.getElementById('sensors-btn');
    // Add a click event listener to the log button for redirection
    sensorsButton.addEventListener('click', function() {
        // Redirect to the /update route
        window.location.href = '/sensors';
    });

    const clockButton = document.getElementById('clock-btn');
    // Add a click event listener to the log button for redirection
    clockButton.addEventListener('click', function() {
        // Redirect to the /update route
        window.location.href = '/clock';
    });

    const settingButton = document.getElementById('setting-btn');
    settingButton.addEventListener('click', function() {
        window.location.href = '/setting';
    });

    const logutton = document.getElementById('log-btn');
    logutton.addEventListener('click', function() {
        window.location.href = '/log';
    });
});

document.addEventListener('DOMContentLoaded', function() {
    // Fetch phone numbers from server when the page loads
    fetch('/GetPhones')
        .then(response => response.json())  // Parse the JSON response
        .then(data => {
            // Ensure the PhoneNumbers key exists
            if (data.hasOwnProperty('PhoneNumbers')) {
                const phoneNumbers = data.PhoneNumbers;

                // Loop through the received phone numbers and update the inputs
                phoneNumbers.forEach((entry, index) => {
                    // Map index (0-3) to phone number inputs (1-4)
                    const phoneIndex = index + 1;
                    const phoneInput = document.getElementById(`admin-phone-${phoneIndex}`);
                    const adminCheckbox = document.getElementById(`admin-check-${phoneIndex}`);

                    // Get the phone number key (like Number1, Number2, etc.)
                    const numberKey = `Number${phoneIndex}`;

                    // Update the phone number field if it exists
                    if (entry.hasOwnProperty(numberKey)) {
                        phoneInput.value = entry[numberKey];
                    }

                    // Update the admin checkbox based on isAdmin value
                    if (entry.hasOwnProperty('isAdmin')) {
                        adminCheckbox.checked = entry.isAdmin;
                    }
                });
            }
        })
        .catch(error => {
            console.error('Error fetching phone numbers:', error);
        });
});

document.addEventListener('DOMContentLoaded', function() {
    // Fetch the system state from the server
    fetch('/SystemState')
        .then(response => response.json()) // Parse the JSON data
        .then(data => {
            // Log the data to ensure it's fetched correctly
            console.log('System state:', data);

            document.getElementById('radar-slider').value = data.RadarDistance;
            updateSliderRadarValue(data.RadarDistance)
            document.getElementById('rgb-slider').value = data.RgbRing;
            updateSliderRGBValue(data.RgbRing);
            // document.getElementById('rgb-toggle').checked = data.RgbRing === 1;
            document.getElementById('temperature-toggle').checked = data.TempSensorEn === 1;
            document.getElementById('humidity-toggle').checked = data.HumiditySensorEn === 1;
            document.getElementById('fire-toggle').checked = data.FireSensorEn === 1;
            document.getElementById('co-toggle').checked = data.CoSensorEn === 1;
            document.getElementById('log-count').placeholder = "Last Set: " + data.LogCount;
            document.getElementById('alarm-delay').placeholder = "Last Set: " + data.AlarmDelay + "(S)";
            document.getElementById('temp-limit').placeholder = "Last Set: " + data.Templimit + "°C";
            document.getElementById('credit-limit').placeholder = data.Creditlimit + " Rials";
            document.getElementById('co-limit').placeholder = "Last Set: " + data.Colimit;
            document.getElementById('fire-limit').placeholder = "Last Set: " + data.Firelimit;
            document.getElementById('alarm-duration').placeholder = "Last Set: " + data.AlarmDuration;
            setInitialColor("#" + data.AlarmColor, "Alarm");
            setInitialColor("#" + data.SystemOnColor, "SystemOn");
            setInitialColor("#" + data.SystemOffColor, "SystemOff");
        })
        .catch(error => {
            console.error('Error fetching system state:', error);
        });
});

// Function to update the displayed value
function updateSliderRadarValue(value) {
    document.getElementById('radar-slider-value').textContent = "Detection Distance (" + value + ")";
}
function updateSliderRGBValue(value) {
    document.getElementById('rgb-slider-value').textContent = "RGB Brightness (" + value + ")";
}
// Set default value to match initial slider value
// document.getElementById('radar-slider').value = "Alarm Volume (-)";
// updateSliderVolumeValue(50);
// document.getElementById('rgb-slider').value = "RGB Brightness (-)";
// updateSliderRGBValue(50);

//---------------AUTO LOGOUT-------------------

let logoutTimer; // Variable to store the timer

// Function to reset the timer
function resetTimer() {
    clearTimeout(logoutTimer); // Clear the existing timer
    startLogoutTimer(); // Start a new timer
}

// Function to start the logout timer
function startLogoutTimer() {
    logoutTimer = setTimeout(function() {
        autoLogout();
    }, 300000); // 300000 milliseconds = 5 minutes
}

// Function to log out the user
function autoLogout() {
    fetch('/logout', {
        method: 'GET'
    })
    .then(response => response.text())
    .then(data => {
        // alert('Session expired due to inactivity. You have been logged out.'); // Alert the user
        window.location.href = '/'; // Redirect to login page
    })
    .catch(error => console.error('Error:', error));
}

// Start the logout timer when the page loads
startLogoutTimer();

// Reset the timer on any user activity
document.addEventListener('mousemove', resetTimer);
document.addEventListener('keypress', resetTimer);
document.addEventListener('mousedown', resetTimer); // Reset on mouse click
document.addEventListener('touchstart', resetTimer); // Reset on touch for mobile

// Logout button click event
document.getElementById('logoutBtn').addEventListener('click', function() {
    autoLogout(); // Call the logout function
});