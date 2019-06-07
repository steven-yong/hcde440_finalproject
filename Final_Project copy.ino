////////////////////////////////////////////////
//The libraries needed for the code to function.
////////////////////////////////////////////////
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SPI.h>
#include "Adafruit_TSL2591.h"


/////////////////////////////////////////////////////////////////////////
//Create instances of AdafruitIO_Feed and connect them to specific feeds.
/////////////////////////////////////////////////////////////////////////
AdafruitIO_Feed *exitFarmersMarket = io.feed("exitFarmersMarket");
AdafruitIO_Feed *notification = io.feed("notification");



////////////////////////////////////////////////////////////
//Create an instance of the light sensor and call it "tsl."
////////////////////////////////////////////////////////////
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);



///////////////////////////////////////////////////////////////////////////
//The global variables needed to save and pass between different functions.
///////////////////////////////////////////////////////////////////////////
String timestamp = "";

int ledPin = 12;

int pirPin = 14;
int pirState = LOW;

int luminosity = 0;
int val = 0;

unsigned long currentMotionTime;
unsigned long startLightTimer = millis();

boolean lightFlag = false;

String farmersMarketID = "";
String farmersMarketAddress = "";
String farmersMarketSchedule = "";

int farmersMarketLength = 0;
String farmersMarketDayTime = "";
String startMonth = "";
String startDay = "";
String endMonth = "";
String endDay = "";
int startMonthInt = 0;
int startDayInt = 0;
int endMonthInt = 0;
int endDayInt = 0;

int timezone = -8;
int dst = 1;
int currentMonthInt = 0;
int currentDayInt = 0;
String dayOfWeek = "";
String currentTime = "";

const char* host = "search.ams.usda.gov";
const int httpsPort = 443;
const char* fingerprint = "4E 32 96 9C 0F 5E BC 6D AF 6E 8C 83 88 70 C5 47 02 9F 8B 7D";

typedef struct {
  String ip;
  String ln;
  String lt;
} GeoData;
GeoData location;



//////////////////////////////////////////////////////////////////////////////////////////////////////
//When first booting up the ESP8266, start the serial monitor, test to see if the light sensor is 
//working, connect to the WiFi, configure the time, connect to Adafruit IO, and initilialize the pins.
//////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  while(! Serial);

  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("Starting Adafruit TSL2591 Test!"));
  if (tsl.begin()) 
  {
    Serial.println(F("Found a TSL2591 sensor"));
  } 
  else 
  {
    Serial.println(F("No sensor found ... check your wiring?"));
    while (1);
  }
  displaySensorDetails();
  configureSensor();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.print("Connecting Wifi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  configTime(timezone * 3600, dst * 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("Connecting to Adafruit IO");
  io.connect();
  while(io.status() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    delay(500);
  }
  Serial.println();
  Serial.println(io.statusText());
  exitFarmersMarket->onMessage(handleMessage);
  exitFarmersMarket->get();

  pinMode(ledPin, OUTPUT);
  pinMode(pirPin, INPUT);

  getGeo();
  getFarmersMarketID();
  getTime();
  getFarmersMarketData();
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Maintain connection with Adafruit IO and check everyday if the closest farmers market is today. Constantly read from 
//the light and motion sensor; if it gets too bright or no motion has been sensed for 30 minutes, turn off the light.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  io.run();

  getTime();
  if (currentTime == "07:00") {
    getGeo();
    getFarmersMarketID();
    getFarmersMarketData();
  }

  simpleRead();
  if (lightFlag == false || luminosity >= 500) {
    digitalWrite(ledPin, LOW);
    Serial.println("The light has been turned off because you did not visit the farmers market or it's too bright.");
  }
  
  val = digitalRead(pirPin);
  if (val == HIGH) {
    if (lightFlag == true && luminosity <= 500) {
      digitalWrite(ledPin, HIGH);
      Serial.println("The light has been turned on!");
    }
    if (pirState == LOW) {
      // we have just turned on
      Serial.println("Motion detected!");
      pirState = HIGH;
     } else {
       if (pirState == HIGH){
        // we have just turned of
        Serial.println("Motion ended!");
        pirState = LOW;
       }
     }
  }
  
  static unsigned long startMotionTime = 0;
  currentMotionTime = millis();
  if (startMotionTime == 0) {
    digitalWrite(ledPin, LOW);
    startMotionTime = currentMotionTime;
    Serial.println("The light has been turned off because nobody has been in the area for 30 minutes.");
  }
  if (currentMotionTime - startMotionTime >= 6000UL) {
    startMotionTime = 0;
  }
}



