/*********************************************************************************************************************
 ** 2017-01-28, RDU: First draft to control Mitsubishi HVAC using IR over a Homie node
 **
 **                  D1 = SCL for OLED \__ Add GND and 3.3v to power OLED screen
 **                  D2 = SDA for OLED /
 **                  D3 = DHT22 data pin
 **                  D4 = Power for IRDA LED
 **
 **
 ** 2017-02-02, RDU: Problems to send float values to Homie custom device over MQTT
 **                  Sent as 42.60 but echoed as 2.5957243E-6
 **                  Some feelings that float is not represented the same way between C++ and Java
 **
 ** 2017-02-04, RDU: Rollback for pidome-mqttbroker.jar from 2017-01-29 20:49 to 2016-06-10 07:44
 **                  Server side fix addressed float parameters issue with latest snapshot of PiDome
 **
 *********************************************************************************************************************/
#include <Homie.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
  // Software specifications
    #define FW_NAME    "D1Mini-HVAC"
    #define FW_VERSION "0.17.2.4"

  // OLED consts/vars
    // TODO
    #include <SPI.h>
    #include <Wire.h>
    #include <Adafruit_GFX.h>
    #include <Adafruit_SSD1306.h>
    #define OLED_RESET 0  // GPIO0
    #define OLEDPIN_SCL D1
    #define OLEDPIN_SDA D2
    Adafruit_SSD1306 display(OLED_RESET);

    #define NUMFLAKES 10
    #define XPOS 0
    #define YPOS 1
    #define DELTAY 2

  // DHT sensor consts/vars
    #include "DHT.h"
    #define DHTPIN     D5     // What pin we're connected to
    #define DHTTYPE    DHT22  // Can be DHT11, DHT22 (AM2302), DHT21 (AM2301)
    DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor for normal 16mhz Arduino
  // IR
    #include <IRremoteESP8266.h>
    #define IRLEDPIN   D6
    IRsend irsend(IRLEDPIN);

  // Sensor consts/vars
    HomieNode hvacNode("HVAC", "HVAC");
    HomieNode temperatureNode("temperature", "temperature");
    HomieNode humidityNode("humidity", "humidity");

// Measure loop
    const int MEASURE_INTERVAL = 10; // How often to poll DHT22 for temperature and humidity
    unsigned long lastMeasureSent = 0;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OLED screen

void printOLED_To(int x, int y, char* text)
{
  display.setTextSize(1);
  display.setCursor(x, y); // Char is 7 pixels
  display.setTextColor(BLACK, BLACK);
  display.println("                    "); // 128/4=32 chars

  display.setCursor(x, y); // Char is 7 pixels
  display.setTextColor(WHITE, BLACK);
  display.println(text);
  display.display();
}


void printOLED_Temp(float temp)
{
  char sTemp[6];
  dtostrf(temp, 2, 2, sTemp);

  char outstr[20];
  sprintf(outstr, "Temp: %s\0", sTemp);
  printOLED_To(0, 16, outstr);
}

void printOLED_Humi(float humidity)
{
  char sHumidity[6];
  dtostrf(humidity, 2, 2, sHumidity);

  char outstr[20];
  sprintf(outstr, "Humi: %s\0", sHumidity);
  printOLED_To(0, 24, outstr);
}

void printOLED_NodeInfo()
{
  char outstrName[40];
  sprintf(outstrName, "Name: %s", FW_NAME);
  printOLED_To(0, 0, outstrName);

  char outstrVersion[40];
  sprintf(outstrVersion, "Ver : %s", FW_VERSION);
  printOLED_To(0, 8, outstrVersion);

  printOLED_Temp(0.0);
  printOLED_Humi(0.0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual code
bool hvacModeHandler(const HomieRange& range, const String& value) {
  // Sanity checks : We don't garbage so we validate info
  if (value == "") return false;

  // Actual processing
  Homie.getLogger() << "HVAC mode received : " << value << endl;
  if (value == "off") {
    Homie.getLogger() << "  > Send Power Off to HVAC" << endl;
  } else if (value == "dry") {
    Homie.getLogger() << "  > Send dry mode to HVAC" << endl;
  } else if (value == "Heat22") {
    Homie.getLogger() << "  > Send Heating / Auto / 22°c to HVAC" << endl;
  } else {
    Homie.getLogger() << "  > Unknown mode!" << endl;
  }

  // Get out, job done!
  return true;
}

void setupHandler() {
  // Hardware part
    // OLED screen
      // Actual setup
        Homie.getLogger() << "OLED screen on pins SCL:" << OLEDPIN_SCL << ", SDA:" << OLEDPIN_SDA << endl;
        display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
        display.display();
        delay(2000);
        display.clearDisplay();
        Homie.getLogger() << "  >> Setup OK" << endl;
      // Node infos
        printOLED_NodeInfo();
        display.display();


    // DHT22
      Homie.getLogger() << "DHT " << DHTTYPE << " on pin " << DHTPIN << endl;
      pinMode(DHTPIN, OUTPUT);
      dht.begin();
      Homie.getLogger() << "  >> Setup OK" << endl;

    // IR LED
      Homie.getLogger() << "IR LED on pin " << IRLEDPIN << endl;
      irsend.begin();
      Homie.getLogger() << "  >> Setup OK" << endl;
}

void loopHandler() {
  if (millis() - lastMeasureSent >= MEASURE_INTERVAL * 1000UL || lastMeasureSent == 0) {
    // Read from DHT22 sensor
    float temperature = dht.readTemperature(); // Read temperature as Celsius
    float humidity = dht.readHumidity(); // Read humidity as relative [0-100]%

    // Process read values
    if (isnan(temperature) || isnan(humidity)) {
      Homie.getLogger() << F("Failed to read from DHT sensor!") << endl;
    } else {
      Homie.getLogger() << F("DHT sensor reading validated!") << endl;

      // Print to log
        Homie.getLogger() << F("\tTemperature: ") << temperature << " °C" << endl;
        Homie.getLogger() << F("\tHumidity   : ") << humidity << " %" << endl;

      // Print to OLED : We use float data type
        Homie.getLogger() << F("\tPrint to OLED . . .") << endl;
        printOLED_Temp(temperature);
        printOLED_Humi(humidity);
        display.display();

      // Actual publish
        Homie.getLogger() << F("\tPublish to MQTT . . .") << endl;
        temperatureNode.setProperty("degrees").send(String(temperature));
        humidityNode.setProperty("relative").send(String(humidity));
    }
    lastMeasureSent = millis();
  }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic Homie setup and loop
void setup() {
  Serial.begin(115200); // Required to enable serial output

  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  // Advertise nodes properties
    hvacNode.advertise("mode").settable(hvacModeHandler);
    temperatureNode.advertise("degrees");
    humidityNode.advertise("relative");

  // Finalize setup
  Homie.setup();
}

void loop() {
  Homie.loop();
}
