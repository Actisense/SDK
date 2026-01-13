/**************************************************************************/ /**
 \file       bdtp_protocol.cpp
 \brief      Implementation of Binary Data Transfer Protocol (BDTP)
 \details    DLE/STX/ETX framing and BST datagram extraction

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bdtp/bdtp_protocol.hpp"

#include <numeric>
#include <sstream>

#include "util/debug_log.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		static constexpr std::string_view kProtocolId = "bdtp";

		/* Public Function Definitions ------------------------------------------ */

		BdtpProtocol::BdtpProtocol()
			: state_(State::Idle), frame_buffer_(), frames_received_(0), frames_dropped_(0) {
			frame_buffer_.reserve(kMaxFrameSize);
		}

		BdtpProtocol::~BdtpProtocol() = default;

		std::string_view BdtpProtocol::id() const noexcept {
			return kProtocolId;
		}

		std::size_t BdtpProtocol::parse(ConstByteSpan data, MessageEmitter emitMessage,
										ErrorEmitter emitError) {
			std::size_t bytesConsumed = 0;

			ACTISENSE_LOG_HEX(LogLevel::Trace, "BDTP", "Parse input", data.data(), data.size());

			for (const uint8_t byte : data) {
				++bytesConsumed;

				switch (state_) {
					case State::Idle:
						if (byte == BdtpChars::DLE) {
							state_ = State::GotDLE;
							ACTISENSE_LOG_TRACE("BDTP", "State: Idle -> GotDLE");
						}
						/* Ignore any other bytes while idle */
						break;

					case State::GotDLE:
						if (byte == BdtpChars::STX) {
							/* Frame start detected */
							state_ = State::InFrame;
							frame_buffer_.clear();
							ACTISENSE_LOG_DEBUG("BDTP", "Frame start (DLE STX)");
						} else if (byte == BdtpChars::DLE) {
							/* Double DLE outside frame - stay in GotDLE */
							/* This shouldn't happen in valid streams but handle gracefully */
							ACTISENSE_LOG_WARN("BDTP", "Double DLE outside frame");
						} else {
							/* Invalid sequence - back to idle */
							state_ = State::Idle;
							{
								std::ostringstream ss;
								ss << "Invalid byte after DLE outside frame: 0x" 
								   << std::hex << static_cast<int>(byte);
								ACTISENSE_LOG_WARN("BDTP", ss.str());
							}
						}
						break;

					case State::InFrame:
						if (byte == BdtpChars::DLE) {
							state_ = State::InFrameGotDLE;
						} else {
							/* Regular data byte */
							if (frame_buffer_.size() < kMaxFrameSize) {
								frame_buffer_.push_back(byte);
							} else {
								/* Frame too large - abort and signal error */
								ACTISENSE_LOG_ERROR("BDTP", "Frame exceeds maximum size");
								if (emitError) {
									emitError(ErrorCode::MalformedFrame,
											  "BDTP frame exceeds maximum size");
								}
								++frames_dropped_;
								state_ = State::Idle;
								frame_buffer_.clear();
							}
						}
						break;

					case State::InFrameGotDLE:
						if (byte == BdtpChars::ETX) {
							/* Frame complete */
							{
								std::ostringstream ss;
								ss << "Frame complete, " << frame_buffer_.size() << " bytes";
								ACTISENSE_LOG_DEBUG("BDTP", ss.str());
							}
							processCompletedFrame(emitMessage, emitError);
							state_ = State::Idle;
						} else if (byte == BdtpChars::DLE) {
							/* Escaped DLE - literal 0x10 byte */
							if (frame_buffer_.size() < kMaxFrameSize) {
								frame_buffer_.push_back(BdtpChars::DLE);
							}
							state_ = State::InFrame;
						} else if (byte == BdtpChars::STX) {
							/* New frame start - abort current frame */
							{
								std::ostringstream ss;
								ss << "Frame aborted by new DLE STX! Buffer had " 
								   << frame_buffer_.size() << " bytes";
								ACTISENSE_LOG_ERROR("BDTP", ss.str());
								ACTISENSE_LOG_HEX(LogLevel::Debug, "BDTP", "Aborted frame data",
												  frame_buffer_.data(), frame_buffer_.size());
							}
							if (emitError) {
								emitError(ErrorCode::MalformedFrame,
										  "BDTP frame aborted - new frame started");
							}
							++frames_dropped_;
							frame_buffer_.clear();
							state_ = State::InFrame;
						} else {
							/* Invalid escape sequence */
							{
								std::ostringstream ss;
								ss << "Invalid escape sequence: DLE 0x" 
								   << std::hex << static_cast<int>(byte);
								ACTISENSE_LOG_ERROR("BDTP", ss.str());
							}
							if (emitError) {
								emitError(ErrorCode::MalformedFrame,
										  "Invalid BDTP escape sequence");
							}
							++frames_dropped_;
							state_ = State::Idle;
							frame_buffer_.clear();
						}
						break;
				}
			}

			return bytesConsumed;
		}

		bool BdtpProtocol::encode(std::string_view messageType, ConstByteSpan payload,
								  std::vector<uint8_t>& outFrame, std::string& outError) {
			(void)messageType; /* BDTP doesn't use message types for encoding */

			if (payload.empty()) {
				outError = "Cannot encode empty payload";
				return false;
			}

			if (payload.size() > 255) {
				outError = "Payload exceeds maximum BST length (255 bytes)";
				return false;
			}

			/* Build payload with zero-sum checksum */
			std::vector<uint8_t> payloadWithChecksum;
			payloadWithChecksum.reserve(payload.size() + 1);
			payloadWithChecksum.insert(payloadWithChecksum.end(), payload.begin(), payload.end());

			/* Calculate checksum such that sum of all bytes (including checksum) equals zero */
			const uint8_t sum = calculateChecksum(payloadWithChecksum);
			const uint8_t checksum = static_cast<uint8_t>(-sum);
			payloadWithChecksum.push_back(checksum);

			encodeFrame(payloadWithChecksum, outFrame);
			return true;
		}

		void BdtpProtocol::reset() {
			state_ = State::Idle;
			frame_buffer_.clear();
		}

		void BdtpProtocol::encodeFrame(ConstByteSpan data, std::vector<uint8_t>& outFrame) {
			outFrame.clear();
			outFrame.reserve(data.size() + 4 + (data.size() / 16)); /* Estimate with escaping */

			/* Frame start */
			outFrame.push_back(BdtpChars::DLE);
			outFrame.push_back(BdtpChars::STX);

			/* Data with DLE escaping */
			for (const uint8_t byte : data) {
				if (byte == BdtpChars::DLE) {
					outFrame.push_back(BdtpChars::DLE);
				}
				outFrame.push_back(byte);
			}

			/* Frame end */
			outFrame.push_back(BdtpChars::DLE);
			outFrame.push_back(BdtpChars::ETX);
		}

		void BdtpProtocol::encodeBst(const BstDatagram& datagram, std::vector<uint8_t>& outFrame) {
			/* Build BST payload: ID + Length + Data + Checksum */
			std::vector<uint8_t> bstPayload;
			bstPayload.reserve(datagram.data.size() + 3);

			bstPayload.push_back(datagram.bstId);
			bstPayload.push_back(static_cast<uint8_t>(datagram.data.size()));
			bstPayload.insert(bstPayload.end(), datagram.data.begin(), datagram.data.end());

			/* Calculate checksum such that sum of all bytes (including checksum) equals zero */
			const uint8_t sum = calculateChecksum(bstPayload);
			const uint8_t checksum = static_cast<uint8_t>(-sum);
			bstPayload.push_back(checksum);

			/* Encode with BDTP framing */
			encodeFrame(bstPayload, outFrame);
		}

		uint8_t BdtpProtocol::calculateChecksum(ConstByteSpan data) noexcept {
			/* Simple sum of all bytes, truncated to 8 bits */
			uint8_t sum = 0;
			for (const uint8_t byte : data) {
				sum = static_cast<uint8_t>(sum + byte);
			}
			return sum;
		}

		std::string_view BdtpProtocol::stateName() const noexcept {
			switch (state_) {
				case State::Idle:
					return "Idle";
				case State::GotDLE:
					return "GotDLE";
				case State::InFrame:
					return "InFrame";
				case State::InFrameGotDLE:
					return "InFrameGotDLE";
				default:
					return "Unknown";
			}
		}

		std::size_t BdtpProtocol::framesReceived() const noexcept {
			return frames_received_;
		}

		std::size_t BdtpProtocol::framesDropped() const noexcept {
			return frames_dropped_;
		}

		void BdtpProtocol::processCompletedFrame(MessageEmitter emitMessage,
												 ErrorEmitter emitError) {
			if (frame_buffer_.empty()) {
				/* Empty frame - ignore */
				return;
			}

			/* Try to parse as BST datagram */
			BstDatagram datagram;
			std::string parseError;

			if (parseBstFromFrame(frame_buffer_, datagram, parseError)) {
				++frames_received_;

				if (emitMessage) {
					ParsedMessageEvent event;
					event.protocol = std::string(kProtocolId);
					event.messageType = "BST_" + std::to_string(static_cast<int>(datagram.bstId));
					event.payload = std::move(datagram);

					emitMessage(event);
				}
			} else {
				++frames_dropped_;

				if (emitError) {
					emitError(ErrorCode::MalformedFrame, parseError);
				}
			}

			frame_buffer_.clear();
		}

		bool BdtpProtocol::parseBstFromFrame(ConstByteSpan frameData, BstDatagram& outDatagram,
											 std::string& outError) const {
			/* Minimum BST frame: ID (1) + Length (1) + Checksum (1) = 3 bytes */
			if (frameData.size() < 3) {
				outError = "BST frame too short (minimum 3 bytes)";
				return false;
			}

			const uint8_t bstId = frameData[0];
			const uint8_t storeLength = frameData[1];

			/* Expected total: ID + Length + Data + Checksum */
			const std::size_t expectedSize = static_cast<std::size_t>(2) + storeLength + 1;

			if (frameData.size() < expectedSize) {
				outError = "BST frame incomplete - expected " + std::to_string(expectedSize) +
						   " bytes, got " + std::to_string(frameData.size());
				return false;
			}

			/* Verify checksum (sum of all bytes including checksum should be zero) */
			const auto checksumData = frameData.subspan(0, 2 + storeLength + 1);
			const uint8_t checksumResult = calculateChecksum(checksumData);

			if (checksumResult != 0) {
				outError = "BST checksum mismatch - sum is 0x" + std::to_string(checksumResult) +
						   " (expected 0)";
				return false;
			}

			/* Extract datagram */
			outDatagram.bstId = bstId;
			outDatagram.storeLength = storeLength;
			outDatagram.data.assign(frameData.begin() + 2, frameData.begin() + 2 + storeLength);

			return true;
		}

		ProtocolPtr createBdtpProtocol() {
			return std::make_unique<BdtpProtocol>();
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
