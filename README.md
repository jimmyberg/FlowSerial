#FlowSerial
FlowSerial is a library that can be used to share a array with another peer. The array can both be read as been written by the other party. This simplifies and generalizes the way to communicate.
The array is to be generated by the user and given to the FlowSerial class. This is done by assigning a pointer.

##Install
To use this in a non git project use
```
git clone https://github.com/overlord1123/FlowSerial.git
```
To use this in a git repository use
```
git submodule add https://github.com/overlord1123/FlowSerial.git
```
##Example

```c++
#include <iostream>   //For printing to console
using namespace std;
#include <unistd.h>   //For usleep specific for this example
#include "FlowSerial/FlowSerial.hpp" //Include the FlowSerial library here

int main(int argc, char** argv){
	//Example array used to communicate to a arduino
	uint8_t flowRegister[32];
	//Construct a UsbSocket object to communicate
	FlowSerial::UsbSocket arduino(flowRegister, sizeof(flowRegister) / sizeof(flowRegister[0]));
	//Connect to the serial device. Useally /dev/ttyACM0 for Arduino's
	arduino.connectToDevice("/dev/ttyACM0", 115200);
	//Set element 0 to zero to see if it changes.
	arduino.flowRegister[0] = 0;

	while(1){
		static int i;
		static uint8_t out = 1;
		//Show wich loop this is
		cout << "\nloop: " << ++i << endl;
		cout << "Updating FlowSerial...";
		cout.flush();
		//Use this funtion to process incomming information automaticly
		arduino.update();
		cout << "done" << endl;
		//Show if element 0 has changed
		cout << "flowRegister[0] is now: " << static_cast<int>(arduino.flowRegister[0]) << endl;
		//Alternate out with 0 and 1
		out ^= 1;
		cout << "Writing = " << static_cast<int>(out) << " to arduino at index 2..." << endl;
		cout.flush();
		//Preform a write. Out will be writen to element 2. out is of size 1
		arduino.writeToPeer(2, &out, 1);
		count << "done" << endl;
		uint8_t readBuffer[10];
		cout << "Preform a read from arduino. index 2...";
		cout.flush();
		//Preform a read from element 2 to comfirm reading and writing
		arduino.readFromPeerAddress(2, readBuffer, 1);
		count << "done\n" << "recieved " << +readBuffer[0] << endl;
		//Use sleep to sleep for 10^6 microseconds = 1 second
		usleep(1000000);
	}
	return 0;
}

```
