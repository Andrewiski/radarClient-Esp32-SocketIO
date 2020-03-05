

//#include <Arduino.h>
#include <WiFi.h>
//#include <WiFiMulti.h>

#include <SocketIoClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FeatherOLED.h>
#include <Adafruit_FeatherOLED_WiFi.h>

#include <AutoConnect.h>

String radarServerHostName =  "10.100.22.45";  //"10.100.6.36";
int radarServerPortNumber = 8080; //12336,

WebServer Server;
AutoConnect portal(Server);
AutoConnectConfig Config;
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

bool clearDisplay = false;

void connectEvent(const char * payload, size_t length){
  USE_SERIAL.println("connectEvent Wifi Connected");
  oled.clearDisplay();
  oled.println("Connected to Radar Server");
  oled.display();
  clearDisplay = true;
}

void disconnectEvent(const char * payload, size_t length){
  USE_SERIAL.println("disconnectEvent Wifi Disconnected");
  oled.clearDisplay();
  oled.println("Disconnected from Radar Server");
  oled.display();
  clearDisplay = true;
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
    USE_SERIAL.println();
    USE_SERIAL.println();
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    //Wire.begin(23,22);
    speedIn.begin(0x70);
    speedIn.writeDigitRaw(0, 0x3FFF);
    speedIn.writeDigitRaw(1, 0x3FFF);
    speedIn.writeDigitRaw(2, 0x3FFF);
    speedIn.writeDigitRaw(3, 0x3FFF);
    speedIn.writeDisplay();
    //displayWriteFloat(speedIn,8888,0);
    //Wire.begin()
    speedOut.begin(0x71);
    //displayWriteFloat(speedOut,8888,0);
    speedOut.writeDigitRaw(0, 0x3FFF);
    speedOut.writeDigitRaw(1, 0x3FFF);
    speedOut.writeDigitRaw(2, 0x3FFF);
    speedOut.writeDigitRaw(3, 0x3FFF);
    speedOut.writeDisplay();
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

    webSocket.on("connect", connectEvent);
    webSocket.on("disconnect", disconnectEvent);
    
    
    

    Config.autoReconnect = true;
    uint64_t chipid = ESP.getEfuseMac();
    Config.apid = "radarDisplay" + String((uint32_t)chipid, HEX);
    Config.psk = "";
    
    Config.ticker = true;
    Config.tickerPort = 13;
    Config.tickerOn = LOW;
    //Config.autoSave = AC_SAVECREDENTIAL_NEVER;
    portal.config(Config);
    portal.onDetect(atDetect);
    if (portal.begin()) {
      oled.clearDisplay();
      oled.println("IP:" + WiFi.localIP().toString());
      oled.println("Radar Monitor: " + radarServerHostName + ":" +  String(radarServerPortNumber));
      oled.display();
      Serial.println("Started, IP:" + WiFi.localIP().toString());
      Serial.println("Trying Connect to Radar Monitor: " + radarServerHostName + ":" +  String(radarServerPortNumber));
      //webSocket.begin("10.100.6.36",12336,"/socket.io/?transport=websocket" );
      webSocket.begin(radarServerHostName.c_str(), radarServerPortNumber,"/socket.io/?transport=websocket" );
    // use HTTP Basic Authorization this is optional remove if not needed
    //webSocket.setAuthorization("username", "password");
      periodicLoop();
      clearDisplay = true;
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

  if(clearDisplay){
    oled.setBatteryVisible(false);
    oled.setRSSIVisible(false);
    oled.setIPAddressVisible(false);
    oled.setConnectedVisible(false);
    oled.clearDisplay();
    // update the display 
    oled.display();
    clearDisplay = false;
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
    oled.println("IP :" + WiFi.localIP().toString() );
    oled.setConnectedVisible(true);
    oled.setConnected(true);
    float battery = getBatteryVoltage();
    // update the battery icon
    oled.setBattery(battery);
    oled.renderBattery();
    oled.refreshIcons();
    btnAClicked = false;
    oled.display();
    clearDisplay = true;
  }
  
  /*
  oled.clearDisplay();
 

  if((count % 2) == 0){
  // text display tests
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0);
    oled.println("Connecting to SSID\n'adafruit':");
    // print the count value to the OLED
    oled.print("count: ");
    oled.println(count);
  }
  */
  

  // increment the counter by 1
  count++;
  lastPeriodicLoop = millis();
}


void loop() {
    portal.handleClient();
    if (WiFi.status() == WL_IDLE_STATUS) {
      USE_SERIAL.println("Wifi is idle restarting");
      oled.clearDisplay();
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
    if(btnBPressed == false && !digitalRead(BUTTON_B)){
      btnBPressed = true;
      USE_SERIAL.println("Button B");
      periodicLoop();
    }
    if(btnCPressed == false && !digitalRead(BUTTON_C)){
      btnCPressed = true;
      USE_SERIAL.println("Button C");
      periodicLoop();
    }
}
