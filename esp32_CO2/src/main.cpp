#include <Arduino.h>

#include <WiFi.h>
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <Adafruit_SCD30.h>
#include <ESP32Ping.h>

#include "RTClib.h"
#include "private_data.h"

#include <Seeed_HM330X.h>

#include <Adafruit_SGP30.h>

int LED_BUILTIN = 2;

RTC_DS3231 rtc;


uint64_t chipid;
HTTPClient http;

//co2 sensor
Adafruit_SCD30  scd30;

typedef struct 
  {
    float co2;
    float rh;
    float temp;
  } SCD30_DATA;

SCD30_DATA scd30_data;
int scd30_init_status = -1;
int scd30_init();


///////
int number_of_people_in_room;
int ping_phone(IPAddress phone_ip);

void initWiFi();
int send_data_to_server();


int get_scd30_data(SCD30_DATA* data);
void print_SCD30_data(SCD30_DATA data, char * timestamp);

//rtc clock
char str_datetime[20];
void read_rtc(char *str_datetime);
int rtc_init_status = -1;
int rtc_init();


//dust sensor hm3301
HM330X hm3301_sensor;
uint8_t hm3301_buf[30];
const char *str[] = {"sensor num: ", "PM1.0 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
                     "PM2.5 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
                     "PM10 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
                     "PM1.0 concentration(Atmospheric environment,unit:ug/m3): ",
                     "PM2.5 concentration(Atmospheric environment,unit:ug/m3): ",
                     "PM10 concentration(Atmospheric environment,unit:ug/m3): ",
};
// to store parsed values, indexes coinciding with str array
uint16_t hm3301_values[7];

typedef struct 
  {
    unsigned int sensor_number;
    unsigned int pm1_0;
    unsigned int pm_2_5;
    unsigned int pm_10;
  } HM3301_DATA;

HM3301_DATA hm3301_data;

HM330XErrorCode hm3301_print_results(uint16_t *values);
HM330XErrorCode hm3301_parse_result(uint8_t *data, uint16_t *values);
int hm3301_get_data( uint16_t *values);

int hm3301_init_status = -1;
int hm3301_init();

//VOC sensor
Adafruit_SGP30 sgp30;
uint16_t TVOC_base = 0 , eCO2_base = 0;
int sgp30_get_data(float temperature, float humidity);
uint32_t getAbsoluteHumidity(float temperature, float humidity);
void sgp30_print_data();
int sgp30_get_baseline_calibration(uint16_t *eCO2_base, uint16_t *TVOC_base);
int sgp30_init_status = -1;
int sgp30_init();

