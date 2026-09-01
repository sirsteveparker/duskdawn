/* 
Author: Steve Parker
Date: 19 Oct 2024
Description:
Wemo with OLED board - Compile for NodeMCU 1.0 board. 
Wemo D1 = LOLIN(WEMOS) D1 mini
Updates:
 5 Dec 2025 - Add a REST API . GET /status
16 Mar 2026 - Add Over The Air updates v1.0.9
24 Aug 2026 - Use Nautical Sunset instead of Civil v1.0.10
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include <sunset.h>
#include <SSD1306Wire.h>
#include <TLog.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "API.hpp"
#include "Server.hpp"
#include "config.hpp"


#if SCREEN == 1
SSD1306Wire  display(0x3C, D1, D2);   // Initialize OLED display
#define RELAY D3
#else       
#define RELAY D1
#endif

#ifdef SYSLOG_HOST
#include <SyslogStream.h>
SyslogStream syslogStream = SyslogStream();
#endif

time_t nowtime;
bool screenOn=true;
// unsigned long now;
unsigned long count;
String line0 ;
String line1 ;
String line2 ;
String line3 ;
String line4 ;
String line5 ; 
String *lineptr[6];
SunSet sun;
int mpm;
static int currentDay = 32;
int sleeptime = int(LOOPWAIT); 
bool currentState;
bool requiredState;
WiFiClient client;

String getMAC()
{
  uint8_t mac[6];
  char result[14];
  WiFi.macAddress(mac);
  snprintf( result, sizeof( result ), "%02x%02x%02x%02x%02x%02x", mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );
  return String( result );
}

String getDevID()
{
  unsigned long chipID = ESP.getChipId();
  String devID = String(chipID, HEX);
  return devID;
}
// utility function for digital clock display: prints leading 0
String twoDigits(int digits)
{
    if(digits < 10) {
        String i = '0'+String(digits);
        return i;
    }
    else {
        return String(digits);
    }
}

bool onoroff()
{
    if (digitalRead(RELAY) == HIGH) {
       return true;
    } else {
       return false;
    }
}

#if SCREEN == 1
void screenWrite() {
  display.clear();
  int col=0;
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  for (int line=0 ; line < 6; line++) {
    display.drawString(col,line * 10, *lineptr[line]);     // Print string (deref the ptr)
  }
  display.display();
  // Read that these screens suffer from burn in, so turn it off after 10m
  //if ( (now > 600000 && now % 600000 < 10000 ) && screenOn) {
  //  display.displayOff();
  //  screenOn = false;
  //  Serial.println("Screen off to prevent burn-in");
  //}

}
#endif

void checkForUpdates()
{
    String devID = getDevID();
    String fwURL = "http://";
    fwURL.concat(host);
    fwURL.concat(fwURLLoc);
    fwURL.concat(devID);
    String fwVersionURL = fwURL;
    fwVersionURL.concat( ".ver" );
    Log.println( "Checking for firmware updates." );
    Log.printf( "chipID: %s\n" , devID );
    Log.print( "Firmware version URL:" );
    Log.println( fwVersionURL );

    HTTPClient httpClient;
    httpClient.begin(client, fwVersionURL );
    int httpCode = httpClient.GET();
    Log.printf("httpCode: %d\n",httpCode);
    if ( httpCode == 200 ) {
      String bodyContent = httpClient.getString();
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, bodyContent);
      if (error) {
          Log.printf("deserializeJson() failed: %s\n", error.c_str());
          return;
      }
      String newModel = doc["Model"];
      String newVersion = doc["Version"];
      Log.printf(" Model Number: %s vs %s\n ",String(MODEL),newModel);
      Log.printf(" Firmware version: %s vs %s\n ",String(VERSION),newVersion);
      if ( ! newVersion.equals( String(VERSION))) {
        Log.println( "Preparing to update" );
        // Constuct URL for new firmware
        String fwImageURL = fwURL;
        fwImageURL.concat( ".bin" );
        Log.println( "Firmware Image File URL: ");
        Log.println( fwImageURL);
        ESPhttpUpdate.rebootOnUpdate(true);
        // Update the firmware
        t_httpUpdate_return ret = ESPhttpUpdate.update( client, fwImageURL);
    
        // Error handling
        switch(ret) {
          case HTTP_UPDATE_FAILED:
              Log.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
              break;

          case HTTP_UPDATE_NO_UPDATES:
               Log.println("HTTP_UPDATE_NO_UPDATES");
               break;
        } // end of switch
      } // end of: if ( ! newVersion.equals( VERSION ))
   
      else {   // newVersion.equals ( VERSION ) 
        Log.println( "Already on latest version" );
      }
    } //end of:  if ( httpCode == 200 )
    else  { // httpCode !== 200
      Log.printf( "Firmware version check failed, HTTP response code: %d\n", httpCode );
    }
    httpClient.end();
}

void setup()
{
    Serial.begin(115200);
    Serial.println();
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to the WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    #ifdef SYSLOG_HOST
    syslogStream.setDestination(SYSLOG_HOST);
    syslogStream.setRaw(false); // wether or not the syslog server is a modern(ish) unix.
    #ifdef SYSLOG_PORT
    syslogStream.setPort(SYSLOG_PORT);
    #endif
    const std::shared_ptr<LOGBase> syslogStreamPtr = std::make_shared<SyslogStream>(syslogStream);
    Log.addPrintStream(syslogStreamPtr);
    #endif

    Log.begin();
    Log.println("Duskdawn light control starting....");
    Log.printf("Current Version Number: %s\n",String(VERSION));

    #if SCREEN == 1
    lineptr[0] = &line0;
    lineptr[1] = &line1;
    lineptr[2] = &line2;
    lineptr[3] = &line3;
    lineptr[4] = &line4;
    lineptr[5] = &line5;  
    display.init();
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    line0 = String("Wifi: " + String(WIFI_SSID));
    line1 = String("MyIP:" + WiFi.localIP().toString());
    #endif
    /* Get our time sync started */
    /* Set our position and a default timezone value */
    sun.setPosition(LATITUDE, LONGITUDE, TIMEZONE);
    sun.setTZOffset(TIMEZONE);
    pinMode(RELAY,OUTPUT);
    digitalWrite(RELAY, LOW);
    configTime(TIMEZONE * 3600, DST * 3600, "pool.ntp.org", "time.nist.gov");
    configTzTime("GMT0BST,M3.5.0/2,M10.5.0/3","pool.ntp.org");
    Log.println("\nGathering time info");
    #if SCREEN == 1
      screenWrite();
    #endif
    delay(3000);
    Log.println(String("MyIP:" + WiFi.localIP().toString()));
    Log.println("Duskdawn API control starting....");
    currentState = onoroff();
    Log.printf("REG currentState: %d\n",currentState);
    requiredState = false;
    Log.printf("REG requiredState: %d\n",requiredState);

    InitServer();    
}

