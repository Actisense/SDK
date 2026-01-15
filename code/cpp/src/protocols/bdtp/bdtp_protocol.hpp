#ifndef __ACTISENSE_SDK_BDTP_PROTOCOL_HPP
#define __ACTISENSE_SDK_BDTP_PROTOCOL_HPP

/**************************************************************************/ /**
 \file       bdtp_protocol.hpp
 \brief      Binary Data Transfer Protocol (BDTP) implementation
 \details    DLE/STX/ETX framing protocol used by Actisense devices.
			 Extracts BST (Binary Serial Transfer) datagrams from byte stream.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <vector>

#include "protocols/protocol.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      BDTP control characters
		 *******************************************************************************/
		struct BdtpChars
		{
			static constexpr uint8_t DLE = 0x10; ///< Data Link Escape
			static constexpr uint8_t STX = 0x02; ///< Start of Text
			static constexpr uint8_t ETX = 0x03; ///< End of Text
		};

		/**************************************************************************/ /**
		 \brief      BST (Binary Serial Transfer) datagram structure
		 \details    Payload extracted from BDTP frame
		 *******************************************************************************/
		struct BstDatagram
		{
			uint8_t bstId;			   ///< BST message type identifier
			uint16_t storeLength;	   ///< Length of data payload (16-bit for BST Type 2 frames)
			std::vector<uint8_t> data; ///< Payload data
		};

		/**************************************************************************/ /**
		 \brief      BDTP protocol parser
		 \details    Implements DLE-escaped framing:
					 - Frame start: DLE STX
					 - Frame end: DLE ETX
					 - Escape: DLE DLE represents literal DLE byte

					 Extracts BST datagrams from frames.
		 *******************************************************************************/
		class BdtpProtocol final : public IProtocol
		{
		public:
			/**************************************************************************/ /**
			 \brief      Default constructor
			 *******************************************************************************/
			BdtpProtocol();

			/**************************************************************************/ /**
			 \brief      Destructor
			 *******************************************************************************/
			~BdtpProtocol() override;

			/* IProtocol interface -------------------------------------------------- */

			[[nodiscard]] std::string_view id() const noexcept override;

			std::size_t parse(ConstByteSpan data, MessageEmitter emitMessage,
							  ErrorEmitter emitError) override;

			bool encode(std::string_view messageType, ConstByteSpan payload,
						std::vector<uint8_t>& outFrame, std::string& outError) override;

			void reset() override;

			/* BDTP-specific methods ------------------------------------------------ */

			/**************************************************************************/ /**
			 \brief      Encode raw data into BDTP frame
			 \param[in]  data      Raw data to encode
			 \param[out] outFrame  BDTP-encoded frame with DLE escaping
			 *******************************************************************************/
			static void encodeFrame(ConstByteSpan data, std::vector<uint8_t>& outFrame);

			/**************************************************************************/ /**
			 \brief      Encode BST datagram into BDTP frame
			 \param[in]  datagram  BST datagram to encode
			 \param[out] outFrame  Complete BDTP frame ready for transmission
			 *******************************************************************************/
			static void encodeBst(const BstDatagram& datagram, std::vector<uint8_t>& outFrame);

			/**************************************************************************/ /**
			 \brief      Calculate BST checksum
			 \param[in]  data  Data to checksum (BST ID + length + payload)
			 \return     8-bit checksum value
			 \details    Simple sum of all bytes, truncated to 8 bits
			 *******************************************************************************/
			[[nodiscard]] static uint8_t calculateChecksum(ConstByteSpan data) noexcept;

			/**************************************************************************/ /**
			 \brief      Get parser state name (for debugging)
			 \return     Current state as string
			 *******************************************************************************/
			[[nodiscard]] std::string_view stateName() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get number of frames successfully parsed
			 \return     Frame count since construction or last reset
			 *******************************************************************************/
			[[nodiscard]] std::size_t framesReceived() const noexcept;

			/**************************************************************************/ /**
			 \brief      Get number of frames with errors
			 \return     Error frame count since construction or last reset
			 *******************************************************************************/
			[[nodiscard]] std::size_t framesDropped() const noexcept;

		private:
			/**************************************************************************/ /**
			 \brief      Parser state machine states
			 *******************************************************************************/
			enum class State
			{
				Idle,		  ///< Waiting for DLE
				GotDLE,		  ///< Received DLE, waiting for STX/ETX/DLE
				InFrame,	  ///< Inside frame, collecting data
				InFrameGotDLE ///< Inside frame, received DLE escape
			};

			State state_;
			std::vector<uint8_t> frame_buffer_;
			std::size_t frames_received_;
			std::size_t frames_dropped_;

			static constexpr std::size_t kMaxFrameSize = 512;

			void processCompletedFrame(MessageEmitter emitMessage, ErrorEmitter emitError);

			bool parseBstFromFrame(ConstByteSpan frameData, BstDatagram& outDatagram,
								   std::string& outError) const;
		};

		/**************************************************************************/ /**
		 \brief      Create a BDTP protocol instance
		 \return     Unique pointer to new BDTP protocol
		 *******************************************************************************/
		[[nodiscard]] ProtocolPtr createBdtpProtocol();

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BDTP_PROTOCOL_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
