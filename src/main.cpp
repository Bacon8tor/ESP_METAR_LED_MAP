#include <NTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <pgmspace.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>

//Pin for LEDs
#define DATA_PIN 25

//How many minutes map updates
#define UPADTE_TIME 15

//DEFINE LED TYPE - WS2812B is default
//#define WS2811

//Airports List
const char* airports[] PROGMEM = {"KCHD", "KPHX", "KGYR", "KGEU", "KDVT", 
                                  "KSDL", "KFFZ", "KIWA", "KSRQ", "KSPG",
                                  "KPIE", "KTPA", "KBKV", "KZPH", "KLAL"};

// Debug mode
bool debug = false;

//Get Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -25200, 60000); //set for MST -7 
//Use Chart Below to Set your Correct TimeZone
// Time Zone	Offset (Hours)	Offset (Seconds)
// UTC	0	0
// Eastern (EST)	-5	-18000
// Central (CST)	-6	-21600
// Mountain (MST)	-7	-25200
// Pacific (PST)	-8	-28800
// UTC+1	+1	3600
// UTC+2	+2	7200
// UTC+3	+3	10800


// Struct to hold RGB values
struct RGBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

const RGBColor VFR = {0,255,0};
const RGBColor MVFR = {0,0,255};
const RGBColor IFR = {255,0,0};
const RGBColor LIFR = {120,255,180};

#ifdef WS2811
//RGB
CRGB vfr_color(VFR.r, VFR.g, VFR.b);
CRGB mvfr_color(MVFR.r, MVFR.g, MVFR.b);
CRGB ifr_color(IFR.r, IFR.g, IFR.b);
CRGB lifr_color(LIFR.r, LIFR.g, LIFR.b);

#else
//GRB for WS2812B
CRGB vfr_color(VFR.g, VFR.r, VFR.b);
CRGB mvfr_color(MVFR.g, MVFR.r, MVFR.b);
CRGB ifr_color(IFR.g, IFR.r, IFR.b);
CRGB lifr_color(LIFR.g, LIFR.r, LIFR.b);

#endif


// Timing interval (15 minutes)
constexpr unsigned long INTERVAL = UPADTE_TIME * 60 * 1000; // Milliseconds
          
//map 1
//String airports[] = {"KDUG", "KOLS", "KTUS", "KPHX", "KNYL", "KGXF", "KPAN", "KSOW", "KDVT", "KINW", "KPGA", "KFLG", "KIGM", "KGCN", "KPRC", "KCMR", "KSEZ"};

// MAP 2
//String airports[] = {"KDUG", "KOLS", "KSOW", "KDVT", "KTUS", "KGXF", "KNYL", "KA39", "KSEZ", "KPHX", "KINW", "KFLG", "KGCN", "KPGA"};


//DONT CHANGE ANYTHING BELOW HERE======================================================================================//
 
int ledBrightness = 75; 

struct Preference {
  const char* name;
  int value;
};

Preference settings[] = {
{"led_brightness",75},
{"start_time",7},
{"end_time",20}
};

Preferences preferences;

unsigned long previousMillis = 0;
const int NUM_AIRPORTS = sizeof(airports) / sizeof(airports[0]);
CRGB leds[NUM_AIRPORTS];

//Need to update to JSON Document 
DynamicJsonDocument doc(512); // Adjust size as needed
JsonArray lastMetars;

// Web server setup
AsyncWebServer server(80);

//===================================================== Helper Functions ===================================================================//
// Debug print helper
void debugPrint(const char* format, ...) {
  if (debug) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
  }
}

//Checks if within the Scheduled Time Window
bool isTimeInRange() {
    timeClient.update();
    int currentHour = timeClient.getHours();
    //Serial.println(currentHour);
    return (currentHour >= settings[1].value && currentHour < settings[2].value);
}

//===================================================== Get/Set Preferences ================================================================//

 // Function to get the value of a setting by name (loads from Preferences)
