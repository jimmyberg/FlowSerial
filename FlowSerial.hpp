
#ifndef _FLOWSERIAL_HPP_
#define _FLOWSERIAL_HPP_

#include <iostream>
#include <string>
#include <fstream>

using namespace std;

class FlowSerialConnection{
public:
	explicit FlowSerialConnection(string iserialPort);
	bool update();
	void requestId();
	void readFromPeerAddress(uint8_t startAddress, uint8_t nBytes);
	void writeToPeerAddress(uint8_t startAddress, uint8_t data[], size_t arraySize);
	int available();
	static uint8_t flowRegister[20];
private:
	const static uint inputBufferSize = 256;
	bool debugEnabled = false;
	uint8_t flowId = 0x00;
	uint8_t inputBufferAvailable;
	uint8_t inputBuffer[inputBufferSize];

	fstream serialPort;
	enum FlowSerialState{
		idle,
		startByteRecieved,
		instructionRecieved,
		argumentsRecieved,
		lsbChecksumRecieved,
		msbChecksumRecieved,
		checksumOk
	} flowSerialState;
	enum Instruction{
		idRequest,
		read,
		write,
		returnRequestedData
	}instruction;
	void returnData(uint8_t data[], size_t arraySize);
	void returnData(uint8_t data);
	void sendArray(uint8_t startAddress, uint8_t data[], size_t arraySize, Instruction instruction);
};


#endif //_FLOWSERIAL_HPP_