void(* resetFunc) (void) = 0; //declare reset function at address 0 - MUST BE ABOVE LOOP

void loop()
{
    while (WiFi.status() != WL_CONNECTED) {
       WiFi.mode(WIFI_STA);
       WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
       Serial.print("Connecting to the WiFi");
       delay(250);
    }

    Log.loop();


    struct tm * timeinfo;
    time(&nowtime);
    timeinfo = localtime(&nowtime);  
    char timestr[15];
    int sunrise;
    int sunset;
    int mus;
//    double civilsunrise;
//    double civilsunset;
//    double astrosunrise;
//    double astrosunset;
//    double astrosunset;
//    double nauticalsunrise;
    int nauticalsunset;
//    double customsunrise;
//    double customsunset;

    if (currentDay != timeinfo->tm_mday) {
        sun.setCurrentDate(timeinfo->tm_year, timeinfo->tm_mon +1, timeinfo->tm_mday);
        currentDay = timeinfo->tm_mday;
        Log.printf("Setting day of month to : %d\n", currentDay);
    }
    mpm = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    sunrise = static_cast<int>(sun.calcCivilSunrise());
    sunset = static_cast<int>(sun.calcCivilSunset());
    strftime(timestr, sizeof(timestr),"%Y%m%dT%H:%M:%S", timeinfo);
//    sunrise = static_cast<int>(sun.calcSunrise());
//    sunset = static_cast<int>(sun.calcSunset());
//    civilsunrise = sun.calcCivilSunrise();
//    civilsunset = sun.calcCivilSunset();
//    nauticalsunrise = sun.calcNauticalSunrise();
    nauticalsunset = static_cast<int>(sun.calcNauticalSunset());
//    astrosunrise = sun.calcAstronomicalSunrise();
//    astrosunset = static_cast<int>(sun.calcAstronomicalSunset());
//    customsunrise = sun.calcCustomSunrise(90.0);
//    customsunset = sun.calcCustomSunset(90.0);
    Log.printf("Time: %s\n",&timestr);
    Log.printf("Sunrise at %02d:%02d\n",sunrise/60,sunrise%60);
    Log.printf("Civil Sunset at %02d:%02d\n",sunset/60,sunset%60);
    Log.printf("Nautical Sunset at %02d:%02d\n",nauticalsunset/60,nauticalsunset%60);
    Log.printf("MinutesPastMidnight: %d\n",mpm);
        
    #if SCREEN == 1
    line2 = String("Time: " + String(timestr));
    line3 = String("Rise: " + String(sunrise/60) + ":" + String(twoDigits(sunrise%60)));
    line4 = String("Set: " + String(sunset/60) + ":" + String(twoDigits(sunset%60)));
    #endif
    
    if (requiredState == true ) {
        Log.printf("Status: Force , sunrise: %d\n",sunrise);
        Log.printf("Minutes until sunrise: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: Force");
    #endif
        digitalWrite(RELAY, HIGH);
    } 
    else if (mpm >= nauticalsunset && mpm < BEDTIME ) {
        Log.printf("Status: ON #1 , sunrise: %d\n",sunrise);
        mus = (BEDTIME - mpm) + sunrise;
        Log.printf("Minutes until sunrise: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: ON #1");
    #endif
        digitalWrite(RELAY, HIGH);
    } 
    else if (mpm > RISETIME && mpm  <  sunrise) {
        Log.printf("Status: ON #2 , sunrise: %d\n",sunrise);
        mus = (sunrise -mpm);
        Log.printf("Minutes until sunrise: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: ON #2");
    #endif
        digitalWrite(RELAY, HIGH);
    } 
    else {
        Log.printf("Status: OFF , sunset: %d\n",nauticalsunset);
        mus = (nauticalsunset - mpm);
        Log.printf("Minutes until sunset: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: OFF");
    #endif
        digitalWrite(RELAY, LOW);
    }

    currentState = onoroff();
    Log.printf("REG requiredState: %d, currentState: %d\n",requiredState,currentState);

    #if SCREEN == 1
    screenWrite();
    #endif
    if (mus> 0 && mus < int(LOOPWAIT)) {
       sleeptime=mus;
    } else {
       sleeptime = int(LOOPWAIT);
    }
    checkForUpdates();
    Log.println("Sleep for "+ String(sleeptime) + " minutes");
    delay(60000 * sleeptime);
    Log.println("Wake up for check");
    //if ( millis()  >= 86400000UL ) {
    if ( millis()  >= 604800000UL ) {
	//call reset every 7 days (1 Week).
        Log.println("Unit running for 7 days.... Calling reset to reboot unit");
	Log.stop();
	delay(10000);
        resetFunc(); 
    }	
}
