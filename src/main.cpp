/*
 * This file is part of the esp8266 web interface
 *
 * Copyright (C) 2018 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/*
 * This file is part of the esp32-web-interface-port
 *
 * Copyright (C) 2021 Bedirhan Teymur <tbedirhan@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <Arduino.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ETH.h>
#include <WiFi.h>
#include <WebServer.h>
#include <inverter.h>
#include <DBG_DEFS.h>

#define VERSION F("1.0-ESP32")
#define LED_PIN 2
#define SERVER_PORT 80
#define SERVER_INDEX F("/index.html")
#define MAX_NUMBER_OF_FILES 64
#define INVERTER_SERIAL_PORT Serial1
#define MAX_WIFI_CONECTION_ATTEMPTS 10
#define FORMAT_SPIFFS_IF_FAILED true

//TODO: add the abilty to save configuration
#define USE_PRESET_WIFI_CRED false
//char *ssid_STA = 		(char*)"--PRESET SSID HERE--";
//char *password_STA = 	(char*)"--PRESET PASSWORD HERE--";
char *ssid_STA;
char *password_STA;

char *ssid_AP = 		(char*)"Inverter Web";
char *password_AP = 	(char*)"vroomvroom";

wifi_mode_t wifi_mode = WIFI_MODE_NULL;
bool mountedSPIFFS = false;
String files[MAX_NUMBER_OF_FILES];
int numberOfFiles, serverIndex;
File fsUploadFile;

Inverter inverter(INVERTER_SERIAL_PORT);
WebServer server(SERVER_PORT);

void task_handle_client(void *);
void task_system_health(void *);
void init_web_server();
void init_wifi_ap();
bool init_wifi_sta();
void index_FS_files();
void init_SPIFFS();
static void __handleCommand();
static void __handleUpdate();
static void __handleBaud();
static void __handleWifi();
String getContentType(String filename);

class InverterWebHandler : public RequestHandler {
	bool canHandle(HTTPMethod method, String uri){
		
		#ifdef DBG_INV_INTERFACE_PRINT
		if(uri.startsWith("/dbg_print")) return true;
		#endif

		if(uri.startsWith("/wifi")
		|| uri.startsWith("/cmd")
		|| uri.startsWith("/fwupdate")
		|| uri.startsWith("/baud")
		) return true;

		return false;
	}

	bool handle(WebServer& server, HTTPMethod method, String uri){
		if(uri.startsWith("/wifi")){
			__handleWifi();
		} else if(uri.startsWith("/cmd")){
			__handleCommand();
		} else if(uri.startsWith("/fwupdate")){
			__handleUpdate();
		} else if(uri.startsWith("/baud")){
			__handleBaud();
		}

		if(uri.startsWith("/dbg_print")){
			if(!server.hasArg("msg")){
				server.send(500, "text/plain", "BAD ARGS");
			} else {
				String msg = server.arg("msg");
				inverter.__DEBUG_PRINT__(msg);
				server.send(500, "text/plain", "OK");
			}
		}

		return true;
	}
} InverterWebHandler;

class StaticWebHandler : public RequestHandler {
	private:
		int sendIdx;
	
	public:
	bool canHandle(HTTPMethod method, String uri){
		if(!mountedSPIFFS || numberOfFiles < 1) return false;
		sendIdx = -1;
		if(uri == "/" && (serverIndex >= 0)){
			sendIdx = serverIndex;
			return true;
		}
		for(int i = 0; i < numberOfFiles; i++){
			if(uri == files[i] || (uri + ".gz") == files[i]){
				sendIdx = i;
				return true;
			}
		}
		return false;
	}

	bool handle(WebServer& server, HTTPMethod method, String uri){
		return _serveFile(server, (uint8_t)sendIdx);
		/* 		
		if(uri == "/" && (serverIndex >= 0)) return _serveFile(server, serverIndex);
			for(int i = 0; i < numberOfFiles; i++){
				if(uri == files[i] || (uri + ".gz") == files[i])
					return _serveFile(server, i);
			}
			return false;
		*/
	}

	private:
	bool _serveFile(WebServer& server, uint8_t idx){
		if(SPIFFS.exists(files[idx])){
			File file = SPIFFS.open(files[idx], "r");
			server.streamFile(file, getContentType(files[idx]));
			file.close();
			return true;
		}
		return false;
	}

} StaticWebHandler;

