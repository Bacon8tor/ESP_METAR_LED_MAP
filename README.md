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
   Modify the list of airports in the code to get METAR data for the desired locations.

3. **Connect to WiFi**
   Boot the ESP and you should see a WiFi named IFR_MAP_WIFI, you will be then able to connect to your Own Wifi Network.

## ISSUES 
1. The Ceilings for each airport needs to be be corrected to choose the lowest ceiling, and that uses the ceiling for each airport and not the others. 

## License

This project is open-source and available under the MIT License.
