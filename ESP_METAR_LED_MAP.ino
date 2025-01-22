#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>

Preferences preferences;

// Define a key for storing the brightness value
const char* brightnessKey = "led_brightness";

int ledBrightness = 125; // Default brightness

const char* hostname = "ESP32-LED-METAR-MAP";

// Debug mode
bool debug = true; // Set to false to disable Serial output

// Timing interval (5 minutes)
constexpr unsigned long INTERVAL = 5 * 60 * 1000; // Milliseconds

// Variables Place in same order as leds are wired
String airports[] = {"KSFO", "KLAX", "KPHX", "KJFK", "KORD", 
                     "KATL", "KDFW", "KDEN", "KMIA", "KSEA", 
                     "KIAH", "KBOS", "KLAS", "KEWR", "KDTW", 
                     "KMCO", "KPHL", "KSLC", "KMSP", "KCLT", 
                     "KTPA", "KBWI", "KSAN", "KPDX", "KHNL", 
                     "KANC", "KSTL", "KMDW", "KOAK", "KSJC", 
                     "KSMF", "KFLL", "KRDU", "KAUS", "KSAT", 
                     "KPIT", "KIND", "KMEM", "KCVG", "KCMH", 
                     "KOKC", "KMSY", "KPBI", "KSNA", "KJAX", 
                     "KBUF", "KRNO", "KSYR", "KABQ", "KPWM"};


unsigned long previousMillis = 0;

// LED setup
const int NUM_LEDS = sizeof(airports) / sizeof(airports[0]);
#define DATA_PIN 25
CRGB leds[NUM_LEDS];
// Web server setup
AsyncWebServer server(80);

// Weather data structure with ICAO code association
struct WeatherData {
    String icaoCode;
    float visibility;
    int ceiling;
};


// Debug print helper
void debugPrint(const char *format, ...) {
    if (debug) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}
//Save and load brightness 
void saveBrightness() {
    preferences.begin("led_config", false);  // Open the "led_config" namespace for writing
    preferences.putInt(brightnessKey, ledBrightness);  // Store the ledBrightness value
    preferences.end();  // Close the preferences
  debugPrint("Led Brightness saved to %d\n",ledBrightness);
}

void loadBrightness() {
    preferences.begin("led_config", true);  // Open the "led_config" namespace for reading
    ledBrightness = preferences.getInt(brightnessKey, 125);  // Read the saved brightness value or default to 125
    preferences.end();  // Close the preferences
  debugPrint("Led Brightness loaded to %d\n",ledBrightness);
}

// Convert fraction string (e.g., "1/2") to float
float convertFractionToFloat(const String &fraction) {
    int slashIndex = fraction.indexOf('/');
    if (slashIndex > 0) {
        int numerator = fraction.substring(0, slashIndex).toInt();
        int denominator = fraction.substring(slashIndex + 1).toInt();
        if (denominator != 0) {
            return (float)numerator / (float)denominator;
        }
    }
    return 0.0; // Default for invalid input
}

// Extract ceiling information from METAR response
int extractCeiling(const String &response, int start) {
    int lowestCeiling = 99999; // Default to unlimited ceiling (no clouds)
    const char *cloudTypes[] = {"OVC", "BKN", "SCT", "FEW"};

    for (const char *cloudType : cloudTypes) {
        int cloudStart = response.indexOf(cloudType, start);
        while (cloudStart != -1) {
            String cloudLayer = response.substring(cloudStart, cloudStart + 6);
            String heightStr = cloudLayer.substring(3, 6);

            if (heightStr.length() == 3 && isDigit(heightStr[0]) && isDigit(heightStr[1]) && isDigit(heightStr[2])) {
                int height = heightStr.toInt() * 100;
                if (height < lowestCeiling) {
                    lowestCeiling = height;
                }
            }

            debugPrint("Cloud type: %s, Height: %d feet\n", cloudType, lowestCeiling);
            cloudStart = response.indexOf(cloudType, cloudStart + 1);
        }
    }

    debugPrint("Final lowest ceiling: %d feet\n", lowestCeiling);
    return lowestCeiling;
}

