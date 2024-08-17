
#include <SPI.h>
#include <WiFiNINA.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include "arduino_secrets.h"

struct LogEntry {
  long time;
  String mode;
  String doorStatus;
  String temperature;
  int light;
  String reedTop;
  String reedBottom;
  String killSwitch;
};

const int NUM_LOG_ENTRIES = 25;
LogEntry events[NUM_LOG_ENTRIES];
int eventIdx = 0;

int LED1 = 2;
int LED2 = 3;
int LED3 = 4;
int LED4 = 5;

int TEMP1 = A3;

int M1 = 7;
int M2 = 8;
int PWM = 9;

int REED_TOP = 10; // top
int REED_BOTTOM = 11; // bottom

int SWITCH_OPEN = 13;
int SWITCH_CLOSE = 12;

int KILL_SWITCH = 6;

int DAYLIGHT_THRESHOLD = 75;
int NIGHTLIGHT_THRESHOLD = 10;
int DAYLIGHT_MODE = 1;
long DAYLIGHT_CHECK_INTERVAL = 20l*60l*1000l; // 20 mins in ms
//long DAYLIGHT_CHECK_INTERVAL = 1l*60l*1000l; // 1 min in ms
long DAYLIGHT_LAST_CHECK_TIME = 0;

int TIMER_MODE = 0;

char ssid[] = SECRET_SSID; // SSID (case sensitive)
char pass[] = SECRET_PASS; // password
int keyIndex = 0; // network key index number (WEP only)

String temperature = "";
float temperatureFloat = 0.0;
String doorStatus = "ok";
String operationMode = "";

int status = WL_IDLE_STATUS;
WiFiServer server(80);

// needs this library: https://github.com/milesburton/Arduino-Temperature-Control-Library

OneWire oneWire(TEMP1);
DallasTemperature sensors(&oneWire);

int DEBUG = 1;

void addLogEntry() {
  LogEntry ce = events[eventIdx];
  ce.time = millis();
  /*int open = digitalRead(SWITCH_OPEN);
  int close = digitalRead(SWITCH_CLOSE);
  if (open == LOW || close == LOW) {
    ce.mode = "manual";
  } else if (TIMER_MODE) {
    ce.mode = "timer";
  } else if (DAYLIGHT_MODE) {
    ce.mode = "photo";
  }*/
  ce.light = getDaylightStrength();
  if (killSwitchEngaged()) {
    ce.killSwitch = "on";
  } else {
    ce.killSwitch = "off";
  }
  if (reedEngaged(REED_TOP)) {
    ce.reedTop = "on";
  } else {
    ce.reedTop = "off";
  }
  if (reedEngaged(REED_BOTTOM)) {
    ce.reedBottom = "on";
  } else {
    ce.reedBottom = "off";
  }
  ce.doorStatus = String(doorStatus);
  ce.mode = String(operationMode);
  ce.temperature = getTemperature();
  events[eventIdx] = ce;
  eventIdx = (eventIdx + 1)%NUM_LOG_ENTRIES;
}

void setup() {
  sensors.begin();
  setupDoor();
  setupWifi();
}

void setupWifi() {
  if (DEBUG) Serial.begin(9600);
  while (!Serial);
  //delay(1000);
  digitalWrite(LED1, HIGH);
  while (WiFi.status() == WL_NO_MODULE) {
    if (DEBUG) Serial.println("Communication with WiFi module failed");
    digitalWrite(LED2, HIGH);
    delay(1000);
    digitalWrite(LED2, LOW);
    delay(4000);
  }
  digitalWrite(LED2, HIGH);
  if (DEBUG) Serial.println("WiFi module found");
  String fv = WiFi.firmwareVersion();
  if (fv != "1.0.0") {
    if (DEBUG) Serial.println("Please upgrade the firmware");
  }
  if (DEBUG) Serial.println("WiFi firmware ok");
  digitalWrite(LED3, HIGH);
  while (status != WL_CONNECTED) {
    if (DEBUG) Serial.print("Attempting to connect to network: ");
    if (DEBUG) Serial.println(ssid);
    if (DEBUG) Serial.print("Status: ");
    if (DEBUG) Serial.println(status);
    // print the network name (SSID);
    status = WiFi.begin(ssid, pass);
    digitalWrite(LED4, HIGH);
    delay(1000);
    digitalWrite(LED4, LOW);
    delay(9000);
  }
  digitalWrite(LED4, HIGH);
  server.begin();
  printWifiStatus();
}

