#ifndef ARDUINO
#include "ArduinoControl.h"

ArduinoControl::ArduinoControl(){}

void ArduinoControl::Log(std::string msg, int msg_verbosity, int current_verbosity){
	if(msg_verbosity<current_verbosity) std::cout<<msg<<std::endl;
}

bool ArduinoControl::Initialise(std::string com_port_str, int baud_rate_in, int verb_in){
	
	// FIXME function to update this?
	verbosity=verb_in;
	
	Log(m_unique_name+" Initialising...",v_debug,verbosity);
	
	// serial settings
	baud_rate=baud_rate_in;
	com_port = DeviceNameToNumber(com_port_str);
	if(com_port<0){
		Log(m_unique_name+" invalid com port '"+com_port_str+"'",v_error,verbosity);
		return false;
	}
	Log(m_unique_name+" com port '"+com_port_str+"' mapped to port #"+std::to_string(com_port),v_debug,verbosity);
	get_ok = Connect();
	if(!get_ok){
		Log(m_unique_name+" failed to connect to arduino!",v_error,verbosity);
		return false;
	}
	
	return true;
}


bool ArduinoControl::Execute(bool& quit){
	
	get_ok = true;
	
	// get next command from user
	std::cout<<"> Enter next command:"<<std::endl;
	std::string cmd;
	getline(std::cin,cmd);
	std::cout<<"Processing command '"<<cmd<<"'"<<std::endl;
	
	// should be a key-value pair, value is optional depending on command
	// e.g. '275_A 1' will enable 275 LED, while 'BEEP' will emit a beep.
	std::stringstream ss(cmd);
	std::string key, tmp;
	ss >> key >> tmp;
	
	std::cout<<"key: '"<<key<<"', val: '"<<tmp<<"'"<<std::endl;
	
	if(key=="Dark"){
		// turn off all lights
		std::string resp;
		get_ok = SendAndReceive("OFF",resp);
		get_ok = get_ok & resp=="Disabling all lights";
	}
	
	else if(key=="White"){
		get_ok = get_ok && SetState("WHITE", (tmp=="1"));
	}
		
	if(key=="275_A"){
		get_ok = get_ok && SetState("LED275", (tmp=="1"));
	}
	
	if(key=="Deuterium"){
		get_ok = get_ok && SetState("DEUTERIUM", (tmp=="1"));
	}
	
	if(key=="Tungsten"){
		get_ok = get_ok && SetState("TUNGSTEN", (tmp=="1"));
	}
	
	// generic Grove relay control
	for(int i=0; i<5; ++i){
		std::string check="Relay"+std::to_string(i);
		if(key==check){
			if(i<4) get_ok = get_ok && SetState(key, (tmp=="1"));
			else Log(m_unique_name+" Relay4 received; relays are numbered 0-3",v_error, verbosity);
		}
	}
	
	if(key=="Shutter_gad"){
		get_ok = get_ok && SetState("GAD_ARM", (tmp=="1"));
	}
	
	if(key=="Shutter_ref"){
		get_ok = get_ok && SetState("REF_ARM", (tmp=="1"));
	}
	
	if(key=="Shutter_lamp"){
		get_ok = get_ok && SetState("LAMP_SHUTTER", (tmp=="1"));
	}
	
	// this just enables/disables control via the DB15 connector on the back of the lamp
	// note not all electronics boxes support this; some have it hard-wired to 5V
	if(key=="Lamp_DB15"){
		get_ok = get_ok && SetState("LAMP_DB15", (tmp=="1"));
	}
	
	if(key=="Valve_gad"){
		get_ok = get_ok && SetState("TUBE", (tmp=="1"));
	}
	
	if(key=="Valve_parallel"){
		get_ok = get_ok && SetState("PARALLEL", (tmp=="1"));
	}
	
	if(key=="Valve_pump"){
		get_ok = get_ok && SetState("PUMP", (tmp=="1"));
	}
	
	if(key=="LED_temp"){
		get_ok = get_ok && GetLedTemp();
	}
	
	if(key=="Sol_temps"){
		get_ok = get_ok && GetSolTemps();
	}
	
	if(key=="Flow_check"){
		get_ok = get_ok && GetFlowStatus();
	}
	
	if(key=="BEEP"){
		// TODO add support for beep patterns
		get_ok = get_ok && SerialWrite("BEEP");
	}
	
	if(key=="QUIT"){
		quit=true;
	}
	
	std::cout<<(get_ok ? "OK" : "ERR")<<std::endl;
	return get_ok;
}


