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

//#define _DEBUG_FLOW_SERIAL_

using namespace std;

namespace FlowSerial{

	BaseSocket::BaseSocket(uint8_t* iflowRegister, size_t iregisterLength):
		flowRegister(iflowRegister),
		registerLength(iregisterLength)
	{}

	bool BaseSocket::handleData(const uint8_t* const data, size_t arraySize){
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
					flowSerialState = State::instructionReceived;
					instruction = static_cast<Instruction>(input);
					argumentBuffer.clearAll();
					checksum += input;
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "instructionReceived" << endl;
					#endif
					break;
				case State::instructionReceived:
					argumentBuffer.set(&input, 1);
					checksum += input;
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "argument bytes = " << argumentBuffer.getStored() << endl;
					#endif
					switch(instruction){
						case Instruction::read:
							if(argumentBuffer.getStored() >= 2){
								flowSerialState = State::argumentsReceived;
							}
							break;
						case Instruction::write:
							if(argumentBuffer.getStored() >= 2 && argumentBuffer.getStored() >= (argumentBuffer[1] + 2u)){
								flowSerialState = State::argumentsReceived;
							}
							break;
						case Instruction::returnRequestedData:
							if(argumentBuffer.getStored() >= 2 && argumentBuffer.getStored() >= argumentBuffer[0] + 1u){
								flowSerialState = State::argumentsReceived;
							}
							break;
					}
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
					unsigned int startIndex;
					unsigned int endIndex;
					uint8_t* copyPointer;
					switch(instruction){
						case Instruction::read:
							#ifdef _DEBUG_FLOW_SERIAL_
							cout << "received Instruction::read request" << endl;
							#endif
							returnData(&FlowSerial::BaseSocket::flowRegister[argumentBuffer[0]], argumentBuffer[1]);
							break;
						case Instruction::write:
							#ifdef _DEBUG_FLOW_SERIAL_
							cout << "received Instruction::write request" << endl;
							#endif
							startIndex = argumentBuffer[0];
							endIndex = argumentBuffer[0] + argumentBuffer[1];
							copyPointer = &argumentBuffer[2];
							for (uint i = startIndex; i < endIndex; ++i){
								FlowSerial::BaseSocket::flowRegister[i] = copyPointer[i];
							}
							break;
						case Instruction::returnRequestedData:
							#ifdef _DEBUG_FLOW_SERIAL_
							cout << "Got requested data. " << +argumentBuffer[0] << " bytes containing:" << endl;
							for (uint i = 0; i < argumentBuffer[0]; ++i){
								cout << +argumentBuffer[i+1] << endl;
							}
							#endif
							inputBuffer.set(&argumentBuffer[1], argumentBuffer[0]);
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
	void BaseSocket::write(uint8_t startAddress, const uint8_t data[], size_t size){
		sendFlowMessage(startAddress, data, size, Instruction::write);
	}
	size_t BaseSocket::returnDataSize(){
		size_t ret = inputBuffer.getStored();
		return ret;
	}
	void BaseSocket::clearReturnedData(){
		inputBuffer.clearAll();
	}
	size_t BaseSocket::getReturnedData(uint8_t dataReturn[], size_t size){
		size_t ret = inputBuffer.get(dataReturn, size);
		return ret;
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
		writeToInterface(charOut, outIndex);
	}
}
