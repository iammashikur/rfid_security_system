#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <string>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <Servo.h>

#define SSID "AlphaESP";
#define PASS "password";


struct settings {
  char ssid[30];
  char password[30];
} user_wifi = {};


const char *softAP_ssid = SSID;
const char *softAP_pass = PASS;

// The access points IP address and net mask
// It uses the default Google DNS IP address 8.8.8.8 to capture all
// Android dns requests

IPAddress apIP(8, 8, 8, 8);
IPAddress netMsk(255, 255, 255, 0);

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
ESP8266WebServer server(80);

constexpr uint8_t RST_PIN = 16;  // Define pin D0 for the RST pin
constexpr uint8_t SDA_PIN = 15;  // Define pin D8 for the SDA pin

byte readCard[4];
String MasterTag = "77A4B9B4";  // Tag ID of your RFID card (TO BE SUBSTITUTED)
String tagID = "";

MFRC522 mfrc522(SDA_PIN, RST_PIN);  // Create MFRC522 instance

//Your Domain name with URL path or IP address with path
String serverName = "http://mashikur.dev.alpha.net.bd/esp/";

//Servo
Servo myservo;


// check if this string is an IP address
boolean isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

// checks if the request is for the controllers IP, if not we redirect automatically to the
// captive portal
boolean captivePortal() {
  if (!isIp(server.hostHeader())) {
    Serial.println("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send(302, "text/plain", "");
    server.client().stop();
    return true;
  }
  return false;
}


void handleRoot() {
  if (captivePortal()) {
    return;
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    WiFi.begin(ssid.c_str(), password.c_str());
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {

      // Save the wifi credentials to EEPROM
      String ssid = server.arg("ssid");
      String password = server.arg("password");
      EEPROM.begin(512);
      EEPROM.put(0, ssid);
      EEPROM.put(32, password);
      EEPROM.commit();

      // Connected
      server.send(200, "text/html", Template(String("AlphaESP - Success!"), String("--- Connected to Internet! ---")));
      delay(1000);

      // Close the hotspot
      WiFi.softAPdisconnect(true);

    } else {
      // Error
      server.send(200, "text/html", Template(String("AlphaEsp - Error!"), String("--- Invalid Credencials ---")));
    }
  } else {
    // Setup
    server.send(200, "text/html", Template(String("AlphaESP - WiFi Setup!"), String("<form autocomplete='off' class='form' action='/' method='POST'> <div class='control'> <h1 class='title'> --- WiFi --- </h1> </div><div class='control block-cube block-input'> <input name='ssid' placeholder='SSID' type='text'> <div class='bg-top'> <div class='bg-inner'></div></div><div class='bg-right'> <div class='bg-inner'></div></div><div class='bg'> <div class='bg-inner'></div></div></div><div class='control block-cube block-input'> <input name='password' placeholder='PASSWORD' type='password'> <div class='bg-top'> <div class='bg-inner'></div></div><div class='bg-right'> <div class='bg-inner'></div></div><div class='bg'> <div class='bg-inner'></div></div></div><button class='btn block-cube block-cube-hover' type='submit'> <div class='bg-top'> <div class='bg-inner'></div></div><div class='bg-right'> <div class='bg-inner'></div></div><div class='bg'> <div class='bg-inner'></div></div><div class='text'> Connect </div></button> </form>")));
  }
}


void handleNotFound() {
  if (captivePortal()) {
    return;
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/plain", "404");
}

void setup() {

  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);
  EEPROM.begin(sizeof(struct settings));
  EEPROM.get(0, user_wifi);
  WiFi.mode(WIFI_STA);
  WiFi.begin(user_wifi.ssid, user_wifi.password);
  SPI.begin();
  mfrc522.PCD_Init();
  myservo.attach(2);

  byte tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (tries++ > 30) {
      delay(1000);
      WiFi.mode(WIFI_STA);
      WiFi.softAPConfig(apIP, apIP, netMsk);

      // its an open WLAN access point without a password parameter
      WiFi.softAP(softAP_ssid, softAP_pass);
      delay(1000);
      dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
      dnsServer.start(DNS_PORT, "*", apIP);

      server.on("/", handleRoot);
      server.onNotFound(handleNotFound);
      server.begin();
      break;
    }
  }
}