bool ArduinoControl::Finalise(){
	
	bool allok = ShutItDown();
	allok = allok && Disconnect();
	
	return allok;
	
}

bool ArduinoControl::SetState(const std::string& name, bool enable){
	Log(m_unique_name+" SetState("+name+", "+std::to_string(enable)+")",v_debug,verbosity);
	if(enable) return Enable(name);
	return Disable(name);
}

bool ArduinoControl::Disable(const std::string& name){
	std::string resp;
	return SendAndReceive(name+" DISABLE", resp);
}

bool ArduinoControl::Enable(const std::string& name){
	std::string resp;
	return SendAndReceive(name+" ENABLE", resp);
}

bool ArduinoControl::ShutItDown(){
	
	// turn off lightsq
	std::map<std::string, bool> results;
	results["LED275"] = Disable("LED275");
	results["White"] = Disable("WHITE");
	results["Deuterium"] = Disable("DEUTERIUM");
	results["Tungsten"] = Disable("TUNGSTEN");
	// close shutters
	results["GAD"] = Disable("GAD_ARM");
	results["REF"] = Disable("REF_ARM");
	results["LAMP"] = Disable("LAMP_SHUTTER");
	// disable lamp DB15 control
	results["LAMP_DB15"] = Disable("LAMP_DB15");
	// disable other relays, whatever they may be used for
	results["PUMP"] = Disable("PUMP");
	results["Relay3"] = Disable("RELAY3");
	// close valves
	results["PARALLEL"] = Disable("PARALLEL");
	results["TUBE"] = Disable("TUBE");
	
	bool allok=true;
	for(auto&& res : results){
		if(!res.second){
			allok = false;
			Log(m_unique_name+"::ShutItDown error disabling "+res.first,v_error,verbosity);
		}
	}
	
	return allok;
}

bool ArduinoControl::GetLedTemp(){
	std::string resp;
	bool ok = SendAndReceive("LED_TEMP",resp);
	if(!ok){
		Log(m_unique_name+"::GetLedTemp error '"+resp+"'",v_error,verbosity);
		return false;
	}
	// format is 'LED_TEMP: X' (so we know we're getting the right number)
	std::stringstream ss(resp);
	std::string tmp;
	double led_temp=0;
	ss >> tmp >> led_temp;
	ok = (!ss.fail() && (ss>>std::ws).eof());
	if(!ok || tmp!="LED_TEMP:"){
		Log(m_unique_name+"::GetLedTemp error '"+resp+"'",v_error,verbosity);
		return false;
	}
	std::cout<<"LED_T: "<<led_temp<<" C"<<std::endl;
	return true;
}

bool ArduinoControl::GetSolTemp(int sol_num, double& temp){
	// solenoid numbers should be 0-2
	std::string key = "SOL"+std::to_string(sol_num)+"_TEMP";
	std::string resp;
	bool ok = SendAndReceive(key,resp);
	if(!ok){
		Log(m_unique_name+"::GetSolTemp error '"+resp+"'",v_error,verbosity);
		return false;
	}
	
	// format is 'SOLN_TEMP: X' (so we know we're getting the right number)
	std::string expected_key = "SOL"+std::to_string(sol_num)+"_TEMP:";
	std::stringstream ss(resp);
	std::string tmp;
	double sol_temp=0;
	ss >> tmp >> sol_temp;
	ok = (!ss.fail() && (ss>>std::ws).eof());
	if(!ok || tmp!=expected_key){
		Log(m_unique_name+"::GetSolTemp error '"+resp+"'",v_error,verbosity);
		return false;
	}
	temp=sol_temp;
	std::cout<<"expected_key: "<<sol_temp<<" C"<<std::endl;;
	return true;
}

