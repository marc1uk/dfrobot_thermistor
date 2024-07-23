/* vim:set noexpandtab tabstop=4 wrap */
// installation:
// pip3 install --user pyserial
// sudo apt-get install libc6-i386
// sudo chmod -aG dialout ${USER}
// either logout and log back in, or run 'exec su -l $USER' from a shell,
// then start arduino IDE from that shell session
// add http://download.dfrobot.top/boards/package_DFRobot_index.json
// to the 'Additional boards manager URLs' in File->Preferences
// then Tools->BoardManager, on LHS scroll to find 'DFRobot FireBeetle-ESP8266' and click install.

// docs: https://wiki.dfrobot.com/FireBeetle_ESP8266_IOT_Microcontroller_SKU__DFR0489

#define RELAY_PIN D2     // relay is on first GPIO digital pin with no extra functionality, D2
#define LED_BUILTIN D9   // in-built LED on the DFRobot
#define KTY0_PIN A0      // voltage across KTY thermistor 1
#define KTY1_PIN A1      // voltage across KTY thermistor 1

// for connecting to a WiFi network
//#include <WiFi.h>       // ESP32; not us.
#include "ESP8266WiFi.h"  // ESP8266; yes.
// for connecting to the strongest available network for which we have credentials
//#include <WiFiMulti.h>

// for registering multiple SSIDs
//WiFiMulti wifiMulti;

// RemoteClient is who we're going to talk to
WiFiClient RemoteClient;
WiFiServer Server;
const uint ServerPort = 23;
const char[] mode = "Server";  // or "Client"
const char[] HostName = "dfrobot";

// https://github.com/espressif/arduino-esp32
// https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
// https://randomnerdtutorials.com/esp32-access-point-ap-web-server/

// for periodically taking action
unsigned long previousMillis = 0;
unsigned long interval = 30000;

void setup(){
	Serial.begin(115200);
	delay(10); // units ms
	
	// set built-in LED as an output
	pinMode(LED_BUILTIN, OUTPUT);
	// blink 3 times to show we're initialising
	for(int i=0; i<3; ++i){
		digitalWrite(LED_BUILTIN, HIGH);
		delay(1000);
	}
	
	// set WiFi mode:
	 WiFi.mode(WIFI_STA)      // station mode: the ESP32 connects to an access point
	// WiFi.mode(WIFI_AP)       // access point mode: stations can connect to the ESP32
	// WiFi.mode(WIFI_AP_STA)   // access point and a station connected to another access point
	
	// if using WIFI_AP, configure it with this:
	// WiFi.softAP(const char* ssid, const char* password, int channel, int ssid_hidden, int max_connections)
	// note max_connections = 1->4 is max number of simultaneously connected clients
	// wifi channel number = 1->13
	
	/*
	// Set a Static IP address
	IPAddress local_IP(192, 168, 1, 184);
	IPAddress gateway(192, 168, 1, 1);
	IPAddress subnet(255, 255, 0, 0);
	IPAddress primaryDNS(8, 8, 8, 8);   // optional, use INADDR_NONE if not required
	IPAddress secondaryDNS(8, 8, 4, 4); // optional, use INADDR_NONE if not required
	if(!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)){
		Serial.println("STA Failed to configure");
	}
	*/
	
	// set hostname of the DFRobot
	// n.b. apparently you must call WiFi.config() first for this to take effect
	// but we can use null values if we don't want a static IP
	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
	WiFi.setHostname(HostName);
	
	// scan available networks
	Serial.println("Scanning for available networks...");
	int n = WiFi.scanNetworks();
	
	// print found networks
	Serial.print(n);
	Serial.println(" networks found");
	if(n>0){
		for(int i = 0; i < n; ++i){
			// Print SSID and RSSI for each network found
			Serial.print(i+1);
			Serial.print(": ");
			Serial.print(WiFi.SSID(i));
			Serial.print(" (signal strength: ");
			Serial.print(WiFi.RSSI(i));  // dB? will be negative: a *lower* absolute value means a stronger Wi-Fi connection
			Serial.print(")");
			Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
			/*  encryption types:
				WIFI_AUTH_OPEN
				WIFI_AUTH_WEP
				WIFI_AUTH_WPA_PSK
				WIFI_AUTH_WPA2_PSK
				WIFI_AUTH_WPA_WPA2_PSK
				WIFI_AUTH_WPA2_ENTERPRISE
			*/
			delay(10);
		}
	}
	
	// connect to a specific WiFi network
	WiFi.begin("2012kamioka", "neutrinooscillation");          // FIXME make sure we don't upload this to github
	
	// connect to the strongest of multiple possible networks
	//wifiMulti.addAP("2012kamioka", "neutrinooscillation");   // FIXME make sure we don't upload this to github
	//wifiMulti.addAP("kamguest", "kamiokaobs");               // FIXME make sure we don't upload this to github
	//wifiMulti.run();
	wifiMulti.run(connectTimeoutMs)
	
	// wait for connection to be established
	Serial.print("Wait for WiFi... ");
	while(WiFi.status() != WL_CONNECTED){
		/* WIFi.status() or wifiMulti.run() return:
			0   WL_IDLE_STATUS        temporary status assigned when WiFi.begin() is called
			1   WL_NO_SSID_AVAIL      when no SSID are available
			2   WL_SCAN_COMPLETED     scan networks is completed
			3   WL_CONNECTED          when connected to a WiFi network
			4   WL_CONNECT_FAILED     when the connection fails for all the attempts
			5   WL_CONNECTION_LOST    when the connection is lost
			6   WL_DISCONNECTED	when  disconnected from a network
		*/
		Serial.print(".");
		delay(500);
	}
	
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	
	if(mode=="Server"){
		// Use WiFiServer class to allow TCP queries to us
		//WiFiServer(uint16_t port=80, uint8_t max_clients=4);
		//WiFiServer(const IPAddress& addr, uint16_t port=80, uint8_t max_clients=4);
		Server = WiFiServer(ServerPort);
		Server.begin();
	}
	
	delay(500);
	
}

