

//#include <Arduino.h>
#include <WiFi.h>
//#include <WiFiMulti.h>
//#include "EEPROM.h"
#include <SocketIoClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FeatherOLED.h>
#include <Adafruit_FeatherOLED_WiFi.h>
#include <Arduino_CRC32.h>
#include <AutoConnect.h>
//#include <AutoConnectCredential.h>
#include <Preferences.h>

Preferences preferences;

String radarServerHostName =  "10.100.22.45";  //"10.100.6.36";
int radarServerPortNumber = 8080; //12336,

WebServer Server;
AutoConnect portal(Server);
AutoConnectConfig Config;
AutoConnectCredential Credential;
//#define speedLedType Adafruit_7segment 
#define speedLedType Adafruit_AlphaNum4

speedLedType speedIn = speedLedType();

speedLedType speedOut = speedLedType();

#define USE_SERIAL Serial


//WiFiMulti WiFiMulti;
SocketIoClient webSocket;


#define BUTTON_A 15
#define BUTTON_B 32
#define BUTTON_C 14
#define BUILTIN_LED 13
Adafruit_FeatherOLED_WiFi oled;
//Adafruit_FeatherOLED oled;
//Adafruit_SSD1306 oled;
// integer variable to hold current counter value
int count = 0;


unsigned long lastLedUpdate; // last update of Led used to turn off Blinking after an update
int  ledBlinkInterval = 2000;

unsigned long lastPeriodicLoop; // last time we ran PeriodicLoop
int  periodicLoopInterval = 10000;

bool btnAPressed = false;
bool btnAReleased = true;
bool btnAClicked = false;

bool btnBPressed = false;
bool btnBReleased = true;
bool btnBClicked = false;

bool btnCPressed = false;
bool btnCReleased = true;
bool btnCClicked = false;

bool clearOledDisplay = false;
bool clearLedDisplay = false;
bool lowLocalBattery = false;
bool restartWebsockets = false;
float serverBatteryVoltage = 0.00;
void handleRoot() {
  String page = PSTR(
"<html>"
"<head>"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<script type=\"text/javascript\">"
  "function updateConfig(){"
  "var url=\"/config?\";"
   "url += \"serverHostName=\" + document.getElementById(\"serverHostName\").value;"
   "url += \"&serverPort=\" + document.getElementById(\"serverPort\").value;"
   "url += \"&password=\" + document.getElementById(\"password\").value;"
   "window.location.href = url;"
  "}"
  "</script>"
  "<style type=\"text/css\">"
    "body {"
    "-webkit-appearance:none;"
    "-moz-appearance:none;"
    "font-family:'Arial',sans-serif;"
    "text-align:center;"
    "}"
    ".menu > a:link {"
    "position: absolute;"
    "display: inline-block;"
    "right: 12px;"
    "padding: 0 6px;"
    "text-decoration: none;"
    "}"
    ".button {"
    "display:inline-block;"
    "border-radius:7px;"
    "background:#73ad21;"
    "margin:0 10px 0 10px;"
    "padding:10px 20px 10px 20px;"
    "text-decoration:none;"
    "color:#000000;"
    "}"
  "</style>"
"</head>"
"<body>"
  "<div class=\"menu\">" AUTOCONNECT_LINK(BAR_32) "</div>"
  "Radar Server Config<br>"
  "Server :");
  
  page += String(F(" <input id=\"serverHostName\" value=\""));
  page += radarServerHostName;
  page += String(F("\"/><br/>"));
  page += String(F(" Port <input id=\"serverPort\" value=\""));
  page += String(radarServerPortNumber);
  page += String(F("\"/><br/>"));
  page += String(F(" Password <input id=\"password\" type=\"password\" value=\"\"/><br/>"));
  page += String(F("<p><a class=\"button\" href=\"javascript:updateConfig();\">Save</a></p>"));
  page += String(F("</body></html>"));
  portal.host().send(200, "text/html", page);
}


void writeConfigData(){
  preferences.begin("radarConfig", false);
    //Limit 15 Char Name
  preferences.putString("ServerHostName", radarServerHostName);
  preferences.putInt("ServerPort", radarServerPortNumber);
  preferences.end();
}




void sendRedirect(String uri) {
  WebServerClass& server = portal.host();
  server.sendHeader("Location", uri, true);
  server.send(302, "text/plain", "");
  server.client().stop();
}