int getSettingValue(const char* key) {
  preferences.begin("settings", true);  // Open Preferences in read-only mode
  int value = preferences.getInt(key, -1);  // Get value or return -1 if not found
  preferences.end();

  if (value == -1) {  // If not found, use the default from settings array
      for (int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
          if (strcmp(settings[i].name, key) == 0) {
              value = settings[i].value;
              break;
          }
      }
      debugPrint("getSettingValue: '%s' not found in Preferences, using default %d\n", key, value);
  } else {
      debugPrint("getSettingValue: Loaded '%s' with value %d from Preferences\n", key, value);
  }

  return value;
}

 // Function to set the value of a setting by name (saves to Preferences)
void setSettingValue(const char* key, int newValue) {
  for (int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
      if (strcmp(settings[i].name, key) == 0) {
          settings[i].value = newValue;  // Update the in-memory setting
          
          // Save to Preferences
          preferences.begin("settings", false);  // Open Preferences in write mode
          preferences.putInt(key, newValue);
          preferences.end();

          debugPrint("setSettingValue: Saved '%s' with new value %d to Preferences\n", key, newValue);
          return;
      }
  }
  debugPrint("setSettingValue: Setting '%s' not found!\n", key);
}

//======================================================METAR Processing /API Functions ====================================================//
 // Set the LED color based on flight category
 void setLEDColor(String flightCat, int LED){
  if (flightCat == "VFR") {
    leds[LED] = vfr_color;
  } else if (flightCat == "MVFR") {
    leds[LED] = mvfr_color;
  } else if (flightCat == "IFR") {
    leds[LED] = ifr_color;
  } else if (flightCat == "LIFR") {
    leds[LED] = lifr_color;
  }
}

// Function to determine flight category
 String determineFlightCategory(float visibility, int ceiling,String type) {
  if (type == "FEW" || type == "CLR" || type == "SCT") {
    ceiling = 10000;
 }
  if (visibility > 5.0 && ceiling > 3000) {
    return "VFR"; // Visual Flight Rules
  } else if (visibility >= 3.0 && visibility <= 5.0 || (ceiling >= 1000 && ceiling <= 3000)) {
    return "MVFR"; // Marginal Visual Flight Rules
  } else if (visibility >= 1.0 && visibility < 3.0 || (ceiling >= 500 && ceiling < 1000)) {
    return "IFR"; // Instrument Flight Rules
  } else if (visibility < 1.0 || ceiling < 500) {
    return "LIFR"; // Low Instrument Flight Rules
  }

  return "N/A"; // Catch-all for undetermined cases
}