void setup() {
  pinMode (LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  

  initWiFi();
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  rtc_init_status = rtc_init();
  scd30_init_status = scd30_init();
  hm3301_init_status = hm3301_init();
  sgp30_init_status = sgp30_init();

  //wait for a minute to ensure there will be data available from the co2 sensor
  delay(60000);
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

  //wait for co2 sensor to have available data
  int resp = 0;
  while(resp == 0 || resp == -1){
    resp = get_scd30_data(&scd30_data);
    if (resp == -1){
      Serial.println("error scd30");
    }
    delay(1000);
  }
   
  Serial.println("Data available!");

  read_rtc(str_datetime);

  print_SCD30_data(scd30_data, str_datetime);

  int hm3301_resp = hm3301_get_data(hm3301_values);
  hm3301_print_results(hm3301_values);

  int sgp30_resp = sgp30_get_data(scd30_data.temp, scd30_data.rh);
  sgp30_get_baseline_calibration(&TVOC_base, &eCO2_base);
  sgp30_print_data();

  //pings to phone ip to check if im present in room or not
  number_of_people_in_room = ping_phone(phone_ip_p) + ping_phone(phone_ip_u);
  Serial.print("people: ");
  Serial.println(number_of_people_in_room);
  Serial.println("");

  send_data_to_server();

  //for the autocalibration to work,
  //sgp30 sensor must be read every second
  // read it for a minute
  for (int i = 0; i<40; i++){
    delay(1000);
    sgp30_resp = sgp30_get_data(scd30_data.temp, scd30_data.rh);
  }

}


////////////////////////////////////////////////
int ping_phone(IPAddress phone_ip){
  Serial.print("Pinging ip ");
  Serial.println(phone_ip);

  if(Ping.ping(phone_ip,2)) {
    Serial.println("Success!!\n");
    return 1;
  } else {
    Serial.println("not found :(\n");
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

int send_data_to_server()
{
  if(WiFi.status()== WL_CONNECTED){
 
   HTTPClient http;   
 
   http.begin(SERVER_URL);
   //http.addHeader("Content-Type", "text/plain"); 
   http.addHeader("Content-Type", "application/json");           
 
   StaticJsonDocument<300> doc;
    // Add values in the document
    //
    doc["DATETIME"] = str_datetime;

    doc["TEMP"] = scd30_data.temp;
    doc["RH"] = scd30_data.rh;
    doc["CO2"] = scd30_data.co2;

    doc["PM1_0"] = hm3301_data.pm1_0;
    doc["PM2_5"] = hm3301_data.pm_2_5;
    doc["PM10"] = hm3301_data.pm_10;

    doc["TVOC"] = sgp30.TVOC;
    doc["ECO2"] = sgp30.eCO2;
    doc["RAW_H2"] = sgp30.rawH2;
    doc["RAW_ETHANOL"] = sgp30.rawEthanol;
    doc["BASELINE_ECO2"] = eCO2_base;
    doc["BASELINE_TVOC"] = TVOC_base;

    
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



void read_rtc(char *str_datetime){
  DateTime timestamp = rtc.now();
  sprintf(str_datetime, "%04d-%02d-%02d %02d:%02d:%02d", timestamp.year(),
        timestamp.month(), timestamp.day(), 
        timestamp.hour(), timestamp.minute(), timestamp.second());
}




int hm3301_get_data( uint16_t *values){
  if (hm3301_sensor.read_sensor_value(hm3301_buf, 29)) {
          Serial.println("HM330X read result failed!!!");
          return -1;
      }
      hm3301_parse_result(hm3301_buf, values);

      //copy values to sensor structure
      hm3301_data.sensor_number = values[0];
      hm3301_data.pm1_0 = values[4];
      hm3301_data.pm_2_5 = values[5];
      hm3301_data.pm_10 = values[6];
      return 0;
}

HM330XErrorCode hm3301_print_results(uint16_t *values) {
    for (int i = 0; i < 7; i++ ){
        Serial.print(str[i]);
        Serial.println(values[i]);
    }
        Serial.println('\n');
    return NO_ERROR;
}

HM330XErrorCode hm3301_parse_result(uint8_t *data, uint16_t *values) {
    if (NULL == data)
        return ERROR_PARAM;
    for (int i = 1; i < 8; i++) {
        values[i-1] = (uint16_t) data[i * 2] << 8 | data[i * 2 + 1];
    }

    return NO_ERROR;
}



uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

int sgp30_get_data(float temperature, float humidity){
  // If you have a temperature / humidity sensor, 
  //you can set the absolute humidity to enable the humditiy compensation for the air quality signals
  
  if (temperature == -100.0 || humidity == -100.0){
    //if absolute humidity is set to 0, compensation is deactivated
    sgp30.setHumidity(0);
  }
  else{
    sgp30.setHumidity(getAbsoluteHumidity(temperature, humidity));
  }

  //read voc and eCO2
  if (! sgp30.IAQmeasure()) {
      Serial.println("sgp30 Measurement failed");
      return -1;
    }

  // read raw h2 and ethanol
  if (! sgp30.IAQmeasureRaw()) {
    Serial.println("sgp30 Raw Measurement failed");
    return -1;
  }
  return 0;
}

int sgp30_get_baseline_calibration(uint16_t *eCO2_base, uint16_t *TVOC_base){
  if (! sgp30.getIAQBaseline(eCO2_base, TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return -1;
    }

  return 0;
}



void sgp30_print_data(){
  Serial.print("TVOC "); Serial.print(sgp30.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp30.eCO2); Serial.println(" ppm");

  Serial.print("Raw H2 "); Serial.print(sgp30.rawH2); Serial.print(" \t");
  Serial.print("Raw Ethanol "); Serial.print(sgp30.rawEthanol); Serial.println("");

  Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
  Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
  Serial.print("****Baseline values: eCO2: "); Serial.print(eCO2_base);
  Serial.print(" & TVOC: "); Serial.println(TVOC_base);
}



int rtc_init(){
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    return -1;
  }
  else{
    Serial.println("RTC found");
    return 0;
  }
  //set clock to current time (adjust accordingly)
  //rtc.adjust(DateTime(2023,11,1,20,4,10));
  ///////////////
}

int scd30_init(){
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    return -1;
  }
  Serial.println("SCD30 Found!");
  delay(100);

  if (!scd30.setMeasurementInterval(30)){
    Serial.println("Failed to set measurement interval");
    return -1;
  }

  Serial.print("Measurement Interval: "); 
  Serial.print(scd30.getMeasurementInterval()); 
  Serial.println(" seconds");
  return 0;
  ///////////////////
}

int hm3301_init(){
  if (hm3301_sensor.init()) {
        Serial.println("HM330X init failed!!!");
        return -1;
    }
    else{
      Serial.println("HM3301 found");
      return 0;
    }
}

int sgp30_init(){
  if (! sgp30.begin()){
    Serial.println("sgp30 Sensor not found :(");
    return -1;
  }
  else{
    Serial.print("Found SGP30 serial #");
    Serial.print(sgp30.serialnumber[0], HEX);
    Serial.print(sgp30.serialnumber[1], HEX);
    Serial.println(sgp30.serialnumber[2], HEX);
    return 0;
  }
}