void handleConfigChange() {
  WebServerClass& server = portal.host();
  if (server.arg("password") == "radar"){
    radarServerHostName = server.arg("serverHostName");
    radarServerPortNumber = server.arg("serverPort").toInt();
    writeConfigData();
    sendRedirect("/?save=success");
    restartWebsockets = true;
  }else{
    sendRedirect("/?save=fail&msg=Bad%20Password");
  }
  
}

void socketConnectEvent(const char * payload, size_t length){
  USE_SERIAL.println("connectEvent Socket Connected");
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,0);
  oled.println("Connected to");
  oled.println("Radar Server");
  oled.println(radarServerHostName + ":" +  String(radarServerPortNumber));
  oled.display();
  clearOledDisplay = true;
}

void socketDisconnectEvent(const char * payload, size_t length){
  USE_SERIAL.println("disconnectEvent Socket Disconnected");
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,0);
  oled.println("Disconnected from");
  oled.println("Radar Server");
  oled.display();
  clearOledDisplay = true;
}

void radarSpeedEvent(const char * payload, size_t length) {
  USE_SERIAL.printf("got message: %s\n", payload);
   // Deserialize the JSON document
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    USE_SERIAL.print(F("deserializeJson() failed: "));
    USE_SERIAL.println(error.c_str());
  }else{
    float inMaxSpeed = doc["inMaxSpeed"];
    float outMaxSpeed = doc["outMaxSpeed"];
    displayWriteFloat(speedIn,inMaxSpeed,2);
    displayWriteFloat(speedOut,outMaxSpeed,2);
    //speedIn.print(inMaxSpeed);
    //speedIn.blinkRate(2);
    //speedOut.print(outMaxSpeed);
    //speedOut.blinkRate(2);
    //speedIn.writeDisplay();
    //speedOut.writeDisplay();
    lastLedUpdate = millis();
    USE_SERIAL.printf("[RadarSpeed] In %f Out  %f...\n", inMaxSpeed, outMaxSpeed );
    USE_SERIAL.flush();
    
  }
}


void radarConfigPropertyEvent(const char * payload, size_t length) {
  USE_SERIAL.printf("got message: %s\n", payload);
   // Deserialize the JSON document
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    USE_SERIAL.print(F("deserializeJson() failed: "));
    USE_SERIAL.println(error.c_str());
  }else{

    if ( doc["Property"] == "TransmiterControl"){
        if ( doc["data"] == 0){
            USE_SERIAL.println("[TransmiterControl] radar powered off");
            clearLedDisplay = true;
        }else{
          USE_SERIAL.println("[TransmiterControl] radar powered on");
          writeLedTestPattern();
          clearLedDisplay = true;
        }
    }
  }
}

void radarBatteryVoltageEvent(const char * payload, size_t length) {
  USE_SERIAL.printf("got message: %s\n", payload);
   // Deserialize the JSON document
  StaticJsonDocument<100> doc;
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    USE_SERIAL.print(F("deserializeJson() failed: "));
    USE_SERIAL.println(error.c_str());
  }else{
    serverBatteryVoltage =  doc["batteryVoltage"];
    
  }
}
//batteryVoltage

void writeLedTestPattern(){
   speedIn.writeDigitRaw(0, 0x3FFF);
    speedIn.writeDigitRaw(1, 0x3FFF);
    speedIn.writeDigitRaw(2, 0x3FFF);
    speedIn.writeDigitRaw(3, 0x3FFF);
    speedIn.writeDisplay();
    speedOut.writeDigitRaw(0, 0x3FFF);
    speedOut.writeDigitRaw(1, 0x3FFF);
    speedOut.writeDigitRaw(2, 0x3FFF);
    speedOut.writeDigitRaw(3, 0x3FFF);
    speedOut.writeDisplay();
}

//on Quad fix 5 0b0000000011101101, // 5

void displayWriteFloat(speedLedType ledDisplay, float speedValue,  int blinkRate){
  char buff[10];
  snprintf (buff, sizeof(buff), "%f", speedValue);
  int offset = 0;
  
  for (int a=0; a<5; a++) {
    if(buff[a+1] == '.'){
      ledDisplay.writeDigitAscii(a-offset,buff[a], true);
      offset++;
    }else if(buff[a] != '.'){
      ledDisplay.writeDigitAscii(a-offset,buff[a], false);
    }
  }
  ledDisplay.blinkRate(blinkRate);
  ledDisplay.writeDisplay();
}