void loop() {

  // check if the device is connected to the internet
  if (WiFi.status() != WL_CONNECTED) {

    digitalWrite(BUILTIN_LED, HIGH);
    delay(100);
    digitalWrite(BUILTIN_LED, LOW);
    delay(100);

  } else {


    digitalWrite(BUILTIN_LED, HIGH);

    //Wait until new tag is available
    while (getUID()) {

      Serial.println(tagID);


      WiFiClient client;
      HTTPClient http;

      String serverPath = serverName + "?tag=" + tagID;

      // Your Domain name with URL path or IP address with path
      http.begin(client, serverPath.c_str());


      // Send HTTP GET request
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {


        String json = http.getString();


        StaticJsonDocument<200> doc;
        deserializeJson(doc, json);



        // Extract the data
        bool status = doc["status"];

        if (status) {

          String id = doc["user"]["id"];
          String name = doc["user"]["name"];
          String email = doc["user"]["email"];
          String phone = doc["user"]["phone"];

          // Print the data
          Serial.print("Status: ");
          Serial.println(status);
          Serial.print("ID: ");
          Serial.println(id);
          Serial.print("Name: ");
          Serial.println(name);
          Serial.print("Email: ");
          Serial.println(email);
          Serial.print("Phone: ");
          Serial.println(phone);



          int previousPosition;
          previousPosition = myservo.read();  //store the previous position
          myservo.write(30);                  // rotate the servo to 30 degrees clockwise
          delay(1000);                        // wait for 1 second
          myservo.write(previousPosition);    //rotate the servo counterclockwise back to the previous position
        }


      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();

      delay(2000);
    }
  }

  //DNS
  dnsServer.processNextRequest();
  //HTTP
  server.handleClient();
}

String Template(String title, String content) {
  return String("<!DOCTYPE html><html lang='en'><head> <meta charset='UTF-8'><meta http-equiv='X-UA-Compatible' content='IE=edge'><meta name='viewport' content='width=device-width, initial-scale=1.0'> <title>") + title + String("</title><style>*, ::after, ::before{box-sizing: border-box;}::-webkit-scrollbar-track{-webkit-box-shadow: inset 0 0 6px rgba(0, 0, 0, 0.3); border-radius: 10px; background-color: #F5F5F5;}::-webkit-scrollbar{width: 12px; background-color: #F5F5F5;}::-webkit-scrollbar-thumb{border-radius: 10px; -webkit-box-shadow: inset 0 0 6px rgba(0, 0, 0, .3); background-color: #D62929;}body{background-color: #212121; color: #fff; font-family: monospace, serif; letter-spacing: 0.05em; height: 100%; margin:0; padding: 0;}.title{padding-top: 20px; padding-bottom: 20px; text-align: center; font-size: 16px;}.container{display: flex; align-items: center; justify-content: center; min-height: 100vh;}.form{width: 300px; margin: 0 auto;}.form .control{margin: 0 0 24px;}.form .control input{width: 100%; padding: 14px 16px; border: 0; background: transparent; color: #fff; font-family: monospace, serif; letter-spacing: 0.05em; font-size: 12px;}.form .control input:hover, .form .control input:focus{outline: none; border: 0;}.form .btn{width: 100%; display: block; padding: 14px 16px; background: transparent; outline: none; border: 0; color: #fff; letter-spacing: 0.1em; font-weight: bold; font-family: monospace; font-size: 12px; cursor: pointer;}.block-cube{position: relative;}.block-cube .bg-top{position: absolute; height: 10px; background: #020024; background: linear-gradient(90deg, #020024 0%, #340979 37%, #00d4ff 94%); bottom: 100%; left: 5px; right: -5px; transform: skew(-45deg, 0); margin: 0;}.block-cube .bg-top .bg-inner{bottom: 0;}.block-cube .bg{position: absolute; left: 0; top: 0; right: 0; bottom: 0; background: #020024; background: linear-gradient(90deg, #020024 0%, #340979 37%, #00d4ff 94%);}.block-cube .bg-right{position: absolute; background: #020024; background: #00d4ff; top: -5px; z-index: 0; bottom: 5px; width: 10px; left: 100%; transform: skew(0, -45deg);}.block-cube .bg-right .bg-inner{left: 0;}.block-cube .bg .bg-inner{transition: all 0.2s ease-in-out;}.block-cube .bg-inner{background: #212121; position: absolute; left: 2px; top: 2px; right: 2px; bottom: 2px;}.block-cube .text{position: relative; z-index: 2;}.block-cube.block-input input{position: relative; z-index: 2;}.block-cube.block-input input:focus~.bg-right .bg-inner, .block-cube.block-input input:focus~.bg-top .bg-inner, .block-cube.block-input input:focus~.bg-inner .bg-inner{top: 100%; background: rgba(255, 255, 255, 0.5);}.block-cube.block-input .bg-top, .block-cube.block-input .bg-right, .block-cube.block-input .bg{background: rgba(255, 255, 255, 0.5); transition: background 0.2s ease-in-out;}.block-cube.block-input .bg-right .bg-inner, .block-cube.block-input .bg-top .bg-inner{transition: all 0.2s ease-in-out;}.block-cube.block-input:focus .bg-top, .block-cube.block-input:focus .bg-right, .block-cube.block-input:focus .bg, .block-cube.block-input:hover .bg-top, .block-cube.block-input:hover .bg-right, .block-cube.block-input:hover .bg{background: rgba(255, 255, 255, 0.8);}.block-cube.block-cube-hover:focus .bg .bg-inner, .block-cube.block-cube-hover:hover .bg .bg-inner{top: 100%;}</style></head><body> <div class='container'>" + content + String("</div></body></html>"));
}


boolean getUID() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return false;
  }
  tagID = "";
  for (uint8_t i = 0; i < 4; i++) {
    tagID.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  tagID.toUpperCase();
  mfrc522.PICC_HaltA();  // Stop reading
  return true;
}