String getContentType(String filename){
	if(server.hasArg("download")) return "application/octet-stream";
	else if(filename.endsWith(".htm")) return "text/html";
	else if(filename.endsWith(".html")) return "text/html";
	else if(filename.endsWith(".css")) return "text/css";
	else if(filename.endsWith(".js")) return "application/javascript";
	else if(filename.endsWith(".png")) return "image/png";
	else if(filename.endsWith(".gif")) return "image/gif";
	else if(filename.endsWith(".jpg")) return "image/jpeg";
	else if(filename.endsWith(".ico")) return "image/x-icon";
	else if(filename.endsWith(".xml")) return "text/xml";
	else if(filename.endsWith(".pdf")) return "application/x-pdf";
	else if(filename.endsWith(".zip")) return "application/x-zip";
	else if(filename.endsWith(".gz")) return "application/x-gzip";
	return "text/plain";
}

void handleFileUpload() {
    if(server.uri() != "/edit") return;
    HTTPUpload &upload = server.upload();
    if(upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if(!filename.startsWith("/")) filename = "/" + filename;
        fsUploadFile = SPIFFS.open(filename, "w");
        filename = String();
    } else if(upload.status == UPLOAD_FILE_WRITE) {
        if(fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    } else if(upload.status == UPLOAD_FILE_END) {
        if(fsUploadFile) fsUploadFile.close();
		index_FS_files();
    }
}

void handleFileDelete() {
    if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
    String path = server.arg(0);
    //DBG_OUTPUT_PORT.println("handleFileDelete: " + path);

    if(path == "/") return server.send(500, "text/plain", "BAD PATH");

    if(!SPIFFS.exists(path)) return server.send(404, "text/plain", "FileNotFound");

    SPIFFS.remove(path);
    server.send(200, "text/plain", "");
	index_FS_files();
}

void handleFileCreate() {
    if(server.args() == 0)
        return server.send(500, "text/plain", "BAD ARGS");


    String path = server.arg(0);
    DBG_OUTPUT_PORT.println("handleFileCreate: " + path);

    if(path == "/")
        return server.send(500, "text/plain", "BAD PATH");

    if(SPIFFS.exists(path))
        return server.send(500, "text/plain", "FILE EXISTS");


    File file = SPIFFS.open(path, "w");

    if(file)
		file.close();
    else
		return server.send(500, "text/plain", "CREATE FAILED");


    server.send(200, "text/plain", "");
    index_FS_files();
}

void handleFileList(){
	String path = "/";
	if(server.hasArg("dir")) String path = server.arg("dir");
	//DBG_OUTPUT_PORT.println("handleFileList: " + path);

	File root = SPIFFS.open(path);
	String output = "[";
  

    if(!root){
        Serial.println("- failed to open directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(!file.isDirectory()){
			if (output != "[") output += ',';
			output += "{\"type\":\"";
			output += "file";
			output += "\",\"name\":\"";
			output += String(file.name()).substring(1);
			output += "\"}";
        }

        file = root.openNextFile();
    }
	output += "]";
	server.send(200, "text/json", output);
}

static void __handleCommand(){
	const int cmdBufSize = 128;
	if (!server.hasArg("cmd")){
		server.send(500, "text/plain", "BAD ARGS");
		return;
	}

	String output;
	String cmd = server.arg("cmd").substring(0, cmdBufSize);
	int repeat = 0;
	if(server.hasArg("repeat")) repeat = server.arg("repeat").toInt();
	
	inverter.command(cmd, repeat, output);

	server.sendHeader("Access-Control-Allow-Origin", "*");
	server.send(200, "text/json", output);
}

static void __handleUpdate(){
	if(!server.hasArg("step") || !server.hasArg("file")){
		server.send(500, "text/plain", "BAD ARGS");
		return;
	}

	int step = server.arg("step").toInt();
	String path = server.arg("file");

	String message;
	int pages;

	if(server.hasArg("pagesize")){
		int pageSize = server.arg("pagesize").toInt();
		inverter.update(step, path, pageSize, pages, message);
	} else {
		inverter.update(step, path, pages, message);
	}

	server.send(200, "text/json", "{ \"message\": \"" + message + "\", \"pages\": " + pages + " }");
}

static void __handleBaud(){
	if (inverter.fastUart)
		server.send(200, "text/html", "fastUart on");
	else
		server.send(200, "text/html", "fastUart off");
}

static void __handleWifi(){
	bool updated = true;
	if(server.hasArg("apSSID") && server.hasArg("apPW")){
		ssid_AP = (char*)server.arg("apSSID").c_str();
		password_AP = (char*)server.arg("apPW").c_str();
		init_wifi_ap();
	} else if (server.hasArg("staSSID") && server.hasArg("staPW")){
		ssid_STA = (char*)server.arg("staSSID").c_str();
		password_STA = (char*)server.arg("staPW").c_str();
		init_wifi_sta();

	} else {
		File file = SPIFFS.open("/wifi.html", "r");
		String html = file.readString();
		file.close();
		html.replace("%staSSID%", ssid_STA);
		html.replace("%apSSID%", ssid_AP);
		html.replace("%staIP%", WiFi.localIP().toString());
		server.send(200, "text/html", html);
		updated = false;
	}

	if(updated){
		File file = SPIFFS.open("/wifi-updated.html", "r");
		server.streamFile(file, getContentType("wifi-updated.html"));
		file.close();
	}
}

void index_FS_files(){
	if(!mountedSPIFFS) return;
	numberOfFiles = 0;
	serverIndex = -1;

	File root = SPIFFS.open("/");
    File file = root.openNextFile();
	
    while(file){
        if(file.isDirectory()){
			file = root.openNextFile();
			continue;
		}
		if(numberOfFiles > MAX_NUMBER_OF_FILES - 1){
			file.close();
			break;
		}
		String name = String(file.name());
		if(name == SERVER_INDEX) serverIndex = numberOfFiles;
		files[numberOfFiles++] = name;
        file = root.openNextFile();
    }
}

void init_web_server(){
	//Handler for all static files
	server.addHandler(&StaticWebHandler);
	//Handler for all inverter related requests
	server.addHandler(&InverterWebHandler);
	//Handlers for everything else
	server.on("/list", HTTP_GET, handleFileList);
	server.on("/edit", HTTP_PUT, handleFileCreate);
	server.on("/edit", HTTP_DELETE, handleFileDelete);
	server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);
	server.on("/version", [](){ server.send(200, "text/plain", VERSION); });

	server.onNotFound([](){
		if(!mountedSPIFFS){
			server.send(404, "text/plain", F("SPIFFS failed to mount!"));
			return;
		}
		server.send(404, "text/plain", "FileNotFound");
	});

	server.begin();

	xTaskCreate(
		task_handle_client,
		"server_handler",
		10000,
		NULL,
		1,
		NULL
	);
}

