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
int scd30_init_status = -1;
int scd30_read_status = -1;
int scd30_init();


///////
int number_of_people_in_room;
int ping_phone(IPAddress phone_ip);

void initWiFi();



int get_scd30_data();
void print_SCD30_data(char * timestamp);

//rtc clock
char str_datetime[20];
void read_rtc(char *str_datetime);
int rtc_init_status = -1;
int rtc_read_status = -1;
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
int hm3301_read_status = -1;
int hm3301_init();

//VOC sensor
Adafruit_SGP30 sgp30;
uint16_t TVOC_base = 0 , eCO2_base = 0;
int sgp30_get_data(float temperature, float humidity);
uint32_t getAbsoluteHumidity(float temperature, float humidity);
void sgp30_print_data();
int sgp30_get_baseline_calibration(uint16_t *eCO2_base, uint16_t *TVOC_base);
int sgp30_init_status = -1;
int sgp30_read_status = -1;
int sgp30_init();

/////////////
//all sensors


typedef struct 
  {
    char str_datetime[20];
    int rtc_status;

    float scd30_co2;
    float scd30_rh;
    float scd30_temp;
    int scd30_status;

    unsigned int hm3301_pm1_0;
    unsigned int hm3301_pm_2_5;
    unsigned int hm3301_pm_10;
    int hm3301_status;

    unsigned int sgp30_TVOC;
    unsigned int sgp30_eCO2;
    unsigned int sgp30_rawH2;
    unsigned int sgp30_rawEthanol;
    uint16_t sgp30_eCO2_base;
    uint16_t sgp30_TVOC_base;
    int sgp30_status;

    unsigned int n_people;
    int n_people_status;


  } SENSOR_DATA;

SENSOR_DATA sensor_data[1];
int send_data_to_server(SENSOR_DATA *data);

#define RING_BUFFER_SIZE 8
typedef struct 
{
  SENSOR_DATA sensor_data[RING_BUFFER_SIZE];
  unsigned  int count = 0;
  unsigned int tail = 0;
  unsigned int head = 0;
} RING_BUFFER;

RING_BUFFER ring_buffer;

void copy_data_to_ringbuffer(RING_BUFFER *buffer);
int isFull(RING_BUFFER *buffer);
int isEmpty(RING_BUFFER *buffer);
int enqueue(RING_BUFFER *buffer);
int dequeue(RING_BUFFER *buffer);

void init_sensors();

int people_read_status = -1;



