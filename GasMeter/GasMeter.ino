/*
  GasMeter

  This program reads a Gas Meter with an impulse via an reed switch.
  The result is stored sent via POST to the API of mindergas.nl

  The Led will blink whenever an impulse is detected.
  The data is stored to the EEPROM every 5min to allow recovery from a power reset
  The Gas usage is print to the serial
*/
#include <EEPROM.h>

#define IMPULSE_RESOLUTION 0.010f // 0.01 m3 per impulse
#define IMPULSE_DEBOUNCE 2500 // 2.5s before a pulse is released or allowed again

#define IMPULSE_SENSOR_PIN 18
#define IMPULSE_SENSOR_ACTIVE_EDGE LOW

#define EEPROM_GAS_ADDRESS_START 0 // memory to save the gas usage 
#define EEPROM_GAS_INITIAL_VALUE 26702.960f // Starting Value m3

#define EEPROM_RESET_ADDRESS_START 16 // memory to save the reset trigger 
#define EEPROM_RESET_VALUE 3 // Change this to force a save and reset of the value, it will check what is stored 

#define LED_ON_TIME (250) // ms
#define SAVE_EEPROM_TIME (6 * 60 * 60 * 1000) // ms

float gas_m3 = 0.000f;
uint32_t reset_value = 0;

uint32_t time_impulse = 0;
uint32_t time_idle = 0;
bool allow_to_trigger = true;

uint32_t led_on_time = 0;
uint32_t save_time = 0;
uint32_t post_time = 0;

void save_gas_usage() {
  EEPROM.put(EEPROM_GAS_ADDRESS_START, gas_m3);
  EEPROM.put(EEPROM_RESET_ADDRESS_START, EEPROM_RESET_VALUE);
  EEPROM.commit();

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

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  EEPROM.begin(512);

  // Load the gas usage from EEPROM
  load_gas_usage();

  Serial.print("Initial Gas Usage: ");
  Serial.print(gas_m3);
  Serial.println(" m3");

  // Connect to WIFI
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
  save_time++;
  if (save_time >= SAVE_EEPROM_TIME) {
    save_gas_usage();
    save_time = 0;
  }

  // Check when to POST

  delay(1);

}
