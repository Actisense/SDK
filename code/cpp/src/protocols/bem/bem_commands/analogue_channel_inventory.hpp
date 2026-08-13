#ifndef __ACTISENSE_SDK_BEM_ANALOGUE_CHANNEL_INVENTORY_HPP
#define __ACTISENSE_SDK_BEM_ANALOGUE_CHANNEL_INVENTORY_HPP

/**************************************************************************/ /**
 \file       analogue_channel_inventory.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 12/08/2026
 \brief      Analogue Channel Inventory BEM command types and helpers
 \details    Wire format and decode helpers for the Analogue Channel Inventory
			 (0x63) BEM command. The request is a GET with no payload; the
			 response is one or more messages, each carrying a repeated envelope
			 followed by a sub-list of fixed-size channel records:

			   Envelope (8 bytes)
				 +0      u8    Transfer ID (cycles 1..255)
				 +1-4    u32   Structure Variant ID
				 +5      u8    Total channels in the full inventory
				 +6      u8    First channel index in this sub-list
				 +7      u8    Number of records in this sub-list

			   Record (30 bytes)
				 +0      u8    Channel id (as taken by 0x60 / 0x61 / 0x62)
				 +1      u8    Unit type        (AnalogueUnitType)
				 +2      u8    Flags            (AnalogueChannelFlag bits)
				 +3      s8    Unit exponent, e.g. -6 for micro-units
				 +4-7    s32   Range minimum, in unit type scaled by the exponent
				 +8-11   s32   Range maximum, same units
				 +12     u8    Paired channel id (0xFF = unpaired)
				 +13     u8    Hardware converter id (0xFF = not reported)
				 +14-29  char  Channel name, ASCII, null padded to 16

			 All multi-byte fields are little-endian.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "public/bem_responses/analogue_channel_inventory.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Structure Variant ID carried in every Analogue Channel Inventory message.
		static constexpr uint32_t kAnalogueChannelInventoryStructureVariant = 0x00001105;

		/// Bytes of envelope repeated at the head of every response message.
		static constexpr std::size_t kAnalogueChannelInventoryEnvelopeSize = 8;

		/// Bytes per channel record.
		static constexpr std::size_t kAnalogueChannelInventoryRecordSize = 30;

		/// Bytes of the fixed-width channel name field.
		static constexpr std::size_t kAnalogueChannelInventoryNameSize = 16;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Outcome of feeding one message to
					 AnalogueChannelInventoryAccumulator
		 *******************************************************************************/
		enum class AnalogueChannelInventoryStatus
		{
			Continue, ///< Accepted; more messages are still outstanding
			Done,	  ///< Accepted; the inventory is complete
			Mismatch  ///< Rejected; the message does not belong to this transfer
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Read a little-endian signed 32-bit field
		 *******************************************************************************/
		[[nodiscard]] inline int32_t readAnalogueChannelS32(const uint8_t* record) {
			const uint32_t raw =
				static_cast<uint32_t>(record[0]) | (static_cast<uint32_t>(record[1]) << 8) |
				(static_cast<uint32_t>(record[2]) << 16) | (static_cast<uint32_t>(record[3]) << 24);
			return static_cast<int32_t>(raw);
		}

		/**************************************************************************/ /**
		 \brief      Decode one Analogue Channel Inventory response message
		 \details    Validates the envelope against the bytes actually present,
					 so a truncated message is reported rather than silently
					 yielding short entries.
		 \param[in]  data       BEM response data (after the 12-byte header)
		 \param[out] response   Decoded message
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool
		decodeAnalogueChannelInventoryResponse(std::span<const uint8_t> data,
											   AnalogueChannelInventoryResponse& response,
											   std::string& outError) {
			if (data.size() < kAnalogueChannelInventoryEnvelopeSize) {
				outError = "Analogue Channel Inventory response too short: expected at least " +
						   std::to_string(kAnalogueChannelInventoryEnvelopeSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			const uint32_t structureVariant =
				static_cast<uint32_t>(data[1]) | (static_cast<uint32_t>(data[2]) << 8) |
				(static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 24);
			if (structureVariant != kAnalogueChannelInventoryStructureVariant) {
				outError = "Analogue Channel Inventory structure variant mismatch: expected 0x" +
						   std::to_string(kAnalogueChannelInventoryStructureVariant) + ", got 0x" +
						   std::to_string(structureVariant);
				return false;
			}

			response.transferId = data[0];
			response.totalChannels = data[5];
			response.firstChannelIndex = data[6];
			const uint8_t subCount = data[7];

			const std::size_t needed =
				kAnalogueChannelInventoryEnvelopeSize +
				(static_cast<std::size_t>(subCount) * kAnalogueChannelInventoryRecordSize);
			if (data.size() < needed) {
				outError =
					"Analogue Channel Inventory sub-list truncated: " + std::to_string(subCount) +
					" records need " + std::to_string(needed) + " bytes, got " +
					std::to_string(data.size());
				return false;
			}

			response.entries.clear();
			response.entries.reserve(subCount);
			for (std::size_t i = 0; i < subCount; ++i) {
				const uint8_t* record = data.data() + kAnalogueChannelInventoryEnvelopeSize +
										(i * kAnalogueChannelInventoryRecordSize);
				AnalogueChannelEntry entry;
				entry.channelId = record[0];
				entry.unitType = static_cast<AnalogueUnitType>(record[1]);
				entry.flags = record[2];
				entry.unitExponent = static_cast<int8_t>(record[3]);
				entry.rangeMin = readAnalogueChannelS32(record + 4);
				entry.rangeMax = readAnalogueChannelS32(record + 8);
				entry.pairedChannelId = record[12];
				entry.hardwareAdcId = record[13];
				/* the name is null padded, and carries no terminator when it
				   fills the field - bound the read to the field width */
				std::size_t nameLength = 0;
				while (nameLength < kAnalogueChannelInventoryNameSize &&
					   record[14 + nameLength] != 0) {
					++nameLength;
				}
				entry.name.assign(reinterpret_cast<const char*>(record + 14), nameLength);
				response.entries.push_back(std::move(entry));
			}
			return true;
		}

		/**************************************************************************/ /**
		 \brief      Accumulator merging Analogue Channel Inventory messages into one
					 result
		 \details    A device answers a single GET with as many messages as the
					 inventory needs, each repeating the envelope. Feed each one
					 here in arrival order; Done is reported once every channel
					 has been seen.

					 Unlike the port inventory, more than one message is the
					 normal case here: a device with many analogue inputs will
					 not fit them all in one response.
		 *******************************************************************************/
		class AnalogueChannelInventoryAccumulator
		{
		public:
			[[nodiscard]] AnalogueChannelInventoryStatus
			feed(const AnalogueChannelInventoryResponse& msg, std::string& outError) {
				if (!initialised_) {
					result_.transferId = msg.transferId;
					result_.totalChannels = msg.totalChannels;
					result_.entries.assign(msg.totalChannels, AnalogueChannelEntry{});
					seen_.assign(msg.totalChannels, false);
					initialised_ = true;
				} else if (msg.transferId != result_.transferId) {
					outError =
						"Analogue Channel Inventory transferId changed mid-transfer: expected " +
						std::to_string(result_.transferId) + ", got " +
						std::to_string(msg.transferId);
					return AnalogueChannelInventoryStatus::Mismatch;
				} else if (msg.totalChannels != result_.totalChannels) {
					outError =
						"Analogue Channel Inventory totalChannels changed mid-transfer: expected " +
						std::to_string(result_.totalChannels) + ", got " +
						std::to_string(msg.totalChannels);
					return AnalogueChannelInventoryStatus::Mismatch;
				}

				const std::size_t end =
					static_cast<std::size_t>(msg.firstChannelIndex) + msg.entries.size();
				if (end > result_.entries.size()) {
					outError =
						"Analogue Channel Inventory sub-list overruns total: firstChannelIndex=" +
						std::to_string(msg.firstChannelIndex) +
						" count=" + std::to_string(msg.entries.size()) +
						" total=" + std::to_string(result_.totalChannels);
					return AnalogueChannelInventoryStatus::Mismatch;
				}

				for (std::size_t i = 0; i < msg.entries.size(); ++i) {
					const std::size_t slot = msg.firstChannelIndex + i;
					result_.entries[slot] = msg.entries[i];
					if (!seen_[slot]) {
						seen_[slot] = true;
						++received_;
					}
				}

				return (received_ >= result_.entries.size())
						   ? AnalogueChannelInventoryStatus::Done
						   : AnalogueChannelInventoryStatus::Continue;
			}

			[[nodiscard]] const AnalogueChannelInventoryResult& result() const noexcept {
				return result_;
			}

			[[nodiscard]] bool initialised() const noexcept { return initialised_; }

			[[nodiscard]] bool isComplete() const noexcept {
				return initialised_ && received_ >= result_.entries.size();
			}

		private:
			AnalogueChannelInventoryResult result_;
			std::vector<bool> seen_;
			bool initialised_ = false;
			std::size_t received_ = 0;
		};

		/**************************************************************************/ /**
		 \brief      Convert AnalogueUnitType to a display string
		 *******************************************************************************/
		[[nodiscard]] inline const char* analogueUnitTypeToString(AnalogueUnitType unit) {
			switch (unit) {
				case AnalogueUnitType::Volts:
					return "V";
				case AnalogueUnitType::Current:
					return "A";
				case AnalogueUnitType::Resistance:
					return "Ohm";
				case AnalogueUnitType::Frequency:
					return "Hz";
				default:
					return "?";
			}
		}

		/**************************************************************************/ /**
		 \brief      Render a scaled range value as a decimal string
		 \details    The device reports its range as an integer plus a power of
					 ten, which keeps the wire format exact. Applying the exponent
					 for display is left here rather than in the entry so that the
					 raw values stay available.
		 \param[in]  value     Scaled integer value
		 \param[in]  exponent  Power of ten the value is expressed in
		 \return     Decimal string, e.g. 35747300 with exponent -6 -> "35.747300"
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatAnalogueScaledValue(int32_t value, int8_t exponent) {
			if (exponent >= 0) {
				std::string out = std::to_string(value);
				out.append(static_cast<std::size_t>(exponent), '0');
				return out;
			}
			const bool negative = value < 0;
			/* negate through an unsigned type so INT32_MIN does not overflow */
			uint32_t magnitude =
				negative ? (~static_cast<uint32_t>(value) + 1U) : static_cast<uint32_t>(value);
			const std::size_t decimals = static_cast<std::size_t>(-static_cast<int>(exponent));
			std::string digits = std::to_string(magnitude);
			if (digits.size() <= decimals) {
				digits.insert(0, decimals + 1 - digits.size(), '0');
			}
			digits.insert(digits.size() - decimals, 1, '.');
			return negative ? ("-" + digits) : digits;
		}

		/**************************************************************************/ /**
		 \brief      Format one inventory entry for display
		 *******************************************************************************/
		[[nodiscard]] inline std::string
		formatAnalogueChannelEntry(const AnalogueChannelEntry& entry) {
			std::string out;
			out.reserve(120);
			out += "[" + std::to_string(entry.channelId) + "] " +
				   (entry.name.empty() ? "(unnamed)" : entry.name);
			out += " ";
			out += formatAnalogueScaledValue(entry.rangeMin, entry.unitExponent);
			out += "..";
			out += formatAnalogueScaledValue(entry.rangeMax, entry.unitExponent);
			out += analogueUnitTypeToString(entry.unitType);
			out += entry.isConfigured() ? " configured" : " unconfigured";
			if (entry.isCalibrated()) {
				out += " calibrated";
			} else if (entry.hasCalibration()) {
				out += " cal-invalid";
			} else {
				out += " uncalibrated";
			}
			if (entry.isPaired()) {
				out += " paired=" + std::to_string(entry.pairedChannelId);
			}
			return out;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_ANALOGUE_CHANNEL_INVENTORY_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
