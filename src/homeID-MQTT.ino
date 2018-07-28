#include <SSD1306.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <wire.h>
#include <stdlib.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//Wiegand Reader calculation variables
#define MAX_BITS 100                  // max number of bits
#define WEIGAND_WAIT_TIME  5000       // time to wait for another weigand pulse.
unsigned char databits[MAX_BITS];     // stores all of the data bits
volatile unsigned int bitCount = 0;   //number of bits read
unsigned char flagDone;               // goes low when data is currently being captured
unsigned int weigand_counter;         // countdown until we assume there are no more bits

volatile unsigned long facilityCode = 0;        // decoded facility code eg: 150
volatile unsigned long cardCode = 0;         // decoded card code eg: 1234 or 66

int authenticated = 0;

// Breaking up card value into 2 chunks to create 10 char HEX value
volatile unsigned long bitHolder1 = 0;
volatile unsigned long bitHolder2 = 0;
volatile unsigned long cardChunk1 = 0;
volatile unsigned long cardChunk2 = 0;
char cardChar[5];

//LED Pins for Wiegand reader
const int LED_GREEN = 0;
const int LED_RED = 2;
const int BEEPER = 14;

//Wiegand reader data0 / data1 input pins (These are mentioned again a bit further down on lines 146-147)
const int DATA0 = 5; //Pin D1 on Wemos D1 Mini
const int DATA1 = 4; //Pin D2 on Wemos D1 Mini

// number of samples to take for the vbatt() monitor
#define NUM_SAMPLES 22
unsigned char sample_count = 0; // current sample number
int sum = 0;                    // sum of samples taken
float voltage = 0.0;            // calculated voltage

//MQTT server, client & topic settings
const char* mqtt_server = ""; //Your MQTT server address
const char* mqtt_topic = "home/Access/Cards"; //The topic to publish card number to
const char* mqtt_username = ""; //The MQTT username (if required)
const char* mqtt_password = ""; //The MQTT password (if required, leave blank for no password)
const char* clientID = "homeIDReader";       // The client id identifies the ESP8266 device.
const char* hostName = "homeIDReader";
long lastReconnectAttempt = 0;    //last MQTT connection attempt time (for auto reconnect)

//Wi-Fi Settings
const char* ssid = ""; //Your Wi-Fi SSID
const char* wifi_password = ""; //Your Wi-Fi Password

//Attatch OLED
SSD1306  display(0x3c, D6, D7); //Pins the OLED is attached to if in use

// Initialise the WiFi and MQTT Clients
WiFiClient espClient;
PubSubClient client(espClient);

//Battery monitoring refresh function for OLED
void vbatt() {
    // take a number of analog samples and add them up
    while (sample_count < NUM_SAMPLES) {
      sum += analogRead(A0);
      sample_count++;
        delay(10);
      }
// calculate the voltage
// use 5.0 for a 5.0V ADC reference voltage //3.3v for a 3v reference i.e. ESP8266
//3.31V is my calibrated voltage base off the ESP8266 regulator VOUT
      voltage = ((float)sum / (float)NUM_SAMPLES * 3.31) / 1024.0;
      display.drawString(100, 0, String(voltage * 8.05));
      display.drawString(121, 0, "v");
      display.display();
      sample_count = 0;
      sum = 0;
}

void setup_wifi() {
  //WiFi.setPhyMode(WIFI_PHY_MODE_11G);
    WiFi.begin(ssid, wifi_password);
    WiFi.hostname(hostName);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      if (WiFi.status() == WL_CONNECTED) {
          display.drawString(0 , 0, "Wi-Fi Connected!");
          display.display();
          Serial.println("Wi-Fi Connected!");
          Serial.print("IP address: ");
          Serial.println(WiFi.localIP());
        }
          else if (WiFi.status() == WL_CONNECT_FAILED) {
            Serial.println("Wi-Fi Failed!");
    }
    delay(500);
}

