#include <iostream>
using namespace std;

#include "FlowSerial.hpp"

int main(int argc, char** argv){
	uint8_t flowRegister[32];
	FlowSerial::UsbSocket arduino(flowRegister, sizeof(flowRegister) / sizeof(flowRegister[0]));
	int i = 0;
	arduino.flowRegister[0] = 20;
	cout << "flowReg is now: " << static_cast<int>(arduino.flowRegister[0]) << endl;
	while(1){
		if(arduino.update()){
			cout << "flowReg is now: " << static_cast<int>(arduino.flowRegister[0]) << endl;
			cout << "loop: " << ++i << endl;
			static uint8_t out = 1;
			out ^= 1;
			cout << "out = " << static_cast<int>(out) << endl << endl;
			arduino.writeToPeer(0, &out, 1);
		}
	}
	return 0;
}