void setupDoor() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  pinMode(PWM, OUTPUT);
  pinMode(REED_TOP, INPUT);
  pinMode(REED_BOTTOM, INPUT);
  pinMode(SWITCH_OPEN, INPUT_PULLUP);
  pinMode(SWITCH_CLOSE, INPUT_PULLUP);
  pinMode(KILL_SWITCH, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  pinMode(TEMP1, INPUT);
}

void loop() {
  doorLoop();
  wifiLoop();
}

void wifiLoop() {
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    if (DEBUG) Serial.println("new client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        if (DEBUG) Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            // the content of the HTTP response follows the header:
            client.println("<head><title>buff orpington</title><style>");
            client.println("body{font-size:16px;}");
            client.println("p{font-size:250%;line-height:1.2;}");
            client.println("td{font-size:250%;line-height:1.2;}");
            client.println("th{font-size:250%;line-height:1.2;}");
            client.println("h1{font-size:250%;}");
            client.println("input{font-size:101%;}");
            client.println("</style></head>");
            client.println("<html>");
            client.println("<h1>Chicken Door Manager V2.1.0</h1>");
            client.println("<p><table border='0'>");
            if (isDoorOpen()==1) {
              client.print("<tr><td>Door Status:</td><td><i>open</i></td></tr>");
            } else if (isDoorClosed()==1) {
              client.print("<tr><td>Door Status:</td><td><i>closed</i></td></tr>");
            } else {
              client.print("<tr><td>Door Status:</td><td><i>unknown</i></td></tr>");
            }
            if (DAYLIGHT_MODE == 1) {
              client.print("<tr><td>Photo Sensor:</td><td><i>active</i></td></tr>");
            } else {
              client.print("<tr><td>Photo Sensor:</td><td><i>disabled</i></td></tr>");
            }
            //
            client.print("<tr><td>Daylight Threshold:</td><td><i>");
            client.print(String(DAYLIGHT_THRESHOLD));
            client.print("</i></td></tr>");
client.print("<tr><td>&nbsp;</td><td><i>");
            client.print("<form action=\"/U\" method=\"GET\"><input type=\"text\" id=\"pt\" name=\"pt\"/><input type=\"submit\" value=\"update\"></form>");
            client.print("</i></td></tr>");
            //
            client.print("<tr><td>Nightlight Threshold:</td><td><i>");
            client.print(String(NIGHTLIGHT_THRESHOLD));
            client.print("</i></td></tr>");
client.print("<tr><td>&nbsp;</td><td><i>");
            client.print("<form action=\"/N\" method=\"GET\"><input type=\"text\" id=\"pt\" name=\"pt\"/><input type=\"submit\" value=\"update\"></form>");
            client.print("</i></td></tr>");
            //
            int lightStrength = getDaylightStrength();
            if (isDaylight()) {
client.print("<tr><td>Environment:</td><td><i>day [");
              client.print(String(lightStrength));
              client.print("]</i></td></tr>");
            } else if (isNightlight()) {
client.print("<tr><td>Environment:</td><td><i>night [");
              client.print(String(lightStrength));
              client.print("]</i></td></tr>");
            } else {
client.print("<tr><td>Environment:</td><td><i>changing [");
              client.print(String(lightStrength));
              client.print("]</i></td></tr>");
            }
            long delta = millis() - DAYLIGHT_LAST_CHECK_TIME;
            // abs() function seems buggy
            if (delta<0) {
              delta = -delta;
            }
            client.print("<tr><td>Photo Sensor Check TTL:</td><td><i>");
client.print(String(long(DAYLIGHT_CHECK_INTERVAL/(1000.0*60.0))-long(delta/(1000.0*60.0))));
            client.print(" min</i></td></tr>");
            //
            if (TIMER_MODE == 1) {
client.print("<tr><td>Timer:</td><td><i>active</i></td></tr>");
            } else {
client.print("<tr><td>Timer:</td><td><i>disabled</i></td></tr>");
            }
            if (getTimerStatus()) {
              client.print("<tr><td>Timer Status:</td><td><i>on</i></td></tr>");
            } else {
              client.print("<tr><td>Timer Status:</td><td><i>off</i></td></tr>");
            }
            client.print("<tr><td>System Status:</td><td><i>");
            client.print(doorStatus);
            client.print("</i></td></tr>");
            client.print("<tr><td>Operation Mode:</td><td><i>");
            client.print(operationMode);
            client.print("</i></td></tr>");
            //
            client.print("<tr><td>Kill Switch:</td><td><i>");
            if (killSwitchEngaged()) {
              client.print("on");
            } else {
              client.print("off");
            }
            client.print("</i></td></tr>");
            //
            client.print("<tr><td>Reed Top:</td><td><i>");
            if (reedEngaged(REED_TOP)) {
              client.print("on");
            } else {
              client.print("off");
            }
            client.print("</i></td></tr>");
            //
            client.print("<tr><td>Reed Bottom:</td><td><i>");
            if (reedEngaged(REED_BOTTOM)) {
              client.print("on");
            } else {
              client.print("off");
            }
            client.print("</i></td></tr>");
            //
            temperature = getTemperature();
client.print("<tr><td>Temperature:</td><td>");
            client.print(temperature);
            client.print(" C</td></tr>");
client.print("<tr><td>Uptime:</td><td><i>");
            client.print(String(long(millis()/(1000.0*60.0))));
            client.print(" min</i></td></tr>");
client.print("<tr><td>SSID:</td><td><i>");
            client.print(WiFi.SSID());
            client.print("</i></td></tr>");
client.print("<tr><td>IP:</td><td><i>");
            client.print(WiFi.localIP());
            client.print("</i></td></tr>");
            client.print("<tr><td>Signal Strength (RSSI):</td><td><i>");
            client.print(WiFi.RSSI());
            client.print("</i></td></tr>");
client.print("<tr><td>&nbsp;</td><td></td></tr>");
            client.print("<tr><td><a href=\"/H\">OPEN</a></td><td></td></tr>");
client.print("<tr><td>&nbsp;</td><td></td></tr>");
            client.print("<tr><td><a href=\"/L\">CLOSE</a></td><td></td></tr>");
client.print("<tr><td>&nbsp;</td><td></td></tr>");
            client.print("<tr><td><a href=\"/P\">TOGGLE PHOTO SENSOR</a></td><td></td></tr>");
client.print("<tr><td>&nbsp;</td><td></td></tr>");
            client.print("<tr><td><a href=\"/T\">TOGGLE TIMER</a></td><td></td></tr>");
client.print("<tr><td>&nbsp;</td><td></td></tr>");
            client.print("<tr><td><a href=\"/\">REFRESH</a></td><td></td></tr>");
            client.println("</table></p>");
            //
            client.println("<p><table border='1'>");
            client.print("<tr>");
client.print("<th></th><th>Time</th><th>Ago</th><th>Operation Mode</th><th>Door Status</th><th>Light</th><th></th><th>Temperature</th><th>Reed Top</th><th>Red Bottom</th><th>Kill Switch</th>");
            client.print("</tr>");
            for (int i=0; i<NUM_LOG_ENTRIES; i++) {
              if (events[i].time == 0) {
                continue;
              }
              client.print("<tr>");
              client.print("<td>"); client.print(String(i)); client.print("</td>");
              client.print("<td>"); client.print(String(events[i].time)); client.print("</td>");
              client.print("<td>"); client.print(getDeltaTimeString(events[i].time)); client.print("</td>");
              client.print("<td>"); client.print(events[i].mode); client.print("</td>");
              client.print("<td>"); client.print(events[i].doorStatus); client.print("</td>");
              client.print("<td>"); client.print(String(events[i].light)); client.print("</td>");
              if (events[i].light > DAYLIGHT_THRESHOLD) {
                client.print("<td>day</td>");
              } else if (events[i].light < DAYLIGHT_THRESHOLD) {
                client.print("<td>night</td>");
              } else {
                client.print("<td></td>");
              }
              client.print("<td>"); client.print(events[i].temperature); client.print("</td>");
              client.print("<td>"); client.print(events[i].reedTop); client.print("</td>");
              client.print("<td>"); client.print(events[i].reedBottom); client.print("</td>");
              client.print("<td>"); client.print(events[i].killSwitch); client.print("</td>");
              client.print("</tr>");
            }
            client.println("</table></p>");
            //
            client.println("</html>");
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
        if (currentLine.endsWith("GET /H")) {
          operationMode = "manual remote";
          openDoor();
        }
        if (currentLine.endsWith("GET /L")) {
          operationMode = "manual remote";
          closeDoor();
        }
        if (currentLine.endsWith("GET /P")) {
          DAYLIGHT_MODE = !DAYLIGHT_MODE;
          if (DAYLIGHT_MODE) {
            TIMER_MODE = 0;
          }
        }
        if (currentLine.endsWith("GET /T")) {
          TIMER_MODE = !TIMER_MODE;
          if (TIMER_MODE) {
            DAYLIGHT_MODE = 0;
          }
        }
        if (currentLine.indexOf("/U?pt=") > 0) {
          String val = currentLine.substring(currentLine.indexOf("/U?pt=")+6);
          DAYLIGHT_THRESHOLD = val.toInt();
        }
        if (currentLine.indexOf("/N?pt=") > 0) {
          String val = currentLine.substring(currentLine.indexOf("/N?pt=")+6);
          NIGHTLIGHT_THRESHOLD = val.toInt();
        }
      }
    }
    client.stop();
    if (DEBUG) Serial.println("client disonnected");
  }
}

