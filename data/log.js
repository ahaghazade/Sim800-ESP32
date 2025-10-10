let isUserScrolling = false;
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
function onLoad(event) {
    initWebSocket();
}
// Event listener for receiving messages via WebSocket
function onMessage(event) {
    console.log("new_readings[Page1]", event.data);
    const data = JSON.parse(event.data);
    if (data.Log) {
        addLog(data.Log);
    }
    else if(data.Time)
    {
        const TimeStamp = data.Time;

        // Replace "50" in the HTML with the fetched log number
        const logTextElement = document.querySelector('.log-container p2');
        if (logTextElement) {
            logTextElement.innerHTML = `<b>Time:</b> ${TimeStamp}`;
        }
    }
};

const logList = document.getElementById('log-list');
const maxLogs = 50; // Maximum number of logs to display
let logs = [];

// Function to add a log to the display
function addLog(message) {
    logs.push(message);

    // Remove the oldest log if more than maxLogs
    if (logs.length > maxLogs) {
        logs.shift(); // Remove the oldest log from the beginning
    }

    // Update the log list display
    updateLogDisplay();
    
    // Auto-scroll to bottom if the user is not actively scrolling
    if (!isUserScrolling) {
        logList.scrollTop = logList.scrollHeight;
    }
}

// Function to update the log list in the DOM
function updateLogDisplay() {
    logList.innerHTML = ''; // Clear the current logs

    // Append each log to the log list
    logs.forEach(log => {
        const logItem = document.createElement('div');
        logItem.className = 'log-item';
        logItem.textContent = log;
        logList.appendChild(logItem);
    });
}


document.addEventListener('DOMContentLoaded', function() {

    // Get the setting button element
    const settingButton = document.getElementById('setting-btn');
    // Add a click event listener to the log button for redirection
    settingButton.addEventListener('click', function() {
        // Redirect to the /update route
        window.location.href = '/setting';
    });

    // Get the setting button element
    const sensorsButton = document.getElementById('sensors-btn');
    // Add a click event listener to the log button for redirection
    sensorsButton.addEventListener('click', function() {
        // Redirect to the /update route
        window.location.href = '/sensors';
    });

    const logutton = document.getElementById('log-btn');
    logutton.addEventListener('click', function() {
        window.location.href = '/log';
    });

    const clockButton = document.getElementById('clock-btn');
    // Add a click event listener to the log button for redirection
    clockButton.addEventListener('click', function() {
        // Redirect to the /update route
        window.location.href = '/clock';
    });

    
    const logList = document.getElementById('log-list');

    // Detect user scrolling
    logList.addEventListener('scroll', function() {
        const maxScroll = logList.scrollHeight - logList.clientHeight;
        if (logList.scrollTop < maxScroll) {
            isUserScrolling = true;
        } else {
            isUserScrolling = false;
        }
    });


    fetch('/getlogs', {
        method: 'GET'
    })
    .then(response => response.json())
    .then(data => {
        const logList = document.getElementById('log-list');
        
        // Check if data contains logs
        if (data && data.logs && Array.isArray(data.logs)) {
            data.logs.forEach(log => {
                const logItem = document.createElement('div');
                logItem.className = 'log-item';
                logItem.textContent = log; // Assuming each log is a string
                logList.appendChild(logItem);
                addLog(log); // Assuming addLog is a function defined elsewhere
                logList.scrollTop = logList.scrollHeight; // Scroll to the bottom
            });
        } else {
            console.warn('No logs found in the response.');
        }
    })
    .catch(error => console.error('Error fetching logs:', error));
});

document.addEventListener("DOMContentLoaded", function() {
    // Fetch the number of logs from the server
    fetch('/LogNum')
    .then(response => response.json())
    .then(data => {
        // Get the number of logs from the response
        const logNum = data.LogNum;

        // Replace "50" in the HTML with the fetched log number
        const logTextElement = document.querySelector('.log-container p');
        if (logTextElement) {
            logTextElement.innerHTML = `Last ${logNum} logs & real-time logs`;
        }
    })
    .catch(error => {
        console.error("Error fetching log number:", error);
    });
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