//////////////////////////////////////////////////////////////////////////////////////
//When there is new data on the exitFarmersMarket feed, set the flag to turn on the 
//lightbox on; set a timer that sets the flag to turn off the lightbox after 24 hours.
//////////////////////////////////////////////////////////////////////////////////////
void handleMessage(AdafruitIO_Data *exitFarmersMarket) {
  timestamp = exitFarmersMarket->toString();
  Serial.print("received <- ");
  Serial.println(timestamp);
  Serial.println("You have visited your local farmers market.");
  unsigned long currentLightTimer = millis();
  unsigned long elapsedLightTimer = currentLightTimer - startLightTimer;
  lightFlag = true;
  if (elapsedLightTimer % 86400000 == 0) {
    lightFlag = false;
  }
  delay(5000);
}



////////////////////////////////////////////////////
//The function to get the IP address using the WiFi.
////////////////////////////////////////////////////
String getIP() {
  HTTPClient theClient;
  String ipAddress;

  theClient.begin("http://api.ipify.org/?format=json");
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {

      DynamicJsonBuffer jsonBuffer;

      String payload = theClient.getString();
      JsonObject& root = jsonBuffer.parse(payload);
      ipAddress = root["ip"].as<String>();

    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
      return "error";
    }
  }
  Serial.println("End of getIP");
  return ipAddress;
}



//////////////////////////////////////////////////////////////////////////////////////////
//Function to get the latitude and longitude using the IP address from the getIP function.
//////////////////////////////////////////////////////////////////////////////////////////
void getGeo() {
  String ipAddress = getIP();
  HTTPClient theClient;
  Serial.println(F(""));
  Serial.println("Making HTTP request to get location.");
  theClient.begin("http://api.ipstack.com/" + ipAddress + "?access_key=" + geo_Key);
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload for location.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return;
      }
      location.ip = root["ip"].as<String>();
      location.lt = root["latitude"].as<String>();
      location.ln = root["longitude"].as<String>();
    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
    }
    Serial.println("End of getGeo");
  }
}



