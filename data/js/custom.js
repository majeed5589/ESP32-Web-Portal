let inputValuesSet = false;

document.getElementById("submit-values").addEventListener("click", function () {
    var minOxygen = document.getElementById("minOxygen").value;
    var maxOxygen = document.getElementById("maxOxygen").value;
    var minPulseRate = document.getElementById("minPulseRate").value;
    var maxPulseRate = document.getElementById("maxPulseRate").value;

    if (minOxygen <= 0 || maxOxygen <= 0 || minPulseRate <= 0 || maxPulseRate <= 0 || 
        minOxygen >= maxOxygen || minPulseRate >= maxPulseRate ||
        minOxygen == maxOxygen || minPulseRate == maxPulseRate) {
        alert("Invalid input values. Please check the ranges and ensure min and max values are not equal.");
        return;
    }

    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            alert(this.responseText);
            inputValuesSet = true;
        }
    };
    xhttp.open("POST", "/set/values", true);
    xhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xhttp.send(`minOxygen=${minOxygen}&maxOxygen=${maxOxygen}&minPulseRate=${minPulseRate}&maxPulseRate=${maxPulseRate}`);
});

document.getElementById("motor-toggle").addEventListener("click", function () {
    if (!inputValuesSet) {
        alert("Please enter valid values for oxygen and pulse rate before starting the motor.");
        return;
    }

    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            alert(this.responseText);
        }
    };
    xhttp.open("GET", "/toggle/motor", true);
    xhttp.send();
});

// Handle username submission and display welcome message
document.getElementById("submit-username").addEventListener("click", function () {
    var username = document.getElementById("username").value;
    if (username.trim() === "") {
        alert("Please enter a valid name.");
        return;
    }
    document.getElementById("welcome-message").textContent = `Welcome, ${username}!`;
    document.getElementById("username-form").style.display = "none";
});

// Function to update the digital display with displaySpeed
function updateDigitalDisplay() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            document.getElementById("digital-display").textContent = `${parseFloat(this.responseText)} RPM`;
        }
    };
    xhttp.open("GET", "/get_rpm", true);
    xhttp.send();
}

// Update the digital display every second
setInterval(updateDigitalDisplay, 1000);

// Handle warnings
function addWarningMessage(message) {
    var warningMessagesDiv = document.getElementById("warning-messages");
    var newWarning = document.createElement("div");
    newWarning.textContent = message;
    warningMessagesDiv.appendChild(newWarning);
}

// Function to receive and display warnings
function fetchWarnings() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            addWarningMessage(this.responseText);
        }
    };
    xhttp.open("GET", "/get/warning", true);
    xhttp.send();
}

// Polling for warnings every 5 seconds
setInterval(fetchWarnings, 5000);
