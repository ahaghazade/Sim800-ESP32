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
}
function onOpen(event) {
    console.log('Connection opened');
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
} 

function onMessage(event) {
    console.log("new_readings[Page1]", event.data);
    const data = JSON.parse(event.data);
    // Check if the message contains switch states
    if (data.hasOwnProperty('LampStatus')  || data.hasOwnProperty('SystemStatus')|| data.hasOwnProperty('Temperature') ||
     data.hasOwnProperty('Pressure') || data.hasOwnProperty('Humidity') || data.hasOwnProperty('Credit') ||
     data.hasOwnProperty('Co') || data.hasOwnProperty('Power') || data.hasOwnProperty('FireSensor') || data.hasOwnProperty('Motion'))
    {
        // Assuming the JSON structure is {"system_status":1,"lamp_status":0,"alarm_status":false,"send_sms":false}
        if (data.hasOwnProperty('LampStatus')) {
            const lampStatus = data.LampStatus;
            // Update the checkbox slider based on the lamp_status
            const relayToggle = document.getElementById('lamp-toggle');
            relayToggle.checked = lampStatus === 1;  // Assuming 1 = On, 0 = Off

            // Update the status text based on the lamp_status
            const relayStatus = document.getElementById('lamp-status');
            relayStatus.textContent = lampStatus === 1 ? 'On' : 'Off';
        }
        if (data.hasOwnProperty('SystemStatus')) {
            const systemToggle = document.getElementById('system-toggle');
            const systemStatus = document.getElementById('system-status');
            
            systemToggle.checked = data.SystemStatus == 1; // Set checkbox based on system status
            systemStatus.textContent = data.SystemStatus == 1 ? 'Active' : 'Inactive'; // Set the status text
        }
        // if (data.hasOwnProperty('send_sms')) {
        //     document.getElementById('sms').checked = data.send_sms;
        // }
        // if (data.hasOwnProperty('alarm_status')) {
        //     document.getElementById('alarm').checked = data.alarm_status;
        // }

        if (data.hasOwnProperty('Temperature')) {
            const temperature = data.Temperature;
            drawGauge(temperature, "temperature-gauge" , "°C");
        }
        if (data.hasOwnProperty('Humidity')) {
            const humidity = data.Humidity;
            drawGauge(humidity, "humidity-gauge" , "%");
        }
        if (data.hasOwnProperty('FireSensor')) {
            document.getElementById('fire-sensor').textContent = data.FireSensor;
        }
        if (data.hasOwnProperty('Motion')) {
            document.getElementById('motion').textContent = data.Motion;
        }
        if (data.hasOwnProperty('Pressure')) {
            document.getElementById('pressure').textContent = data.Pressure;
        }
        if (data.hasOwnProperty('Co')) {
            document.getElementById('co-sensor').textContent = data.Co;
        }
        if (data.hasOwnProperty('Power')) {
            document.getElementById('power-status').textContent = data.Power + "%";
        }
        if (data.hasOwnProperty('Credit')) {
            document.getElementById('sim-credit').textContent = data.Credit + " Rials";
        }
    }
}

function onLoad(event) {
    initWebSocket();
}

document.getElementById('system-toggle').onchange = function() {
    const status = this.checked ? 'Active' : 'Inactive';
    document.getElementById('system-status').textContent = status;
    console.log('system toggled to: ' + this.checked);
    var message = JSON.stringify({ "System": this.checked ? 1 : 0 });
    // Send the message through the WebSocket
    console.log(message);
    websocket.send(message);
};

document.getElementById('lamp-toggle').onchange = function() {
    const status = this.checked ? 'On' : 'Off';
    document.getElementById('lamp-status').textContent = status;
    console.log('relay toggled to: ' + this.checked);
    var message = JSON.stringify({ "Lamp": this.checked ? 1 : 0 });
    // Send the message through the WebSocket
    console.log(message);
    websocket.send(message);
};

document.addEventListener("DOMContentLoaded", function() {
    const dashboardText = document.querySelector('.loading-text');
    dashboardText.classList.add('fadeIn');
});

