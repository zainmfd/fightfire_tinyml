#include <Arduino.h>
#include<SoftwareSerial.h>
SoftwareSerial e5(0, 1);
#include <rpcWiFi.h>
#include"TFT_eSPI.h"
#include <PubSubClient.h>


// Update these with values suitable for your network.
const char* ssid = "zain"; // WiFi Name
const char* password = "1234512345";  // WiFi Password
const char* mqtt_server = "192.168.3.186";  // MQTT Broker URL

TFT_eSPI tft;
WiFiClient wioClient;
PubSubClient client(wioClient);
long lastMsg = 0;
char msg[50];
int value = 0;


void setup_wifi() {

  delay(10);

  tft.setTextSize(2);
  tft.setCursor((320 - tft.textWidth("Connecting to Wi-Fi..")) / 2, 120);
  tft.print("Connecting to Wi-Fi..");

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // Connecting WiFi

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  tft.fillScreen(TFT_BLACK);
  tft.setCursor((320 - tft.textWidth("Connected!")) / 2, 120);
  tft.print("Connected!");

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); // Display Local IP Address
}

void callback(char* topic, byte* payload, unsigned int length) {
  //tft.fillScreen(TFT_BLACK);
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char buff_p[length];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    buff_p[i] = (char)payload[i];
  }
  Serial.println();
  buff_p[length] = '\0';
  String msg_p = String(buff_p);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor((320 - tft.textWidth("MQTT Message")) / 2, 90);
  tft.print("MQTT Message: " );
  tft.setCursor((320 - tft.textWidth(msg_p)) / 2, 120);
  tft.print(msg_p); // Print receved payload

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "WioTerminal-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("WTout", "hello world");
      // ... and resubscribe
      client.subscribe("WTin");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}




static char recv_buf[512];
static bool is_exist = false;

static int at_send_check_response(char *p_ack, int timeout_ms, char *p_cmd, ...)
{
  int ch = 0;
  int index = 0;
  int startMillis = 0;
  va_list args;
  memset(recv_buf, 0, sizeof(recv_buf));
  va_start(args, p_cmd);
  e5.printf(p_cmd, args);
  Serial.printf(p_cmd, args);
  va_end(args);
  delay(200);
  startMillis = millis();

  if (p_ack == NULL)
  {
    return 0;
  }
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
    {
      return 1;
    }

  } while (millis() - startMillis < timeout_ms);
  return 0;
}
static int recv_prase(void)
{
  char ch;
  int index = 0;
  memset(recv_buf, 0, sizeof(recv_buf));
  while (e5.available() > 0)
  {
    ch = e5.read();
    recv_buf[index++] = ch;
    Serial.print((char)ch);
    delay(2);
  }

  if (index)
  {
    char *p_start = NULL;
    char data[32] = { 
      0,
    };
    int rssi = 0;
    int snr = 0;
    p_start = strstr(recv_buf, "RSSI:");
    if (p_start && (1 == sscanf(p_start, "RSSI:%d,", &rssi)))

      p_start = strstr(recv_buf, "SNR:");
    return 1;
  }
  return 0;
}
static int node_recv(uint32_t timeout_ms)
{
  at_send_check_response("+TEST: RXLRPKT", 1000, "AT+TEST=RXLRPKT\r\n");
  int startMillis = millis();
  do
  {
    if (recv_prase())
    {
      return 1;
    }
  } while (millis() - startMillis < timeout_ms);
  return 0;
}
void setup(void)
{
  Serial.begin(115200);
  // while (!Serial);
  e5.begin(9600);
  Serial.print("Receiver\r\n");


  if (at_send_check_response("+AT: OK", 100, "AT\r\n"))
  {
    is_exist = true;
    at_send_check_response("+MODE: TEST", 1000, "AT+MODE=TEST\r\n");
    at_send_check_response("+TEST: RFCFG", 1000, "AT+TEST=RFCFG,866,SF12,125,12,15,14,ON,OFF,OFF\r\n");
    delay(500);
  }
  else
  {
    is_exist = false;
    Serial.print("No E5 module found.\r\n");
  }

  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(3);


  Serial.println();
  setup_wifi();
  client.setServer(mqtt_server, 1883); // Connect the MQTT Server
  client.setCallback(callback);
}
void loop(void)
{
  if (is_exist)
  {
    node_recv(2000);
  }
  delay(6000);
   if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 50, "Wio Terminal #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("WTout", msg);
  }
}
