#ifndef __ACTISENSE_SDK_BEM_TYPES_HPP
#define __ACTISENSE_SDK_BEM_TYPES_HPP

/**************************************************************************/ /**
 \file       bem_types.hpp
 \brief      BEM (Binary Encoded Message) command/response types
 \details    Type definitions for BST-BEM command/response protocol

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bst/bst_types.hpp"
#include "public/error.hpp"
#include "public/operating_mode.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      BEM Gateway→PC response header structure
		 \details    Header present in all BEM responses (14 bytes before data)
		 *******************************************************************************/
		struct BemResponseHeader
		{
			BstId bstId;		   ///< BST message ID (A0, A2, A3, A5)
			uint8_t storeLength;   ///< BST store length
			uint8_t bemId;		   ///< BEM command ID this responds to
			uint8_t sequenceId;	   ///< Sequence ID for correlation
			uint16_t modelId;	   ///< ARL Model ID (little-endian)
			uint32_t serialNumber; ///< Device serial number (little-endian)
			uint32_t errorCode;	   ///< ARL Error Code (0 = success)
		};

		/**************************************************************************/ /**
		 \brief      BEM PC→Gateway command header structure
		 \details    Header for all BEM commands (3 bytes before data)
		 *******************************************************************************/
		struct BemCommandHeader
		{
			BstId bstId;		 ///< BST message ID (A1, A4, A6, A8)
			uint8_t storeLength; ///< BST store length
			uint8_t bemId;		 ///< BEM command ID
		};

		/**************************************************************************/ /**
		 \brief      Decoded BEM response
		 *******************************************************************************/
		struct BemResponse
		{
			BemResponseHeader header;	///< Response header fields
			std::vector<uint8_t> data;	///< Response payload data
			bool checksumValid = false; ///< Checksum validation result
		};

		/**************************************************************************/ /**
		 \brief      BEM command to be sent
		 *******************************************************************************/
		struct BemCommand
		{
			BstId bstId = BstId::Bem_PG_A1; ///< BST command ID
			BemCommandId bemId;				///< BEM command ID
			std::vector<uint8_t> data;		///< Command payload data
		};

		/**************************************************************************/ /**
		 \brief      BEM request tracking information
		 *******************************************************************************/
		struct BemPendingRequest
		{
			uint8_t sequenceId;							  ///< Sequence ID assigned
			BemCommandId commandId;						  ///< Command that was sent
			std::chrono::steady_clock::time_point sentAt; ///< When request was sent
			std::chrono::milliseconds timeout;			  ///< Timeout duration
		};

		/**************************************************************************/ /**
		 \brief      BEM response callback signature
		 \param[in]  response   Decoded response (or empty on timeout/error)
		 \param[in]  errorCode  SDK error code (Ok on success)
		 \param[in]  errorMsg   Error message (empty on success)
		 *******************************************************************************/
		using BemResponseCallback =
			std::function<void(const std::optional<BemResponse>& response, ErrorCode errorCode,
							   std::string_view errorMsg)>;

		/**************************************************************************/ /**
		 \brief      Known ARL Model IDs (subset)
		 \details    From ARLModelCodes.h - commonly used devices
		 *******************************************************************************/
		enum class ArlModelId : uint16_t
		{
			Unknown = 0x0000,
			NGT1 = 0x000E,	   ///< NGT-1 NMEA 2000 Gateway
			NGT1_USB = 0x000F, ///< NGT-1 USB variant
			NGW1 = 0x0010,	   ///< NGW-1 WiFi Gateway
			EMU1 = 0x0011,	   ///< EMU-1 Engine Monitor
			PRO_NDC1 = 0x0020, ///< PRO-NDC-1-E2K
			WGX1 = 0x0030,	   ///< WGX Wireless Gateway
			NGX1 = 0x003B	   ///< NGX-1 NMEA 2000 X-over Gateway
		};

		/**************************************************************************/ /**
		 \brief      Get model name from ARL Model ID
		 *******************************************************************************/
		[[nodiscard]] inline std::string modelIdToString(uint16_t modelId) {
			switch (static_cast<ArlModelId>(modelId)) {
				case ArlModelId::NGT1:
					return "NGT-1";
				case ArlModelId::NGT1_USB:
					return "NGT-1 USB";
				case ArlModelId::NGW1:
					return "NGW-1";
				case ArlModelId::EMU1:
					return "EMU-1";
				case ArlModelId::PRO_NDC1:
					return "PRO-NDC-1-E2K";
				case ArlModelId::WGX1:
					return "WGX";
				case ArlModelId::NGX1:
					return "NGX-1";
				default:
					return "Model-0x" + std::to_string(modelId);
			}
		}

		/**************************************************************************/ /**
		 \brief      True when a model is known to emit SV_DIG_PropEnableList0
					 after the standard list in Rx/Tx F2 PGN-Enable-List responses.
		 \details    Per-sub-PGN proprietary enable bitmaps (PDU2 0xFF00 and
					 0x1FF00 ranges) were added with NGX-1 firmware
					 (NGXSW-3329). Older models (NGT-1, NGW-1, EMU-1, PRO-NDC-1,
					 WGX) never emit the proprietary structure-variant message
					 and the SDK accumulator must complete based on the standard
					 list alone. Future models that ship the feature need an
					 explicit case here.
		 *******************************************************************************/
		[[nodiscard]] inline bool supportsProprietaryEnableListF2(uint16_t modelId) {
			switch (static_cast<ArlModelId>(modelId)) {
				case ArlModelId::NGX1:
					return true;
				default:
					return false;
			}
		}

		/**************************************************************************/ /**
		 \brief      True when a model rewrites byte 0 (the N2K Sequence ID /
					 SID) of a host-injected single-frame / fast-packet PGN on
					 the host-Tx path, so byte 0 cannot be matched against what
					 sendPgn supplied.
		 \details    The NGT-1 N2K stack overwrites the SID field of host-Tx
					 BST 94 / D0 frames with its own running sequence counter
					 (empirically: sent F4 on PGN 126992, observed 02 on the
					 wire). NGT-1 went EOL ~2 years ago so this is not fixable
					 in firmware. NGX-1 / WGX firmware preserves the host-
					 supplied SID after NGXSW-3897, so byte 0 must match there.
					 Integration tests gate their byte-0 comparison on this so
					 NGT-1 rigs stay green while NGX/WGX get full-payload
					 verification (GIT-109). Future legacy models needing the
					 same exemption add an explicit case here.
		 *******************************************************************************/
		[[nodiscard]] inline bool rewritesHostTxSidByte0(uint16_t modelId) {
			switch (static_cast<ArlModelId>(modelId)) {
				case ArlModelId::NGT1:
				case ArlModelId::NGT1_USB:
					return true;
				default:
					return false;
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_TYPES_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
