#include <Arduino.h>

#include <WiFi.h>
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <Adafruit_SCD30.h>

#include "RTClib.h"
#include "private_data.h"

RTC_DS3231 rtc;



void WiFiEvent(WiFiEvent_t event);

uint64_t chipid;
HTTPClient http;

Adafruit_SCD30  scd30;

typedef struct 
  {
    float co2;
    float rh;
    float temp;
  } SCD30_DATA;

SCD30_DATA scd30_data;

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

int get_scd30_data(SCD30_DATA* data)
{
    if (scd30.dataReady())
    {
      if (!scd30.read()){ return -1; }

      data->co2 = scd30.CO2;
      data->rh = scd30.relative_humidity;
      data->temp = scd30.temperature;

      return 1;
  } else {
    return 0;
  }
}

void print_SCD30_data(SCD30_DATA data, char * timestamp)
{ 
  Serial.print("Timestamp: ");
  Serial.println(timestamp);

  Serial.print("Temperature: ");
  Serial.print(data.temp);
  Serial.println(" degrees C");
  
  Serial.print("Relative Humidity: ");
  Serial.print(data.rh);
  Serial.println(" %");
  
  Serial.print("CO2: ");
  Serial.print(data.co2, 3);
  Serial.println(" ppm");
  Serial.println("");
}

int send_scd30_to_server(SCD30_DATA data, char * timestamp)
{
  if(WiFi.status()== WL_CONNECTED){
 
   HTTPClient http;   
 
   http.begin(SERVER_URL);
   //http.addHeader("Content-Type", "text/plain"); 
   http.addHeader("Content-Type", "application/json");           
 
   StaticJsonDocument<200> doc;
    // Add values in the document
    //
    doc["TEMP"] = data.temp;
    doc["RH"] = data.rh;
    doc["CO2"] = data.co2;
   
    // Add an array.
    //
    //JsonArray data = doc.createNestedArray("data");
    //data.add(48.756080);
    //data.add(2.302038);
     
    String requestBody;
    serializeJson(doc, requestBody);
     
    int httpResponseCode = http.POST(requestBody); 
 
   if(httpResponseCode>0){
 
    String response = http.getString();   
 
    Serial.println(httpResponseCode);
    Serial.println(response);          
 
   }else{
 
    Serial.print("Error on sending PUT Request: ");
    Serial.println(httpResponseCode);
    return -1;
   }
 
   http.end();

   return 1;
 
 }else{
    Serial.println("Error in WiFi connection");
    return -1;
 }
}


void setup() {
  Serial.begin(9600);

  initWiFi();
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  HTTPClient http;

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  //set clock to current time (adjust accordingly)
  //rtc.adjust(DateTime(2023,11,1,20,4,10));

  // Try to initialize!
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    while (1) { delay(10); }
  }
  Serial.println("SCD30 Found!");
  delay(100);

   if (!scd30.setMeasurementInterval(10)){
     Serial.println("Failed to set measurement interval");
     while(1){ delay(10);}
   }
  Serial.print("Measurement Interval: "); 
  Serial.print(scd30.getMeasurementInterval()); 
  Serial.println(" seconds");
}


void loop() {
  /*chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length:6bytes)
  Serial.printf("ESP32 Chip ID = %04X",(uint16_t)(chipid>>32));//print High 2 bytes
  Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.
  delay(13000);*/

  int resp = get_scd30_data(&scd30_data);
  DateTime timestamp = rtc.now();
  char str_datetime[50];
  sprintf(str_datetime, "%04d-%02d-%02d %02d:%02d:%02d", timestamp.year(),
        timestamp.month(), timestamp.day(), 
        timestamp.hour(), timestamp.minute(), timestamp.second());



  if (resp)
  {
    Serial.println("Data available!");
    print_SCD30_data(scd30_data, str_datetime);
    send_scd30_to_server(scd30_data, str_datetime);

  }
  else if (resp == -1)
  {
    Serial.println("Error reading sensor data");
  }
  delay(10000);
}




void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);

  switch (event) {
    case SYSTEM_EVENT_WIFI_READY: 
      Serial.println("WiFi interface ready");
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      Serial.println("Completed scan for access points");
      break;
    case SYSTEM_EVENT_STA_START:
      Serial.println("WiFi client started");
      break;
    case SYSTEM_EVENT_STA_STOP:
      Serial.println("WiFi clients stopped");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("Connected to access point");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Disconnected from WiFi access point");
      WiFi.begin(SSID, PASSWORD);
      break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
      Serial.println("Authentication mode of access point has changed");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print("Obtained IP address: ");
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      Serial.println("Lost IP address and IP address is reset to 0");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
      break;
    case SYSTEM_EVENT_AP_START:
      Serial.println("WiFi access point started");
      break;
    case SYSTEM_EVENT_AP_STOP:
      Serial.println("WiFi access point  stopped");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      Serial.println("Client connected");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      Serial.println("Client disconnected");
      break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
      Serial.println("Assigned IP address to client");
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:
      Serial.println("Received probe request");
      break;
    case SYSTEM_EVENT_GOT_IP6:
      Serial.println("IPv6 is preferred");
      break;
    case SYSTEM_EVENT_ETH_START:
      Serial.println("Ethernet started");
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("Ethernet stopped");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("Ethernet connected");
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("Ethernet disconnected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.println("Obtained IP address");
      break;
    default: break;
}}
