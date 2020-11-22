#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <BH1750FVI.h> // Sensor Library
#include <Wire.h> // I2C Library

#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT11

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
unsigned long lastMillis = 0;
uint16_t Light_Intensity = 0;
uint16_t SensorValue = 0;
BH1750FVI LightSensor;

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);
DHT dht(DHTPIN, DHTTYPE);

int LED = 13;
int Motion = 26;
int Motion_value = 0;

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.print("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
}

void publishMessage()
{
  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    float f = dht.readTemperature(true);
  
    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }
  
    // Compute heat index in Fahrenheit (the default)
    float hif = dht.computeHeatIndex(f, h);
    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);
  
    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
    Serial.print(F("°C "));
    Serial.print(f);
    Serial.print(F("°F  Heat index: "));
    Serial.print(hic);
    Serial.print(F("°C "));
    Serial.print(hif);
    Serial.println(F("°F"));
    delay(2000);

    Light_Intensity = LightSensor.GetLightIntensity();
    delay(50);

    //Convert from scale (0, 65535) to (255, 0)
    SensorValue = map(Light_Intensity, 0, 20000, 255, 0);
    //Force the value must be inside the range (0,255)
    SensorValue = constrain(SensorValue,255,0);
    digitalWrite(LED, SensorValue);

    Serial.println(SensorValue);

    Motion_value = digitalRead(Motion);
    if(Motion_value == HIGH){
      Serial.println("Motion detected!");
    }
    else if(Motion_value == LOW){
      Serial.println("No motion detected!");
    }
    
    StaticJsonDocument<200> doc;
    doc["time"] = millis();
    doc["temperature"] = t;
    doc["humidity"] = h;
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer); // print to client
  
    client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  }
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];

  int mess_led = int(doc["led"]);
  
  Serial.println(mess_led);

  if(mess_led == 1) { //if is pressed
     digitalWrite(LED, SensorValue); //write 1 or HIGH to led pin
  }
  else if(mess_led == 0){
    digitalWrite(LED, SensorValue);
  }
}

void setup() {
  Serial.begin(9600);
  connectAWS();
  dht.begin();
  LightSensor.begin();
  pinMode(LED, OUTPUT);
  pinMode(Motion, INPUT);
  LightSensor.SetAddress(Device_Address_L); //Address 0x5C
  LightSensor.SetMode(Continuous_H_resolution_Mode);
}

void loop() {
  publishMessage();
  client.loop();
}
