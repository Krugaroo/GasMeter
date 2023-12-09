/*
  GasMeter

  This program reads a Gas Meter with an impulse via an reed switch.
  The result is stored sent via POST to the API of mindergas.nl

  The Led will blink whenever an impulse is detected.
  The data is stored to the EEPROM every 5min to allow recovery from a power reset
  The Gas usage is print to the serial
*/
#include <EEPROM.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define IMPULSE_RESOLUTION 0.010f // 0.01 m3 per impulse
#define IMPULSE_DEBOUNCE 250 // 0.25s before a pulse is released or allowed again

#define IMPULSE_SENSOR_PIN 18
#define IMPULSE_SENSOR_ACTIVE_EDGE LOW

#define EEPROM_GAS_ADDRESS_START 0 // memory to save the gas usage 
#define EEPROM_GAS_INITIAL_VALUE 26704.04f // Starting Value m3

#define EEPROM_RESET_ADDRESS_START 16 // memory to save the reset trigger 
#define EEPROM_RESET_VALUE 5 // Change this to force a save and reset of the value, it will check what is stored 

#define LED_ON_TIME (250) // ms
#define SAVE_EEPROM_TIME (6 * 60 * 60 * 1000) // ms
#define MAX_RECONNECT_TIME (10 * 60 * 1000) // ms 

#define TIME_OFFSET 3600

#define TIME_NTP_SERVER "nl.pool.ntp.org"
#define TIME_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"

// Replace with your network credentials (STATION)
const char* ssid = "";
const char* password = "";

float gas_m3 = 0.000f;
uint32_t reset_value = 0;
bool gas_changed = false;

uint32_t time_impulse = 0;
uint32_t time_idle = 0;
bool allow_to_trigger = true;

uint32_t wifi_reconnect_time = 0;
uint32_t wifi_reconnect_interval = 1000;

uint32_t led_on_time = 0;
uint32_t save_time = 0;

bool posted_today = false;
time_t time_now = 0;
time_t time_last = 0;

void save_gas_usage() {
  EEPROM.put(EEPROM_GAS_ADDRESS_START, gas_m3);
  EEPROM.put(EEPROM_RESET_ADDRESS_START, EEPROM_RESET_VALUE);
  EEPROM.commit();
  gas_changed = false;

  Serial.println("Saved to EEPROM: ");
  Serial.print(gas_m3);
  Serial.println(" m3");
  Serial.print("Reset value: ");
  Serial.println(reset_value);
}

void load_gas_usage() {
  EEPROM.get(EEPROM_GAS_ADDRESS_START, gas_m3);
  EEPROM.get(EEPROM_RESET_ADDRESS_START, reset_value);
  
  Serial.println("Loaded from EEPROM: ");
  Serial.print(gas_m3);
  Serial.println(" m3");
  Serial.print("Reset value: ");
  Serial.println(reset_value);

  if (gas_m3 < 1.0f || isnan(gas_m3) || reset_value != EEPROM_RESET_VALUE) {
    Serial.println("First Run No Data in Eeprom or Reset Value mismatch");
    gas_m3 = EEPROM_GAS_INITIAL_VALUE;
    save_gas_usage();
  }
}

void check_gas_impulse() {
    uint8_t current_pin_state = digitalRead(IMPULSE_SENSOR_PIN);

  if (current_pin_state == IMPULSE_SENSOR_ACTIVE_EDGE) {
    time_idle = 0;

    // Check whether there is a gas Impulse Trigger
    if (allow_to_trigger == true) {
      time_impulse++;

      if (time_impulse > IMPULSE_DEBOUNCE) {
        // Impulse seen for the debounce time

        gas_m3 += IMPULSE_RESOLUTION;
        gas_changed = true;

        digitalWrite(LED_BUILTIN, HIGH);
        led_on_time = 0;

        Serial.print("Gas Usage: ");
        Serial.print(gas_m3);
        Serial.println(" m3");

        allow_to_trigger = false;
      }
    }
  } else {
    time_impulse = 0;

    // Idle
    if (allow_to_trigger == false) {
      time_idle++;

      if (time_idle > IMPULSE_DEBOUNCE) {
        // Idle for debounce time
        allow_to_trigger = true;
      }
    }
  }
}

void connect_to_wifi() {
  uint8_t tries = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED && tries < 60) {
    Serial.print('.');
    delay(1000);
    tries++;
  }
  Serial.println("");
  Serial.print("IP address:");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
}

void reconnect_to_wifi() {
  Serial.println("Reconnecting to WiFi...");
  WiFi.disconnect();
  WiFi.reconnect();

  wifi_reconnect_interval = wifi_reconnect_interval * 2;

  if (wifi_reconnect_interval >= MAX_RECONNECT_TIME) {
    wifi_reconnect_interval = MAX_RECONNECT_TIME;
  }
}

void post_gas_usage() {
  tm tm;
  char post_body[100] = {0}; 
  
  localtime_r(&time_now, &tm);

  if ((posted_today == false) && (tm.tm_hour == 13) && true) {
    // Try to post at 1 am
    snprintf(post_body, 99, "{\"date\": \"%04d-%02d-%02d\", \"reading\": %f }", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, gas_m3);
    Serial.println("Sending HTTP Post Request");
    Serial.println(post_body);

    WiFiClient client;
    HTTPClient http;
    
    // Domain name with URL path or IP address with path
   // http.begin(client, serverName);
    http.addHeader("AUTH-TOKEN", "XXXX");
    http.addHeader("Content-Type", "application/json");
    String httpRequestData = "api_key=tPmAT5Ab3j7F9";           
    
    // Send HTTP POST request
    //int httpResponseCode = http.POST(post_body);
     
    Serial.println("HTTP Response code: ");
    //Serial.println(httpResponseCode);

    // If post successful set posted to true
    posted_today = true;
        
    // Free resources
    http.end();
  
  } else if (tm.tm_hour == 3) {
    // Reset it at 3 am
    posted_today = false;
  }
}

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  EEPROM.begin(512);

  // Connect to WIFI
  connect_to_wifi();

  // Get the Time
  configTime(0, 0, TIME_NTP_SERVER);  // 0, 0 because we will use TZ in the next line
  setenv("TZ", TIME_TZ, 1);            // Set environment variable with your time zone
  tzset();

  // Load the gas usage from EEPROM
  load_gas_usage();

  Serial.print("Initial Gas Usage: ");
  Serial.print(gas_m3);
  Serial.println(" m3");
}

// the loop function runs over and over again forever
void loop() {
  // Read Gas Usage
  check_gas_impulse();

  // Turn off the LED
  if (led_on_time >= LED_ON_TIME) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    led_on_time++;
  }

  // Check when to save to EEPROM
  if (save_time >= SAVE_EEPROM_TIME) {
    if (gas_changed == true) {
      save_gas_usage();
      save_time = 0;
    }
  } else {
      save_time++;
  }

  // Check to ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    if (wifi_reconnect_time >= wifi_reconnect_interval) {
      reconnect_to_wifi();
    } else {
      wifi_reconnect_time++;
    }
  } else if (wifi_reconnect_time > 0) {
    // Reset the interval
    wifi_reconnect_interval = 1000;
    wifi_reconnect_time = 0;

    Serial.println("");
    Serial.print("IP address:");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  }

  // If WiFi is connected Update time and post
  if (WiFi.status() == WL_CONNECTED) {
    time(&time_now);
    if (time_now != time_last) {
      time_last = time_now;

      // Check when to post
      post_gas_usage();
    }
  }

  delay(1);
}
