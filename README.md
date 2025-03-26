# ESP_IFR_LED_MAP

This project was developed using ChatGPT and interacts with the [Aviation Weather API](https://aviationweather.gov/) to retrieve METAR data for a list of specified airports. Based on the cloud ceiling and visibility data, it determines the flight conditions and displays them using color-coded LEDs.

## Flight Category Definitions

The flight categories are determined by the following parameters:

- **VFR (Visual Flight Rules) - Green**  
  Visibility greater than 5 miles and ceiling greater than 3000ft AGL.

- **MVFR (Marginal Visual Flight Rules) - Blue**  
  Visibility between 3 and 5 miles, and/or ceiling between 1000ft AGL and 3000ft AGL.

- **IFR (Instrument Flight Rules) - Red**  
  Visibility between 1 mile and less than 3 miles, and/or ceiling between 500ft AGL and below 1000ft AGL.

- **LIFR (Low Instrument Flight Rules) - Purple**  
  Visibility less than 1 mile, and/or ceiling below 500ft AGL.

## Libraries Used

|-- ArduinoJson @ 7.3.1
|-- AsyncTCP-esphome @ 2.1.4
|-- ESPAsyncWebServer-esphome @ 3.1.0
|-- FastLED @ 3.9.13
|-- HTTPClient @ 2.0.0
|-- NTPClient @ 3.2.1
|-- Preferences @ 2.0.0
|-- WiFiManager @ 2.0.17

## Setup

1. **Install required libraries:**
   - WiFiManager
   - HttpClient
   - FastLED

2. **Configure Airports:**
   Modify the list of airports in the code to get METAR data for the desired locations. Using the Airports ICAO Code, order them according to how you laid out the LEDS on Your map,starting with 0(zero).
   `// Variables Place in same order as leds are wired
String airports[] = {"KCHD", "KPHX", "KGYR", "KGEU", "KDVT", 
                     "KSDL", "KFFZ", "KIWA", "KSRQ", "KSPG",
                     "KPIE", "KTPA", "KBKV", "KZPH", "KLAL"};`
5. **Data Pin**
  Make sure you have your data pin set to your Data Pin on your leds. 
6. **Upload**
  Project should work with any flavor of the ESP32 Family, but I have had the best luck with the more common ESP32 Dev Module. Make sure you have your esp32 boards installed, as well as the libraries above. 
7. **Connect to WiFi**
   Boot the ESP and you should see a WiFi named IFR_MAP_WIFI, you will be then able to connect to your Own Wifi Network.

## ISSUES 
1. The Ceilings for each airport needs to be be corrected to choose the lowest ceiling, and that uses the ceiling for each airport and not the others. -FIXED


## Platformio Project
I have made this project into a platformio project, and have included the library folders locally. 


## License

This project is open-source and available under the MIT License.
