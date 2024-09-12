#ifndef ArduinoControl_H
#define ArduinoControl_H

#include "rs232.h"
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

struct timer {
	public:
	void reset(){
		start = std::chrono::steady_clock::now();
	}
	unsigned long long ms_elapsed() const {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	}
	private:
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

class ArduinoControl {
	
	public:
	
	ArduinoControl();
	bool Initialise(std::string port_in, int baud_rate_in=19200, int verb_in=1);
	bool Execute(bool& quit);
	bool Finalise();
	void Log(std::string msg, int msg_verbosity, int current_verbosity);
	
	int DeviceNameToNumber(std::string str);
	bool Connect();
	bool ShutItDown();  // turn off lights, valves, close shutters
	bool Disconnect();
	
	std::string SerialRead(int timeout_ms=1000);
	bool SerialWrite(std::string msg);
	bool SendAndReceive(std::string msg, std::string& response, int timeout=3000);
	
	bool Disable(const std::string& name);
	bool Enable(const std::string& name);
	bool SetState(const std::string& name, bool enable);
	bool GetLedTemp();
	bool GetSolTemp(int sol, double& temp);
	bool GetSolTemps();
	bool GetFlowStatus();
	
	private:
	timer atimer;
	int com_port;
	int baud_rate;
	
	int verbosity=1;
	int v_error=0;
	int v_warning=1;
	int v_message=2;
	int v_debug=3;
	int get_ok;
	std::string m_unique_name="ArduinoControl";
	
};


#endif
