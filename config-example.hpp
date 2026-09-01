#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-passwd"
#define LATITUDE 50.00000
#define LONGITUDE 0.00000
#define TIMEZONE  0
#define DST  0
#define RELAY D1 	// What pin is you relay connected to
#define SYSLOG_HOST "192.168.1.2"  // Syslog server
#define SYSLOG_PORT 514		// Syslog port
#define BEDTIME 1350  // Bedtime in minutes of the day, lights wont turn on after this time  
#define RISETIME 360  //  Risetime in minutes of the day , lights wont turn on before this time. 
#define LOOPWAIT 5   // Minutes the loop sleetpfor between checks
//for OTA http updates 
const int httpPort = 80;
const char* VERSION = "1.0.10n";
const char* MODEL = "1";
const char* host = "192.168.1.2";
const char* fwURLLoc = "/otastore/";
