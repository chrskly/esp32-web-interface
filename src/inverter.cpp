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
#include <inverter.h>
#include <DBG_DEFS.h>

#define UART_BUFFER_SIZE 1024 * 2	//Way bigger then it needs to be, but ehhh why not
#define DEFAULT_BAUD 115200
#define FAST_BAUD 921600
#define PAGE_SIZE_BYTES 1024

static uint32_t crc32_word(uint32_t Crc, uint32_t Data){
  int i;

  Crc = Crc ^ Data;

  for(i=0; i<32; i++)
	if (Crc & 0x80000000)
	  Crc = (Crc << 1) ^ 0x04C11DB7; // Polynomial used in STM32
	else
	  Crc = (Crc << 1);

  return(Crc);
}

static uint32_t crc32(uint32_t* data, uint32_t len, uint32_t crc){
   for (uint32_t i = 0; i < len; i++)
	  crc = crc32_word(crc, data[i]);
   return crc;
}

Inverter::Inverter(HardwareSerial& _port){
	interface = &_port;
	interface->begin(DEFAULT_BAUD);
	interface->setRxBufferSize(UART_BUFFER_SIZE);
}

//TODO: Add timeout to serial flush
void Inverter::sendCommand(String cmd){
	interface->print("\n");
	delay(1);
	while(interface->available()){	
		interface->read();
	}
	interface->print(cmd);
	interface->print("\n");
	interface->readStringUntil('\n');	
}

bool Inverter::initFastUart(){
	if (!fastUart && fastUartAvailable){
		sendCommand("fastuart");
		if (interface->readString().startsWith("OK")){
			interface->begin(FAST_BAUD);
			fastUart = true;
		} else {
			fastUartAvailable = false;
		}
	}
	return fastUart;
}

void Inverter::command(String cmd, int repeat, String& res){
	char buffer[255];
	size_t len = 0;

	#ifndef DBG_INV_NO_FAST_UART
	initFastUart();
	#endif

	sendCommand(cmd);

	do {
		memset(buffer, 0, sizeof(buffer));
		len = interface->readBytes(buffer, sizeof(buffer) - 1);
		res += buffer;

		if (repeat){
			repeat--;
			interface->print("!");
			interface->readBytes(buffer, 1); //consume "!"
		}
	} while (len > 0);

}

void Inverter::update(int step, String path, int& pages, String& msg){
	return update(step, path, PAGE_SIZE_BYTES, pages, msg);
}

void Inverter::update(int step, String path, size_t pageSize, int& pages, String& msg){
	File file = SPIFFS.open(path, "r");
	pages = (file.size() + pageSize - 1) / pageSize;

	if(step == -1){
		int c;
		sendCommand("reset");

		if(fastUart){
			interface->begin(DEFAULT_BAUD);
			fastUart = false;
			fastUartAvailable = true; //retry after reboot
		}

		do {
			c = interface->read();
		} while (c != 'S' && c != '2');

		//version 2 bootloader
		if(c == '2'){
			//Send magic
			interface->write(0xAA);
			while (interface->read() != 'S') ;
		}

		interface->write(pages);
		while (interface->read() != 'P'){

		};

		msg = "reset";
	} else {
		bool repeat = true;
		file.seek(step * pageSize);
		char buffer[pageSize];
		size_t bytesRead = file.readBytes(buffer, sizeof(buffer));

		while (bytesRead < pageSize){
			buffer[bytesRead++] = 0xff;
		}
			
		uint32_t crc = crc32((uint32_t *)buffer, pageSize / 4, 0xffffffff);

		while (repeat) {
			interface->write(buffer);
			while (!interface->available()){
				
			};

			char res = interface->read();

			if ('C' == res){
				interface->write((char *)&crc);
				while (!interface->available()){

				};
				res = interface->read();
			}

			switch(res){
				case 'D':
					msg = "Update Done";
					repeat = false;
					fastUartAvailable = true;
					break;
				case 'E':
					while (interface->read() != 'T'){

					}
					break;
				case 'P':
					msg = "Page write success";
					repeat = false;
					break;
				default:
				case 'T':
					break;
			}
		}
	}
	file.close();
}

void Inverter::__DEBUG_PRINT__(String &msg){
	interface->println(msg);
}
