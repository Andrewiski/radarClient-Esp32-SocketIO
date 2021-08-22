#define useOle false
#define useOleLarge false
#define useSpeedIn true
#define useSpeedOut false
#define speedInAddress 0x70
#define speedOutAddress 0x71
#define useAlphaNum false
#define useBattery false

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
#if useOle 
  #if useOleLarge
    #include <Adafruit_SH110X.h>
    #include <Adafruit_FeatherOLED_SH110X.h>
    #include <Adafruit_FeatherOLED_SH110X_WiFi.h>
  #else
    #include <Adafruit_SSD1306.h>
    #include <Adafruit_FeatherOLED.h>
    #include <Adafruit_FeatherOLED_WiFi.h>
  #endif
 
#endif


#include <Arduino_CRC32.h>
#include <AutoConnect.h>
//#include <AutoConnectCredential.h>
#include <Preferences.h>




#if useAlphaNum
  #define speedLedType Adafruit_AlphaNum4
#else
  #define speedLedType Adafruit_7segment 
#endif
//
#define USE_SERIAL Serial


Preferences preferences;

String radarServerHostName =  "10.100.22.45";  //"10.100.6.36";
int radarServerPortNumber = 8080; //12336,
String APSSID = "";
WebServer Server;
AutoConnect portal(Server);
AutoConnectConfig Config;
AutoConnectCredential Credential;

#if useSpeedIn
speedLedType speedIn = speedLedType();
#endif

#if useSpeedOut
speedLedType speedOut = speedLedType();
#endif




//WiFiMulti WiFiMulti;
SocketIoClient webSocket;


#define BUTTON_A 15
#define BUTTON_B 32
#define BUTTON_C 14
#define BUILTIN_LED 13

#if useOle
 #if useOleLarge
    Adafruit_FeatherOLED_SH110X_WiFi oled;
  #else
    Adafruit_FeatherOLED_WiFi oled;
  #endif
  
#endif

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
  #if useOle
    oled.clearDisplay();
    oled.setTextSize(1);
    #if useOleLarge           
        oled.setTextColor(SH110X_WHITE);         
    #else           
      oled.setTextColor(WHITE);         
    #endif
    oled.setCursor(0,0);
    oled.setTextSize(1);
    oled.println("Connected to");
    oled.println("Radar Server");
    oled.println(radarServerHostName + ":" +  String(radarServerPortNumber));
    oled.display();
    clearOledDisplay = true;
  #endif
}

void socketDisconnectEvent(const char * payload, size_t length){
  USE_SERIAL.println("disconnectEvent Socket Disconnected");
  #if useOle
    oled.clearDisplay();
    oled.setTextSize(1);
    #if useOleLarge           
        oled.setTextColor(SH110X_WHITE);         
    #else           
      oled.setTextColor(WHITE);         
    #endif
    
    oled.setCursor(0,0);
    oled.println("Disconnected from");
    oled.println("Radar Server");
    oled.display();
    clearOledDisplay = true;
  #endif
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
    #if useSpeedIn
      USE_SERIAL.printf("Write SpeedIn %f \n" , inMaxSpeed);
      displayWriteFloat(speedIn,inMaxSpeed,2);
    #endif
    #if useSpeedOut
      USE_SERIAL.printf("Write SpeedOut %f \n" , outMaxSpeed);
      displayWriteFloat(speedOut,outMaxSpeed,2);
    #endif
    //speedIn.print(inMaxSpeed);
    //speedIn.blinkRate(2);
    //speedOut.print(outMaxSpeed);
    //speedOut.blinkRate(2);
    //speedIn.writeDisplay();
    //speedOut.writeDisplay();
    lastLedUpdate = millis();
    //USE_SERIAL.printf("[RadarSpeed] In %f Out  %f...\n", inMaxSpeed, outMaxSpeed );
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
    #if useSpeedIn
      #if useAlphaNum
        speedIn.writeDigitRaw(0, 0x3FFF);
        speedIn.writeDigitRaw(1, 0x3FFF);
        speedIn.writeDigitRaw(2, 0x3FFF);
        speedIn.writeDigitRaw(3, 0x3FFF);
      #else
        speedIn.writeDigitNum(0, 8, true);
        speedIn.writeDigitNum(1, 8, true);
        speedIn.drawColon(true);
        speedIn.writeDigitNum(3, 8, true);
        speedIn.writeDigitNum(4, 8, true);
      #endif
      speedIn.writeDisplay();
    #endif
    #if useSpeedOut
      #if useAlphaNum
        speedOut.writeDigitRaw(0, 0x3FFF);
        speedOut.writeDigitRaw(1, 0x3FFF);
        speedOut.writeDigitRaw(2, 0x3FFF);
        speedOut.writeDigitRaw(3, 0x3FFF);
        speedOut.writeDisplay();
      #else
        speedIn.writeDigitNum(0, 8, true);
        speedIn.writeDigitNum(1, 8, true);
        speedIn.drawColon(true);
        speedIn.writeDigitNum(3, 8, true);
        speedIn.writeDigitNum(4, 8, true);
      #endif
    #endif
}

