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
/** \file	FlwoSerial.hpp
 * \author		Jimmy van den Berg at Flow Engineering
 * \date		22-march-2016
 * \brief		A library for the pc to use the FlowSerial protocole.
 * \details 	FlowSerial was designed to send and recieve data ordendenly between devices.
 * 				It is peer based so communucation and behavior should be the same one both sides.
 * 				It was first designed and used for the AMD project in 2014.
 */

#ifndef _FLOWSERIAL_HPP_
#define _FLOWSERIAL_HPP_

#include <string>

using namespace std;

namespace FlowSerial{

	enum class State{
		idle,
		startByteRecieved,
		instructionRecieved,
		argumentsRecieved,
		lsbChecksumRecieved,
		msbChecksumRecieved,
		checksumOk
	};
	enum class Instruction{
		read,
		write,
		returnRequestedData
	};
	
	/**
	 * @brief FlowSerial connection object.
	 * @details Is the portal to communicate to the other device with the FlowSerial protocole
	 */
	class BaseSocket{
	public:
		/**
		 * @brief Contructor
		 * 
		 * @param iserialPort Initialize the serialport to this device file.
		 */
		BaseSocket(uint8_t* iflowRegister, size_t iregisterLenght);
		/**
		 * @brief Request data from the other FlowSerial party register.
		 * @detials When requesting more that one byte the data will be put in the input buffer lowest index first.
		 * 
		 * @param startAddress Start address of the register you want to read. If single this is the register location you will get.
		 * @param nBytes Request data from startAddress register up to nBytes.
		 */
		void sendReadRequest(uint8_t startAddress, size_t nBytes);
		/**
		 * @brief Write to the other FlowSerial party register
		 * @details [long description]
		 * 
		 * @param startAddress Startadrres you want to fill.
		 * @param data Actual data in an array.
		 * @param arraySize Specify the array size of the data array.
		 */
		void writeToPeer(uint8_t startAddress, const uint8_t* const data, size_t arraySize);
		/**
		 * @brief Check available bytes in input buffer.
		 * @details Note that older data will be deleted when new data arrives.
		 * @return Number of byte available in input buffer.
		 */
		size_t available();
		/**
		 * @brief Copies the input buffer into dataReturn.
		 * @details It will fill up to \p FlowSertial::BaseSocket::inputAvailable bytes
		 * 
		 * @param data Adrray where the data will be copied.
		 */
		void getReturnedData(uint8_t dataReturn[]);
		/**
		 * @brief Sets input buffer index to zero. This way all input is cleared.
		 */
		void clearReturnedData();
		/**
		 * Own register which can be read and written to from other party.
		 * It is up to the programmer how to use this.
		 * Either activly send data to the other part or passivly store data for others to read when requested.
		 */
		uint8_t* const flowRegister;
		/**
		 * Defines the size of the array where FlowSerial::BaseSocket::flowRegister points to.
		 */
		const size_t registerLenght;
	protected:
		/**
		 * @brief Update function. Input the recieved data here in chronological order.
		 * @details The data that is been thrown in here will be handled as FlowSerial data. 
		 * After calling this function the data that was passed can be erased/overwritten.
		 * @return True when a full FlowSerial message has been recieved.
		 */
		bool update(const uint8_t data[], size_t arraySize);
		/**
		 * @brief Function that must be used to parse the information generated from this class to the interface.
		 * @details The data must be on chronological order. So 0 is first out.
		 * 
		 * @param data Array this must be send to the interface handeling FlowSerial. 0 is first out.
		 * @param arraySize Defenition for size of the array.
		 */
		virtual void sendToInterface(const uint8_t data[], size_t arraySize) = 0;
	private:

		const static uint inputBufferSize = 256;
		uint8_t inputBufferAvailable = 0;
		uint8_t inputBuffer[inputBufferSize];
		//For update function
		//Own counting checksum
		uint16_t checksum;
		//Checksum read from package
		uint16_t checksumRecieved;
		//keeps track how many arguments were recieved
		uint argumentBytesRecieved;
		//Remember what the start address was
		uint startAddress;
		//How many bytes needs reading or writing. Recieved from package
		uint nBytes;
		//Temporary store writing data into this buffer untill the checksum is read and checked
		uint8_t flowSerialBuffer[256];
	
		State flowSerialState;
		Instruction instruction;

		void returnData(const uint8_t data[], size_t arraySize);
		void returnData(uint8_t data);
		void sendArray(uint8_t startAddress, const uint8_t data[], size_t arraySize, Instruction instruction);
	};
	
	class UsbSocket : public BaseSocket{
	public:
		UsbSocket(uint8_t* iflowRegister, size_t iregisterLenght);
		~UsbSocket();
		void connectToDevice(const char filePath[], uint baudRate);
		void readFromPeerAddress(uint8_t startAddress, uint8_t returnData[], size_t nBytes);
		void closeDevice();
		bool update();
		bool is_open();
	private:
		int fd = -1;
		virtual void sendToInterface(const uint8_t data[], size_t arraySize);
	};

	class ConnectionError{
	public:
		ConnectionError():
			errorMessage("Error: connection error"){}
		ConnectionError(string ierrorMessage):
			errorMessage(ierrorMessage){}
		string errorMessage;
	};
	class CouldNotOpenError: public ConnectionError
	{
	public:
		CouldNotOpenError():ConnectionError("Error: could not open device"){}
	};
	class ReadError: public ConnectionError
	{
	public:
		ReadError():ConnectionError("Error: could not read to device"){}
	};
	class WriteError: public ConnectionError
	{
	public:
		WriteError():ConnectionError("Error: could not write to device"){}
	};
	class TimeoutError: public ConnectionError
	{
	public:
		TimeoutError():ConnectionError("Error: an error occured not read to device"){}
	};
}
#endif //_FLOWSERIAL_HPP_
