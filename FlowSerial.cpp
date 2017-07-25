/*
 * \		______ _                 _____            _                      _             
 * \\		|  ___| |               |  ___|          (_)                    (_)            
 * \\\		| |_  | | _____      __ | |__ _ __   __ _ _ _ __   ___  ___ _ __ _ _ __   __ _ 
 * \\\\		|  _| | |/ _ \ \ /\ / / |  __| '_ \ / _` | | '_ \ / _ \/ _ \ '__| | '_ \ / _` |
 * \\\\\	| |   | | (_) \ V  V /  | |__| | | | (_| | | | | |  __/  __/ |  | | | | | (_| |
 * \\\\\\	\_|   |_|\___/ \_/\_/   \____/_| |_|\__, |_|_| |_|\___|\___|_|  |_|_| |_|\__, |
 * \\\\\\\	                                     __/ |                                __/ |
 * \\\\\\\\                                     |___/                                |___/ 
 * \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
 */
/** \file	FlwoSerial.cpp
 * \author		Jimmy van den Berg at Flow Engineering
 * \date		22-march-2016
 * \brief		A library for the PC to use the FlowSerial protocol.
 * \details 	FlowSerial was designed to send and receive data orderly between devices.
 * 				It is peer based so communication and behavior should be the same one both sides.
 * 				It was first designed and used for the AMD project in 2014.
 */

#include "FlowSerial.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h> //For the open parameter bits open(fd,"blabla", <this stuff>);
#include <termios.h> //For toptions and friends
#include <sys/select.h> //For the pselect command

//#define _DEBUG_FLOW_SERIAL_

using namespace std;

namespace FlowSerial{

	BaseSocket::BaseSocket(uint8_t* iflowRegister, size_t iregisterLength):
		flowRegister(iflowRegister),
		registerLength(iregisterLength)
	{}

	bool BaseSocket::update(const uint8_t* const data, size_t arraySize){
		// By default return false. Return true when a frame has been
		// successfully handled
		bool ret = false;
		for (uint i = 0; i < arraySize; ++i){
			uint8_t input = data[i];
			#ifdef _DEBUG_FLOW_SERIAL_
			cout << "next byte in line is number : " << +input << endl;
			#endif
			switch(flowSerialState){
				case State::idle:
					if(input == 0xAA){
						#ifdef _DEBUG_FLOW_SERIAL_
						cout << "Start received" << endl;
						#endif
						checksum = 0xAA;
						flowSerialState = State::startByteReceived;
					}
					break;
				case State::startByteReceived:
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "instructionReceived" << endl;
					#endif
					instruction = static_cast<Instruction>(input);
					flowSerialState = State::instructionReceived;
					checksum += input;
					argumentBytesReceived = 0;
					break;
				case State::instructionReceived:
					switch(instruction){
						case Instruction::read:
							if(argumentBytesReceived == 0)
								startAddress = input;
							else{ //Presumed 1
								nBytes = input;
								flowSerialState = State::argumentsReceived;
							}
							break;
						case Instruction::write:
							if(argumentBytesReceived == 0)
								startAddress = input;
							else if(argumentBytesReceived == 1)
								nBytes = input;
							else{
								flowSerialBuffer[argumentBytesReceived-2] = input;
								if(argumentBytesReceived-1 >= nBytes)
									flowSerialState = State::argumentsReceived;
							}
							break;
						case Instruction::returnRequestedData:
							if(argumentBytesReceived == 0)
								nBytes = input;
							else{
								flowSerialBuffer[argumentBytesReceived-1] = input;
								if(argumentBytesReceived >= nBytes)
									flowSerialState = State::argumentsReceived;
							}
					}
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "argumentBytesReceived = " << argumentBytesReceived << endl;
					#endif
					checksum += input;
					argumentBytesReceived++;
					break;
				case State::argumentsReceived:
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "LSB received" << endl;
					#endif
					checksumReceived = input & 0xFF;
					flowSerialState = State::lsbChecksumReceived;
					break;
				case State::lsbChecksumReceived:
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "MSB received" << endl;
					#endif
					checksumReceived |= (input << 8) & 0xFF00;
					if(checksum == checksumReceived){
						flowSerialState = State::checksumOk;
						#ifdef _DEBUG_FLOW_SERIAL_
						cout << "checksum OK" << endl;
						#endif
						//No break. Continue to next part.
					}
					else{
						//Message failed. return to idle state.
						#ifdef _DEBUG_FLOW_SERIAL_
						cout << "checksum not OK" << endl;
						cout << 
							"checksum:            " << checksum << '\n' <<
							"!= checksumReceived: " << checksumReceived << endl;
						#endif
						flowSerialState = State::idle;
						break;
					}
				case State::checksumOk:
					switch(instruction){
						case Instruction::read:
							#ifdef _DEBUG_FLOW_SERIAL_
							cout << "received Instruction::read request" << endl;
							#endif
							returnData(&FlowSerial::BaseSocket::flowRegister[startAddress], nBytes);
							break;
						case Instruction::write:
							#ifdef _DEBUG_FLOW_SERIAL_
							cout << "received Instruction::write request" << endl;
							#endif
							for (uint i = 0; i < nBytes; ++i){
								FlowSerial::BaseSocket::flowRegister[i + startAddress] = flowSerialBuffer[i];
							}
							break;
						case Instruction::returnRequestedData:
							#ifdef _DEBUG_FLOW_SERIAL_
							cout << "Got requested data. " << nBytes << " bytes containing:" << endl;
							for (uint i = 0; i < nBytes; ++i){
								cout << +flowSerialBuffer[i] << endl;
							}
							#endif
							mutexInbox.lock();
							inputBufferAvailable = nBytes;
							for (uint i = 0; i < nBytes; ++i){
								inputBuffer[i] = flowSerialBuffer[i];
							}
							inputBuffer[nBytes] = 0;
							mutexInbox.unlock();
					}
					flowSerialState = State::idle;
					ret = true;
					break;
				default:
					flowSerialState = State::idle;
			}
		}
		return ret;
	}
	void BaseSocket::sendReadRequest(uint8_t startAddress, size_t nBytes){
		sendFlowMessage(startAddress, nullptr, nBytes, Instruction::read);
	}
	void BaseSocket::writeToPeer(uint8_t startAddress, const uint8_t data[], size_t size){
		sendFlowMessage(startAddress, data, size, Instruction::write);
	}
	size_t BaseSocket::available(){
		return inputBufferAvailable;
	}
	void BaseSocket::clearReturnedData(){
		inputBufferAvailable = 0;
	}
	void BaseSocket::getReturnedData(uint8_t dataReturn[]){
		mutexInbox.lock();
		for (int i = 0; i < inputBufferAvailable; ++i){
			dataReturn[i] = inputBuffer[i];
		}
		inputBufferAvailable = 0;
		mutexInbox.unlock();
	}
	void BaseSocket::returnData(const uint8_t data[], size_t arraySize){
		sendFlowMessage(0, data, arraySize, Instruction::returnRequestedData);
	}
	void BaseSocket::returnData(uint8_t data){
		sendFlowMessage(0, &data, 1, Instruction::returnRequestedData);
	}

