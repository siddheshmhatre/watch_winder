#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AccelStepper.h>

const char* ssid = "ESP8266_Stepper_Control";
const char* password = "watchmeroll";

ESP8266WebServer server(80);

// Define stepper motor connections for ULN2003
#define FULLSTEP 4
#define STEPS_PER_REVOLUTION 2048  // 28BYJ-48 has 2048 steps per revolution

// Create AccelStepper instance
AccelStepper stepper(FULLSTEP, D1, D5, D2, D6);  // IN1, IN3, IN2, IN4

volatile bool motorRunning = false;
volatile bool motorDirection = true;
volatile bool singleStepMode = false;
volatile int stepsToMove = 0;
volatile int currentPosition = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize stepper
  stepper.setMaxSpeed(1000.0);
  stepper.setAcceleration(50.0);
  stepper.setSpeed(200);
  stepper.setCurrentPosition(0);

  // Set up WiFi Access Point
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Server routes
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/rotate", handleRotate);
  server.on("/direct", handleDirect);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  
  if (motorRunning) {
    if (singleStepMode) {
      if (stepsToMove > 0) {
        stepper.move(motorDirection ? 1 : -1);
        stepsToMove--;
      } else {
        motorRunning = false;
      }
    } else {
      stepper.runSpeed();
    }
  }
  stepper.run();
}

void handleRoot() {
  String html = R"====(
    <html>
    <head>
      <title>Watch Winder Control</title>
      <style>
        /* ... keep existing CSS ... */
      </style>
    </head>
    <body>
      <div class="container">
        <h2>Watch Winder Control</h2>
        <button class="button" onclick="controlMotor(1, 'cw')">Clockwise</button>
        <button class="button" onclick="controlMotor(1, 'ccw')">Anticlockwise</button>
        <button class="button" onclick="controlMotor(1, 'stop')">Stop</button>
        <br><br>
        <input type="range" min="0" max="360" value="0" class="slider" id="angleSlider1" oninput="updateAngleValue(1)">
        <span id="angleValue1">0</span> degrees
        <button class="button" onclick="rotateMotor(1)">Rotate</button>
      </div>
            <script>
        function controlMotor(motor, action) {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "/control?motor=" + motor + "&action=" + action, true);
          xhr.send();
        }
        
        function rotateMotor(motor) {
          var angle = document.getElementById("angleSlider" + motor).value;
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "/rotate?motor=" + motor + "&angle=" + angle, true);
          xhr.send();
        }
        
        function directControl(motor) {
          var angle = document.getElementById("directSlider" + motor).value;
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "/direct?motor=" + motor + "&angle=" + angle, true);
          xhr.send();
        }
        
        function updateAngleValue(motor) {
          var angle = document.getElementById("angleSlider" + motor).value;
          document.getElementById("angleValue" + motor).innerText = angle;
        }
        
        function updateDirectValue(motor) {
          var angle = document.getElementById("directSlider" + motor).value;
          document.getElementById("directValue" + motor).innerText = angle;
        }
      </script>
    </body>
    </html>
  )====";
  
  server.send(200, "text/html", html);
}

void handleControl() {
  if(server.hasArg("action")) {
    String action = server.arg("action");
    if (action == "cw") {
      motorRunning = true;
      motorDirection = true;
      singleStepMode = false;
      stepper.setSpeed(200);
    } else if (action == "ccw") {
      motorRunning = true;
      motorDirection = false;
      singleStepMode = false;
      stepper.setSpeed(-200);
    } else if (action == "stop") {
      motorRunning = false;
      stepper.stop();
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleRotate() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    if (angle >= 0 && angle <= 360) {
      stepsToMove = map(angle, 0, 360, 0, STEPS_PER_REVOLUTION);
      motorRunning = true;
      singleStepMode = true;
      stepper.moveTo(stepsToMove);
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleDirect() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    if (angle >= 0 && angle <= 360) {
      int targetPosition = map(angle, 0, 360, 0, STEPS_PER_REVOLUTION);
      stepper.moveTo(targetPosition);
      motorRunning = true;
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}