void loop(){
	
	// check if we lost WiFi connection
	while(WiFi.status() != WL_CONNECTED){
		Serial.println("WiFi not connected!");
		Serial.println("Attempting reconnect...");
		while(WiFi.status() != WL_CONNECTED){
			WiFi.disconnect();   // not sure if we strictly need this but some use it.
			
			// single SSID mode
			WiFi.reconnect();    // we can use either this or WiFi.begin()
			// mutiple SSID mode
			// allegedly wifiMulti may reconnect autmatically behind the scenes...
			// though i can't see it doing that in the code. At the least we can do this:
			//wifiMulti.run(1000); // Scan for up to 1s...
			
			Serial.print(".");
			delay(1000);
		}
	}
	
	// periodically print what wifi we're conected to and signal strength
	unsigned long currentMillis = millis();
	if((millis - previousMillis) > interval){
		// print which network we're currently on
		Serial.print("WiFi connected: ");
		Serial.print(WiFi.SSID());
		Serial.print(", signal strength: ");
		Serial.println(WiFi.RSSI());
		previousMillis = millis();
	}
	
	// connect to the pi (client mode), or check if someone is trying to connect to us (server mode)
	if(mode=="Client"){
		
		// Use WiFiClient class to make TCP queries to another server
		const char * host = "http://133.11.177.101"; // ip or dns
		const uint16_t port = 80;
		Serial.println("Connecting to remote host...");
		while(!RemoteClient.connect(host, port)){
			Serial.print(".");
			delay(1000); // wait 1 seconds and retry
		}
		
	} else if(mode=="Server"){
		
		// check for new connection attempts
		while(true){
			// is a new client trying to connect?
			if(Server.hasClient()){
				// Do we already have a connection to an existing client?
				if(RemoteClient.connected()){
					// client already connected: reject the new connection
					// (we could actually have up to 4 open connections i believe)
					Serial.println("Another client is already connected. New connection rejected");
					Server.available().stop();
				} else {
					// no client currently connected: accept the new client
					Serial.println("New connection accepted");
					RemoteClient = Server.available();
				}
			}
			
			// if no client is connected, continue to wait
			if(RemoteClient.connect()){
				Serial.println("Waiting for connection from remote client...");
				delay(500);
			} else {
				// new connection attempts handled, continue to rest of code
				break;
			}
		}
		
	} else {
		Serial.print("Uknown Client/Server mode: ");
		Serial.println(mode);
	}
	
	/////////////////////////////////////////////////////////////////////////////////////////////////
	
	// write data to client
	uint8_t SendBuffer[30] = "Hello Client";
	if(RemoteClient.connected()){
		RemoteClient.write(SendBuffer, 30);
		// or:
		RemoteClient.println(SendBuffer);
	}
	
	// receive data in byte-sized chunks:
	uint8_t ReceiveBuffer[30];
	while(RemoteClient.connected() && RemoteClient.available()){
		int ReceivedBytes = RemoteClient.read(ReceiveBuffer, sizeof(ReceiveBuffer));
		// or for a single byte: char c = RemoteClient.read();
		Serial.print("Received message: ");
		Serial.print(ReceivedBytes);
		Serial.println(" bytes: ");
		Serial.println(ReceiveBuffer);
	}
	// or receive it all at once:
	String ReceiveBufferString = RemoteClient.readStringUntil('\r');
	Serial.print("Received ");
	Serial.print(ReceiveBufferString.length());
	Serial.print(" bytes: ");
	Serial.println(ReceiveBufferString);
	
	// control the relay
	if(ReceivedBytes[0]==1){
		digitalWrite(RELAY_PIN, HIGH);
		delay(1000);
		digitalWrite(RELAY_PIN, LOW);
	}
	
	// get thermistor values
	int sensorValue = analogRead(KTY0_PIN);
	float voltage= sensorValue * (3.3 / 1023.0);
	Serial.print("analog 0 sensor value: ");
	Serial.println(voltage);
	
	sensorValue = analogRead(KTY1_PIN);
	voltage= sensorValue * (3.3 / 1023.0);
	Serial.print("analog 1 sensor value: ");
	Serial.println(voltage);
	
}