//Get METAR Data and Set LED Colors
void fetchMetarData() {
  // Construct API URL
  String url = "https://aviationweather.gov/api/data/metar?format=json&ids=";
  for (int i = 0; i < NUM_AIRPORTS; i++) {
    if (i > 0) url += ",";
    url += airports[i];
  }

  debugPrint("Fetching weather data from: %s\n", url.c_str());

  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    debugPrint("HTTP request successful.\n");

    String payload = http.getString();
   
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      debugPrint("JSON deserialization failed: %s\n", error.c_str());
      return;
    }
     JsonArray metars = doc.as<JsonArray>();
    for (int i = 0; i < NUM_AIRPORTS; i++) {
      const char* airportIcao = (const char*)pgm_read_ptr(&(airports[i]));

      // Search through the METAR data for the matching ICAO code
      bool found = false;
      for (JsonObject metar : metars) {
        const String icaoId = metar["icaoId"];

        if (icaoId == airportIcao) {
          found = true;
          
          // Fetch and process METAR data for this airport
          float temp = metar["temp"];
          temp = (temp  * 9.0 / 5.0) + 32.0;
          int wdir = metar["wdir"];
          int wspd = metar["wspd"];
          const char* rawOb = metar["rawOb"];
          const char* name = metar["name"];
          int ceiling = -1; // Initialize ceiling to invalid value
          const char* cloudType;
          float altimeter = metar["altim"];
          altimeter = altimeter * 0.02952998;
          
          float visib;
            if (metar["visib"].is<const char*>()) {
              String visibStr = metar["visib"].as<const char*>();
              if (visibStr == "10+") {
                visib = 10.0; // Use 10.1 to represent "10+" as a float
              } else {
                visib = visibStr.toFloat(); // Convert string to float
              }
            } else if (metar["visib"].is<float>()) {
              visib = metar["visib"].as<float>(); // Directly assign float values
            } else {
              visib = -1.0; // Default value for missing or invalid data
            }
          
          // Determine ceiling based on cloud types OVC, BKN, CLR
          if (metar.containsKey("clouds") && !metar["clouds"].isNull()) {
            JsonArray clouds = metar["clouds"].as<JsonArray>();
            for (JsonObject cloud : clouds) {
              const char* cover = cloud["cover"];
              int base = cloud["base"] | -1; // Default to -1 if base is missing

              if (base > 0 && (strcmp(cover, "OVC") == 0 || strcmp(cover, "BKN") == 0)) {
                if (ceiling == -1 || base < ceiling) {
                  ceiling = base; // Find the lowest cloud base of OVC or BKN
                  cloudType = cover;
                }
              }
              // If cloud cover is CLR (clear), set visibility to CLR
              if (strcmp(cover, "FEW") == 0 || strcmp(cover, "SCT") == 0)               {
                ceiling = base;  // Clear visibility
                cloudType = cover;
              }
              if (strcmp(cover, "CLR") == 0 ){
                ceiling = 60000;  // Clear visibility
                cloudType = cover;
              }
            }
          }

          // Determine the flight category based on visibility and ceiling
          String flightCategory = determineFlightCategory(visib, ceiling,cloudType);

          debugPrint("\nAirport: %s\n", name ? name : "Unknown");
          debugPrint("  ICAO ID: %s\n", icaoId ? icaoId : "Unknown");
          debugPrint("  Flight Conditions: %s\n", flightCategory);
          debugPrint("  Temperature: %.2f°F\n", temp != -999 ? temp : NAN);
          debugPrint("  Altimeter: %.2f inHg\n",altimeter != -999 ? altimeter : NAN);
          debugPrint("  Wind Direction: %s\n", wdir != -1 ? String(wdir).c_str() : "Unknown");
          debugPrint("  Wind Speed: %s knots\n", wspd != -1 ? String(wspd).c_str() : "Unknown");
          debugPrint("  Visibility: %.2f miles\n", visib != -999 ? visib : NAN);
          debugPrint("  Clouds: %s at %s\n",cloudType,String(ceiling).c_str());
          debugPrint("  Metar Report: %s\n", rawOb ? rawOb : "Unknown");
          

          setLEDColor(flightCategory,i);
          

          break; // Found the airport, no need to continue searching for this airport
        }
      }

      // If the airport's ICAO code wasn't found in the METAR response
      if (!found) {
        debugPrint("Missing METAR data for ICAO: %s\n", airportIcao);
        leds[i] = CRGB::Black;  // Set a default color if METAR is missing
      }
    }
    lastMetars = metars;
  } else {
    debugPrint("HTTP request failed with code: %d\n", httpCode);
  }
  
  FastLED.show();
  http.end();
}

//Check Metars disreading 15 min update but still respects the time schedule
void checkMetars(){

  if (isTimeInRange()) {
    Serial.println("Turn ON");
    // Add code to turn on your device

  fetchMetarData();

} else {
  Serial.println("Turn OFF");
  fill_solid(leds, NUM_AIRPORTS, CRGB::Black);
  FastLED.show();
  
}

}

