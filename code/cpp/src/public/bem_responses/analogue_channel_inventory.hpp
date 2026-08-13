#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ANALOGUE_CHANNEL_INVENTORY
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ANALOGUE_CHANNEL_INVENTORY

/**************************************************************************/ /**
 \file       analogue_channel_inventory.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 12/08/2026
 \brief      Public Analogue Channel Inventory response data structures
 \details    Decoded payload of the Analogue Channel Inventory (0x63) BEM
			 command, surfaced through AnalogueChannelInventoryCallback. The
			 wire-format constants and the decode/accumulate/format helpers live
			 in the internal
			 protocols/bem/bem_commands/analogue_channel_inventory.hpp.

			 This is the command that makes a device's analogue inputs
			 discoverable. A device exposes only a channel count and a lookup by
			 id, and channel ids are sparse, so until this command an application
			 could not find out which inputs a device actually has - it had to
			 carry a hard-coded table per product, which drifted from the
			 firmware and only ever existed for one of them.

			 Each entry carries the channel id that the Analogue Channel Range
			 (0x60), I-Feed (0x61) and Sample Request (0x62) commands take, so an
			 application can move straight from discovery to reading a channel.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Channel id meaning "this channel has no paired channel".
		static constexpr uint8_t kAnalogueChannelIdNone = 0xFF;

		/// Hardware ADC id meaning "the device did not report one".
		static constexpr uint8_t kAnalogueAdcIdUnknown = 0xFF;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Physical quantity an analogue channel measures
		 \details    Every channel is a voltage at the converter, but some sense
					 another quantity through a known conversion - a gauge feed
					 channel reads the voltage developed across a sense resistor
					 and reports a current. The unit type says which, so a display
					 can label the reading correctly instead of calling everything
					 volts.
		 *******************************************************************************/
		enum class AnalogueUnitType : uint8_t
		{
			Volts = 0,		///< Potential difference
			Current = 1,	///< Current
			Resistance = 2, ///< Resistance
			Frequency = 3	///< Frequency
		};

		/**************************************************************************/ /**
		 \brief      State bits in an analogue channel entry
		 *******************************************************************************/
		enum class AnalogueChannelFlag : uint8_t
		{
			/// The channel is configured, so it produces engineering values and
			/// not only raw converter counts.
			Configured = 0x01,
			/// The device holds calibration data for this channel.
			CalibrationPresent = 0x02,
			/// That calibration data is valid and is being applied.
			CalibrationValid = 0x04,
			/// The channel is one half of a measured pair - see pairedChannelId.
			Paired = 0x08
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      One analogue input in the device's inventory
		 *******************************************************************************/
		struct AnalogueChannelEntry
		{
			/// Logical channel id, as taken by the Analogue Channel Range, I-Feed
			/// and Sample Request commands. Sparse - a device does not
			/// necessarily use a contiguous id space, which is exactly why the
			/// inventory is needed.
			uint8_t channelId = 0;
			/// Physical quantity this channel measures.
			AnalogueUnitType unitType = AnalogueUnitType::Volts;
			/// AnalogueChannelFlag bits.
			uint8_t flags = 0;
			/// Power of ten the range values are expressed in, typically -6 so
			/// that the ranges are micro-units.
			int8_t unitExponent = 0;
			/// Bottom of the channel's nominal range, in unitType units scaled by
			/// ten to the power of unitExponent. Negative for a bipolar input.
			int32_t rangeMin = 0;
			/// Top of the channel's nominal range, same units and scaling. This
			/// is the nominal range, before any calibration.
			int32_t rangeMax = 0;
			/// The channel acquired alongside this one, or kAnalogueChannelIdNone
			/// when unpaired. Only meaningful when isPaired().
			uint8_t pairedChannelId = kAnalogueChannelIdNone;
			/// Converter this channel is sampled on, or kAnalogueAdcIdUnknown.
			/// Diagnostic only - two channels sharing one are sampled together.
			uint8_t hardwareAdcId = kAnalogueAdcIdUnknown;
			/// The device's own name for the channel. This is an engineering
			/// name, not a display label: some devices name their inputs
			/// readably ("Battery Voltage"), others tersely by their internal
			/// wiring. Up to 16 characters.
			std::string name;

			/// True when the channel is configured and will produce engineering
			/// values rather than only raw converter counts.
			[[nodiscard]] bool isConfigured() const noexcept {
				return (flags & static_cast<uint8_t>(AnalogueChannelFlag::Configured)) != 0;
			}
			/// True when the device holds calibration data for this channel.
			[[nodiscard]] bool hasCalibration() const noexcept {
				return (flags & static_cast<uint8_t>(AnalogueChannelFlag::CalibrationPresent)) != 0;
			}
			/// True when that calibration is valid and being applied. A reading
			/// taken while this is false is an uncalibrated reading, however it
			/// is labelled.
			[[nodiscard]] bool isCalibrated() const noexcept {
				return (flags & static_cast<uint8_t>(AnalogueChannelFlag::CalibrationValid)) != 0;
			}
			/// True when this channel is one half of a measured pair, in which
			/// case pairedChannelId names the other half.
			[[nodiscard]] bool isPaired() const noexcept {
				return (flags & static_cast<uint8_t>(AnalogueChannelFlag::Paired)) != 0;
			}
		};

		/**************************************************************************/ /**
		 \brief      One Analogue Channel Inventory response message
		 \details    A device splits the inventory across as many messages as it
					 needs, each repeating the envelope. entries covers channels
					 [firstChannelIndex, firstChannelIndex + entries.size()).
		 \note       firstChannelIndex is a position in the inventory, not a
					 channel id - the ids themselves are carried per entry.
		 *******************************************************************************/
		struct AnalogueChannelInventoryResponse
		{
			uint8_t transferId = 0;		   ///< Ties the messages of one inventory together
			uint8_t totalChannels = 0;	   ///< Channels in the full inventory
			uint8_t firstChannelIndex = 0; ///< Position of the first entry in this message
			std::vector<AnalogueChannelEntry> entries; ///< This message's sub-list

			/// True when this single message already carries the whole inventory.
			[[nodiscard]] bool isComplete() const noexcept {
				return firstChannelIndex == 0 && entries.size() == totalChannels;
			}
		};

		/**************************************************************************/ /**
		 \brief      A complete inventory, reassembled from one or more messages
		 *******************************************************************************/
		struct AnalogueChannelInventoryResult
		{
			uint8_t transferId = 0;
			uint8_t totalChannels = 0;
			std::vector<AnalogueChannelEntry> entries;

			/// Find a channel by the id the other analogue commands use. Prefer
			/// this over indexing entries, which are in inventory order.
			[[nodiscard]] const AnalogueChannelEntry* findByChannelId(uint8_t id) const {
				for (const AnalogueChannelEntry& entry : entries) {
					if (entry.channelId == id) {
						return &entry;
					}
				}
				return nullptr;
			}

			/// Find a channel by the device's own name for it.
			[[nodiscard]] const AnalogueChannelEntry*
			findByName(std::string_view channelName) const {
				for (const AnalogueChannelEntry& entry : entries) {
					if (entry.name == channelName) {
						return &entry;
					}
				}
				return nullptr;
			}

			/// The partner of a paired channel, or nullptr when it has none.
			[[nodiscard]] const AnalogueChannelEntry*
			partnerOf(const AnalogueChannelEntry& entry) const {
				if (!entry.isPaired()) {
					return nullptr;
				}
				return findByChannelId(entry.pairedChannelId);
			}
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_ANALOGUE_CHANNEL_INVENTORY */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