//RECONNECT
boolean reconnect() {
  if (client.connect(clientID, mqtt_username, mqtt_password)) {     // Once connected, publish an announcement...
    client.publish("home/ConnectionLogs","ESPHIDReader Connected");
    client.subscribe(mqtt_topic);       // ... and resubscribe
  }
  return client.connected();
}

//Function to keep the MQTT connection alive
void keepMQTTAllive() {
    long now = millis();
    display.drawString(0 , 24, "Attempting MQTT...");
    vbatt();
    display.display();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
        if (client.connected()) {
          display.drawString(0 , 36, "MQTT Server Connectted");
          vbatt();
          display.display();
        }
      }
    }
    return;
  }
//Refresh and display on OLED
  void displayOnOLED(){
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.clear();

        display.drawString(0, 0, "FC:");
        display.drawString(0, 12, "CC:");
        display.drawString(0, 24, "HX:");
        display.drawString(23, 0, String(facilityCode));
        display.drawString(23, 12, String(cardCode));
        display.drawString(23, 24, String(cardChunk1, HEX) + String(cardChunk2, HEX));
          vbatt();
            display.display();

              Serial.println("FC: "+String(facilityCode));
              Serial.println("CC: "+String(cardCode));
              Serial.println("HEX: "+String(cardChunk1, HEX) + String(cardChunk2, HEX));
    }
//Publish the card number to MQTT - we only care about card number here not site code
  void PublishToMQTT() {
      int tempCC = cardCode;
      String ccStr = String(tempCC); //What was this for? LOL

        ccStr.toCharArray(cardChar, 5);
        client.publish(mqtt_topic, cardChar);

         Serial.println("Published to MQTT - Ready for next tag.");
    }

  // OTA Upgrading Config
  void setupOTA() {
    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);
    // If not set - Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname(hostName);
    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");
    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
      ArduinoOTA.begin();
  }

void setup() {
  //Initialise Display
    display.init();
    display.clear();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    //Initialise Serial
      Serial.begin(57600);
      Serial.println();

    //RFID Reader pin configuration. High = Off // Low = On
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(BEEPER, OUTPUT);
    digitalWrite(LED_RED, LOW);
    digitalWrite(BEEPER, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    pinMode(DATA0, INPUT);     // DATA0 (INT0)
    pinMode(DATA1, INPUT);     // DATA1 (INT1)

      //Connect to Wi-Fi
      setup_wifi();
      //Refresh battery monitoring for OLED
      vbatt();
      setupOTA();

      // binds the ISR functions to the falling edge of INTO and INT1
      attachInterrupt(5, ISR_INT0, FALLING);
      attachInterrupt(4, ISR_INT1, FALLING);
        weigand_counter = WEIGAND_WAIT_TIME;

      //connect to MQTT Server
          client.setServer(mqtt_server, 1883);


}

void loop() {
  //Keep MQTT connection alive
  if (!client.connected()) {
    display.clear();
      keepMQTTAllive();
  }
  else {
    client.loop();
}

  //Wiegand Processing
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--weigand_counter == 0)
      flagDone = 1;
  }
  // if we have bits and the weigand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
    getCardValues();
    getCardNumAndSiteCode();
    displayOnOLED();
    PublishToMQTT();

     // cleanup and get ready for the next card
     //authenticated = 0;
     bitCount = 0;
     facilityCode = 0;
     cardCode = 0;
     bitHolder1 = 0; bitHolder2 = 0;
     cardChunk1 = 0; cardChunk2 = 0;

     for (i=0; i<MAX_BITS; i++)
     {
       databits[i] = 0;
     }
  }
  ArduinoOTA.handle();
}

