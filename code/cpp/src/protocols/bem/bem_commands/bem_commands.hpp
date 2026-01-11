#ifndef __ACTISENSE_SDK_BEM_BEM_COMMANDS_BEM_COMMANDS_HPP
#define __ACTISENSE_SDK_BEM_BEM_COMMANDS_BEM_COMMANDS_HPP

/**************************************************************************//**
\file       bem_commands.hpp
\brief      BEM (Binary Encoded Message) command/response types
\details    Type definitions for BST-BEM command/response protocol

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_commands/system_status.hpp"

namespace Actisense
{
namespace Sdk
{

	/* Definitions ---------------------------------------------------------- */

	/**************************************************************************//**
	\brief      BEM Command IDs
	\details    Extended command codes sent via BST-BEM protocol.
	            Note: Command IDs are sent via BST A1H, responses come via BST A0H.
	            The BEM ID in the response matches the command that was sent.
	*******************************************************************************/
	enum class BemCommandId : uint8_t
	{
		/* Device Information Commands */
		GetSetOperatingMode = 0x11, ///< Get/Set operating mode (BST A1H→A0H)
		
		/* Unsolicited Messages (no command, response only via BST A0H) */
		StartupStatus = 0xF0,   ///< (Unsolicited) startup status information
		ErrorReport   = 0xF1,   ///< (Unsolicited) error report information
		SystemStatus  = 0xF2,   ///< (Unsolicited) System status information
	};

	/**************************************************************************//**
	\brief      Get human-readable name for BEM command ID
	\param[in]  id  BEM command ID
	\return     String description
	*******************************************************************************/
	[[nodiscard]] inline std::string bemCommandIdToString(BemCommandId id)
	{
		switch (id)
		{
		case BemCommandId::GetSetOperatingMode: return "GetSetOperatingMode";
		case BemCommandId::StartupStatus:       return "StartupStatus";
		case BemCommandId::ErrorReport:         return "ErrorReport";
		case BemCommandId::SystemStatus:        return "SystemStatus";
		default:
			return "BEM-0x" + std::to_string(static_cast<unsigned>(id));
		}
	}

	/**************************************************************************//**
	\brief      Check if BEM command ID is an unsolicited message type
	\param[in]  id  BEM command ID
	\return     True if this is an unsolicited message (F0-FF range)
	*******************************************************************************/
	[[nodiscard]] constexpr bool isBemUnsolicited(BemCommandId id) noexcept
	{
		return static_cast<uint8_t>(id) >= 0xF0;
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

#endif /* __ACTISENSE_SDK_BEM_BEM_COMMANDS_BEM_COMMANDS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
