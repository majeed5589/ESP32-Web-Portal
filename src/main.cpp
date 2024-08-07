#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <DHT.h>
#include <ESP32Servo.h>
#include <MAX30100_PulseOximeter.h>

// Replace with your network credentials
const char *ssid = "Shahid NET";
const char *password = "66554400";

AsyncWebServer server(80);
const int buzzer = 19;  
const int servoPin = 18; // Pin for servo control
bool motorState = false; // State to check if the motor should be running

#define REPORTING_PERIOD_MS 1000

PulseOximeter pox;
uint32_t tsLastReport = 0;
void onBeatDetected();

int minOxygen = 0;
int maxOxygen = 0;
int minPulseRate = 0;
int maxPulseRate = 0;
int motorSpeed = 0;
float displaySpeed;

bool inputValuesSet = false;
String customWarning = "";

// DHT sensor settings
DHT my_sensor(5, DHT22);  //Data pin for Temprature sensor

float temperature = 0.0;
float speedRPM;
float delayBetweenSteps;

// Servo motor settings
ESP32Servo myServo;
int currentServoPosition = 0; // Keep track of the current servo position

float calculateAverage(int minValue, int maxValue) {
    return (minValue + maxValue) / 2.0;
}

void moveServoBackAndForth(int delayBetweenSteps) {
    for (int pos = 0; pos <= 180; pos++) {
        myServo.write(pos);
        delay(delayBetweenSteps);
    }
    for (int pos = 180; pos >= 0; pos--) {
        myServo.write(pos);
        delay(delayBetweenSteps);
    }
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup() {
    Serial.begin(115200);
    pinMode(buzzer, OUTPUT);
    digitalWrite(buzzer, LOW);  // Initially turn off the Buzzer

// Configure the PulseOximeter

    Serial.println("Initializing Pulse Oximeter...");
    if (!pox.begin()) {
        Serial.println("FAILED");
        for(;;);
    } else {
        Serial.println("SUCCESS");
    }
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setOnBeatDetectedCallback(onBeatDetected);
    


    Serial.println("Connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("WiFi Failed!\n");
        return;
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");

    my_sensor.begin();

    myServo.attach(servoPin); // Attach the servo on the specified pin

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Requesting index page...");
        request->send(SPIFFS, "/index.html", "text/html");
    });

    server.on("/css/custom.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/css/custom.css", "text/css");
    });

    server.on("/js/custom.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/js/custom.js", "text/javascript");
    });

    server.on("/toggle/motor", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!inputValuesSet) {
            request->send(400, "text/plain", "Input values not set. Please enter valid values for oxygen and pulse rate.");
            return;
        }
        Serial.println("Received /toggle/motor request");
        motorState = !motorState;
        request->send(200, "text/plain", motorState ? "Motor is ON" : "Motor is OFF");
    });

    server.on("/set/values", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("minOxygen", true) && request->hasParam("maxOxygen", true) &&
            request->hasParam("minPulseRate", true) && request->hasParam("maxPulseRate", true)) {
            
            int minOxy = request->getParam("minOxygen", true)->value().toInt();
            int maxOxy = request->getParam("maxOxygen", true)->value().toInt();
            int minPulse = request->getParam("minPulseRate", true)->value().toInt();
            int maxPulse = request->getParam("maxPulseRate", true)->value().toInt();

            if (minOxy <= 0 || maxOxy <= 0 || minPulse <= 0 || maxPulse <= 0 || 
                minOxy > 100 || maxOxy > 100 || minPulse > 200 || maxPulse > 200 || 
                minOxy == maxOxy || minPulse == maxPulse) {
                request->send(400, "text/plain", "Invalid input values. Please check the ranges and ensure min and max values are not equal.");
                return;
            }

            minOxygen = minOxy;
            maxOxygen = maxOxy;
            minPulseRate = minPulse;
            maxPulseRate = maxPulse;

            float avgOxygen = calculateAverage(minOxygen, maxOxygen);
            float avgPulseRate = calculateAverage(minPulseRate, maxPulseRate);

            inputValuesSet = true;

            Serial.printf("Received values - Min Oxygen: %d, Max Oxygen: %d, Min Pulse Rate: %d, Max Pulse Rate: %d\n",
                minOxygen, maxOxygen, minPulseRate, maxPulseRate);
            Serial.printf("Average Oxygen: %.2f, Average Pulse Rate: %.2f\n", avgOxygen, avgPulseRate);

            request->send(200, "text/plain", "Values updated");
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    server.on("/set/warning", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("message", true)) {
            customWarning = request->getParam("message", true)->value();
            Serial.printf("Received warning message: %s\n", customWarning.c_str());
            request->send(200, "text/plain", "Warning message received");
        } else {
            request->send(400, "text/plain", "Missing message parameter");
        }
    });

    server.on("/get/warning", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", customWarning);
        customWarning = ""; // Clear warning after sending
    });

    // Endpoint to get the current motor speed in RPM
    server.on("/get_rpm", HTTP_GET, [](AsyncWebServerRequest *request) {
        String rpmString = String(displaySpeed);
        request->send(200, "text/plain", rpmString);
    });

    server.onNotFound(notFound);

    server.begin();
    Serial.println("Server started");
}

void loop() {

    // Read temperature from DHT22
    temperature = my_sensor.readTemperature();

    // Print the temperature to the Serial Monitor
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
    digitalWrite(buzzer, LOW); 

    if (motorState) {

        // Control the servo motor based on temperature
       
        if (temperature <= 34) {
            delayBetweenSteps = 0.1; // Fast speed
            moveServoBackAndForth(delayBetweenSteps);
            int stepsPerCycle = 360; // 180 degrees up and 180 degrees down
            float timePerCycle = stepsPerCycle * delayBetweenSteps / 1000.0; // Convert to seconds
            speedRPM = 60.0 / timePerCycle; // RPM calculation
            Serial.print("Motor Speed:");
            displaySpeed=speedRPM/20; 
            Serial.print(displaySpeed);
            Serial.println(" rpm");

        } else if (temperature >= 34 && temperature < 35) {
            delayBetweenSteps = 5; // Medium speed
            moveServoBackAndForth(delayBetweenSteps);
            int stepsPerCycle = 360; // 180 degrees up and 180 degrees down
            float timePerCycle = stepsPerCycle * delayBetweenSteps / 1000.0; // Convert to seconds
            speedRPM = 60.0 / timePerCycle; // RPM calculation
            Serial.print("Motor Speed:"); 
            displaySpeed=speedRPM;
            Serial.print(displaySpeed);
            Serial.println(" rpm"); 
        } else if (temperature >= 35) {
            displaySpeed=0;
            myServo.detach();
            customWarning = "Pulse Rate / Oxygen Level of user is not NORMAL. STOPPING MOTOR!!!";
            digitalWrite(buzzer, HIGH);  //  turn ON the Buzzer
            
        }

    } 
}