bool ArduinoControl::GetSolTemps(){
	// more likely we're gonna want all 3, so we have a command for that
	std::string resp;
	bool ok = SendAndReceive("SOL_TEMPS",resp);
	if(!ok){
		Log(m_unique_name+"::GetSolTemps error '"+resp+"'",v_error,verbosity);
		return false;
	}
	// response should be of the form: 'SOL_TEMPS:,T1,T2,T3'
	std::stringstream ss(resp);
	std::string tmp;
	std::vector<double> temps;
	while(std::getline(ss, tmp, ',')){
		Log(m_unique_name+": next part: '"+tmp+"'",v_debug,verbosity);
		if(tmp=="SOL_TEMPS:") continue;
		try{
			double temp = std::stod(tmp);
			temps.push_back(temp);
		} catch(...){
			Log(m_unique_name+"::GetSolTemps error; bad temp format '"+resp+"'",v_error,verbosity);
			return false;
		}
	}
	if(temps.size()>3){
		Log(m_unique_name+"::GetSolTemps too many temperatures ("+std::to_string(temps.size())+")"
		    +", response: '"+resp+"'",v_error,verbosity);
		return false;
	} else {
		std::cout<<"Sol_Ts: "<<temps[0]<<", "<<temps[1]<<", "<<temps[2]<<" C"<<std::endl;
	}
	return true;
}

bool ArduinoControl::GetFlowStatus(){
	std::string resp;
	bool ok = SendAndReceive("FLOW_SENSE",resp);
	if(!ok){
		Log(m_unique_name+"::GetFlowStatus error '"+resp+"'",v_error,verbosity);
		return false;
	}
	// format is 'FLOW_SENSE: X' (so we know we're getting the right number)
	// X is either 0 or 1, currently used flow sensor does not provide an actual rate
	std::stringstream ss(resp);
	std::string tmp;
	char flow_state=0;
	ss >> tmp >> flow_state;
	ok = (!ss.fail() && (ss>>std::ws).eof());
	bool good_header = (tmp=="FLOW_SENSE:");
	bool good_flow_state = (flow_state=='1' || flow_state=='0');
	if(!ok || !good_header || !good_flow_state){
		Log(m_unique_name+"::GetFlowStatus error '"+resp+"'",v_error,verbosity);
		return false;
	}
	int flow_val = std::atoi(&flow_state);
	std::cout<<"Flow_Status: "<<flow_val<<std::endl;
	return true;
}

bool ArduinoControl::Connect(){
	
	// default arduino serial is 8 data bits, 1 stop bit, no parity, no flow control
	char mode[]={'8','N','1',0};
	int flowctl=0;
	
	if(RS232_OpenComport(com_port, baud_rate, mode, flowctl)){
		Log(m_unique_name+" Unable to open com port "+std::to_string(com_port),v_error,verbosity);
		return false;
	}
	
	// make sure nothing stray in the buffers
	// wait for arduino to do initial setup - this prints things we can't suppress
	// (from relay i2c library)
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	RS232_flushRXTX(com_port);
	
	std::string bootupmsg= "Scanning...\nI2C device found at address 0x11  !\nFound 1 devices\nfound grove on I2C address 17\nrelay comms initialised";
	
	// flush doesn't seem to work, so let's read out any garbage...?
	std::string response = SerialRead(1000);
	Log(m_unique_name+" flushing connection string '"+response+"'",v_debug,verbosity);
	
	// check comms are working as expected
	get_ok = SendAndReceive("HELLO",response);
	// as a standard, the arduino sketch returns strings starting with 'Err: ABC' when it fails to do something
	//if(response.substr(0,4)=="Err:"){
	// or we can just check for the exact expected response
	if(!get_ok || response!="Hello!"){
		if(get_ok) Log(m_unique_name+" Error with initial comms check; expected response 'Hello!', got '"+response+"'",v_error,verbosity);
		return false;
	}
	
	// request arduino to connect to the grove relay controller over SPI
	get_ok = SendAndReceive("GROVE",response);
	if(!get_ok) return false;
	
	// turn off all lights, close all shutters and valves.
	//get_ok = ShutItDown();
	
	return get_ok;
	
}

bool ArduinoControl::Disconnect(){
	// tell arduino to close serial comms
	// (it'll open it again in 5 seconds and start listening for new connections)
	get_ok = SerialWrite("QUIT");
	if(!get_ok){
		Log(m_unique_name+" Error sending message 'QUIT'",v_error,verbosity);
		return false;
	}
	return true;
}

