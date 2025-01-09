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

- [WiFiManager](https://github.com/tzapu/WiFiManager): Used to manage Wi-Fi connections easily.
- [HttpClient](https://github.com/amcewen/HttpClient): Used for making HTTP requests.
- [FastLED](https://github.com/FastLED/FastLED): Controls WS2812B LEDs to display the flight category status.

## Setup

1. **Install required libraries:**
   - WiFiManager
   - HttpClient
   - FastLED

2. **Configure Airports:**
   Modify the list of airports in the code to get METAR data for the desired locations. Using the Airports ICAO Code, order them according to how you laid out the LEDS on Your map,starting with 0(zero).
   `Airport airports[] = {
   //{"ICAO",LED_POSITION} //Example
  {"KSFO", 0},  // San Francisco International Airport
  {"KLAX", 1},  // Los Angeles International Airport
  {"KPHX", 2},  // Phoenix Sky Harbor International Airport
  {"KJFK", 3},  // John F. Kennedy International Airport*
  {"KORD", 4}  // Chicago O'Hare International Airport
  };`
4. **Change NUM_AIRPORTS**
  Change this to how many airports you added above.
5. **Data Pin**
  Make sure you have your data pin set to your Data Pin on your leds. 
6. **Upload**
  Project should work with any flavor of the ESP32 Family, but I have had the best luck with the more common ESP32 Dev Module. Make sure you have your esp32 boards installed, as well as the libraries above. 
7. **Connect to WiFi**
   Boot the ESP and you should see a WiFi named IFR_MAP_WIFI, you will be then able to connect to your Own Wifi Network.

## ISSUES 
1. The Ceilings for each airport needs to be be corrected to choose the lowest ceiling, and that uses the ceiling for each airport and not the others. -FIXED

## Wokwi Project 
If you want to experiment with this code try Wokwi its a free esp simulator, I have adapted the project to work with their Wifi connection. I have noticed sometimes it doesnt work and sometimes takes a long time to compile. [Wokwi_Metar_Project](https://wokwi.com/projects/418459180318780417) .

## Arduino Cloud Sketch
You can also use the arduino cloud editor to edit and upload the sketch. 
[Arduino_Cloud_Sketch](https://app.arduino.cc/sketches/994ec304-6f9b-4d0b-b816-7703e34aebc3?view-mode=preview)
## License

This project is open-source and available under the MIT License.