/////////////////////////////////////////////////////////////////////////

/*
	The ESP32 generates the following Wi-Fi events:
		0	ARDUINO_EVENT_WIFI_READY				ESP32 Wi-Fi ready
		1	ARDUINO_EVENT_WIFI_SCAN_DONE			ESP32 finishes scanning AP
		2	ARDUINO_EVENT_WIFI_STA_START			ESP32 station start
		3	ARDUINO_EVENT_WIFI_STA_STOP				ESP32 station stop
		4	ARDUINO_EVENT_WIFI_STA_CONNECTED		ESP32 station connected to AP
		5	ARDUINO_EVENT_WIFI_STA_DISCONNECTED		ESP32 station disconnected from AP
		6	ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE	the auth mode of AP connected by ESP32 station changed
		7	ARDUINO_EVENT_WIFI_STA_GOT_IP			ESP32 station got IP from connected AP
		8	ARDUINO_EVENT_WIFI_STA_LOST_IP			ESP32 station lost IP and the IP is reset to 0
		9	ARDUINO_EVENT_WPS_ER_SUCCESS			ESP32 station wps succeeds in enrollee mode
		10	ARDUINO_EVENT_WPS_ER_FAILED				ESP32 station wps fails in enrollee mode
		11	ARDUINO_EVENT_WPS_ER_TIMEOUT			ESP32 station wps timeout in enrollee mode
		12	ARDUINO_EVENT_WPS_ER_PIN				ESP32 station wps pin code in enrollee mode
		13	ARDUINO_EVENT_WIFI_AP_START				ESP32 soft-AP start
		14	ARDUINO_EVENT_WIFI_AP_STOP				ESP32 soft-AP stop
		15	ARDUINO_EVENT_WIFI_AP_STACONNECTED		a station connected to ESP32 soft-AP
		16	ARDUINO_EVENT_WIFI_AP_STADISCONNECTED	a station disconnected from ESP32 soft-AP
		17	ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED		ESP32 soft-AP assign an IP to a connected station
		18	ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED	Receive probe request packet in soft-AP interface
		19	ARDUINO_EVENT_WIFI_AP_GOT_IP6			ESP32 access point v6IP addr is preferred
		19	ARDUINO_EVENT_WIFI_STA_GOT_IP6			ESP32 station v6IP addr is preferred
		19	ARDUINO_EVENT_ETH_GOT_IP6				Ethernet IPv6 is preferred
		20	ARDUINO_EVENT_ETH_START					ESP32 ethernet start
		21	ARDUINO_EVENT_ETH_STOP					ESP32 ethernet stop
		22	ARDUINO_EVENT_ETH_CONNECTED				ESP32 ethernet phy link up
		23	ARDUINO_EVENT_ETH_DISCONNECTED			ESP32 ethernet phy link down
		24	ARDUINO_EVENT_ETH_GOT_IP				ESP32 ethernet got IP from connected AP
		25	ARDUINO_EVENT_MAX
	
	// a function to be called when we get disconnected:
	void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
		Serial.println("Disconnected from WiFi access point because: ");
		Serial.println(info.wifi_sta_disconnected.reason);
	}
	WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
	
	
	// different ways to register a function:
	// get called for all events
	WiFi.onEvent(myWiFiEventHandlerFunction);
	// get called only for a given type of event:
	WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
	// using a lambda function for the event handler
	WiFiEventId_t eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
		Serial.print("WiFi lost connection. Reason: ");
		Serial.println(info.wifi_sta_disconnected.reason);
	}, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
	
	// to unregister an event:
	WiFi.removeEvent(eventID);
	
	
	
	// generic event handler:
	 void WiFiEvent(WiFiEvent_t event){
		Serial.printf("[WiFi-event] event: %d\n", event);
		switch (event) {
		    case ARDUINO_EVENT_WIFI_READY: 
		        Serial.println("WiFi interface ready");
		        break;
		    case ARDUINO_EVENT_WIFI_SCAN_DONE:
		        Serial.println("Completed scan for access points");
		        break;
		    case ARDUINO_EVENT_WIFI_STA_START:
		        Serial.println("WiFi client started");
		        break;
		    case ARDUINO_EVENT_WIFI_STA_STOP:
		        Serial.println("WiFi clients stopped");
		        break;
		    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
		        Serial.println("Connected to access point");
		        break;
		    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
		        Serial.println("Disconnected from WiFi access point");
		        break;
		    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
		        Serial.println("Authentication mode of access point has changed");
		        break;
		    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
		        Serial.print("Obtained IP address: ");
		        Serial.println(WiFi.localIP());
		        // or Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
		        break;
		    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
		        Serial.println("Lost IP address and IP address is reset to 0");
		        break;
		    case ARDUINO_EVENT_WPS_ER_SUCCESS:
		        Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
		        break;
		    case ARDUINO_EVENT_WPS_ER_FAILED:
		        Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
		        break;
		    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
		        Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
		        break;
		    case ARDUINO_EVENT_WPS_ER_PIN:
		        Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
		        break;
		    case ARDUINO_EVENT_WIFI_AP_START:
		        Serial.println("WiFi access point started");
		        break;
		    case ARDUINO_EVENT_WIFI_AP_STOP:
		        Serial.println("WiFi access point  stopped");
		        break;
		    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
		        Serial.println("Client connected");
		        break;
		    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
		        Serial.println("Client disconnected");
		        break;
		    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
		        Serial.println("Assigned IP address to client");
		        break;
		    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
		        Serial.println("Received probe request");
		        break;
		    case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
		        Serial.println("AP IPv6 is preferred");
		        break;
		    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
		        Serial.println("STA IPv6 is preferred");
		        break;
		    case ARDUINO_EVENT_ETH_GOT_IP6:
		        Serial.println("Ethernet IPv6 is preferred");
		        break;
		    case ARDUINO_EVENT_ETH_START:
		        Serial.println("Ethernet started");
		        break;
		    case ARDUINO_EVENT_ETH_STOP:
		        Serial.println("Ethernet stopped");
		        break;
		    case ARDUINO_EVENT_ETH_CONNECTED:
		        Serial.println("Ethernet connected");
		        break;
		    case ARDUINO_EVENT_ETH_DISCONNECTED:
		        Serial.println("Ethernet disconnected");
		        break;
		    case ARDUINO_EVENT_ETH_GOT_IP:
		        Serial.println("Obtained IP address");
		        break;
		    default: break;
		}}
*/
