#include <iostream>
using namespace std;

#include "FlowSerial.hpp"

int main(int argc, char** argv){
	system("stty -F /dev/ttyACM0 cs8 115200 ignbrk -brkint -icrnl -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts");
	//system("stty -F /dev/ttyACM0 cs8 115200");
	FlowSerialConnection arduino("/dev/ttyACM0");
	//FlowSerialConnection arduino("/dev/ttyACM0");
	int i = 0;
	uint8_t out = 1;
	arduino.flowRegister[0] = 20;
	cout << "flowReg is now: " << static_cast<int>(arduino.flowRegister[0]) << endl;
	while(1){
		if(arduino.update()){
			cout << "flowReg is now: " << static_cast<int>(arduino.flowRegister[0]) << endl;
			cout << "loop: " << ++i << endl;
			static uint8_t out = 1;
			out ^= 1;
			cout << "out = " << static_cast<int>(out) << endl << endl;
			arduino.writeToPeerAddress(0, &out, 1);
		}
	}
	return 0;
}