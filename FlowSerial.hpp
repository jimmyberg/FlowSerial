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
 * \brief		A library for the PC to use the FlowSerial protocol.
 * \details 	FlowSerial was designed to send and receive data orderly between devices.
 * 				It is peer based so communication and behavior should be the same one both sides.
 * 				It was first designed and used for the Active Mass Damper project in 2014 by Flow Engineering.
 */

#ifndef _FLOWSERIAL_HPP_
#define _FLOWSERIAL_HPP_

#include <stdint.h>
#include <CircularBuffer>
#include <LinearBuffer>

using namespace std;

namespace FlowSerial{

	enum class State{
		idle,
		startByteReceived,
		instructionReceived,
		argumentsReceived,
		lsbChecksumReceived,
		msbChecksumReceived,
		checksumOk
	};
	enum class Instruction{
		read,
		write,
		returnRequestedData
	};
	
	/**
	 * @brief      Handles FlowSerial data communication.
	 * @details    This object takes a array of bytes and allows FlowSerial
	 *             peers to read and write in this register.
	 */
	class BaseSocket{
	public:
		/**
		 * @brief      Constructor
		 *
		 * @param      iflowRegister    Pointer to the register.
		 * @param[in]  iregisterLength  The length of the register
		 */
		BaseSocket(uint8_t* iflowRegister, size_t iregisterLength);
		/**
		 * @brief      Request data from the other FlowSerial party register.
		 *
		 *             This is for low level use where you can poll
		 *             BaseSocket::returnDataSize or even better (tip), instead of
		 *             polling first do other stuff and than poll for incoming
		 *             data. This is more efficient than using the read command.
		 *
		 * @note       This function does not grantee the data from the other
		 *             peer to actually arrive nor does it a resend after a
		 *             timeout. It just sends a request!
		 *
		 * @param      startAddress  Start address of the register you want to
		 *                           read. If single this is the register
		 *                           location you will get.
		 * @param      nBytes        Request data from startAddress register up
		 *                           to nBytes.
		 */
		void sendReadRequest(uint8_t startAddress, size_t nBytes);
		/**
		 * @brief      Reads from peer address. This has a timeout functionality
		 *             of 500 ms.
		 *
		 *             It will try five times with resending of the request
		 *             before throwing an exception.
		 *
		 * @todo Implement this function as follows. Step 1: Call
		 * BaseSocket::sendReadRequest to send a read request. Step 2: Put new
		 * incoming data from the interface into the BaseSocket::handleData
		 * function. Step 3: Check if BaseSocket::returnDataSize equals nBytes. Of
		 * not goto step 2. If yes call BaseSocket::getReturnedData with
		 * returnData and size as arguments. After this you are done. Make sure
		 * you implement some sort of timeout when waiting for
		 * BaseSocket::returnDataSize. After an timeout goto step 1.
		 *
		 * @param[in]  startAddress  The start address where to begin to read
		 *                           from other peer
		 * @param      returnData    Array that will be filled with requested
		 *                           data.
		 * @param[in]  size          Desired number of data starting from start
		 *                           address. Must be smaller than the size of
		 *                           returnData.
		 */
		virtual void read(uint8_t startAddress, uint8_t returnData[], size_t size) = 0;
		/**
		 * @brief      Write to the other FlowSerial party register.
		 *
		 * @note       This function does not guarantee nor check an actual
		 *             write. It only sends a write appropriate request.
		 *
		 * @param      startAddress  Startadrres you want to fill.
		 * @param      data          Actual data in an array.
		 * @param[in]  size          The size
		 * @param      arraySize  Specify the array size of the data array.
		 */
		void write(uint8_t startAddress, const uint8_t data[], size_t size);
		/**
		 * @brief      Check available bytes in the returned data buffer.
		 *
		 * @note       When receiving multiple returns, the data will be
		 *             appended to the buffer FIFO style.
		 *
		 * @return     Number of byte available in input buffer.
		 */
		size_t returnDataSize();
		/**
		 * @brief      Copies the input buffer into dataReturn up to size bytes.
		 * @note       When receiving multiple returns, the data will be
		 *             appended to the buffer FIFO style.
		 *
		 * @param      dataReturn  The data that has been returned from read
		 *                         request
		 * @param[in]  size        Size of dataReturn. It will fetch up to this
		 *                         number. (tip) May also be smaller than the
		 *                         actual size of dataReturn when a certain
		 *                         amount is required.
		 *
		 * @return     Size of actual data copied into dataReturn. A number
		 *             smaller than size indicates the buffer being empty.
		 */
		size_t getReturnedData(uint8_t dataReturn[], size_t size);
		/**
		 * @brief      Clears all returned data in buffer. I.e.
		 *             BaseSocket::returnDataSize will return 0;
		 */
		void clearReturnedData();
		/**
		 * Own register which can be read and written to from other party.
		 *
		 * It is up to the programmer what kind of data it actually contains.
		 * This array can be read and written into by the other party and vise
		 * versa.
		 */
		uint8_t* const flowRegister;
		/**
		 * Defines the size of the array where
		 * FlowSerial::BaseSocket::flowRegister points to.
		 */
		const size_t registerLength;
	protected:
		/**
		 * @brief      Update function. Input the received data here in
		 *             chronological order.
		 * @details    The data that is been thrown in here will be handled as
		 *             FlowSerial data. After calling this function the data
		 *             that was passed can be erased/overwritten.
		 *
		 * @todo When receiving data then this function has to be called by the
		 * derived class on order for this class to be useful.
		 *
		 * @param[in]  data       The data
		 * @param[in]  arraySize  The array size
		 *
		 * @return     True when a full FlowSerial message has been received.
		 */
		bool handleData(const uint8_t data[], size_t arraySize);
		/**
		 * @brief      This function will be called by member functions in order
		 *             to operate.
		 * @details    The data must be on chronological order. So 0 is first
		 *             out.
		 *
		 * @todo At least make sure data is send to the peer. Extra code that
		 * would grantee that the data makes it to the other side is not
		 * necessary but it would be nice.
		 *
		 * @param      data       Array this must be send to the interface
		 *                        handling FlowSerial. 0 is first out.
		 * @param      arraySize  size of date that is being held in the array
		 *                        that needs to be send.
		 */
		virtual void writeToInterface(const uint8_t data[], size_t arraySize) = 0;
	private:
		void returnData(const uint8_t data[], size_t arraySize);
		void returnData(uint8_t data);
		void sendFlowMessage(uint8_t startAddress, const uint8_t data[], size_t arraySize, Instruction instruction);
		CircularBuffer<uint8_t, 256> inputBuffer;
		// Temporary store argument data into this buffer until the checksum is
		// read and checked
		LinearBuffer<uint8_t, 256> argumentBuffer;
		uint16_t checksum;         // These two will be compared at the and of an package.
		uint16_t checksumReceived; // These two will be compared at the and of an package.
		// keeps track how many arguments were received
		
		State flowSerialState = State::idle;
		Instruction instruction;
	};
}
#endif //_FLOWSERIAL_HPP_