void init_wifi_ap(){
	static IPAddress local_IP(1,1,1,1);
	static IPAddress gateway(192,168,4,1);
	static IPAddress subnet(255,255,255,0);
	WiFi.mode(WIFI_MODE_AP);
	WiFi.softAPConfig(local_IP, gateway, subnet);
	WiFi.softAP(ssid_AP, password_AP);
	wifi_mode = WIFI_MODE_AP;
	(DBG_OUTPUT_PORT).print(F("\n WiFi: Mode set to softAP."));
}

bool init_wifi_sta(){
	WiFi.mode(WIFI_MODE_STA);
	WiFi.begin(ssid_STA, password_STA);

	int attempts = MAX_WIFI_CONECTION_ATTEMPTS;

	while(WiFi.status() != WL_CONNECTED){
		if(attempts > 0){
			delay(500);
			attempts--;
			continue;
		}
		(DBG_OUTPUT_PORT).print(F("\nWiFi: Connection to network failed!"));
		return false;
	}

	wifi_mode = WIFI_MODE_STA;
	(DBG_OUTPUT_PORT).print(F("\n WiFi: Connected as station.\nIP address: "));
  	(DBG_OUTPUT_PORT).println(WiFi.localIP());
	
	return true;
}

void init_SPIFFS(){
    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        (DBG_OUTPUT_PORT).print(F("\nSPIFFS: Mount failed!"));
        mountedSPIFFS = false;
    } else {
		(DBG_OUTPUT_PORT).print(F("\nSPIFFS: Mount success."));
		mountedSPIFFS = true;
	}
	index_FS_files();
}

void task_handle_client(void *){
	for(;;){
		server.handleClient();
		vTaskDelay(1);
	}
}

void task_system_health(void *){
	bool ledState = false;
	for(;;){
		ledState = !ledState;
		digitalWrite(LED_PIN, ledState);
		if(wifi_mode == WIFI_MODE_STA && WiFi.status() != WL_CONNECTED){
			(DBG_OUTPUT_PORT).println("WiFi: Lost connection to AP!\nSwitching to softAP mode.");
			init_wifi_ap();
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void setup(){
	(DBG_OUTPUT_PORT).begin(DBG_OUTPUT_BAUD);
	pinMode(LED_PIN, OUTPUT);

	ETH.begin();

	init_SPIFFS();

	if(USE_PRESET_WIFI_CRED){
		if(!init_wifi_sta()) init_wifi_ap();
	} else {
		init_wifi_ap();
	}

	init_web_server();

	xTaskCreate(
		task_system_health,
		"system_health",
		5000,
		NULL,
		1,
		NULL
	);
}

void loop(){}