void doorLoop() {
  delay(100);
  int open = digitalRead(SWITCH_OPEN);
  int close = digitalRead(SWITCH_CLOSE);
  if (close==LOW) {
    operationMode = "manual local";
    closeDoor();
  } else if (open==LOW) {
    operationMode = "manual local";
    openDoor();
  } else {
    // check photo sensor
    actDoorDaylight();
    // check timer
    actDoorTimer();
  }
}

void printWifiStatus() {
  if (!DEBUG) {
    return;
  }
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

int isDaylight() {
  int lightLevel = analogRead(A0);
  if (lightLevel > DAYLIGHT_THRESHOLD) {
    return 1;
  } else {
    return 0;
  }
}

int isNightlight() {
  int lightLevel = analogRead(A0);
  if (lightLevel < NIGHTLIGHT_THRESHOLD) {
    return 1;
  } else {
    return 0;
  }
}

int getDaylightStrength() {
  return analogRead(A0);
}

int getTimerStatus() {
  return !digitalRead(A5);
}

String getTemperature() {
  sensors.requestTemperatures();
  temperatureFloat = sensors.getTempCByIndex(0);
  temperature = String(temperatureFloat);
  return temperature;
}

int actDoorTimer() {
  if (TIMER_MODE == 0) {
    return 0;
  }
  operationMode = "timer";
  if (getTimerStatus() == 1) {
    openDoor();
    return 1;
  } else {
    closeDoor();
    return 0;
  }
}

String getDeltaTimeString(long ts) {
  long now = millis();
  long delta = now - ts;
  // abs() function sems buggy
  if (delta < 0) {
    delta = -delta;
  }
  long secs = delta / 1000;
  long mins = secs / 60;
  long hours = mins / 60;
  long days = hours / 24;
  if (days > 0) {
    return String(days) + " days";
  }
  if (hours > 0) {
    return String(hours) + " hours";
  }
  if (mins > 0) {
    return String(mins) + " minutes";
  }
  return String(secs) + " seconds";
}

int actDoorDaylight() {
  if (DAYLIGHT_MODE == 0) {
    return 0;
  }
  operationMode = "photosensor";
  long now = millis();
  long delta = now - DAYLIGHT_LAST_CHECK_TIME;
  // abs() function sems buggy
  if (delta < 0) {
    delta = -delta;
  }
  if (delta < DAYLIGHT_CHECK_INTERVAL) {
    return 0;
  }
  //blinkIn();
  DAYLIGHT_LAST_CHECK_TIME = now;
  if (isDaylight()) {
    openDoor();
    return 1;
  } else if (isNightlight()) {
    closeDoor();
    return -1;
  }
  return 0;
}

void motorRight() {
  analogWrite(PWM, 255);
  digitalWrite(M1, HIGH);
  digitalWrite(M2, LOW);
}

void motorLeft() {
  analogWrite(PWM, 255);
  digitalWrite(M1, LOW);
  digitalWrite(M2, HIGH);
}

void motorRightSpeed(int speed) {
  analogWrite(PWM, speed);
  digitalWrite(M1, HIGH);
  digitalWrite(M2, LOW);
}

void motorLeftSpeed(int speed) {
  analogWrite(PWM, speed);
  digitalWrite(M1, LOW);
  digitalWrite(M2, HIGH);
}

void motorStop() {
  digitalWrite(M1, LOW);
  digitalWrite(M2, LOW);
}

void motorFadeInLeft() {
  int pwm = 0;
  analogWrite(PWM, pwm);
  digitalWrite(M1, LOW);
  digitalWrite(M2, HIGH);
  for (int pwm=0; pwm<255; pwm++) {
    analogWrite(PWM, pwm);
    delay(10);
  }
  analogWrite(PWM, 255);
}

void motorFadeInRight() {
  int pwm = 0;
  analogWrite(PWM, pwm);
  digitalWrite(M1, HIGH);
  digitalWrite(M2, LOW);
  for (int pwm=0; pwm<255; pwm++) {
    analogWrite(PWM, pwm);
    delay(10);
  }
  analogWrite(PWM, 255);
}

void blinkDown() {
  for (int i=0; i<5; i++) {
    digitalWrite(LED1, HIGH);
    delay(30);
    digitalWrite(LED1, LOW);
    delay(30);
    digitalWrite(LED2, HIGH);
    delay(30);
    digitalWrite(LED2, LOW);
    delay(30);
    digitalWrite(LED3, HIGH);
    delay(30);
    digitalWrite(LED3, LOW);
    delay(30);
    digitalWrite(LED4, HIGH);
    delay(30);
    digitalWrite(LED4, LOW);
    delay(300);
  }
}

void blinkUp() {
  for (int i=0; i<5; i++) {
    digitalWrite(LED4, HIGH);
    delay(30);
    digitalWrite(LED4, LOW);
    delay(30);
    digitalWrite(LED3, HIGH);
    delay(30);
    digitalWrite(LED3, LOW);
    delay(30);
    digitalWrite(LED2, HIGH);
    delay(30);
    digitalWrite(LED2, LOW);
    delay(30);
    digitalWrite(LED1, HIGH);
    delay(30);
    digitalWrite(LED1, LOW);
    delay(300);
  }
}

void blinkIn() {
  for (int i=0; i<3; i++) {
    digitalWrite(LED4, HIGH);
    digitalWrite(LED1, HIGH);
    delay(60);
    digitalWrite(LED3, HIGH);
    digitalWrite(LED2, HIGH);
    delay(60);
    digitalWrite(LED3, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED4, LOW);
    digitalWrite(LED1, LOW);
    delay(60);
    delay(300);
  }
}

void blinkOut() {
  for (int i=0; i<5; i++) {
    digitalWrite(LED3, HIGH);
    digitalWrite(LED2, HIGH);
    delay(60);
    digitalWrite(LED3, LOW);
    digitalWrite(LED2, LOW);
    delay(60);
    digitalWrite(LED4, HIGH);
    digitalWrite(LED1, HIGH);
    delay(60);
    digitalWrite(LED4, LOW);
    digitalWrite(LED1, LOW);
    delay(60);
    delay(300);
  }
}

int killSwitchEngaged() {
  return !digitalRead(KILL_SWITCH);
}

int reedEngaged(int reedNum) {
  int reedStatus = digitalRead(reedNum);
  if (reedStatus == 1) {
    return 1;
  } else {
    return 0;
  }
}

int waitForReedOrKill(int reedNum) {
  int reedStatus;
  for (;;) {
    delay(5);
    reedStatus = digitalRead(reedNum);
    if (reedStatus == 1) {
      return 0;
    }
    int killSwitch = digitalRead(KILL_SWITCH);
    if (killSwitch==LOW) {
      return 1;
    }
  }
}

int waitForReed(int reedNum) {
  int reedStatus;
  for (;;) {
    delay(5);
    reedStatus = digitalRead(reedNum);
    if (reedStatus == 1) {
      return 0;
    }
  }
}

int isDoorOpen() {
  int reedStatus = digitalRead(REED_TOP);
  if (reedStatus == 1) {
    return 1;
  }
  return 0;
}

int isDoorClosed() {
  int reedStatus = digitalRead(REED_BOTTOM);
  if (reedStatus == 1) {
    return 1;
  }
  return 0;
}

void openDoor() {
  if (isDoorOpen()) {
    return;
  }
  blinkUp();
  long start = millis();
  int killCnt = 0;
  int motorState = 0; // 0 = stop, 1 = left, 2 = right
  motorLeftSpeed(255);
  motorState = 1;
  while (true) {
    delay(5); // without this we get wrong readings on kill switch status
    long now = millis();
    if ((now-start)/1000>30) {
      // timeout after 30 sec
      motorStop();
      doorStatus = "timeout on open";
      addLogEntry();
      return;
    }
    if (reedEngaged(REED_TOP)) {
      motorStop();
      doorStatus = "open";
      addLogEntry();
      return;
    }
    if (killSwitchEngaged()) {
      killCnt++;
      if (killCnt>=3) {
        motorStop();
        doorStatus = "jammed on open";
        addLogEntry();
        return;
      } else if (motorState == 1) {
        motorRightSpeed(255);
        motorState = 2;
        doorStatus = "reversed right";
        delay(400); // if kill switch time to disengage
      } else if (motorState == 2) {
        motorLeftSpeed(255);
        motorState = 1;
        doorStatus = "reversed left";
        delay(400); // if kill switch time to disengage
      } else {
        motorStop();
        doorStatus = "unknown malfunction";
        addLogEntry();
        return;
      }
    }
  }
  motorStop();
  doorStatus = "unknown malfunction";
  addLogEntry();
  return;
}

/*void openDoor() {
  if (isDoorOpen()) {
    return;
  }
  blinkUp();
  motorLeftSpeed(255);
  int status = waitForReedOrKill(REED_TOP);
  if (status == 1) {
    motorRightSpeed(255);
    waitForReed(REED_TOP);
  }
  motorStop();
}*/

/*void closeDoor() {
  if (isDoorClosed()) {
    return;
  }
  blinkDown();
  motorRightSpeed(255);
  int status = waitForReedOrKill(REED_BOTTOM);
  if (status == 1) {
    motorLeftSpeed(255);
    waitForReed(REED_BOTTOM);
  }
  delay(600);
  motorStop();
}*/

void closeDoor() {
  if (isDoorClosed()) {
    return;
  }
  blinkDown();
  long start = millis();
  int killCnt = 0;
  int motorState = 0; // 0 = stop, 1 = left, 2 = right
  motorRightSpeed(255);
  motorState = 2;
  while (true) {
    delay(5); // without this we get wrong readings on kill switch status
    long now = millis();
    if ((now-start)/1000>30) {
      // timeout after 30 sec
      motorStop();
      doorStatus = "timeout on close";
      addLogEntry();
      return;
    }
    if (reedEngaged(REED_BOTTOM)) {
      delay(600); // wait to engage locks
      motorStop();
      doorStatus = "closed";
      addLogEntry();
      return;
    }
    if (killSwitchEngaged()) {
      killCnt++;
      if (killCnt>=3) {
        motorStop();
        doorStatus = "jammed on close";
        addLogEntry();
        return;
      } else if (motorState == 1) {
        motorRightSpeed(255);
        motorState = 2;
        doorStatus = "reversed right";
        delay(400); // if kill switch time to disengage
      } else if (motorState == 2) {
        motorLeftSpeed(255);
        motorState = 1;
        doorStatus = "reversed left";
        delay(400); // if kill switch time to disengage
      } else {
        motorStop();
        doorStatus = "unknown malfunction";
        addLogEntry();
        return;
      }
    }
  }
  motorStop();
  doorStatus = "unknown malfunction";
  addLogEntry();
  return;
}

