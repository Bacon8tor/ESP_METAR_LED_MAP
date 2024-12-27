#include <WiFiManager.h>
#include <HTTPClient.h>
#include <FastLED.h>

// Debug mode
bool debug = true; // Set to false to disable Serial output



// Sleep duration (30 minutes)
#define SLEEP_DURATION 30 * 60 * 1000000ULL // Microseconds

// Define the number of airports
constexpr int NUM_AIRPORTS = 4; // You can change this number

// Airport data
struct Airport {
  String icaoCode;
  int ledIndex;
};
Airport airports[NUM_AIRPORTS] = {
  {"KTVC", 0},
  {"KPHX", 1},
  {"KMCI", 2},
  {"KDRO", 3}
};

// LED setup
#define NUM_LEDS NUM_AIRPORTS        // Number of LEDs
#define DATA_PIN 5        // Pin connected to the LED strip
CRGB leds[NUM_LEDS];

// Weather data
struct WeatherData {
  float visibility; // Visibility in miles
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

// Set LED colors based on visibility
void setLEDColors(const std::vector<WeatherData>& weatherData) {
  for (size_t i = 0; i < NUM_LEDS; i++) {
    if (i < weatherData.size()) {
      float visibility = weatherData[i].visibility;
      if (visibility > 5) leds[i] = CRGB::Green;       // VFR
      else if (visibility >= 3) leds[i] = CRGB::Yellow; // MVFR
      else if (visibility >= 1) leds[i] = CRGB::Red;    // IFR
      else leds[i] = CRGB::Purple;                     // LIFR
    } else {
      leds[i] = CRGB::Black; // Default off
    }
    debugPrint("LED %d: Visibility %.2f miles\n", i, weatherData[i].visibility);
  }
  FastLED.show();
}

// Parse weather data from the API response
std::vector<WeatherData> parseWeatherData(const String& response) {
  std::vector<WeatherData> weatherData;
  int start = 0;

  while ((start = response.indexOf("SM", start)) != -1) {
    debugPrint("Found 'SM' at index: %d\n", start);

    int end = start;
    while (end > 0 && (isDigit(response[end - 1]) || response[end - 1] == '/')) {
      end--;
    }

    String visibilityStr = response.substring(end, start);
    visibilityStr.trim();
    debugPrint("Extracted raw visibility string: '%s'\n", visibilityStr.c_str());

    float visibility = 0.0;
    if (visibilityStr.indexOf('/') != -1) {
      visibility = convertFractionToFloat(visibilityStr);
    } else {
      visibility = visibilityStr.toFloat();
    }

    debugPrint("Converted visibility: %.2f miles\n", visibility);
    weatherData.push_back({visibility});
    start += 2;
  }

  return weatherData;
}

// Fetch weather data from the API
void fetchWeatherData() {
  String url = "https://aviationweather.gov/api/data/metar?ids=";
  for (size_t i = 0; i < sizeof(airports) / sizeof(airports[0]); i++) {
    if (i > 0) url += "%2C";
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
