#include <iostream>
using namespace std;
#include <unistd.h>
#include "FlowSerial.hpp"

int main(int argc, char** argv){
	uint8_t flowRegister[32];
	FlowSerial::UsbSocket arduino(flowRegister, sizeof(flowRegister) / sizeof(flowRegister[0]));
	arduino.connnectToDevice("/dev/ttyACM0", 115200);
	arduino.flowRegister[0] = 0;

	while(1){
		arduino.update();
		static int i;
		cout << "flowReg is now: " << static_cast<int>(arduino.flowRegister[0]) << endl;
		cout << "loop: " << ++i << endl;
		static uint8_t out = 1;
		out ^= 1;
		cout << "out = " << static_cast<int>(out) << endl << endl;
		arduino.writeToPeer(0, &out, 1);
		usleep(1000000);
	}
	return 0;
}
