
#include "FlowSerial.hpp"
#include <ctime>

//#define DEBUG

using namespace std;

FlowSerialConnection::FlowSerialConnection(string iserialPort): serialPort(iserialPort){}

uint8_t FlowSerialConnection::flowRegister[20];

bool FlowSerialConnection::update(){
	static uint16_t checksum;
	static uint16_t checksumRecieved;
	static uint argumentBytesRecieved;
	static uint startAddress;
	static uint nBytes;
	static uint8_t flowSerialBuffer[256];
	static time_t lastTime;
	bool ret = false;
	if(serialPort.is_open()){
		while(!serialPort.eof()){
			uint8_t input;
			serialPort.read(reinterpret_cast<char*>(&input), 1);
			if(!serialPort.eof()){
				switch(flowSerialState){
					case idle:
						if(input == 0xAA){
							#ifdef DEBUG
							cout << "Start recieved" << endl;
							#endif
							checksum = 0xAA;
							flowSerialState = startByteRecieved;
						}
						break;
					case startByteRecieved:
						#ifdef DEBUG
						cout << "instructionRecieved" << endl;
						#endif
						instruction = static_cast<Instruction>(input);
						flowSerialState = instructionRecieved;
						checksum += input;
						argumentBytesRecieved = 0;
						break;
					case instructionRecieved:
						switch(instruction){
							case idRequest:
								//Note idRequest does not give any arguments so this byte is the lsB of checksum.
								checksumRecieved = input;
								flowSerialState = lsbChecksumRecieved;
								break;
							case read:
								if(argumentBytesRecieved == 0)
									startAddress = input;
								else{ //Pressumed 1
									nBytes = input;
									flowSerialState = argumentsRecieved;
								}
								break;
							case write:
								if(argumentBytesRecieved == 0)
									startAddress = input;
								else if(argumentBytesRecieved == 1)
									nBytes = input;
								else{
									flowSerialBuffer[argumentBytesRecieved-2] = input;
									if(argumentBytesRecieved > nBytes)
										flowSerialState = argumentsRecieved;
								}
								break;
							case returnRequestedData:
								if(argumentBytesRecieved == 0)
									nBytes = input;
								else{
									flowSerialBuffer[argumentBytesRecieved-1] = input;
									if(argumentBytesRecieved > nBytes)
										flowSerialState = argumentsRecieved;
								}
						}
						#ifdef DEBUG
						cout << "argumentBytesRecieved = " << argumentBytesRecieved << endl;
						#endif
						checksum += input;
						argumentBytesRecieved++;
						break;
					case argumentsRecieved:
						#ifdef DEBUG
						cout << "LSB" << endl;
						#endif
						checksumRecieved = (input << 8) & 0xFF00;
						flowSerialState = msbChecksumRecieved;
						break;
					case msbChecksumRecieved:
						#ifdef DEBUG
						cout << "MSB" << endl;
						#endif
						checksumRecieved |= input & 0x00FF;
						flowSerialState = lsbChecksumRecieved;
					case lsbChecksumRecieved:
						if(checksum == checksumRecieved){
							flowSerialState = checksumOk;
							#ifdef DEBUG
							cout << "checksum ok" << endl;
							#endif
						}
						else{
							//Message failed. return to idle state.
							#ifdef DEBUG
							cout << "checksum not ok" << endl;
							cout << "checksum, checksumRecieved:" << checksum << ", " << checksumRecieved << endl;
							#endif
							flowSerialState = idle;
						}
					case checksumOk:
						switch(instruction){
							case idRequest:
								#ifdef DEBUG
								cout << "recieved id request" << endl;
								#endif
								returnData(flowId);
								break;
							case read:
								#ifdef DEBUG
								cout << "recieved read request" << endl;
								#endif
								returnData(&FlowSerialConnection::flowRegister[startAddress], nBytes);
								break;
							case write:
								#ifdef DEBUG
								cout << "recieved write request" << endl;
								#endif
								for (uint i = 0; i < nBytes; ++i)
								{
									FlowSerialConnection::flowRegister[i + startAddress] = flowSerialBuffer[i];
								}
								break;
							case returnRequestedData:
								#ifdef DEBUG
								cout << "Got requested data" << endl;
								#endif
								inputBufferAvailable = nBytes;
								for (uint i = 0; i < nBytes; ++i){
									inputBuffer[i] = flowSerialBuffer[i];
								}
						}
						flowSerialState = idle;
						ret = true;
						break;
					default:
						flowSerialState = idle;
				}
			}
		}
		serialPort.clear();
	}
	else{
		//Error port not connected.
	}
	return ret;
}
void FlowSerialConnection::requestId(){
	char out[] = {0xAA, 0, 0x00, 0xAA};
	serialPort.write(out, 4);
}
void FlowSerialConnection::writeToPeerAddress(uint8_t startAddress, uint8_t data[], size_t arraySize){
	sendArray(startAddress, data, arraySize, write);
}
void FlowSerialConnection::returnData(uint8_t data[], size_t arraySize){
	sendArray(0, data, arraySize, returnRequestedData);
}
void FlowSerialConnection::returnData(uint8_t data){
	sendArray(0, &data, 1, returnRequestedData);
}
void FlowSerialConnection::sendArray(uint8_t startAddress, uint8_t data[], size_t arraySize, Instruction instruction){
	uint16_t checksum = 0xAA + static_cast<char>(instruction);
	char charOut = 0xAA;
	serialPort.write(&charOut, 1);
	charOut = static_cast<char>(instruction);
	serialPort.write(&charOut, 1);
	if(instruction == write){
		charOut = static_cast<char>(startAddress);
		serialPort.write(&charOut, 1);
		checksum += startAddress;
	}
	charOut = static_cast<char>(arraySize);
	serialPort.write(&charOut, 1);
	checksum += arraySize;
	for (uint i = 0; i < arraySize; ++i){
		checksum += data[i];
	}
	cout << checksum << endl;
	serialPort.write(reinterpret_cast<const char*>(data), arraySize);
	serialPort.write(reinterpret_cast<const char*>(&checksum), 2);
}