//INTERRUPT processing functions
// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0()
{
  bitCount++;
  flagDone = 0;

  if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
  }
  else {
      bitHolder2 = bitHolder2 << 1;
  }
  weigand_counter = WEIGAND_WAIT_TIME;
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;

   if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
      bitHolder1 |= 1;
   }
   else {
     bitHolder2 = bitHolder2 << 1;
     bitHolder2 |= 1;
   }
   weigand_counter = WEIGAND_WAIT_TIME;
}


void getCardNumAndSiteCode(){
     unsigned char i;

    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for more info
    // also specifically: www.brivo.com/app/static_data/js/calculate.js
    switch (bitCount) {

    ///////////////////////////////////////
    // standard 26 bit format
    // facility code = bits 2 to 9
    case 26:

    //Serial.println(databits[26]);

      for (i=1; i<9; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }

      // card code = bits 10 to 23
      for (i=9; i<25; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];

      }

      break;

    ///////////////////////////////////////
    // 33 bit HID Generic
    case 33:
      for (i=1; i<8; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];

      }

      // card code
      for (i=8; i<32; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }

      break;

    ///////////////////////////////////////
    // 34 bit HID Generic
    case 34:
      for (i=1; i<17; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }

      // card code
      for (i=17; i<33; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
      break;

    ///////////////////////////////////////
    // 35 bit HID Corporate 1000 format
    // facility code = bits 2 to 14
    case 35:
      for (i=2; i<14; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }

      // card code = bits 15 to 34
      for (i=14; i<34; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
      break;

    }
    return;

}


//////////////////////////////////////
// Function to append the card value (bitHolder1 and bitHolder2) to the necessary array then tranlate that to
// the two chunks for the card value that will be output
void getCardValues() {
  switch (bitCount) {
    case 26:
        // Example of full card value
        // |>   preamble   <| |>   Actual card value   <|
        // 000000100000000001 11 111000100000100100111000
        // |> write to chunk1 <| |>  write to chunk2   <|

       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 2){
            bitWrite(cardChunk1, i, 1); // Write preamble 1's to the 13th and 2nd bits
          }
          else if(i > 2) {
            bitWrite(cardChunk1, i, 0); // Write preamble 0's to all other bits above 1
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 20)); // Write remaining bits to cardChunk1 from bitHolder1
          }
          if(i < 20) {
            bitWrite(cardChunk2, i + 4, bitRead(bitHolder1, i)); // Write the remaining bits of bitHolder1 to cardChunk2
          }
          if(i < 4) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i)); // Write the remaining bit of cardChunk2 with bitHolder2 bits
          }
        }
        break;

    case 27:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 3){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 3) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 19));
          }
          if(i < 19) {
            bitWrite(cardChunk2, i + 5, bitRead(bitHolder1, i));
          }
          if(i < 5) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 28:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 4){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 4) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 18));
          }
          if(i < 18) {
            bitWrite(cardChunk2, i + 6, bitRead(bitHolder1, i));
          }
          if(i < 6) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 29:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 5){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 5) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 17));
          }
          if(i < 17) {
            bitWrite(cardChunk2, i + 7, bitRead(bitHolder1, i));
          }
          if(i < 7) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 30:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 6){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 6) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 16));
          }
          if(i < 16) {
            bitWrite(cardChunk2, i + 8, bitRead(bitHolder1, i));
          }
          if(i < 8) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 31:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 7){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 7) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 15));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i + 9, bitRead(bitHolder1, i));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 32:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 8){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 8) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 14));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i + 10, bitRead(bitHolder1, i));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 33:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 9){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 9) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 13));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i + 11, bitRead(bitHolder1, i));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 34:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 10){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 10) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 12));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i + 12, bitRead(bitHolder1, i));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 35:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 11){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 11) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 11));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i + 13, bitRead(bitHolder1, i));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 36:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 12){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 12) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 10));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i + 14, bitRead(bitHolder1, i));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 37:
       for(int i = 19; i >= 0; i--) {
          if(i == 13){
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 9));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i + 15, bitRead(bitHolder1, i));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;
  }
  return;
}