bool atDetect(IPAddress ip) {
  
  Serial.println("C.P. started, IP:" + ip.toString());
  oled.clearDisplay();
  oled.println("AP started: " + ip.toString());
  oled.display();
  return true;
}



void setup() {
    USE_SERIAL.begin(115200);

    USE_SERIAL.setDebugOutput(true);
    USE_SERIAL.println("Starting ..." );  
    preferences.begin("radarConfig", true);
    //Limit 15 Char Name
    radarServerHostName = preferences.getString("ServerHostName", radarServerHostName);
    radarServerPortNumber = preferences.getInt("ServerPort", radarServerPortNumber);
    preferences.end();
    USE_SERIAL.println();
    USE_SERIAL.println();
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);
    pinMode(BUILTIN_LED, OUTPUT);
    digitalWrite(BUILTIN_LED, LOW);
    //Wire.begin(23,22);

        
    speedIn.begin(0x70);
    speedOut.begin(0x71);
    writeLedTestPattern();
    
    //oled = Adafruit_SSD1306(128, 32, &Wire, -1);
    //oled = Adafruit_FeatherOLED(&Wire, -1);
    oled = Adafruit_FeatherOLED_WiFi(&Wire, -1);
    //oled.display();
    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }
    speedIn.clear();
    speedIn.writeDisplay();
    speedOut.clear();
    speedOut.writeDisplay();
    //oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    
    oled.init();
    
    oled.clearDisplay();
    oled.display();
    
    
    webSocket.on("radarSpeed", radarSpeedEvent);
    webSocket.on("batteryVoltage", radarBatteryVoltageEvent);
    webSocket.on("radarConfigProperty", radarConfigPropertyEvent); 
    webSocket.on("connect", socketConnectEvent);
    webSocket.on("disconnect", socketDisconnectEvent);
    
    
    //Credential = AutoConnectCredential();
    Config.boundaryOffset = 256;
    Config.autoReconnect = true;
    
    uint64_t chipid = ESP.getEfuseMac();
    Config.apid = "radarDisplay" + String((uint32_t)chipid, HEX);
    Config.psk = "";
    
    //Config.ticker = true;
    //Config.tickerPort = BUILTIN_LED;
    //Config.tickerOn = HIGH;
    //Config.autoSave = AC_SAVECREDENTIAL_NEVER;
    portal.config(Config);
    portal.onDetect(atDetect);
    if (portal.begin()) {
      Server.on("/", handleRoot);
      Server.on("/config", handleConfigChange);
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(0,0);
      oled.println("IP:" + WiFi.localIP().toString());
      oled.println("Server: " + radarServerHostName + ":" +  String(radarServerPortNumber));
      oled.display();
      Serial.println("Started, IP:" + WiFi.localIP().toString());
      Serial.println("Trying Connect to Radar Monitor: " + radarServerHostName + ":" +  String(radarServerPortNumber));
      webSocket.begin(radarServerHostName.c_str(), radarServerPortNumber,"/socket.io/?transport=websocket" );
    // use HTTP Basic Authorization this is optional remove if not needed
    //webSocket.setAuthorization("username", "password");
      periodicLoop();
      clearOledDisplay = true;
    }
    else {
      oled.clearDisplay();
      oled.println("Connection failed.");
      oled.display();
      Serial.println("Connection failed.");
      while (true) { yield(); }
    }
}

float getBatteryVoltage() {

    float measuredvbat = analogRead(A13);

    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 4098; // convert to voltage
    return measuredvbat;

  }





