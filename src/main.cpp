#ifndef ARDUINO
#include "ArduinoControl.h"
#include <cstring>

int main(int argc, const char* argv[]){
	
	if(argc==2 && ((strcmp(argv[1],"--help")==0) || (strcmp(argv[1],"-h")==0))){
		std::cerr<<"Usage: "<<argv[0]<<" [com_port=/dev/ttyACM0] [verbosity=0]"<<std::endl;
		return 0;
	}
	
	std::cout<<"making ArduinoControl class"<<std::endl;
	ArduinoControl ctrl;
	std::cout<<"Initialising"<<std::endl;
	if(argc==1){
		ctrl.Initialise("/dev/ttyACM0");
	} else if(argc==2){
		ctrl.Initialise(argv[1]);
	} else {
		int verbosity = std::atoi(argv[2]);
		if(verbosity==0 && strcmp(argv[2],"0")!=0){
			std::cerr<<"Error parsing verbosity '"<<argv[2]<<"'"<<std::endl;
			return 1;
		}
		ctrl.Initialise(argv[1],19200,verbosity);
	}
	
	bool quit;
	do {
		ctrl.Execute(quit);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	} while(!quit);
	
	ctrl.Finalise();
	
	return 0;
}

#endif