//on Quad fix 5 0b0000000011101101, // 5

void displayWriteFloat(speedLedType ledDisplay, float speedValue,  int blinkRate){
  char buff[10];
  snprintf (buff, sizeof(buff), "%f", speedValue);
  int offset = 0;
  #if useAlphaNum
  for (int a=0; a<5; a++) {
    if(buff[a+1] == '.'){
      ledDisplay.writeDigitAscii(a-offset,buff[a], true);
      offset++;
    }else if(buff[a] != '.'){
      ledDisplay.writeDigitAscii(a-offset,buff[a], false);
    }
  }
  #else
    ledDisplay.print(speedValue);
  #endif
  ledDisplay.blinkRate(blinkRate);
  ledDisplay.writeDisplay();
}

bool atDetect(IPAddress ip) {
  
  Serial.println("A.P. started, IP:" + ip.toString());
  #if useOle
    oled.clearDisplay();
    oled.println("AP started:");
    oled.println(ip.toString());
    oled.println("SSID:");
    oled.println(APSSID);
    oled.display();
  #endif
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
    #if useOle
      pinMode(BUTTON_A, INPUT_PULLUP);
      pinMode(BUTTON_B, INPUT_PULLUP);
      pinMode(BUTTON_C, INPUT_PULLUP);
    #endif
    pinMode(BUILTIN_LED, OUTPUT);
    digitalWrite(BUILTIN_LED, LOW);
    //Wire.begin(23,22);

    #if useAlphaNum 
      USE_SERIAL.printf("Using Alpha Numeric Led \n");   
    #else
      USE_SERIAL.printf("Using 7 Seg Numeric Led \n");        
    #endif
    

    #if useSpeedIn 
      USE_SERIAL.printf("SpeedIn I2C Address %d...\n", speedInAddress);   
      speedIn.begin(speedInAddress);
    #endif
    #if useSpeedOut 
      USE_SERIAL.printf("SpeedOut I2C Address %d...\n", speedInAddress);
      speedOut.begin(speedOutAddress);
    #endif
    writeLedTestPattern();
    
    #if useOle
      #if useOleLarge
        oled = Adafruit_FeatherOLED_SH110X_WiFi(&Wire, -1);
      #else
        oled = Adafruit_FeatherOLED_WiFi(&Wire, -1);
      #endif
      
    #endif
    
    //oled.display();
    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }
    #if useSpeedIn
      speedIn.clear();
      speedIn.setBrightness(15);
      speedIn.writeDisplay();
    #endif
    #if useSpeedOut
      speedOut.clear();
      speedIn.setBrightness(15);
      speedOut.writeDisplay();
    #endif
    

    #if useOle
      oled.init();
      oled.clearDisplay();
      oled.display();
    #endif
    
    webSocket.on("radarSpeed", radarSpeedEvent);
    webSocket.on("batteryVoltage", radarBatteryVoltageEvent);
    webSocket.on("radarConfigProperty", radarConfigPropertyEvent); 
    webSocket.on("connect", socketConnectEvent);
    webSocket.on("disconnect", socketDisconnectEvent);
    
    
    //Credential = AutoConnectCredential();
    Config.boundaryOffset = 256;
    Config.autoReset = false;     // Not reset the module even by intentional disconnection using AutoConnect menu.
    Config.autoReconnect = true;  // Reconnect to known access points.
    Config.reconnectInterval = 6; // Reconnection attempting interval is 3[min].
    Config.retainPortal = true; 
    
    uint64_t chipid = ESP.getEfuseMac();
    APSSID = "radarDisplay" + String((uint32_t)chipid, HEX);
    Config.apid = APSSID;
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
      #if useOle
        oled.clearDisplay();
        oled.setTextSize(1);
        #if useOleLarge
          oled.setTextColor(SH110X_WHITE);
        #else
          oled.setTextColor(WHITE);
        #endif
        
        oled.setCursor(0,0);
        oled.println("IP:" + WiFi.localIP().toString());
        oled.println("Server: " + radarServerHostName + ":" +  String(radarServerPortNumber));
        oled.display();
      #endif
      Serial.println("Started, IP:" + WiFi.localIP().toString());
      Serial.println("Trying Connect to Radar Monitor: " + radarServerHostName + ":" +  String(radarServerPortNumber));
      webSocket.begin(radarServerHostName.c_str(), radarServerPortNumber,"/socket.io/?transport=websocket" );
    // use HTTP Basic Authorization this is optional remove if not needed
    //webSocket.setAuthorization("username", "password");
      periodicLoop();
      clearOledDisplay = true;
    }
    else {
      #if useOle
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.println("Connection failed.");
        oled.display();
      #endif
      Serial.println("Connection failed.");
      while (true) { yield(); }
    }
}

