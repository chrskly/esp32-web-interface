# ESP32 Web Interface Port
This fork is a port of the [OpenInverter](https://openinverter.org) esp8266-web-interface for the ESP32.

At this time SWD upload functionality is not available.
_All other features are available however, and should work as you're used to.

## Configuration
There are a few simple configurable options -if so needed- for you use case.

##### Inverter
You can change the default port for communication with the inverter:
```C++
#define INVERTER_SERIAL_PORT [ Interface ]
```
Where Interface can be any object derived from Print interface (like Serial, Serial2, SoftwareSerial).

> inverter.cpp

Here you can change baud rates and page sizes. 
_Note: Unless you're using your own implementation of the OpenInverter firmware, don't change these!
_Note 2: The buffer for the print interface is set at 2048 bytes. It's way over the top, I know.

##### WiFi
You can change the defualt softAP SSID and password by modifying the definitions like so:
```C++
char *ssid_AP =      (char*)"--DEFUALT SSID--";
char *password_AP =  (char*)"--DEFUALT PASSWORD--";
```

You can also define credentials for station mode, which will try connect on boot:
```C++
#define USE_PRESET_WIFI_CRED true
char *ssid_STA =      (char*)"--PRESET SSID HERE--";
char *password_STA =  (char*)"--PRESET PASSWORD HERE--";
```

##### Web Server
The webserver port can be changed with:
```C++
#define SERVER_PORT [PORT NUMBER]
```

The index file can be defined with:
```C++
#define SERVER_INDEX F(["--PATH TO INDEX FILE--"])
```

Currently the contents of the SPIFFS filesystem is "cached", I plan to change this soon but for now if you need more then 48 files in your filesystem image you can change the cache size with:
```C++
#define MAX_NUMBER_OF_FILES [ Number ]
```

## Extras
If you want to be able to communicate with multiple inverters across multiple ports you can create a new instance of the Inverter class:
```C++
Inverter inverter( PrintInterface );
```
Note that the website doesn't support this use case and therefore the webserver doesn't either, but the option is there if you want to build a more custom solution for your inverter interface needs.
_One possible way to do this is to create a new RequestHandler derived class that will handle all requests sent with a chosen prefex.
