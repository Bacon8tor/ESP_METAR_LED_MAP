#include <WiFiManager.h>
#include <HTTPClient.h>
#include <FastLED.h>

// Debug mode
bool debug = true; // Set to false to disable Serial output

// Sleep duration (60 minutes)
#define SLEEP_DURATION 60 * 60 * 1000000ULL // Microseconds

// Define the number of airports
constexpr int NUM_AIRPORTS = 8; // You can change this number

// Airport data
struct Airport {
  String icaoCode;
  int ledIndex;
};
Airport airports[NUM_AIRPORTS] = {
  {"KGYR", 0},
  {"KLUF", 1},
  {"KPHX", 2},
  {"KCHD", 3},
  {"KIWA", 4},
  {"KFFZ", 5},
  {"KSDL", 6},
  {"KDVT", 7}
};

// LED setup
#define NUM_LEDS NUM_AIRPORTS        // Number of LEDs
#define DATA_PIN 5                   // Pin connected to the LED strip
CRGB leds[NUM_LEDS];

// Weather data structure with ICAO code association
struct WeatherData {
  String icaoCode; // ICAO code of the airport
  float visibility; // Visibility in miles
  int ceiling;      // Ceiling height in feet
};

// Debug print helper
void debugPrint(const char* format, ...) {
  if (debug) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
  }
}

// Convert fraction string (e.g., "1/2") to float
float convertFractionToFloat(const String& fraction) {
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

int extractCeiling(const String& response, int start) {
  int ceiling = 99999; // Default to unlimited ceiling (SKC/CLR)
  const char* cloudTypes[] = {"OVC", "BKN", "SCT", "FEW"};

  // Check for clear skies
  if (response.indexOf("CLR", start) != -1 || response.indexOf("SKC", start) != -1) {
    debugPrint("Clear skies or SKC (ceiling: unlimited)\n");
    return ceiling; // Return unlimited ceiling (no clouds)
  }

  // Check for specific cloud types
  for (const char* cloudType : cloudTypes) {
    int cloudStart = response.indexOf(cloudType, start);
    if (cloudStart != -1) {
      String cloudLayer = response.substring(cloudStart, cloudStart + 6);
      String heightStr = cloudLayer.substring(3, 6);

      debugPrint("Found cloud layer: %s, Extracted height string: %s\n", cloudLayer.c_str(), heightStr.c_str());

      ceiling = heightStr.toInt() * 100; // Convert hundreds of feet to feet
      debugPrint("Converted ceiling height: %d feet\n", ceiling);
      return ceiling;
    }
  }

  debugPrint("No significant cloud layers found for this section.\n");
  return ceiling; // Return default (unlimited) ceiling
}


// Parse weather data from the API response
std::vector<WeatherData> parseWeatherData(const String& response) {
  std::vector<WeatherData> weatherData;

  for (size_t i = 0; i < NUM_AIRPORTS; i++) {
    const String& icaoCode = airports[i].icaoCode;
    int start = response.indexOf(icaoCode);

    if (start != -1) {
      debugPrint("Found ICAO code: %s at index: %d\n", icaoCode.c_str(), start);

      int smIndex = response.indexOf("SM", start);
      if (smIndex != -1) {
        int end = smIndex;
        while (end > start && (isDigit(response[end - 1]) || response[end - 1] == '/')) {
          end--;
        }

        String visibilityStr = response.substring(end, smIndex);
        visibilityStr.trim();
       // debugPrint("Extracted visibility string for %s: '%s'\n", icaoCode.c_str(), visibilityStr.c_str());

        float visibility = 0.0;
        if (visibilityStr.indexOf('/') != -1) {
          visibility = convertFractionToFloat(visibilityStr);
        } else {
          visibility = visibilityStr.toFloat();
        }

        debugPrint("Converted visibility for %s: %.2f miles\n", icaoCode.c_str(), visibility);

        int ceiling = extractCeiling(response, start);
        weatherData.push_back({icaoCode, visibility, ceiling});
      }
    } else {
      debugPrint("ICAO code %s not found in response.\n", icaoCode.c_str());
    }
  }

  return weatherData;
}

// Set LED colors based on parsed weather data
void setLEDColors(const std::vector<WeatherData>& weatherData) {
  for (size_t i = 0; i < NUM_AIRPORTS; i++) {
    const String& icaoCode = airports[i].icaoCode;
    auto it = std::find_if(weatherData.begin(), weatherData.end(),
                           [&icaoCode](const WeatherData& data) {
                             return data.icaoCode == icaoCode;
                           });

    if (it != weatherData.end()) {
      float visibility = it->visibility;
      int ceiling = it->ceiling;
      if (visibility > 5 && ceiling > 3000) leds[i] = CRGB::Green;       // VFR
      else if ((visibility >= 3 && visibility <= 5) || (ceiling >= 1000 && ceiling <= 3000)) leds[i] = CRGB::Blue; // MVFR
      else if ((visibility >= 1 && visibility < 3) || (ceiling >= 500 && ceiling < 1000)) leds[i] = CRGB::Red;    // IFR
      else leds[i] = CRGB::Purple;                     // LIFR
      debugPrint("LED %d (%s): Visibility %.2f miles, Ceiling %d feet\n", i, icaoCode.c_str(), visibility, ceiling);
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
  for (size_t i = 0; i < NUM_AIRPORTS; i++) {
    if (i > 0) url += ",";
    url += airports[i].icaoCode;
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

// Initialize Wi-Fi
void setupWiFi() {
  WiFiManager wm;
  if (!wm.autoConnect("WeatherMonitor-AP", "password")) {
    debugPrint("Failed to connect to Wi-Fi.\n");
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Blue; // Signal error
    FastLED.show();
    delay(5000); // Wait before restart
    ESP.restart();
  }
  Serial.println("Wi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Setup function
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Initializing LEDs...\n");
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50); // Set brightness to 50% (range: 0-255)
  FastLED.clear();
  FastLED.show();

  Serial.println("Setting up Wi-Fi...\n");
  setupWiFi();

  Serial.println("Fetching weather data...\n");
  fetchWeatherData();

  Serial.println("Entering deep sleep...\n");
  esp_deep_sleep(SLEEP_DURATION);
}

// Loop function
void loop() {
  // Device will not reach here due to deep sleep
}
