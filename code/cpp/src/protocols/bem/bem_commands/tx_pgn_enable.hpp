#ifndef __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_HPP
#define __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_HPP

/**************************************************************************/ /**
 \file       tx_pgn_enable.hpp
 \author     (Created) Claude Code
 \date       (Created) 27/01/2026
 \brief      Tx PGN Enable BEM command types and helpers
 \details    Structures and functions for encoding/decoding Tx PGN Enable
			 (0x47) BEM commands. Controls which PGNs are transmitted
			 on NMEA 2000 or J1939 interfaces, with configurable rate and priority.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Constants ------------------------------------------------------------ */

		/// Tx PGN Enable response data size (14 bytes)
		static constexpr std::size_t kTxPgnEnableResponseSize = 14;

		/// Tx PGN Enable GET request data size (4 bytes: PGN only)
		static constexpr std::size_t kTxPgnEnableGetRequestSize = 4;

		/// Tx PGN Enable basic SET request data size (5 bytes: PGN + enable)
		static constexpr std::size_t kTxPgnEnableBasicSetRequestSize = 5;

		/// Tx PGN Enable extended SET request data size (9 bytes: PGN + enable + rate)
		static constexpr std::size_t kTxPgnEnableExtendedSetRequestSize = 9;

		/// Special Tx rate value: Use device default rate
		static constexpr uint32_t kTxRateDefault = 0xFFFFFFFF;

		/// Special Tx rate value: Event-driven only (no periodic transmission)
		static constexpr uint32_t kTxRateEventDriven = 0;

		/* Enumerations --------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable flag values
		 \details    Controls PGN transmission behavior
		 *******************************************************************************/
		enum class TxPgnEnableFlag : uint8_t
		{
			Disabled = 0x00,   ///< PGN transmission disabled
			Enabled = 0x01,	   ///< PGN transmission enabled at configured rate
			RespondMode = 0x02 ///< Transmit only when requested (ISO Request)
		};

		/* Data Structures ------------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable request structure
		 \details    Used for building Get/Set Tx PGN Enable commands
		 *******************************************************************************/
		struct TxPgnEnableRequest
		{
			uint32_t pgn = 0;					   ///< 24-bit PGN ID (stored in 32-bit)
			std::optional<TxPgnEnableFlag> enable; ///< Enable flag (omit for GET)
			std::optional<uint32_t> txRate;		   ///< TX rate in ms (optional for SET)
		};

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable response structure
		 \details    Decoded response from Tx PGN Enable command
		 *******************************************************************************/
		struct TxPgnEnableResponse
		{
			uint32_t pgn = 0;									///< PGN ID
			TxPgnEnableFlag enable = TxPgnEnableFlag::Disabled; ///< Current enable state
			uint32_t txRate = 0;								///< TX rate in milliseconds
			uint32_t txTimeout = 0; ///< TX timeout (deprecated, usually 0)
			uint8_t txPriority = 3; ///< CAN priority (0-7, default 3)
		};

		/* Helper Functions ----------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Decode Tx PGN Enable response from BEM data payload
		 \param[in]  data       BEM response data (after 12-byte header)
		 \param[out] response   Decoded response structure
		 \param[out] outError   Error message if decoding fails
		 \return     True on success, false on error
		 *******************************************************************************/
		[[nodiscard]] inline bool decodeTxPgnEnableResponse(std::span<const uint8_t> data,
															TxPgnEnableResponse& response,
															std::string& outError) {
			if (data.size() < kTxPgnEnableResponseSize) {
				outError = "Tx PGN Enable response too short: expected " +
						   std::to_string(kTxPgnEnableResponseSize) + " bytes, got " +
						   std::to_string(data.size());
				return false;
			}

			/* PGN ID: bytes 0-3, little-endian */
			response.pgn = static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
						   (static_cast<uint32_t>(data[2]) << 16) |
						   (static_cast<uint32_t>(data[3]) << 24);

			/* Enable flag: byte 4 */
			response.enable = static_cast<TxPgnEnableFlag>(data[4]);

			/* TX Rate: bytes 5-8, little-endian */
			response.txRate =
				static_cast<uint32_t>(data[5]) | (static_cast<uint32_t>(data[6]) << 8) |
				(static_cast<uint32_t>(data[7]) << 16) | (static_cast<uint32_t>(data[8]) << 24);

			/* TX Timeout (deprecated): bytes 9-12, little-endian */
			response.txTimeout =
				static_cast<uint32_t>(data[9]) | (static_cast<uint32_t>(data[10]) << 8) |
				(static_cast<uint32_t>(data[11]) << 16) | (static_cast<uint32_t>(data[12]) << 24);

			/* TX Priority: byte 13 */
			response.txPriority = data[13];

			return true;
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable GET request data
		 \param[in]  pgn        PGN ID to query
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeTxPgnEnableGetRequest(uint32_t pgn, std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kTxPgnEnableGetRequestSize);

			/* PGN ID: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable basic SET request data
		 \param[in]  pgn        PGN ID to configure
		 \param[in]  enable     Enable flag
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeTxPgnEnableSetRequest(uint32_t pgn, TxPgnEnableFlag enable,
												std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kTxPgnEnableBasicSetRequestSize);

			/* PGN ID: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));

			/* Enable flag: 1 byte */
			outData.push_back(static_cast<uint8_t>(enable));
		}

		/**************************************************************************/ /**
		 \brief      Encode Tx PGN Enable extended SET request data (with rate)
		 \param[in]  pgn        PGN ID to configure
		 \param[in]  enable     Enable flag
		 \param[in]  txRate     Transmission rate in milliseconds
		 \param[out] outData    Encoded request data
		 *******************************************************************************/
		inline void encodeTxPgnEnableSetRequestWithRate(uint32_t pgn, TxPgnEnableFlag enable,
														uint32_t txRate,
														std::vector<uint8_t>& outData) {
			outData.clear();
			outData.reserve(kTxPgnEnableExtendedSetRequestSize);

			/* PGN ID: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(pgn & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((pgn >> 24) & 0xFF));

			/* Enable flag: 1 byte */
			outData.push_back(static_cast<uint8_t>(enable));

			/* TX Rate: 4 bytes, little-endian */
			outData.push_back(static_cast<uint8_t>(txRate & 0xFF));
			outData.push_back(static_cast<uint8_t>((txRate >> 8) & 0xFF));
			outData.push_back(static_cast<uint8_t>((txRate >> 16) & 0xFF));
			outData.push_back(static_cast<uint8_t>((txRate >> 24) & 0xFF));
		}

		/**************************************************************************/ /**
		 \brief      Convert TxPgnEnableFlag enum to string
		 \param[in]  flag       Enable flag value
		 \return     Human-readable flag name
		 *******************************************************************************/
		[[nodiscard]] inline const char* txPgnEnableFlagToString(TxPgnEnableFlag flag) {
			switch (flag) {
				case TxPgnEnableFlag::Disabled:
					return "Disabled";
				case TxPgnEnableFlag::Enabled:
					return "Enabled";
				case TxPgnEnableFlag::RespondMode:
					return "Respond Mode";
				default:
					return "Unknown";
			}
		}

		/**************************************************************************/ /**
		 \brief      Format TX rate for display
		 \param[in]  txRate     TX rate value in milliseconds
		 \return     Human-readable rate string
		 *******************************************************************************/
		[[nodiscard]] inline std::string formatTxRate(uint32_t txRate) {
			if (txRate == kTxRateDefault) {
				return "Default";
			}
			if (txRate == kTxRateEventDriven) {
				return "Event-driven";
			}
			return std::to_string(txRate) + " ms";
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_TX_PGN_ENABLE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/