	void BaseSocket::sendFlowMessage(uint8_t startAddress, const uint8_t data[], size_t arraySize, Instruction instruction){
		uint16_t checksum = 0xAA + static_cast<char>(instruction);
		size_t outIndex = 0;
		uint8_t charOut[1024];
		charOut[outIndex++] = 0xAA;
		charOut[outIndex++] = static_cast<uint8_t>(instruction);
		if(instruction == Instruction::write || instruction == Instruction::read){
			charOut[outIndex++] = startAddress;
			checksum += startAddress;
		}
		charOut[outIndex++] = static_cast<uint8_t>(arraySize);
		checksum += static_cast<uint8_t>(arraySize);
		if(data != NULL){
			for (uint i = 0; i < arraySize; ++i){
				charOut[outIndex++] = data[i];
				checksum += data[i];
			}
		}
		charOut[outIndex++] = checksum & 0xFF;
		charOut[outIndex++] = checksum >> 8;
		sendToInterface(charOut, outIndex);
	}

	UsbSocket::UsbSocket(uint8_t* iflowRegister, size_t iregisterLength)
		:BaseSocket(iflowRegister, iregisterLength)
	{}

	UsbSocket::~UsbSocket()
	{
		closeDevice();
		stopUpdateThread();
	}

	void UsbSocket::connectToDevice(const char filePath[], uint baudRate){
		if (fd < 0){
			fd = open(filePath, O_RDWR | O_NOCTTY);
			if (fd < 0){
				throw CouldNotOpenError();
			}
			#ifdef _DEBUG_FLOW_SERIAL_
			else{
				cout << "successfully connected to UsbSocket " << filePath << endl;
			}
			#endif
			struct termios toptions;
			// Get current options of "Terminal" (since it is a tty i think)
			tcgetattr(fd, &toptions);
			// Adjust baud rate. see "man termios"
			switch(baudRate){
				case 0:
					cfsetspeed(&toptions, B0);
					break;
				case 50:
					cfsetspeed(&toptions, B50);
					break;
				case 75:
					cfsetspeed(&toptions, B75);
					break;
				case 110:
					cfsetspeed(&toptions, B110);
					break;
				case 134:
					cfsetspeed(&toptions, B134);
					break;
				case 150:
					cfsetspeed(&toptions, B150);
					break;
				case 200:
					cfsetspeed(&toptions, B200);
					break;
				case 300:
					cfsetspeed(&toptions, B300);
					break;
				case 600:
					cfsetspeed(&toptions, B600);
					break;
				case 1200:
					cfsetspeed(&toptions, B1200);
					break;
				case 1800:
					cfsetspeed(&toptions, B1800);
					break;
				case 2400:
					cfsetspeed(&toptions, B2400);
					break;
				case 4800:
					cfsetspeed(&toptions, B4800);
					break;
				case 9600:
					cfsetspeed(&toptions, B9600);
					break;
				case 19200:
					cfsetspeed(&toptions, B19200);
					break;
				case 38400:
					cfsetspeed(&toptions, B38400);
					break;
				case 57600:
					cfsetspeed(&toptions, B57600);
					break;
				case 115200:
					cfsetspeed(&toptions, B115200);
					break;
				case 230400:
					cfsetspeed(&toptions, B230400);
					break;
				default:
					cerr << "Warning: the set baud rate is not available.\nSelected 9600.\nPlease next time select one of the available following baud rates:\n" << 
					"0\n" <<
					"50\n" <<
					"75\n" <<
					"110\n" <<
					"134\n" <<
					"150\n" <<
					"200\n" <<
					"300\n" <<
					"600\n" <<
					"1200\n" <<
					"1800\n" <<
					"2400\n" <<
					"4800\n" <<
					"9600\n" <<
					"19200\n" <<
					"38400\n" <<
					"57600\n" <<
					"115200\n" <<
					"230400" << endl;
					cfsetspeed(&toptions, B9600);
			}

			// Adjust to 8 bits, no parity, no stop bits
			toptions.c_cflag &= ~PARENB;
			toptions.c_cflag &= ~CSTOPB;
			toptions.c_cflag &= ~CSIZE;
			toptions.c_cflag |= CS8;

			// Non-canonical mode. Means do not wait for newline before sending it through.
			toptions.c_lflag &= ~ICANON;
			// commit the serial port settings
			tcsetattr(fd, TCSANOW, &toptions);
		}
		else{
			cerr << "Error: Trying opening " << filePath << " but fd >= 0 so it must be already open." << endl;
			throw CouldNotOpenError();
		}
	}

