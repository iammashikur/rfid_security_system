#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>


constexpr uint8_t RST_PIN = 16;  // Define pin D0 for the RST pin
constexpr uint8_t SDA_PIN = 15;  // Define pin D8 for the SDA pin

byte readCard[4];
String MasterTag = "77A4B9B4";  // Tag ID of your RFID card (TO BE SUBSTITUTED)
String tagID = "";

MFRC522 mfrc522(SDA_PIN, RST_PIN);  // Create MFRC522 instance

const char* ssid = "TonmoyOp";
const char* password = "Tonmoy5*";

//Your Domain name with URL path or IP address with path
String serverName = "http://mashikur.dev.alpha.net.bd/esp/";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

void setup() {
  Serial.begin(115200);



  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }


  while (!Serial)
    ;
  SPI.begin();
  mfrc522.PCD_Init();


  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {





  //Wait until new tag is available
  while (getUID()) {

    Serial.println(tagID);

    if (WiFi.status() == WL_CONNECTED) {
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


      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
    delay(2000);
  }
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