// Parse weather data from the API response
std::vector<WeatherData> parseWeatherData(const String &response) {
    std::vector<WeatherData> weatherData;

    int startIdx = 0;
    for (size_t i = 0; i < NUM_LEDS; i++) {
        const String &icaoCode = airports[i];
        int icaoIndex = response.indexOf(icaoCode, startIdx);

        if (icaoIndex != -1) {
            int endIdx = response.indexOf("\n", icaoIndex);
            String airportMetar = response.substring(icaoIndex, endIdx != -1 ? endIdx : response.length());

            debugPrint("Found METAR for %s: %s\n", icaoCode.c_str(), airportMetar.c_str());

            int smIndex = airportMetar.indexOf("SM");
            if (smIndex != -1) {
                int end = smIndex;
                while (end > 0 && (isDigit(airportMetar[end - 1]) || airportMetar[end - 1] == '/')) {
                    end--;
                }

                String visibilityStr = airportMetar.substring(end, smIndex);
                visibilityStr.trim();

                float visibility = 0.0;
                if (visibilityStr.indexOf('/') != -1) {
                    visibility = convertFractionToFloat(visibilityStr);
                } else {
                    visibility = visibilityStr.toFloat();
                }

                debugPrint("Converted visibility for %s: %.2f miles\n", icaoCode.c_str(), visibility);

                int ceiling = extractCeiling(airportMetar, 0);
                weatherData.push_back({icaoCode, visibility, ceiling});
            }

            startIdx = endIdx + 1;
        } else {
            debugPrint("ICAO code %s not found in response.\n", icaoCode.c_str());
        }
    }

    return weatherData;
}

// Determine flight condition based on visibility and ceiling
String getFlightCondition(float visibility, int ceiling) {
    if (ceiling < 500 || visibility < 1.0) {
        return "LIFR"; // Low Instrument Flight Rules
    } else if (ceiling < 1000 || visibility < 3.0) {
        return "IFR"; // Instrument Flight Rules
    } else if (ceiling < 3000 || visibility < 5.0) {
        return "MVFR"; // Marginal VFR
    } else {
        return "VFR"; // Visual Flight Rules
    }
}

// Set LED colors based on parsed weather data
void setLEDColors(const std::vector<WeatherData> &weatherData) {
    for (size_t i = 0; i < NUM_LEDS; i++) {
        const String &icaoCode = airports[i];
        auto it = std::find_if(weatherData.begin(), weatherData.end(),
                               [&icaoCode](const WeatherData &data) {
                                   return data.icaoCode == icaoCode;
                               });

        if (it != weatherData.end()) {
            float visibility = it->visibility;
            int ceiling = it->ceiling;

            String condition = getFlightCondition(visibility, ceiling);

            if (visibility > 5 && ceiling > 3000)
                leds[i] = CRGB::Green; // VFR
            else if ((visibility >= 3 && visibility <= 5) || (ceiling >= 1000 && ceiling <= 3000))
                leds[i] = CRGB::Blue; // MVFR
            else if ((visibility >= 1 && visibility < 3) || (ceiling >= 500 && ceiling < 1000))
                leds[i] = CRGB::Red; // IFR
            else
                leds[i] = CRGB::Purple; // LIFR

            char buffer[128];
            snprintf(buffer, sizeof(buffer), "LED %d (%s): Visibility %.2f miles, Ceiling %d feet, Condition: %s", i, icaoCode.c_str(), visibility, ceiling, condition.c_str());
            Serial.println(buffer);

        } else {
            leds[i] = CRGB::Black; // Default off for missing data
            debugPrint("LED %d (%s): No data available\n", i, icaoCode.c_str());
        }
    }

    FastLED.show();
}

// Fetch weather data from the API
void fetchWeatherData() {
    String url = "https://aviationweather.gov/api/data/metar?ids=";
    for (size_t i = 0; i < NUM_LEDS; i++) {
        if (i > 0) url += ",";
        url += airports[i];
    }

    debugPrint("Fetching weather data from: %s\n", url.c_str());
    HTTPClient http;
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        debugPrint("API Response:\n%s\n", payload.c_str());
        auto weatherData = parseWeatherData(payload);
        setLEDColors(weatherData);
    } else {
        debugPrint("HTTP request failed with code: %d\n", httpCode);
        setLEDColors({}); // Default LEDs to off on failure
    }
    http.end();
}