/////////////////////////////////////////////////////////////////////////////////////////////////////
//Using the latitude and longitude from the getGeo function get the ID of the closest farmers market.
/////////////////////////////////////////////////////////////////////////////////////////////////////
void getFarmersMarketID() {
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  if (client.verify(fingerprint, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  String url = "/farmersmarkets/v1/data.svc/locSearch?lat=" + location.lt + "&lng=" + location.ln;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");

  DynamicJsonBuffer jsonBuffer;
  Serial.println("Parsing...");
  JsonObject& root = jsonBuffer.parse(line);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  } else {
    Serial.println("Success!");
  }
  farmersMarketID = root["results"][5]["id"].as<String>();
  Serial.println("Farmers Market ID: " + farmersMarketID);
  Serial.println("End of getFarmersMarketID");
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Using the ID from the getFarmersMarketID function, get the address, date range, and time of the closest farmers market.
//Comparing with the current date and day of the week from the getTime function, assess if there is a farmers market.
//If there, send the address and time to Adafruit IO.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void getFarmersMarketData() {
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  if (client.verify(fingerprint, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  String url = "/farmersmarkets/v1/data.svc/mktDetail?id=1012152";
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");

  DynamicJsonBuffer jsonBuffer;
  Serial.println("Parsing...");
  JsonObject& root = jsonBuffer.parse(line);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  } else {
    Serial.println("Success!");
  }
  farmersMarketAddress = root["marketdetails"]["Address"].as<String>();
  farmersMarketSchedule = root["marketdetails"]["Schedule"].as<String>();
  Serial.println("Farmers Market Address: " + farmersMarketAddress);
  Serial.println("Farmers Market Schedule: " + farmersMarketSchedule);
  startMonth = farmersMarketSchedule.substring(0, 2);
  startMonthInt = startMonth.toInt();
  Serial.println(startMonthInt);
  
  startDay = farmersMarketSchedule.substring(3, 5);
  startDayInt = startDay.toInt();
  Serial.println(startDayInt);
  
  endMonth = farmersMarketSchedule.substring(14, 16);
  endMonthInt = endMonth.toInt();
  Serial.println(endMonthInt);
  
  endDay = farmersMarketSchedule.substring(17, 19);
  endDayInt = endDay.toInt();
  Serial.println(endDayInt);
  
  farmersMarketLength = farmersMarketSchedule.indexOf(";");
  farmersMarketDayTime = farmersMarketSchedule.substring(24, farmersMarketLength);
  farmersMarketDayTime.trim();
  Serial.println("Day & Time: " + farmersMarketDayTime);

  if (startMonthInt <= currentMonthInt && endMonthInt >= currentMonthInt && farmersMarketDayTime.startsWith(dayOfWeek) == true) {
    Serial.println("Your local farmers market is today!");
    notification->save(farmersMarketAddress);
    notification->save(farmersMarketDayTime);
  }
  Serial.println("End of getFarmersMarketData");
}



///////////////////////////////////////////////////////////////////////////////////////////////
//Get the current time and process the information to get ready to use in getFarmersMarketData.
///////////////////////////////////////////////////////////////////////////////////////////////
void getTime() {
  time_t now = time(nullptr);
  String dateTime = ctime(&now);
  Serial.println(dateTime);
  dayOfWeek = dateTime.substring(0, 3);
  Serial.println("Day of Week: " + dayOfWeek);
  int index_1 = dateTime.indexOf(":");
  String currentMonth = dateTime.substring(4, 7);
  
  if (currentMonth == "Jan") {
    currentMonthInt = 1;
  }
  else if (currentMonth == "Feb") {
    currentMonthInt = 2;
  }
  else if (currentMonth == "Mar") {
    currentMonthInt = 3;
  }
  else if (currentMonth == "Apr") {
    currentMonthInt = 4;
  }
  else if (currentMonth == "May") {
    currentMonthInt = 5;
  }
  else if (currentMonth == "Jun") {
    currentMonthInt = 6;
  }
  else if (currentMonth == "Jul") {
    currentMonthInt = 7;
  }
  else if (currentMonth == "Aug") {
    currentMonthInt = 8;
  }
  else if (currentMonth == "Sep") {
    currentMonthInt = 9;
  }
  else if (currentMonth == "Oct") {
    currentMonthInt = 10;
  }
  else if (currentMonth == "Nov") {
    currentMonthInt = 11;
  }
  else if (currentMonth == "Dec") {
    currentMonthInt = 12;
  }

  Serial.println(currentMonth);
  Serial.println(currentMonthInt);
  String currentDay = dateTime.substring(index_1 - 5, index_1 - 3);
  currentDay.trim();
  Serial.println("Day: " + currentDay);
  currentDayInt = currentDay.toInt();
  Serial.println(currentDayInt);
  currentTime = dateTime.substring(11, 16);
  Serial.println(currentTime);
  delay(1000);
}



//////////////////////////////////////////////////////////////////////////////////////////////
//The rest of the code pertains to the light sensor and is needed for its full functionality.
//////////////////////////////////////////////////////////////////////////////////////////////
void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.print  (F("Sensor:       ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:   ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:    ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:    ")); Serial.print(sensor.max_value); Serial.println(F(" lux"));
  Serial.print  (F("Min Value:    ")); Serial.print(sensor.min_value); Serial.println(F(" lux"));
  Serial.print  (F("Resolution:   ")); Serial.print(sensor.resolution, 4); Serial.println(F(" lux"));  
  Serial.println(F("------------------------------------"));
  Serial.println(F(""));
  delay(500);
}

void configureSensor(void)
{
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  //tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
  tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
  //tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain
  
  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)

  /* Display the gain and integration time for reference sake */  
  Serial.println(F("------------------------------------"));
  Serial.print  (F("Gain:         "));
  tsl2591Gain_t gain = tsl.getGain();
  switch(gain)
  {
    case TSL2591_GAIN_LOW:
      Serial.println(F("1x (Low)"));
      break;
    case TSL2591_GAIN_MED:
      Serial.println(F("25x (Medium)"));
      break;
    case TSL2591_GAIN_HIGH:
      Serial.println(F("428x (High)"));
      break;
    case TSL2591_GAIN_MAX:
      Serial.println(F("9876x (Max)"));
      break;
  }
  Serial.print  (F("Timing:       "));
  Serial.print((tsl.getTiming() + 1) * 100, DEC); 
  Serial.println(F(" ms"));
  Serial.println(F("------------------------------------"));
  Serial.println(F(""));
}

void simpleRead()
{
  // Simple data read example. Just read the infrared, fullspecrtrum diode 
  // or 'visible' (difference between the two) channels.
  // This can take 100-600 milliseconds! Uncomment whichever of the following you want to read
  uint16_t x = tsl.getLuminosity(TSL2591_VISIBLE);
  //uint16_t x = tsl.getLuminosity(TSL2591_FULLSPECTRUM);
  //uint16_t x = tsl.getLuminosity(TSL2591_INFRARED);

  Serial.print(F("[ ")); Serial.print(millis()); Serial.print(F(" ms ] "));
  Serial.print(F("Luminosity: "));
  Serial.println(x, DEC);

  luminosity = x;
}
