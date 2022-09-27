#include <Wire.h>
#include <SHT1x.h>
#include <SensirionI2CSht4x.h>
#define BTN_START           0                         // 1: press button to start, 0: loop
#define BTN_PIN             WIO_5S_PRESS              // Pin that button is connected to
#define SAMPLING_FREQ_HZ    4                         // Sampling frequency (Hz)
#define SAMPLING_PERIOD_MS  1000 / SAMPLING_FREQ_HZ   // Sampling period (ms)
#define NUM_SAMPLES         8                         // 8 samples at 4 Hz is 2 seconds
#define dataPin 6
#define clockPin 8
SHT1x sht1x(dataPin, clockPin);
SensirionI2CSht4x sht4x;

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Wire.begin();
  uint16_t error;
  char errorMessage[256];
  sht4x.begin(Wire);
  uint32_t serialNumber;
}

void loop() {
  uint16_t error;
  unsigned long timestamp;
  char errorMessage[256];

  float soil_temp;
  float soil_humidity;
  float temperature;
  float humidity;

  // Read values from the sensor
  error = sht4x.measureHighPrecision(temperature, humidity);
  soil_temp = sht1x.readTemperatureC();
  soil_humidity = sht1x.readHumidity();


#if BTN_START
  while (digitalRead(BTN_PIN) == 1);
#endif

  // Print header
  Serial.println("timestamp,soiltemp,soilhum,atmtemp,atmhum");

  // Transmit samples over serial port
  for (int i = 0; i < NUM_SAMPLES; i++) {

    // Take timestamp so we can hit our target frequency
    timestamp = millis();

    // Print CSV data with timestamp
    Serial.print(timestamp);
    Serial.print(",");
    Serial.print(soil_temp);
    Serial.print(",");
    Serial.print(soil_humidity);
    Serial.print(",");
    Serial.print(temperature);
    Serial.print(",");
    Serial.print(humidity);
    Serial.println();

    // Wait just long enough for our sampling period
    while (millis() < timestamp + SAMPLING_PERIOD_MS);
  }

  // Print empty line to transmit termination of recording
  Serial.println();

  // Make sure the button has been released for a few milliseconds
#if BTN_START
  while (digitalRead(BTN_PIN) == 0);
  delay(100);
#endif
}
