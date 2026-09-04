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
 1 Sep 2026 - Switch to use Pico-Syslog
 3 Sep 2026 - Switch to use tcpsyslog
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include <PicoSyslog.h>
#include <time.h>
#include <sunset.h>
#include <SSD1306Wire.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#if SCREEN == 1
SSD1306Wire  display(0x3C, D1, D2);   // Initialize OLED display
#define RELAY D3
#else       
#define RELAY D1
#endif

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error "This board is not supported."
#endif

PicoSyslog::Logger syslog("Duskdawn");

#include "Server.hpp"
#include "config.hpp"

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

bool getRelay()
{
    if (digitalRead(RELAY) == HIGH) {
       return true;
    } else {
       return false;
    }
}

  void setRelay(bool state)
  {
    digitalWrite(RELAY, state ? HIGH : LOW);
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
    fwVersionURL.concat(".ver");
    syslog.println( "Checking for firmware updates." );
    syslog.printf( "chipID: %s\n" , devID.c_str() );
    syslog.printf( "Firmware version URL: %s\n", fwVersionURL.c_str() );

    HTTPClient httpClient;
    httpClient.begin(client, fwVersionURL );
    int httpCode = httpClient.GET();
    syslog.printf("httpCode: %d\n",httpCode);
    if ( httpCode == 200 ) {
      String bodyContent = httpClient.getString();
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, bodyContent);
      if (error) {
          syslog.printf("deserializeJson() failed: %s\n", error.c_str());
          return;
      }
      // New format: Expect a boolean "Force" (default false) and a "Version" string
      bool forceUpdate = false;
      if (doc.containsKey("Force")) {
        if (doc["Force"].is<bool>()) {
          forceUpdate = doc["Force"].as<bool>();
        } else {
          String f = doc["Force"].as<String>();
          f.toLowerCase();
          forceUpdate = (f == "true" || f == "1");
        }
      }
      String newVersion = "";
      if (doc.containsKey("Version")) newVersion = doc["Version"].as<String>();

      syslog.printf("ForceUpdate flag in .ver: %s\n", forceUpdate ? "true" : "false");
      syslog.printf("Firmware version: %s vs %s\n",String(VERSION),newVersion);

      if ( forceUpdate || ( newVersion.length() > 0 && ! newVersion.equals( String(VERSION)))) {
        if (forceUpdate) {
          syslog.warning.println("Forced update requested by server (.ver contains Force=true). Proceeding to update.");
        } else {
          syslog.println( "Preparing to update (version mismatch)" );
        }
        // Constuct URL for new firmware
        String fwImageURL = fwURL;
        fwImageURL.concat( ".bin" );
        syslog.println( "Firmware Image File URL: ");
        syslog.println( fwImageURL);
        ESPhttpUpdate.rebootOnUpdate(true);
        // Update the firmware
        t_httpUpdate_return ret = ESPhttpUpdate.update( client, fwImageURL);
    
        // Error handling
        switch(ret) {
          case HTTP_UPDATE_FAILED:
              syslog.error.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
              break;

          case HTTP_UPDATE_NO_UPDATES:
               syslog.warning.println("HTTP_UPDATE_NO_UPDATES");
               break;
        } // end of switch
      } // end of: if ( forceUpdate || version mismatch)
     
      else {   // newVersion.equals ( VERSION ) 
        syslog.println("Already on latest version and no force flag set" );
      }
    } //end of:  if ( httpCode == 200 )
    else  { // httpCode !== 200
      syslog.error.printf( "Firmware version check failed, HTTP response code: %d\n", httpCode );
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

    syslog.server = SYSLOG_HOST;
    //syslog.app = "Duskdawn";
    // This log level will be used by default
    syslog.default_loglevel = PicoSyslog::LogLevel::information;


    syslog.println("Duskdawn light control starting....");
    syslog.printf("Current Version Number: %s \n",String(VERSION));

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
    configTime(TIMEZONE * 3600, DST * 3600, "pool.ntp.org", "time.nist.gov");
    configTzTime("GMT0BST,M3.5.0/2,M10.5.0/3","pool.ntp.org");
    syslog.println("Gathering time info");
    #if SCREEN == 1
      screenWrite();
    #endif
    delay(3000);
    syslog.println(String("MyIP:" + WiFi.localIP().toString()));
    syslog.println("Duskdawn API control starting....");
    currentState = getRelay();
    syslog.printf("REG currentState: %d\n",currentState);
    setRelay(false);
    requiredState = false;
    syslog.printf("REG requiredState: %d\n",requiredState);
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
        syslog.printf("Setting day of month to : %d\n", currentDay);
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
    syslog.printf("Time: %s\n",&timestr);
    syslog.printf("Sunrise at %02d:%02d\n",sunrise/60,sunrise%60);
    syslog.printf("Civil Sunset at %02d:%02d\n",sunset/60,sunset%60);
    syslog.printf("Nautical Sunset at %02d:%02d\n",nauticalsunset/60,nauticalsunset%60);
    syslog.printf("MinutesPastMidnight: %d\n",mpm);
        
    #if SCREEN == 1
    line2 = String("Time: " + String(timestr));
    line3 = String("Rise: " + String(sunrise/60) + ":" + String(twoDigits(sunrise%60)));
    line4 = String("Set: " + String(sunset/60) + ":" + String(twoDigits(sunset%60)));
    #endif
    
    if (requiredState == true ) {
        syslog.printf("Status: Force , sunrise: %d\n",sunrise);
        syslog.printf("Minutes until sunrise: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: Force");
    #endif
        setRelay(true);
    } 
    else if (mpm >= nauticalsunset && mpm < BEDTIME ) {
        syslog.printf("Status: ON #1 , sunrise: %d\n",sunrise);
        mus = (BEDTIME - mpm) + sunrise;
        syslog.printf("Minutes until sunrise: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: ON #1");
    #endif
        setRelay(true);
    } 
    else if (mpm > RISETIME && mpm  <  sunrise) {
        syslog.printf("Status: ON #2 , sunrise: %d\n",sunrise);
        mus = (sunrise -mpm);
        syslog.printf("Minutes until sunrise: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: ON #2");
    #endif
        setRelay(true);
    } 
    else {
        syslog.printf("Status: OFF , sunset: %d\n",nauticalsunset);
        mus = (nauticalsunset - mpm);
        syslog.printf("Minutes until sunset: %d\n",mus);
    #if SCREEN == 1
        line5 = String("Status: OFF");
    #endif
        setRelay(false);
    }

    currentState = getRelay();
    syslog.printf("REG requiredState: %d, currentState: %d\n",requiredState,currentState);

    #if SCREEN == 1
    screenWrite();
    #endif
    if (mus> 0 && mus < int(LOOPWAIT)) {
       sleeptime=mus;
    } else {
       sleeptime = int(LOOPWAIT);
    }
    checkForUpdates();
    syslog.println("Sleep for "+ String(sleeptime) + " minutes");
    delay(60000 * sleeptime);
    syslog.println("Wake up for check");
    //if ( millis()  >= 86400000UL ) {
    if ( millis()  >= 604800000UL ) {
	//call reset every 7 days (1 Week).
        syslog.println("Unit running for 7 days.... Calling reset to reboot unit");
	delay(10000);
        resetFunc(); 
    } 	
}
