# ESP32 Web Interface Port
This is a port of the 8266-web-interface for the ESP32.
All static files are located in the data folder.

At this time SWD upload functionality is not available.
All other features are available, and therefore should be fully compatible with the 8266 version.


###### Configuration
There are a few simple configurable options -if so needed- for you use case.

You can change the default port for communication with the inverter with:
```C++
#define INVERTER_SERIAL_PORT [ Interface ]`
```
Where Interface can be any object derived from Print interface (like Serial, Serial2, SoftwareSerial)

You can change the defualt softAP SSID and password by modifying the definitions like so:
```C++
char *ssid_AP = 		(char*)"--DEFUALT SSID--";
char *password_AP = 	(char*)"--DEFUALT PASSWORD--";
```

You can also define credentials for station mode, which will try connect on boot:
```C++
#define USE_PRESET_WIFI_CRED true
char *ssid_STA = 		(char*)"--PRESET SSID HERE--";
char *password_STA = 	(char*)"--PRESET PASSWORD HERE--";
```
