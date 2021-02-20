#ifndef _INVERTER_
#define _INVERTER_

class Inverter{
	private:
		HardwareSerial *interface;
		bool initFastUart();
		void sendCommand(String);
	public:
		bool fastUartAvailable = true;
		bool fastUart = false;
		void command(String cmd, int repeat, String& res);
		void update(int step, String file, int &pages, String &msg);
		void update(int step, String file, size_t pageSize, int &pages, String &msg);
		void __DEBUG_PRINT__(String&);
		Inverter(HardwareSerial&);
};

#endif