document.addEventListener("DOMContentLoaded", function() {
    const dashboardText = document.querySelector('.loading-text');
    const items = document.querySelectorAll('.status .item');

    // Trigger the fade-in for the dashboard text
    dashboardText.classList.add('fadeIn');

    // Delay and animate each box
    items.forEach((item, index) => {
        item.style.animationDelay = `${0.2 * (index + 1)}s`;
        item.classList.add('slideInUp');
    });
});
// Function to draw the temperature gauge
function drawGauge(value , gaugeType, gaugeUnit) {
    const canvas = document.getElementById(gaugeType);
    const ctx = canvas.getContext('2d');
    
    const maxValue = 100; // Maximum temperature value
    const minValue = -20; // Minimum temperature value
    const startAngle = 0.75 * Math.PI; // Start angle for semicircle (135 degrees)
    const endAngle = 2.25 * Math.PI; // End angle for semicircle (405 degrees)
    const gaugeRadius = 60; // Radius of the gauge
    const centerX = canvas.width / 2;
    const centerY = canvas.height / 2;

    // Clear the canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw gauge background
    ctx.beginPath();
    ctx.arc(centerX, centerY, gaugeRadius, startAngle, endAngle, false);
    ctx.lineWidth = 15;
    ctx.strokeStyle = '#e0e0e0';
    ctx.stroke();

    // Calculate the angle for the current temperature
    const valueRange = maxValue - minValue;
    const adjustedValue = value - minValue;
    const angle = startAngle + (adjustedValue / valueRange) * (endAngle - startAngle);

    // Draw gauge fill for the temperature value
    ctx.beginPath();
    ctx.arc(centerX, centerY, gaugeRadius, startAngle, angle, false);
    ctx.lineWidth = 15;
    
    // Create a blue gradient for temperature
    const gradient = ctx.createLinearGradient(0, 0, canvas.width, 0);
    gradient.addColorStop(0, '#ADD8E6'); // Light blue for low temperatures
    gradient.addColorStop(1, '#00008B'); // Dark blue for high temperatures
    ctx.strokeStyle = gradient;
    ctx.stroke();

    // Draw gauge needle
    const needleLength = gaugeRadius - 10;
    const needleAngle = angle;
    ctx.beginPath();
    ctx.moveTo(centerX, centerY);
    ctx.lineTo(centerX + needleLength * Math.cos(needleAngle), centerY + needleLength * Math.sin(needleAngle));
    ctx.lineWidth = 3;
    ctx.strokeStyle = '#333';
    ctx.stroke();

    // Draw the needle pivot
    ctx.beginPath();
    ctx.arc(centerX, centerY, 5, 0, 2 * Math.PI);
    ctx.fillStyle = '#333';
    ctx.fill();

    // Draw text value slightly lower in the center
    ctx.font = '16px Roboto';
    ctx.fillStyle = '#333';
    ctx.textAlign = 'center';
    ctx.fillText(`${value}` + gaugeUnit, centerX, centerY + 30); // Adjusted the position lower
}

document.addEventListener("DOMContentLoaded", function() {
    drawGauge(-20, "temperature-gauge", "°C");
    drawGauge(0, "humidity-gauge" , "%");
});

document.addEventListener("DOMContentLoaded", function() {

    // Get the log button element
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

    const sensorButton = document.getElementById('sensors-btn');
    sensorButton.addEventListener('click', function() {
        window.location.href = '/sensors';
    });
    
});

// Function to fetch system state and update the UI
function fetchSystemState() {
    fetch('/SystemState')
        .then(response => response.json())  // Parse the JSON response
        .then(data => {
            console.log('System state received:', data);
            
            // Update system status
            if (data.hasOwnProperty('SystemStatus')) {
                const systemToggle = document.getElementById('system-toggle');
                const systemStatus = document.getElementById('system-status');
                
                systemToggle.checked = data.SystemStatus == 1; // Set checkbox based on system status
                systemStatus.textContent = data.SystemStatus == 1 ? 'Active' : 'Inactive'; // Set the status text
            }

            // Update lamp status
            if (data.hasOwnProperty('LampStatus')) {
                const relayToggle = document.getElementById('lamp-toggle');
                const relayStatus = document.getElementById('lamp-status');
                
                relayToggle.checked = data.LampStatus === 1;  // Assuming lamp_status 1 = On, 0 = Off
                relayStatus.textContent = data.LampStatus === 1 ? 'On' : 'Off'; // Set the status text
            }
            if (data.hasOwnProperty('Credit')) {
                document.getElementById('sim-credit').textContent = data.Credit + " Rials";
            }
        })
        .catch(error => {
            console.error('Error fetching system state:', error);
        });
}

// Call the function to fetch the system state when the page loads
document.addEventListener('DOMContentLoaded', function() {
    fetchSystemState();
});


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