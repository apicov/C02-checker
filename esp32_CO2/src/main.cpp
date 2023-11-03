#include <Arduino.h>

#include <WiFi.h>
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <Adafruit_SCD30.h>
#include <ESP32Ping.h>

#include "RTClib.h"
#include "private_data.h"

int LED_BUILTIN = 2;

RTC_DS3231 rtc;


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


int ping_phone(IPAddress phone_ip){
  Serial.print("Pinging ip ");
  Serial.println(phone_ip);

  if(Ping.ping(phone_ip)) {
    Serial.println("Success!!\n");
    return 1;
  } else {
    Serial.println("Error :(\n");
    return 0;
  }
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.print('.');
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
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
  //Serial.println("");
}

int send_data_to_server(SCD30_DATA data, char * timestamp, int number_of_people_in_room)
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
    doc["DATETIME"] = timestamp;
    doc["PEOPLE"] = number_of_people_in_room;
   
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
  pinMode (LED_BUILTIN, OUTPUT);
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

   if (!scd30.setMeasurementInterval(30)){
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
  if (WiFi.status() != WL_CONNECTED){
    initWiFi();
    Serial.print("wifi connected.");
  }

  int resp = get_scd30_data(&scd30_data);


  if (resp)
  {

    DateTime timestamp = rtc.now();
    char str_datetime[20];
    sprintf(str_datetime, "%04d-%02d-%02d %02d:%02d:%02d", timestamp.year(),
        timestamp.month(), timestamp.day(), 
        timestamp.hour(), timestamp.minute(), timestamp.second());

    Serial.println("Data available!");

    //pings to phone ip to check if im present in room or not
    int number_of_people_in_room = ping_phone(phone_ip);

    print_SCD30_data(scd30_data, str_datetime);
    Serial.print("people: ");
    Serial.println(number_of_people_in_room);
    Serial.println("");


    send_data_to_server(scd30_data, str_datetime, number_of_people_in_room);

  }
  else if (resp == -1)
  {
    Serial.println("Error reading sensor data");
  }
  delay(10000);
}