	void UsbSocket::closeDevice(){
		if(fd >= 0){
			if(close(fd) < 0){
				cerr << "Error: could not close the device." << endl;
			}
			else{
				fd = -1;
			}
		}
	}

	bool UsbSocket::update(){
		return update(0);
	}

	bool UsbSocket::update(uint timeoutMs){
		if(fd >= 0){
			if(timeoutMs != 0){
				//Configure time to wait for response
				const uint timeoutNs = (timeoutMs * 1000000) % 1000000000;
				const uint timeoutS = timeoutMs / 1000;
				#ifdef _DEBUG_FLOW_SERIAL_
				cout << "timeout us = " << timeoutNs << endl;
				cout << "timeout s = " << timeoutS << endl;
				#endif
				struct timespec timeout = {timeoutS, timeoutNs};
				//A set with fd's to check for timeout. Only one fd used here.
				fd_set readDiscriptors;
				FD_ZERO(&readDiscriptors);
				FD_SET(fd, &readDiscriptors);
				#ifdef _DEBUG_FLOW_SERIAL_
				cout << "Serial is open." << endl;
				#endif
				int pselectReturnValue = pselect(1, &readDiscriptors, NULL, NULL, &timeout, NULL);
				if(pselectReturnValue == -1){
					cerr << "Error: FlowSerial USB connection error. pselect function had an error" << endl;
					throw ConnectionError();
				}
				#ifdef _DEBUG_FLOW_SERIAL_
				else if(pselectReturnValue){
					cout << "Debug: FlowSerial Received a message within timeout" << endl;
				}
				#endif
				else if (pselectReturnValue == 0){
					cerr << "Error: FlowSerial USB connection error. timeout reached." << endl;
					throw TimeoutError();
				}
			}
			uint8_t inputBuffer[256];
			ssize_t receivedBytes = ::read(fd, inputBuffer, sizeof(inputBuffer));
			if(receivedBytes < 0){
				throw ReadError();
			}
			#ifdef _DEBUG_FLOW_SERIAL_
			if(receivedBytes > 0)
				cout << "received bytes = " << receivedBytes << endl;
			#endif
			uint arrayMax = sizeof(inputBuffer) / sizeof(*inputBuffer);
			if(receivedBytes > arrayMax)
				receivedBytes = arrayMax;
			return FlowSerial::BaseSocket::update(inputBuffer, receivedBytes);
		}
		#ifdef _DEBUG_FLOW_SERIAL_
		cerr << "Error: Could not read from device/file. File is closed. fd < 0" << endl;
		#endif
		throw ReadError();
		return false;
	}