// Serve the HTML webpage
void serveWebPage() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"rawliteral(
            <!DOCTYPE html>
            <html>
            <head>
                <title>ESP Metar Map Config</title>
                <!-- Bootstrap CSS -->
                <link href="https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css" rel="stylesheet">
                <style>
                    body { margin: 20px; }
                    .container { max-width: 600px; }
                    .form-group { margin-bottom: 1rem; }

                    /* Default to one column */
                    .airport-list {
                        display: grid;
                        grid-template-columns: repeat(1, 1fr); /* 1 column by default */
                        gap: 10px;
                    }

                    /* For 10-19 airports, switch to two columns */
                    .airport-list.two-columns {
                        grid-template-columns: repeat(2, 1fr);
                    }

                    /* For 20-29 airports, switch to three columns */
                    .airport-list.three-columns {
                        grid-template-columns: repeat(3, 1fr);
                    }

                    /* For 30+ airports, switch to four columns */
                    .airport-list.four-columns {
                        grid-template-columns: repeat(4, 1fr);
                    }
                </style>
            </head>
            <body>
                <div class="container">
                    <h1 class="text-center">ESP Metar Map Config</h1>
                    <form action="/update" method="POST">
                        <div class="form-group">
                            <label for="brightness">LED Brightness (0-255):</label>
                            <input type="range" id="brightness" name="brightness" class="form-control-range" min="0" max="255" value=")rawliteral";
        html += String(ledBrightness);
        html += R"rawliteral(" step="1">
                        </div>
                        <button type="submit" class="btn btn-primary btn-block">Update Settings</button>
                    </form>
                    <br>
                    <button onclick="fetchWeather()" class="btn btn-secondary btn-block">Fetch Weather Data</button>
                    
                    <!-- Move the airport list below -->
                    <p>Airports being monitored are:</p>
                    <div class="airport-list">
        )rawliteral";

        // Add each airport with a number in front
        for (size_t i = 0; i < sizeof(airports) / sizeof(airports[0]); i++) {
            html += "<div>" + String(i + 1) + ". " + airports[i] + "</div>";
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

        html += R"rawliteral(
                    </div>
                </div>

                <!-- Bootstrap JS, Popper.js, and jQuery (required for Bootstrap components) -->
                <script src="https://code.jquery.com/jquery-3.5.1.slim.min.js"></script>
                <script src="https://cdn.jsdelivr.net/npm/@popperjs/core@2.5.2/dist/umd/popper.min.js"></script>
                <script src="https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/js/bootstrap.min.js"></script>
                <script>
                    function fetchWeather() {
                        fetch('/fetch');
                        alert('Weather data fetch triggered!');
                    }
                </script>
            </body>
            </html>
        )rawliteral";

        request->send(200, "text/html", html);
    });

    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
       
        if (request->hasParam("brightness", true)) {
            ledBrightness = request->getParam("brightness", true)->value().toInt();
            FastLED.setBrightness(ledBrightness);
            FastLED.show();
          // Save the new brightness to EEPROM
            saveBrightness();
          debugPrint("New Led Brightness: %d\n",ledBrightness);
        }
        request->redirect("/");
    });

    server.on("/fetch", HTTP_GET, [](AsyncWebServerRequest *request) {
        fetchWeatherData();
        request->send(200, "text/plain", "Weather fetch triggered.");
    });
}

void setupWiFi() {
    WiFiManager wm;
    if (!wm.autoConnect("METAR_LED_MAP")) {
        debugPrint("Failed to connect to Wi-Fi.\n");
        for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Blue;
        FastLED.show();
        delay(5000);
        ESP.restart();
    }
    Serial.println("Wi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
   // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(hostname);

if (!MDNS.begin("ESP-MetarMap")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");


}

void setup() {
    Serial.begin(115200);
    delay(1000);

    loadBrightness();
  
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(ledBrightness);
    FastLED.clear();
    FastLED.show();

    setupWiFi();
    serveWebPage();
    server.begin();
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
   fetchWeatherData();
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= INTERVAL) {
        previousMillis = currentMillis;
        fetchWeatherData();
    }
}