void printMetars(){
  for (JsonObject metar : lastMetars) {
        const String icaoId = metar["icaoId"];
        // Fetch and process METAR data for this airport
        float temp = metar["temp"];
        int wdir = metar["wdir"];
        int wspd = metar["wspd"];
        const char* rawOb = metar["rawOb"];
        const char* name = metar["name"];
        int ceiling = -1; // Initialize ceiling to invalid value
         float visib;
          if (metar["visib"].is<const char*>()) {
            String visibStr = metar["visib"].as<const char*>();
            if (visibStr == "10+") {
              visib = 10.0; // Use 10.1 to represent "10+" as a float
            } else {
              visib = visibStr.toFloat(); // Convert string to float
            }
          } else if (metar["visib"].is<float>()) {
            visib = metar["visib"].as<float>(); // Directly assign float values
          } else {
            visib = -1.0; // Default value for missing or invalid data
          }
      // debugPrint("\nAirport: %s\n", name ? name : "Unknown");
      // debugPrint("  ICAO ID: %s\n", icaoId ? icaoId : "Unknown");
      // debugPrint("  Temperature: %.1f°C\n", temp != -999 ? temp : NAN);
      // debugPrint("  Wind Direction: %s\n", wdir != -1 ? String(wdir).c_str() : "Unknown");
      // debugPrint("  Wind Speed: %s knots\n", wspd != -1 ? String(wspd).c_str() : "Unknown");
      // debugPrint("  Visibility: %.1f miles\n", visib != -999 ? visib : NAN);
      // debugPrint("  Metar Report: %s\n", rawOb ? rawOb : "Unknown");
    delay(30000);
  }
}

//=======================================================HTML/WEB Functions=================================================================//
 // Function to load HTML from SPIFFS
String loadHTML(const char* filename) {
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }
  
  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  
  return content;
}

 // Serve the HTML webpage
void serveWebPage() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      String html = loadHTML("/index.html");

       // Add each airport as a list item
    String airportListHtml = "";
    for (size_t i = 0; i < sizeof(airports) / sizeof(airports[0]); i++) {
      // Correctly read the string pointer from PROGMEM using pgm_read_ptr
      const char* airportIcao = (const char*)pgm_read_ptr(&(airports[i]));
      
      // Concatenate the airport to the HTML string
      airportListHtml += "<div>" + String(airportIcao) + "</div>";
    }

    // Apply CSS class based on the number of airports
    size_t numAirports = sizeof(airports) / sizeof(airports[0]);
    if (numAirports > 30) {
        html += "<style>.airport-list { grid-template-columns: repeat(4, 1fr); }</style>";  // Four columns
    } else if (numAirports >= 20) {
        html += "<style>.airport-list { grid-template-columns: repeat(3, 1fr); }</style>";  // Three columns
    } else if (numAirports >= 10) {
        html += "<style>.airport-list { grid-template-columns: repeat(2, 1fr); }</style>";  // Two columns
    }

    // Replace the placeholder with the dynamic airport list
    html.replace("<!-- AIRPORT_LIST_PLACEHOLDER -->", airportListHtml);
    html.replace("{{LED_BRIGHTNESS}}",String(ledBrightness));
    html.replace("{{START_TIME}}",String(settings[1].value));
    html.replace("{{END_TIME}}",String(settings[2].value));

    request->send(200, "text/html", html);
});

server.on("/updatebrightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("brightness", true)) {
        ledBrightness = request->getParam("brightness", true)->value().toInt();
        FastLED.setBrightness(ledBrightness);
        FastLED.show();
        setSettingValue("led_brightness", ledBrightness);
        debugPrint("New Led Brightness: %d\n", ledBrightness);
    }
    request->redirect("/");
});

server.on("/updatestarttime", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("starttime", true)) {
        int startTime = request->getParam("starttime", true)->value().toInt();
        setSettingValue("start_time", startTime);
        debugPrint("New Start Time: %d\n", startTime);
    }
    request->redirect("/");
});