float getBatteryVoltage() {

    #if useBattery
      float measuredvbat = analogRead(A13);
      measuredvbat *= 2;    // we divided by 2, so multiply back
      measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
      measuredvbat *= 1.1;  // Multiply by 1.1V, the adC reference voltage
      measuredvbat /= 4098; // convert to voltage
    #else
      float measuredvbat = 5.55;
    #endif
    return measuredvbat;

  }





void periodicLoop(){


  if(count % 10 == 0) {
    float battery = getBatteryVoltage();
    if (battery <= 3.4){
      Serial.println("Low Battery Going to Deep Sleep.");
      ESP.deepSleep(0);
    }else if (battery <= 3.5){
      #if useOle
        oled.clearDisplay();
        oled.setBatteryVisible(true);
        oled.setBattery(battery);
        oled.renderBattery();
        oled.display();
      #endif
      lowLocalBattery = true;
      clearLedDisplay = true;
    }else if (battery <= 3.6){
      #if useOle
        oled.clearDisplay();
        oled.setBatteryVisible(true);
        oled.setBattery(battery);
        oled.renderBattery();
        oled.display();
      #endif
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
    #if useOle
      oled.setBatteryVisible(false);
      oled.setRSSIVisible(false);
      oled.setIPAddressVisible(false);
      oled.setConnectedVisible(false);
      oled.clearDisplay();
      // update the display 
      oled.display();
    #endif
    clearOledDisplay = false;
  }

  if (btnAClicked){
    #if useOle
      oled.clearDisplay();
      oled.setTextSize(1);
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
      oled.display();
    #endif
    
    btnAClicked = false;
    
    clearOledDisplay = true;
  }

  if (btnBClicked){
    #if useOle
      oled.clearDisplay();
      // get the current voltage of the battery from
      // one of the platform specific functions below 
      oled.setTextSize(1);
      #if useOleLarge           
        oled.setTextColor(SH110X_WHITE);         
      #else           
        oled.setTextColor(WHITE);         
      #endif
      oled.setCursor(0,0);
      oled.println("Server: " + radarServerHostName + ":" +  String(radarServerPortNumber));
      oled.println("Volts: " +  String(serverBatteryVoltage));
      oled.println("Connected");
      oled.display();
    #endif
    btnBClicked = false;
    clearOledDisplay = true;
  }

  if (btnCClicked){
    #if useOle
      oled.clearDisplay();
      oled.setTextSize(1);
      #if useOleLarge           
        oled.setTextColor(SH110X_WHITE);         
      #else           
        oled.setTextColor(WHITE);         
      #endif
      oled.setCursor(0,0);
      oled.println("Restarting Server Connection");
      oled.display();
    #endif
    restartWebsockets = true;
    btnCClicked = false;
    clearOledDisplay = true;
  }


  if (clearLedDisplay){
    #if useSpeedIn
      speedIn.blinkRate(0);
      speedIn.clear();
      speedIn.writeDisplay();
    #endif
    #if useSpeedOut
      speedOut.blinkRate(0);
      speedOut.clear();
      speedOut.writeDisplay();
    #endif
    clearLedDisplay = false;
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
    if (WiFi.status() != WL_CONNECTED) {
      USE_SERIAL.println("Wifi is disconnected");
      #if useOle
        oled.clearDisplay();
        oled.setTextSize(1);
        #if useOleLarge           
          oled.setTextColor(SH110X_WHITE);         
        #else           
          oled.setTextColor(WHITE);         
        #endif
        oled.setCursor(0,0);
        oled.println("SSID:");
        oled.println(APSSID);
        oled.println("Wifi is disconnected");
        oled.display();
      #endif
      //ESP.restart();
      delay(1000);
    }
    webSocket.loop();

    unsigned long currentMillis = millis();
    
    if(lastLedUpdate > 0){
      if((currentMillis - lastLedUpdate) > ledBlinkInterval)  // time to turn off blink
      {
        lastLedUpdate = 0;
        #if useSpeedIn
          speedIn.blinkRate(0);
        #endif
        #if useSpeedOut
          speedOut.blinkRate(0);
        #endif
      }
    }
    
    if((currentMillis - lastPeriodicLoop) > periodicLoopInterval)  // time to run periodicLoop
      {
        
        periodicLoop();
        USE_SERIAL.println("periodicLoop");
        lastPeriodicLoop = millis();
      }
    #if useOle
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
     #endif
}