void setup() {
  pinMode (LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  

  initWiFi();
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  init_sensors();

  //wait for a minute to ensure there will be data available from the co2 sensor
  delay(60000);

}


void loop() {
  /*chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length:6bytes)
  Serial.printf("ESP32 Chip ID = %04X",(uint16_t)(chipid>>32));//print High 2 bytes
  Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.
  delay(13000);*/

  //connect to network in case there is no conection
  if (WiFi.status() != WL_CONNECTED){
    initWiFi();
    Serial.print("wifi connected.");
  }

  //try to reinitialize sensors with init_status -1
  init_sensors();

  //if sensor scd30 was correctly initialized
  if (scd30_init_status == 0 ){

    //wait for co2 sensor to have available data
    scd30_read_status = get_scd30_data();
    while(scd30_read_status == 1){
      scd30_read_status = get_scd30_data();
      delay(1000);
    }

    if (scd30_read_status == -1){
      Serial.println("error scd30");
    }

    print_SCD30_data(str_datetime);
  }
  else{
    scd30_read_status = -1;
  }



  if (rtc_init_status == 0){
    read_rtc(str_datetime);
    rtc_read_status = 0;
  }
  else{
    rtc_read_status = -1;
    //poner que lea hora del internet
    Serial.println("rtc not found");
  }
  

  if (hm3301_init_status == 0){
    hm3301_read_status = hm3301_get_data(hm3301_values);
    hm3301_print_results(hm3301_values);

    if(hm3301_read_status ==-1){
      Serial.println("HM330X read result failed!!!");
    }
  }
  else{
    hm3301_read_status = -1;
  }


  if (sgp30_init_status == 0){
    //if scd30 is working, use its temp and humidity readings
    // to compensate and improve sgp30 accuracy
    if ((scd30_init_status == 0) && (scd30_read_status == 0)){
      sgp30_read_status = sgp30_get_data(scd30.temperature, scd30.relative_humidity);
    }
    else{//dont use compensation
      sgp30_read_status = sgp30_get_data(-100.0,-100.0);
    }

    sgp30_get_baseline_calibration(&TVOC_base, &eCO2_base);
    sgp30_print_data();

    if(sgp30_read_status ==-1){
      Serial.println("sgp30 Measurement failed");
    }

  }
  else{
    sgp30_read_status = -1;
  }

  if (WiFi.status() == WL_CONNECTED){
    //pings to phone ip to check if im present in room or not
    number_of_people_in_room = ping_phone(phone_ip_p) + ping_phone(phone_ip_u);
    Serial.print("people: ");
    Serial.println(number_of_people_in_room);
    Serial.println("");
    people_read_status = 0;
  }
  else{
    people_read_status = -1;
  }

  //push data to buffer
  enqueue(&ring_buffer);

 
  //if connected to the network, send data in buffer to server
  if((WiFi.status() == WL_CONNECTED)){
    while (!isEmpty(&ring_buffer)){
      dequeue(&ring_buffer);
    }
  }


  if (isFull(&ring_buffer)){
      Serial.println("buffer full");
    }
    Serial.print("tail:");
    Serial.println(ring_buffer.tail);
    Serial.print("head:");
    Serial.println(ring_buffer.head);
    Serial.print("count:");
    Serial.println(ring_buffer.count);




  //for the autocalibration to work,
  //sgp30 sensor must be read every second
  // read it for a minute
  for (int i = 0; i<40; i++){
    delay(1000);

    if( sgp30_init_status == 0){

      if ((scd30_init_status == 0) && (scd30_read_status == 0)){
        sgp30_get_data(scd30.temperature, scd30.relative_humidity);
      }
      else{//dont use compensation
        sgp30_get_data(-100.0,-100.0);
      }
      
    }

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

int get_scd30_data()
{
    if (scd30.dataReady())
    {
      //try to read 3 times
      for(int i=0; i<3 ; i++){
        if (scd30.read()){
          //if sensor was read, return ok status
          return 0;
        }
        Serial.println("scd30 not read");
        delay(100);
      }
      //if after 3 attempts sensor does not
      //respond, send error status and
      //set init status to -1 so next time
      //the uc reinitializes the sensor.
      scd30_init_status = -1;
      return -1;
    } 
    else {
      return 1;
    }
}

void print_SCD30_data( char * timestamp)
{ 
  Serial.print("Timestamp: ");
  Serial.println(timestamp);

  Serial.print("Temperature: ");
  Serial.print(scd30.temperature);
  Serial.println(" degrees C");
  
  Serial.print("Relative Humidity: ");
  Serial.print(scd30.relative_humidity);
  Serial.println(" %");
  
  Serial.print("CO2: ");
  Serial.print(scd30.CO2, 3);
  Serial.println(" ppm");
  //Serial.println("");
}

int send_data_to_server(SENSOR_DATA *data)
{
  if(WiFi.status()== WL_CONNECTED){
 
   HTTPClient http;   
 
   http.begin(SERVER_URL);
   //http.addHeader("Content-Type", "text/plain"); 
   http.addHeader("Content-Type", "application/json");           
 
   StaticJsonDocument<350> doc;
    // Add values in the document
    //
    doc["DATETIME"] = data->str_datetime;
    doc["DATETIME_STAT"] = data->rtc_status;

    doc["TEMP"] = data->scd30_temp;
    doc["RH"] = data->scd30_rh;
    doc["CO2"] = data->scd30_co2;
    doc["SCD30_STAT"] = data->scd30_status;

    doc["PM1_0"] = data->hm3301_pm1_0;
    doc["PM2_5"] = data->hm3301_pm_2_5;
    doc["PM10"] = data->hm3301_pm_10;
    doc["HM3301_STAT"] = data->hm3301_status;

    doc["TVOC"] = data->sgp30_TVOC;
    doc["ECO2"] = data->sgp30_eCO2;
    doc["RAW_H2"] = data->sgp30_rawH2;
    doc["RAW_ETHANOL"] = data->sgp30_rawEthanol;
    doc["BASELINE_ECO2"] = data->sgp30_eCO2_base;
    doc["BASELINE_TVOC"] = data->sgp30_TVOC_base;
    doc["SGP30_STAT"] = data->sgp30_status;
    
    doc["PEOPLE"] = data->n_people;
   
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
  //try to read sensor 3 times
  for(int i=0; i<3 ; i++){

    if (hm3301_sensor.read_sensor_value(hm3301_buf, 29) == NO_ERROR){
      hm3301_parse_result(hm3301_buf, values);
      //copy values to sensor structure
      hm3301_data.sensor_number = values[0];
      hm3301_data.pm1_0 = values[4];
      hm3301_data.pm_2_5 = values[5];
      hm3301_data.pm_10 = values[6];
      return 0;
    }

    
    delay(100);
  }

  //if after 3 attempts sensor does not
  //respond, send error status and
  //set init status to -1 so next time
  //the uc reinitializes the sensor.
  hm3301_init_status = -1;
  return -1;
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

  for (int i=0; i<3; i++){
    //read voc and eCO2               raw h2 and ethanol 
    if(sgp30.IAQmeasure() && sgp30.IAQmeasureRaw()){
      return 0;
    }
    
    Serial.println("sgp30 not read");
    delay(100);
  }

  sgp30_init_status = -1;
  return -1;
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


void init_sensors(){
  //check status of sensors
  //if not initialized (-1), initialize
  if (rtc_init_status == -1)
    rtc_init_status = rtc_init();
  if (scd30_init_status == -1)
    scd30_init_status = scd30_init();
  if (hm3301_init_status == -1)
    hm3301_init_status = hm3301_init();
  if (sgp30_init_status == -1)
    sgp30_init_status = sgp30_init();
}


void copy_data_to_ringbuffer(RING_BUFFER *buffer){
  int idx = buffer->head;

  strcpy(buffer->sensor_data[idx].str_datetime, str_datetime);
  buffer->sensor_data[idx].rtc_status = rtc_read_status;

  buffer->sensor_data[idx].scd30_co2 = scd30.CO2;
  buffer->sensor_data[idx].scd30_rh = scd30.relative_humidity;
  buffer->sensor_data[idx].scd30_temp = scd30.temperature;
  buffer->sensor_data[idx].scd30_status = scd30_read_status;

  buffer->sensor_data[idx].hm3301_pm1_0 = hm3301_data.pm1_0;
  buffer->sensor_data[idx].hm3301_pm_2_5 = hm3301_data.pm_2_5;
  buffer->sensor_data[idx].hm3301_pm_10 = hm3301_data.pm_10;
  buffer->sensor_data[idx].hm3301_status = hm3301_read_status;

  buffer->sensor_data[idx].sgp30_TVOC = sgp30.TVOC;
  buffer->sensor_data[idx].sgp30_eCO2 = sgp30.eCO2;
  buffer->sensor_data[idx].sgp30_rawH2 = sgp30.rawH2;
  buffer->sensor_data[idx].sgp30_rawEthanol = sgp30.rawEthanol;
  buffer->sensor_data[idx].sgp30_eCO2_base = eCO2_base;
  buffer->sensor_data[idx].sgp30_TVOC_base = TVOC_base;
  buffer->sensor_data[idx].sgp30_status = sgp30_read_status;

  buffer->sensor_data[idx].n_people = number_of_people_in_room;
  buffer->sensor_data[idx].n_people_status = people_read_status;
}



int isFull(RING_BUFFER *buffer){
  return buffer->count ==  RING_BUFFER_SIZE;
}

int isEmpty(RING_BUFFER *buffer){
  return buffer->count ==  0;
}

int enqueue(RING_BUFFER *buffer){
    int ret_val = 0;
    //if buffer is already full, it will
    //overwrite the element in tail
    if (isFull(buffer)){
      //adjust new tail position
      buffer->tail = (buffer->tail + 1 ) & (RING_BUFFER_SIZE -1);
      // reduce one to count so after the next increment i continues
      //to be RING_BUFFER_SIZE
      buffer->count--;
      ret_val = 1; //buffer full return
    }

    copy_data_to_ringbuffer(buffer);

    //and operation is equivalent to modulo (%)
    //when the size of the buffer is a power of 2
    buffer->head = (buffer->head + 1 ) & (RING_BUFFER_SIZE -1);
    buffer->count++;

    return ret_val;
}

int dequeue(RING_BUFFER *buffer){
  if (isEmpty(buffer)){
    return 1;
  }

  //ver que hacer cuando servidor no contesta
  send_data_to_server(&(buffer->sensor_data[buffer->tail]));
  buffer->tail = (buffer->tail + 1 ) & (RING_BUFFER_SIZE -1);
  buffer->count--;

  return 0;
}