bool ArduinoControl::SendAndReceive(std::string msg, std::string& response, int timeout){
	Log(m_unique_name+" SendAndReceive of command '"+msg+"'",v_debug,verbosity);
	// send command
	//std::cout<<"Writing command"<<std::endl;
	get_ok = SerialWrite(msg);
	if(!get_ok){
		Log(m_unique_name+" Error sending message '"+msg+"'",v_error,verbosity);
		return false;
	}
	// read response
	//std::cout<<"Reading response..."<<std::endl;
	response = SerialRead(timeout);
	if(response.empty()){
		Log(m_unique_name+" Error: No response to command '"+msg+"'",v_error,verbosity);
		return false;
	}
	// as a standard, the arduino sketch returns strings starting with 'Err: ABC' when it fails to do something
	if(response.substr(0,4)=="Err:"){
		Log(m_unique_name+" Error: command '"+msg+"' returned '"+response+"'",v_error,verbosity);
		return false;
	}
	
	return true;
}

bool ArduinoControl::SerialWrite(std::string msg){
	if(msg.size()==0){
		Log(m_unique_name+" SerialWrite with empty message!",v_error,verbosity);
		return false;
	}
	if(msg.back()!='\n') msg.push_back('\n');
	get_ok = RS232_SendBuf(com_port, (unsigned char*)(msg.data()), msg.size());
	if(get_ok!=msg.size()){
		Log(m_unique_name+" Error writing message '"+msg+"'; sent "+std::to_string(get_ok)
		    +" bytes of "+std::to_string(msg.size()),v_error,verbosity);
		return false;
	}
	return true;
}

std::string ArduinoControl::SerialRead(int timeout_ms){
	int bytesread=0;
	const int maxbytes = 4096; // don't receive more than this many bytes
	std::string response;
	unsigned char buf[4096];
	atimer.reset();
	while(atimer.ms_elapsed()<timeout_ms){
		bytesread = RS232_PollComport(com_port, buf, 4096);
		if(bytesread>0){
			//printf("received %d bytes\n",bytesread);
			//for(int i=0; i<bytesread; ++i) printf("'%c' ",buf[i]);  // "%X: %c",buf[i],buf[i]);
			//printf("\n");
			response += std::string((const char*)(buf),bytesread);
		}
		// arduino ends SerialWrite with CRLF ('0x0D 0x0A') ('\n' == LF)
		if(bytesread>0 && (buf[bytesread-1]=='\n' || buf[bytesread-1]=='\0')){
			// received termination char
			//printf("got termchar!\n");
			break;
		} else if(response.size()>maxbytes){
			// don't just keep adding indefinitely.. something wrong.
			// instead return what we got and flush the remainder of the buffer.
			RS232_flushRXTX(com_port);
			Log(m_unique_name+" Warning! Too many chars in serial buffer! Flushing any remainder",v_warning,verbosity);
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	//printf("total response: '%s'\n",response.c_str());
	// trim whitespace
	while(response.size() && (isspace(response.back()) || response.back()=='\n')) response.pop_back();
	//printf("trimmed response: '%s'\n",response.c_str());
	return response;
}

int ArduinoControl::DeviceNameToNumber(std::string name){
	static const std::vector<std::string> devicenames{
	 "/dev/ttyS0","/dev/ttyS1","/dev/ttyS2","/dev/ttyS3","/dev/ttyS4","/dev/ttyS5",
	 "/dev/ttyS6","/dev/ttyS7","/dev/ttyS8","/dev/ttyS9","/dev/ttyS10","/dev/ttyS11",
	 "/dev/ttyS12","/dev/ttyS13","/dev/ttyS14","/dev/ttyS15","/dev/ttyUSB0",
	 "/dev/ttyUSB1","/dev/ttyUSB2","/dev/ttyUSB3","/dev/ttyUSB4","/dev/ttyUSB5",
	 "/dev/ttyAMA0","/dev/ttyAMA1","/dev/ttyACM0","/dev/ttyACM1",
	 "/dev/rfcomm0","/dev/rfcomm1","/dev/ircomm0","/dev/ircomm1",
	 "/dev/cuau0","/dev/cuau1","/dev/cuau2","/dev/cuau3",
	 "/dev/cuaU0","/dev/cuaU1","/dev/cuaU2","/dev/cuaU3"
	};
	auto it = std::find(devicenames.begin(),devicenames.end(),name);
	if(it!=devicenames.end()) return std::distance(devicenames.begin(),it);
	else return -1;
}

#endif