	void UsbSocket::startUpdateThread(){
		threadRunning = true;
		sem_init(&producer, 0, 1);
		sem_init(&consumer, 0, 0);
		threadChild = thread(&UsbSocket::updateThread, this);
	}

	void UsbSocket::stopUpdateThread(){
		if(threadRunning == true){
			threadRunning = false;
			closeDevice();
			sem_destroy(&producer);
			sem_destroy(&consumer);
			//Wait for thread to be stopped
			threadChild.join();
		}
	}

	void UsbSocket::sendToInterface(const uint8_t data[], size_t arraySize){
		ssize_t writeRet = ::write(fd, data, arraySize);
		if(writeRet < 0){
			cerr << "Error: could not write to device/file" << endl;
			throw WriteError();
		}
		#ifdef _DEBUG_FLOW_SERIAL_
		for (size_t i = 0; i < arraySize; ++i)
		{
			cout << "byte[" << i << "] = " << +data[i] << endl;
		}
		cout << "Written " << writeRet << " of " << arraySize << " bytes to interface" << endl;
		#endif
	}

	bool UsbSocket::is_open(){
		return fd > 0;
	}

	void UsbSocket::read(uint8_t startAddress, uint8_t returnData[], size_t size){
		sendReadRequest(startAddress, size);
		// Wait for the data to be reached
		int trials = 0;
		while(available() < size){
			if(threadRunning == true){
				//Used to measure time for timeout
				struct timespec ts;
				while(available() < size){
					if(clock_gettime(CLOCK_REALTIME, &ts) == -1){
						throw string("cold not get the time from clock_gettime");
					}
					ts.tv_sec += 1; //500ms
					int retVal = sem_timedwait(&consumer, &ts);
					if(retVal == -1){
						int errnoVal = errno;
						if(errnoVal == ETIMEDOUT){
							if(trials < 5){
								#ifdef _DEBUG_FLOW_SERIAL_
								cout << "Received timeout. bytes received so far = " << available() << " of " << size << "bytes" << '\n';
								cout << "Sending another read request." << endl;
								#endif
								//Reset input data
								clearReturnedData();
								//Send another read request
								sendReadRequest(startAddress, size);
								trials++;
							}
							else{
								throw TimeoutError();
							}
						}
						else{
							throw string("Error at semaphore somehow");
						}
					}
					else{
						#ifdef _DEBUG_FLOW_SERIAL_
						cout << "package is being handled. Posting producer." << endl;
						#endif
						sem_post(&producer);
					}
				}
			}
			else{
				try{
					update(500);
				}
				catch (TimeoutError){
					if(trials < 5){
						//Indicates error
						#ifdef _DEBUG_FLOW_SERIAL_
						cout << "Received timeout. bytes received so far = " << available() << " of " << size << "bytes" << '\n';
						cout << "Sending another read request." << endl;
						#endif
						//Reset input data
						clearReturnedData();
						//Send another read request
						sendReadRequest(startAddress, size);
						trials++;
					}
					else{
						closeDevice();
						throw ReadError();
					}
				}
			}
		}
		getReturnedData(returnData);
	}
	
	void UsbSocket::updateThread(){
		while(threadRunning){
			update();
			if(available()){
				int retVal = sem_trywait(&producer);
				if(retVal == -1){
					//semaphore unchaned since there was a error.
					int errnoVal = errno;
					if(errnoVal == EAGAIN){
						//Sem was empty it seems
						/*#ifdef _DEBUG_FLOW_SERIAL_
						cout << "thread has made a package available. Semaphore full though." << endl;
						#endif*/
					}
					else{
						throw string("Some error with the sem_trywait() has occured. It was not empty that i know of.");
					}
				}
				else{
					//Semaphore has been lowered. Lifting the consumer semaphore
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "thread has made a package available. Posting semaphore." << endl;
					#endif
					retVal = sem_post(&consumer);
				}
			}
		}
	}
}
