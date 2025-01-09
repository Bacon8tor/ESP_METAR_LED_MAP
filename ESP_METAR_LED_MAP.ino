#include <WiFiManager.h>
#include <HTTPClient.h>
#include <FastLED.h>

// Debug mode
bool debug = false; // Set to false to disable Serial output

// Sleep duration (60 minutes)
#define SLEEP_DURATION 60 * 60 * 1000000ULL // Microseconds

// Define the number of airports
constexpr int NUM_AIRPORTS = 49; // You can change this number

// Airport data
struct Airport {
  String icaoCode;
  int ledIndex;
};
Airport airports[] = {
  {"KSFO", 0},  // San Francisco International Airport
  {"KLAX", 1},  // Los Angeles International Airport
  {"KPHX", 2},  // Phoenix Sky Harbor International Airport
  {"KJFK", 3},  // John F. Kennedy International Airport*
  {"KORD", 4},  // Chicago O'Hare International Airport
  {"KATL", 5},  // Hartsfield-Jackson Atlanta International Airport
  {"KDFW", 6},  // Dallas/Fort Worth International Airport
  {"KDEN", 7},  // Denver International Airport
  {"KMIA", 8},  // Miami International Airport
  {"KSEA", 9},  // Seattle-Tacoma International Airport
  {"KIAH", 10}, // George Bush Intercontinental Airport
  {"KBOS", 11}, // Boston Logan International Airport*
  {"KLAS", 12}, // Harry Reid International Airport (Las Vegas)
  {"KEWR", 13}, // Newark Liberty International Airport
  {"KDTW", 14}, // *Detroit Metropolitan Airport
  {"KMCO", 15}, // Orlando International Airport
  {"KPHL", 16}, // *Philadelphia International Airport
  {"KSLC", 17}, // Salt Lake City International Airport
  {"KMSP", 18}, // Minneapolis-Saint Paul International Airport
  {"KCLT", 19}, // Charlotte Douglas International Airport
  {"KTPA", 20}, // Tampa International Airport
  {"KBWI", 21}, // Baltimore/Washington International Thurgood Marshall Airport
  {"KSAN", 22}, // San Diego International Airport
  {"KPDX", 23}, // Portland International Airport
  {"KHNL", 24}, // Daniel K. Inouye International Airport (Honolulu)
  {"KANC", 25}, // Ted Stevens Anchorage International Airport
  {"KSTL", 26}, // St. Louis Lambert International Airport
  {"KMDW", 27}, // Chicago Midway International Airport
  {"KOAK", 28}, // Oakland International Airport
  {"KSJC", 29}, // San Jose International Airport
  {"KSMF", 30}, // Sacramento International Airport
  {"KFLL", 31}, // Fort Lauderdale-Hollywood International Airport
  {"KRDU", 32}, // Raleigh-Durham International Airport
  {"KAUS", 33}, // Austin-Bergstrom International Airport
  {"KSAT", 34}, // San Antonio International Airport
  {"KPIT", 35}, // *Pittsburgh International Airport
  {"KIND", 36}, // Indianapolis International Airport
  {"KMEM", 37}, // Memphis International Airport
  {"KCVG", 38}, // *Cincinnati/Northern Kentucky International Airport
  {"KCMH", 39}, // John Glenn Columbus International Airport
  {"KOKC", 40}, // Will Rogers World Airport (Oklahoma City)
  {"KMSY", 41}, // Louis Armstrong New Orleans International Airport
  {"KPBI", 42}, // Palm Beach International Airport
  {"KSNA", 43}, // John Wayne Airport (Orange County)
  {"KJAX", 44}, // Jacksonville International Airport
  {"KBUF", 45}, // Buffalo Niagara International Airport
  {"KRNO", 46}, // Reno-Tahoe International Airport
  {"KSYR", 47}, // Syracuse Hancock International Airport
  {"KABQ", 48}, // Albuquerque International Sunport
  {"KPWM", 49}  // Portland International Jetport (Maine)
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
  int lowestCeiling = 99999; // Default to unlimited ceiling (no clouds)
  const char* cloudTypes[] = {"OVC", "BKN"};//, "SCT", "FEW"};

  // Iterate through all possible cloud types
  for (const char* cloudType : cloudTypes) {
    int cloudStart = response.indexOf(cloudType, start);
    while (cloudStart != -1) {  // Search for all layers of this cloud type
      String cloudLayer = response.substring(cloudStart, cloudStart + 6); // Extract the layer (e.g., OVC049)
      String heightStr = cloudLayer.substring(3, 6); // Get the height string (e.g., "049")
      
      // Check if heightStr contains a valid number
      if (heightStr.length() == 3 && isDigit(heightStr[0]) && isDigit(heightStr[1]) && isDigit(heightStr[2])) {
        int height = heightStr.toInt() * 100; // Convert height from hundreds to feet
        if (height < lowestCeiling) { // Check if this is the lowest ceiling found so far
          lowestCeiling = height;
        }
      }

      debugPrint("Cloud type: %s, Height: %d feet\n", cloudType, lowestCeiling);
      cloudStart = response.indexOf(cloudType, cloudStart + 1); // Continue searching
    }
  }

  debugPrint("Final lowest ceiling: %d feet\n", lowestCeiling);
  return lowestCeiling;
}


// Parse weather data from the API response
std::vector<WeatherData> parseWeatherData(const String& response) {
  std::vector<WeatherData> weatherData;

  // Split the response into individual METAR data entries based on newlines
  int startIdx = 0;
  for (size_t i = 0; i < NUM_AIRPORTS; i++) {
    const String& icaoCode = airports[i].icaoCode;
    int icaoIndex = response.indexOf(icaoCode, startIdx);

    if (icaoIndex != -1) {
      // Find the newline character after the METAR data for this airport
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

        int ceiling = extractCeiling(airportMetar, 0); // Pass the airport METAR data to extract ceiling
        weatherData.push_back({icaoCode, visibility, ceiling});
      }

      startIdx = endIdx + 1;  // Update startIdx to continue to next METAR data
    } else {
      debugPrint("ICAO code %s not found in response.\n", icaoCode.c_str());
    }
  }

  return weatherData;
}

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

      String condition = getFlightCondition(visibility, ceiling);
      
      if (visibility > 5 && ceiling > 3000) leds[i] = CRGB::Green;       // VFR
      else if ((visibility >= 3 && visibility <= 5) || (ceiling >= 1000 && ceiling <= 3000)) leds[i] = CRGB::Blue; // MVFR
      else if ((visibility >= 1 && visibility < 3) || (ceiling >= 500 && ceiling < 1000)) leds[i] = CRGB::Red;    // IFR
      else leds[i] = CRGB::Purple;                     // LIFR
      char buffer[128]; // Adjust size as needed
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
  if (!wm.autoConnect("IFR_MAP_WIFI")) {
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