server.on("/updateendtime", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("endtime", true)) {
        int endTime = request->getParam("endtime", true)->value().toInt();
        setSettingValue("end_time", endTime);
        debugPrint("New End Time: %d\n", endTime);
    }
    request->redirect("/");
});

server.on("/fetch", HTTP_GET, [](AsyncWebServerRequest *request) {
    checkMetars();
    request->send(200, "text/plain", "Metar fetch triggered.");
});

// Corrected file upload route with separated POST handler
// server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
//   if (Update.hasError()) {
//       request->send(200, "text/plain", "Update Failed");
//   } else {
//       request->send(200, "text/plain", "Update Success, restarting...");
//       ESP.restart();
//   }
// }, handleFileUpload);

}

 //Attempt to Get OTA File Upload Working
void handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (index == 0) { // Start of file upload
      Serial.println("Upload Start");
      // Prepare the update
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Serial.println("Update failed to begin");
          return;
      }
  }

  // Write the uploaded data
  if (!Update.write(data, len)) {
      Serial.println("Update failed while writing");
      return;
  }

  if (final) {
      // End of file upload
      if (Update.end()) {
          Serial.println("Update complete");
          ESP.restart();  // Restart ESP after successful update
      } else {
          Serial.println("Update failed during finalization");
      }
  }
}

//========================================================Start-Up Functions================================================================//

 // Wi-Fi setup
void setupWiFi() {
  String hostname = "esp_metar_map";
  
  WiFiManager wm;
  if (!wm.autoConnect("ESP-MetarMap")) {
    debugPrint("Failed to connect to Wi-Fi.\n");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  if(!MDNS.begin(hostname)) {
    Serial.println("Error Starting mDNS");
    return;
  }

}

 //Start-up LED Sequence
void testStartupSequence() {
  Serial.println("Starting Up...");

  for (int i = 1; i <= NUM_AIRPORTS; i++) { // Start with 1 LED, end with NUM_AIRPORTS LEDs
      uint8_t thisHue = beatsin8(10, 0, 255); // Generate a changing hue
      fill_rainbow(leds, i, thisHue, 10); // Gradually increase the number of LEDs lit
      FastLED.show();
      delay(200);
  }
 // Keep the animation running with all LEDs on
 for (int j = 0; j < 30; j++) { // Run animation for a set duration
  uint8_t thisHue = beatsin8(10, 0, 255);
  fill_rainbow(leds, NUM_AIRPORTS, thisHue, 10);
  FastLED.show();
  delay(100);
}
  // Turn off all LEDs after the sequence
  fill_solid(leds, NUM_AIRPORTS, CRGB::Black);

  FastLED.show();
}

//========================================================Setup Function====================================================================//

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Setting up Wi-Fi...\n");
  setupWiFi();
  delay(1000);
  //loadBrightness();
  ledBrightness = getSettingValue("led_brightness");
  settings[1].value = getSettingValue("start_time");
  settings[2].value = getSettingValue("end_time");
  //Load Depending which led type
  #ifdef WS2811
  FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_AIRPORTS);
  #else
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_AIRPORTS);
  #endif

  FastLED.setBrightness(ledBrightness);
  FastLED.clear();
  FastLED.show();
  
  //SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  serveWebPage();
  server.begin();
  timeClient.begin();
  testStartupSequence();
 if (isTimeInRange()) {
        Serial.println("Turn ON");
        // Add code to turn on your device
          fetchMetarData();
   }
}

void loop() {
    
 unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= INTERVAL) {
          previousMillis = currentMillis;
        if (isTimeInRange()) {
          Serial.println("Turn ON");
          // Add code to turn on your device
      
        fetchMetarData();
      
    } else {
        Serial.println("Turn OFF");
        fill_solid(leds, NUM_AIRPORTS, CRGB::Black);
        FastLED.show();
        
      }
    }
}