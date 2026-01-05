#ifndef __ACTISENSE_SDK_BEM_TYPES_HPP
#define __ACTISENSE_SDK_BEM_TYPES_HPP

/**************************************************************************//**
\file       bem_types.hpp
\brief      BEM (Binary Encoded Message) command/response types
\details    Type definitions for BST-BEM command/response protocol

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "../bst/bst_types.hpp"
#include "public/error.hpp"

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <functional>

namespace Actisense
{
namespace Sdk
{
	/* Definitions ---------------------------------------------------------- */

	/**************************************************************************//**
	\brief      BEM Command IDs
	\details    Extended command codes sent via BST-BEM protocol
	*******************************************************************************/
	enum class BemCommandId : uint8_t
	{
		/* Device Information Commands */
		GetOperatingMode      = 0x11,   ///< Request current operating mode
		SetOperatingMode      = 0x12,   ///< Set device operating mode
		GetDeviceInfo         = 0x13,   ///< Request device information
		GetFirmwareVersion    = 0x14,   ///< Request firmware version
		GetHardwareVersion    = 0x15,   ///< Request hardware version
		GetSerialNumber       = 0x16,   ///< Request serial number

		/* Configuration Commands */
		GetConfiguration      = 0x20,   ///< Request configuration data
		SetConfiguration      = 0x21,   ///< Set configuration data
		SaveConfiguration     = 0x22,   ///< Save configuration to NVRAM

		/* Network Commands */
		GetCanStatus          = 0x30,   ///< Request CAN bus status
		ResetCanBus           = 0x31,   ///< Reset CAN bus interface

		/* Diagnostic Commands */
		GetDiagnostics        = 0x40,   ///< Request diagnostic data
		ClearDiagnostics      = 0x41,   ///< Clear diagnostic counters

		/* Bootloader Commands */
		EnterBootloader       = 0xF0,   ///< Enter bootloader mode
		GetBootloaderVersion  = 0xF1    ///< Request bootloader version
	};

	/**************************************************************************//**
	\brief      BEM Gateway→PC response header structure
	\details    Header present in all BEM responses (14 bytes before data)
	*******************************************************************************/
	struct BemResponseHeader
	{
		BstId     bstId;          ///< BST message ID (A0, A2, A3, A5)
		uint8_t   storeLength;    ///< BST store length
		uint8_t   bemId;          ///< BEM command ID this responds to
		uint8_t   sequenceId;     ///< Sequence ID for correlation
		uint16_t  modelId;        ///< ARL Model ID (little-endian)
		uint32_t  serialNumber;   ///< Device serial number (little-endian)
		uint32_t  errorCode;      ///< ARL Error Code (0 = success)
	};

	/**************************************************************************//**
	\brief      BEM PC→Gateway command header structure
	\details    Header for all BEM commands (3 bytes before data)
	*******************************************************************************/
	struct BemCommandHeader
	{
		BstId     bstId;          ///< BST message ID (A1, A4, A6, A8)
		uint8_t   storeLength;    ///< BST store length
		uint8_t   bemId;          ///< BEM command ID
	};

	/**************************************************************************//**
	\brief      Decoded BEM response
	*******************************************************************************/
	struct BemResponse
	{
		BemResponseHeader     header;     ///< Response header fields
		std::vector<uint8_t>  data;       ///< Response payload data
		bool                  checksumValid = false; ///< Checksum validation result
	};

	/**************************************************************************//**
	\brief      BEM command to be sent
	*******************************************************************************/
	struct BemCommand
	{
		BstId                 bstId = BstId::Bem_PG_A1; ///< BST command ID
		BemCommandId          bemId;      ///< BEM command ID
		std::vector<uint8_t>  data;       ///< Command payload data
	};

	/**************************************************************************//**
	\brief      BEM request tracking information
	*******************************************************************************/
	struct BemPendingRequest
	{
		uint8_t                    sequenceId;    ///< Sequence ID assigned
		BemCommandId               commandId;     ///< Command that was sent
		std::chrono::steady_clock::time_point sentAt; ///< When request was sent
		std::chrono::milliseconds  timeout;       ///< Timeout duration
	};

	/**************************************************************************//**
	\brief      BEM response callback signature
	\param[in]  response   Decoded response (or empty on timeout/error)
	\param[in]  errorCode  SDK error code (Ok on success)
	\param[in]  errorMsg   Error message (empty on success)
	*******************************************************************************/
	using BemResponseCallback = std::function<void(
		const std::optional<BemResponse>& response,
		ErrorCode errorCode,
		std::string_view errorMsg)>;

	/**************************************************************************//**
	\brief      Get human-readable name for BEM command ID
	\param[in]  id  BEM command ID
	\return     String description
	*******************************************************************************/
	[[nodiscard]] inline std::string bemCommandIdToString(BemCommandId id)
	{
		switch (id)
		{
		case BemCommandId::GetOperatingMode:     return "GetOperatingMode";
		case BemCommandId::SetOperatingMode:     return "SetOperatingMode";
		case BemCommandId::GetDeviceInfo:        return "GetDeviceInfo";
		case BemCommandId::GetFirmwareVersion:   return "GetFirmwareVersion";
		case BemCommandId::GetHardwareVersion:   return "GetHardwareVersion";
		case BemCommandId::GetSerialNumber:      return "GetSerialNumber";
		case BemCommandId::GetConfiguration:     return "GetConfiguration";
		case BemCommandId::SetConfiguration:     return "SetConfiguration";
		case BemCommandId::SaveConfiguration:    return "SaveConfiguration";
		case BemCommandId::GetCanStatus:         return "GetCanStatus";
		case BemCommandId::ResetCanBus:          return "ResetCanBus";
		case BemCommandId::GetDiagnostics:       return "GetDiagnostics";
		case BemCommandId::ClearDiagnostics:     return "ClearDiagnostics";
		case BemCommandId::EnterBootloader:      return "EnterBootloader";
		case BemCommandId::GetBootloaderVersion: return "GetBootloaderVersion";
		default:
			return "BEM-0x" + std::to_string(static_cast<unsigned>(id));
		}
	}

	/**************************************************************************//**
	\brief      Known ARL Model IDs (subset)
	\details    From ARLModelCodes.h - commonly used devices
	*******************************************************************************/
	enum class ArlModelId : uint16_t
	{
		Unknown   = 0x0000,
		NGT1      = 0x000E,  ///< NGT-1 NMEA 2000 Gateway
		NGT1_USB  = 0x000F,  ///< NGT-1 USB variant
		NGW1      = 0x0010,  ///< NGW-1 WiFi Gateway
		EMU1      = 0x0011,  ///< EMU-1 Engine Monitor
		PRO_NDC1  = 0x0020,  ///< PRO-NDC-1-E2K
		WGX1      = 0x0030   ///< WGX Wireless Gateway
	};

	/**************************************************************************//**
	\brief      Get model name from ARL Model ID
	*******************************************************************************/
	[[nodiscard]] inline std::string modelIdToString(uint16_t modelId)
	{
		switch (static_cast<ArlModelId>(modelId))
		{
		case ArlModelId::NGT1:      return "NGT-1";
		case ArlModelId::NGT1_USB:  return "NGT-1 USB";
		case ArlModelId::NGW1:      return "NGW-1";
		case ArlModelId::EMU1:      return "EMU-1";
		case ArlModelId::PRO_NDC1:  return "PRO-NDC-1-E2K";
		case ArlModelId::WGX1:      return "WGX";
		default:
			return "Model-0x" + std::to_string(modelId);
		}
	}

	/* BEM Header Size Constants */
	static constexpr std::size_t kBemResponseHeaderSize = 14;  ///< BEM GP header size
	static constexpr std::size_t kBemCommandHeaderSize  = 3;   ///< BEM PG header size

	/* BEM Response Field Offsets (from BST payload start) */
	static constexpr std::size_t kBemGP_OffBemId      = 0;
	static constexpr std::size_t kBemGP_OffSeqId      = 1;
	static constexpr std::size_t kBemGP_OffModelId    = 2;
	static constexpr std::size_t kBemGP_OffSerial     = 4;
	static constexpr std::size_t kBemGP_OffError      = 8;
	static constexpr std::size_t kBemGP_OffData       = 12;

	/* BEM Command Field Offsets (from BST payload start) */
	static constexpr std::size_t kBemPG_OffBemId      = 0;
	static constexpr std::size_t kBemPG_OffData       = 1;

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_TYPES_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