void periodicLoop(){


  if(count % 10 == 0) {
    float battery = getBatteryVoltage();
    if (battery <= 3.4){
      Serial.println("Low Battery Going to Deep Sleep.");
      ESP.deepSleep(0);
    }else if (battery <= 3.5){
      oled.clearDisplay();
      oled.setBatteryVisible(true);
      oled.setBattery(battery);
      oled.renderBattery();
      oled.display();
      lowLocalBattery = true;
      clearLedDisplay = true;
    }else if (battery <= 3.6){
      oled.clearDisplay();
      oled.setBatteryVisible(true);
      oled.setBattery(battery);
      oled.renderBattery();
      oled.display();
      lowLocalBattery = true;
      
    }
    else{  //we Got Power
      if(lowLocalBattery == true){
        lowLocalBattery = false;
        clearLedDisplay = true;
      }
    }
    
  }

  if(clearOledDisplay){
    oled.setBatteryVisible(false);
    oled.setRSSIVisible(false);
    oled.setIPAddressVisible(false);
    oled.setConnectedVisible(false);
    oled.clearDisplay();
    // update the display 
    oled.display();
    clearOledDisplay = false;
  }

  if (btnAClicked){
      oled.clearDisplay();
    // get the current voltage of the battery from
    // one of the platform specific functions below
    oled.setBatteryVisible(true);
    oled.setRSSIVisible(true);
    oled.setRSSI(WiFi.RSSI());
    oled.setIPAddressVisible(true);
    oled.setIPAddress(WiFi.localIP());
    //oled.println("IP :" + WiFi.localIP().toString() );
    oled.setConnectedVisible(true);
    oled.setConnected(true);
    float battery = getBatteryVoltage();
    // update the battery icon
    oled.setBattery(battery);
    oled.renderBattery();
    oled.refreshIcons();
    btnAClicked = false;
    oled.display();
    clearOledDisplay = true;
  }

  if (btnBClicked){
      oled.clearDisplay();
    // get the current voltage of the battery from
    // one of the platform specific functions below 
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0);
    oled.println("Server: " + radarServerHostName + ":" +  String(radarServerPortNumber));
    oled.println("Volts: " +  String(serverBatteryVoltage));
    
    oled.println("Connected");
    btnBClicked = false;
    oled.display();
    clearOledDisplay = true;
  }

  if (btnCClicked){
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0);
    oled.println("Restarting Server Connection");
    restartWebsockets = true;
    btnCClicked = false;
    oled.display();
    clearOledDisplay = true;
  }


  if (clearLedDisplay){
    speedIn.blinkRate(0);
    speedOut.blinkRate(0);
    clearLedDisplay = false;
    speedIn.clear();
    speedOut.clear();
    speedIn.writeDisplay();
    speedOut.writeDisplay();
  }

  if(restartWebsockets){
    webSocket.disconnect();
    webSocket.begin(radarServerHostName.c_str(), radarServerPortNumber,"/socket.io/?transport=websocket" );
    restartWebsockets = false;
  }
  
  // increment the counter by 1
  count++;
  lastPeriodicLoop = millis();
}


void loop() {
    portal.handleClient();
    if (WiFi.status() == WL_IDLE_STATUS) {
      USE_SERIAL.println("Wifi is idle restarting");
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(0,0);
      oled.println("Wifi is idle restarting");
      oled.display();
      ESP.restart();
      delay(1000);
    }
    webSocket.loop();

    unsigned long currentMillis = millis();
    
    if(lastLedUpdate > 0){
      if((currentMillis - lastLedUpdate) > ledBlinkInterval)  // time to turn off blink
      {
        lastLedUpdate = 0;
        speedIn.blinkRate(0);
        speedOut.blinkRate(0);
      }
    }
    
    if((currentMillis - lastPeriodicLoop) > periodicLoopInterval)  // time to run periodicLoop
      {
        
        periodicLoop();
        USE_SERIAL.println("periodicLoop");
        lastPeriodicLoop = millis();
      }
    if(btnAClicked == false && btnAPressed == false && btnAReleased == true && !digitalRead(BUTTON_A)){
      btnAPressed = true;
      btnAReleased = false;
    }else if(btnAClicked == false && btnAPressed == true && btnAReleased == false && digitalRead(BUTTON_A)){
      btnAPressed = false;
      btnAReleased = true;
      btnAClicked = true;
      USE_SERIAL.println("Button A");
      periodicLoop();
    }
    if(btnBClicked == false && btnBPressed == false && btnBReleased == true && !digitalRead(BUTTON_B)){
      btnBPressed = true;
      btnBReleased = false;
    }else if(btnBClicked == false && btnBPressed == true && btnBReleased == false && digitalRead(BUTTON_B)){
      btnBPressed = false;
      btnBReleased = true;
      btnBClicked = true;
      USE_SERIAL.println("Button B");
      periodicLoop();
    }
    
    if(btnCClicked == false && btnCPressed == false && btnCReleased == true && !digitalRead(BUTTON_C)){
      btnCPressed = true;
      btnCReleased = false;
    }else if(btnCClicked == false && btnCPressed == true && btnCReleased == false && digitalRead(BUTTON_C)){
      btnCPressed = false;
      btnCReleased = true;
      btnCClicked = true;
      USE_SERIAL.println("Button C");
      periodicLoop();
    }
}
