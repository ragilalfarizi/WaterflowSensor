/**
 * Capstone Project 2023 - Smart Home
 * Instagram : @thewatermatters
 * GitHub : xxx
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#define SENSOR 18 // GPIO yang digunakan untuk sensor

/**
 * ssid -> WiFi yang akan digunakan
 * password -> Kata sandi WiFi yang digunakan
 * mqtt_server -> Alamat IP server untuk mqtt
 */
const char *ssid = "POCO X5 5G";
const char *password = "123456789";
const char *mqtt_server = "192.168.148.218";

WiFiClient espClient;
PubSubClient client(espClient);

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
boolean ledState = LOW;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned long flowMilliLitres;
unsigned int totalMilliLitres;
float flowLitres;
float totalLitres;

/**
 * @brief Menghubungkan ulang koneksi MQTT apabila gagal
 * @param none
 * @retval none
 */
void reconnect()
{
  // Loop hingga terkoneksi kembali
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Membuat sebuah client ID acak
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Mencoba menghubungkan
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/**
 * @brief Interrupt Handler Function untuk penghitung pulsa sensor
 * @param none
 * @retval none
 */
void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP Mac Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());
  Serial.println("=============================================");

  pinMode(SENSOR, INPUT_PULLUP);

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;

  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  client.setServer(mqtt_server, 1883);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  StaticJsonDocument<32> doc;
  char output[55];

  currentMillis = millis();
  if (currentMillis - previousMillis > interval)
  {
    pulse1Sec = pulseCount;
    pulseCount = 0;

    /**
     * @details karena loop ini mungkin tidak tepat 1 detik pada intervalnya, kita menghitung
     *          angka millisekon yang telah lewat dari eksekusi terakhir dan menggunakannya
     *          untuk menskalakan output. Kita juga menerapkan faktor kalibrasi untuk menskalakan
     *          output berdasarkan jumlah pulsa per detik per satuan ukuran (liter/menit) yang
     *          berasal dari sensor.
     */

    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();

    /**
     * @details Membagi flow rate dalam liter/menit oleh 60 untuk menentukan berapa banyak liter
     *          yang telah melalui sensor dalam 1 detik interval, kemudian mengalikan dengan 1000
     *          untuk mengubah ke mililiter.
     */

    flowMilliLitres = (flowRate / 60) * 1000;
    flowLitres = (flowRate / 60);

    // Menambahkan nilai millilitres di atas ke jumlah kumulatif
    totalMilliLitres += flowMilliLitres;
    totalLitres += flowLitres;

    // Menampilan nilai flow rate detik ini dalam Litres / minutes
    Serial.print("Flow rate: ");
    Serial.print(float(flowRate));
    Serial.print("L/min");
    Serial.print("\t");

    // Menampilkan nilai kumulatif total dari Liter yang mengalir semenjak mulai
    Serial.print("Output Liquid Quantity: ");
    Serial.print(totalMilliLitres);
    Serial.print("mL / ");
    Serial.print(totalLitres);
    Serial.println("L");

    Serial.println("=================================");

    // Memasukan data FlowRate dan totalLitres ke dalam JSON untuk MQTT
    doc["flowRate"] = flowRate;
    doc["totalLitres"] = totalLitres;

    serializeJson(doc, output);
    Serial.println(output);
    client.publish("/home/sensors", output);
    Serial.println("Sent");
  }
}