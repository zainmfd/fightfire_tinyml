#include <SHT1x.h>
#include <Wire.h>
#include "TFT_eSPI.h"
#include <SensirionI2CSht4x.h>
#include "FightFire-Zain-Project_inferencing.h"

#include <Arduino.h>
#include<SoftwareSerial.h>
SoftwareSerial e5(0, 1);
static char recv_buf[512];
int counter = 0;
static int at_send_check_response(char *p_ack, int timeout_ms, char *p_cmd, ...)
{
  int ch;
  int num = 0;
  int index = 0;
  int startMillis = 0;
  va_list args;
  memset(recv_buf, 0, sizeof(recv_buf));
  va_start(args, p_cmd);
  e5.print(p_cmd);
  Serial.print(p_cmd);
  va_end(args);
  delay(200);
  startMillis = millis();
  if (p_ack == NULL)
    return 0;
  do
  {
    while (e5.available() > 0)
    {
      ch = e5.read();
      recv_buf[index++] = ch;
      Serial.print((char)ch);
      delay(2);
    }
    if (strstr(recv_buf, p_ack) != NULL)
      return 1;
  }
  while (millis() - startMillis < timeout_ms);
  Serial.println();
  return 0;
}

#define dataPin 6
#define clockPin 8
SHT1x sht1x(dataPin, clockPin);
SensirionI2CSht4x sht4x;

// Settings
#define DEBUG               1                         // 1 to print out debugging info
#define DEBUG_NN            false                     // Print out EI debugging info
#define ANOMALY_THRESHOLD   0.3                       // Scores above this are an "anomaly"
#define SAMPLING_FREQ_HZ    4                         // Sampling frequency (Hz)
#define SAMPLING_PERIOD_MS  1000 / SAMPLING_FREQ_HZ   // Sampling period (ms)
#define NUM_SAMPLES         EI_CLASSIFIER_RAW_SAMPLE_COUNT  // 4 samples at 4 Hz
#define READINGS_PER_SAMPLE EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME // 8

// Preprocessing constants (drop the timestamp column)
float mins[] = {
  29.06, 60.93, 28.88, 13.46
};
float ranges[] = {
  50.17, 16.45, 45.18, 64.8
};

TFT_eSPI tft;   // Wio Terminal LCD

void setup() {
  // Start serial
  Serial.begin(9600);
  e5.begin(9600);
  Serial.print("E5 LOCAL TEST\r\n");
  at_send_check_response("+TEST: RFCFG", 1000, "AT+TEST=RFCFG,866,SF12,125,12,15,14,ON,OFF,OFF\r\n");
  delay(200);
  Wire.begin();
  uint16_t error;
  char errorMessage[256];

  sht4x.begin(Wire);

  // Configure LCD
  tft.begin();
  tft.setRotation(3);
  tft.setFreeFont(&FreeSansBoldOblique24pt7b);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  uint16_t error;
  unsigned long timestamp;
  char errorMessage[256];
  static float raw_buf[NUM_SAMPLES * READINGS_PER_SAMPLE];
  static signal_t signal;
  float temp;
  int max_idx = 0;
  float max_val = 0.0;
  char str_buf[40];
  // Collect samples and perform inference
  for (int i = 0; i < NUM_SAMPLES; i++) {

    // Take timestamp so we can hit our target frequency
    timestamp = millis();

    float soil_temp;
    float soil_humidity;
    float atmtemp;
    float atmhum;
    error = sht4x.measureHighPrecision(atmtemp, atmhum);
    // Read values from the sensor
    soil_temp = sht1x.readTemperatureC();
    soil_humidity = sht1x.readHumidity();


    // Store raw data into the buffer
    raw_buf[(i * READINGS_PER_SAMPLE) + 0] = soil_temp;
    raw_buf[(i * READINGS_PER_SAMPLE) + 1] = soil_humidity;
    raw_buf[(i * READINGS_PER_SAMPLE) + 2] = atmtemp;
    raw_buf[(i * READINGS_PER_SAMPLE) + 3] = atmhum;

    // Perform preprocessing step (normalization) on all readings in the sample
    for (int j = 0; j < READINGS_PER_SAMPLE; j++) {
      temp = raw_buf[(i * READINGS_PER_SAMPLE) + j] - mins[j];
      raw_buf[(i * READINGS_PER_SAMPLE) + j] = temp / ranges[j];
    }

    // Wait just long enough for our sampling period
    while (millis() < timestamp + SAMPLING_PERIOD_MS);
  }

  // Print out our preprocessed, raw buffer
#if DEBUG
  for (int i = 0; i < NUM_SAMPLES * READINGS_PER_SAMPLE; i++) {
    Serial.print(raw_buf[i]);
    if (i < (NUM_SAMPLES * READINGS_PER_SAMPLE) - 1) {
      Serial.print(", ");
    }
  }
  Serial.println();
#endif

  // Turn the raw buffer in a signal which we can the classify
  int err = numpy::signal_from_buffer(raw_buf, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0) {
    ei_printf("ERROR: Failed to create signal from buffer (%d)\r\n", err);
    return;
  }

  // Run inference
  ei_impulse_result_t result = {0};
  err = run_classifier(&signal, &result, DEBUG_NN);
  if (err != EI_IMPULSE_OK) {
    ei_printf("ERROR: Failed to run classifier (%d)\r\n", err);
    return;
  }


  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    ei_printf("\t%s: %.3f\r\n", result.classification[i].label, result.classification[i].value);
  }


  // Find maximum prediction
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > max_val) {
      max_val = result.classification[i].value;
      max_idx = i;
    }
  }
  tft.fillScreen(TFT_BLACK);
  tft.drawString(result.classification[max_idx].label, 20, 60);
  sprintf(str_buf, "%.3f", max_val);
  tft.drawString(str_buf, 60, 120);

  
  String fire_status = result.classification[max_idx].label;
  if (fire_status == "dry")
  {
    Serial.println("fire detected");
    char cmd[128];
    int x=1;
    sprintf(cmd, "AT+TEST=TXLRSTR,\"%d\"\r\n", x);
      int ret = at_send_check_response("TX DONE", 5000, cmd);
  if (ret == 1)
  {
    Serial.print("Sent successfully!\r\n");
  }
  else
  {
    Serial.print("Send failed!\r\n");
  }
  }
  else {
    Serial.println("no fire");

  }

}
