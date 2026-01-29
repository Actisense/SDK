#ifndef __ACTISENSE_SDK_BEM_BEM_COMMANDS_BEM_COMMANDS_HPP
#define __ACTISENSE_SDK_BEM_BEM_COMMANDS_BEM_COMMANDS_HPP

/**************************************************************************/ /**
 \file       bem_commands.hpp
 \brief      BEM (Binary Encoded Message) command/response types
 \details    Type definitions for BST-BEM command/response protocol

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/activate_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/commit_to_eeprom.hpp"
#include "protocols/bem/bem_commands/commit_to_flash.hpp"
#include "protocols/bem/bem_commands/default_pgn_enable_list.hpp"
#include "protocols/bem/bem_commands/delete_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/error_report.hpp"
#include "protocols/bem/bem_commands/negative_ack.hpp"
#include "protocols/bem/bem_commands/operating_mode.hpp"
#include "protocols/bem/bem_commands/params_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/port_baudrate.hpp"
#include "protocols/bem/bem_commands/port_pcode.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_commands/reinit_main_app.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f1.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/startup_status.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/system_status.hpp"
#include "protocols/bem/bem_commands/total_time.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f1.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"

namespace Actisense
{
	namespace Sdk
	{

		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      BEM Command IDs
		 \details    Extended command codes sent via BST-BEM protocol.
					 Note: Command IDs are sent via BST A1H, responses come via BST A0H.
					 The BEM ID in the response matches the command that was sent.
		 *******************************************************************************/
		enum class BemCommandId : uint8_t
		{
			/* Device Control Commands (Action Commands) */
			ReInitMainApp = 0x00,  ///< Reinitialize main application (reboot)
			CommitToEeprom = 0x01, ///< Commit session settings to EEPROM
			CommitToFlash = 0x02,  ///< Commit session settings to FLASH

			/* Device Information Commands */
			GetSetOperatingMode = 0x11, ///< Get/Set operating mode (BST A1H→A0H)
			GetSetTotalTime = 0x15,		///< Get/Set device total operating time
			Echo = 0x18,				///< Echo test command

			/* Port Configuration Commands */
			GetSetPortPCode = 0x13,	   ///< Get/Set port P-Code configuration
			GetSetPortBaudrate = 0x17, ///< Get/Set port baudrate configuration

			/* NMEA 2000 Product Information Commands */
			GetSupportedPgnList = 0x40, ///< Get list of supported PGNs
			GetProductInfo = 0x41,		///< Get product information
			GetSetCanConfig = 0x42,		///< Get/Set CAN configuration (NMEA NAME)
			GetSetCanInfoField1 = 0x43, ///< Get/Set CAN installation description 1
			GetSetCanInfoField2 = 0x44, ///< Get/Set CAN installation description 2
			GetCanInfoField3 = 0x45,	///< Get CAN manufacturer info (read-only)

			/* PGN Enable Commands */
			GetSetRxPgnEnable = 0x46, ///< Get/Set Rx PGN enable state
			GetSetTxPgnEnable = 0x47, ///< Get/Set Tx PGN enable state

			/* PGN List Management Commands */
			GetSetRxPgnEnableListF1 = 0x48, ///< Get/Set Rx PGN enable list (Format 1, legacy)
			GetSetTxPgnEnableListF1 = 0x49, ///< Get/Set Tx PGN enable list (Format 1, legacy)
			DeletePgnEnableLists = 0x4A,	///< Delete PGN enable lists from session
			ActivatePgnEnableLists = 0x4B,	///< Activate session PGN enable lists
			DefaultPgnEnableList = 0x4C,	///< Restore default PGN enable lists
			ParamsPgnEnableLists = 0x4D,	///< Get PGN enable list parameters/status
			GetSetRxPgnEnableListF2 = 0x4E, ///< Get/Set Rx PGN enable list (Format 2)
			GetSetTxPgnEnableListF2 = 0x4F, ///< Get/Set Tx PGN enable list (Format 2)

			/* Unsolicited Messages (no command, response only via BST A0H) */
			StartupStatus = 0xF0, ///< (Unsolicited) startup status information
			ErrorReport = 0xF1,	  ///< (Unsolicited) error report information
			SystemStatus = 0xF2,  ///< (Unsolicited) System status information
			NegativeAck = 0xF4,	  ///< (Unsolicited) negative acknowledgement
		};

		/**************************************************************************/ /**
		 \brief      Get human-readable name for BEM command ID
		 \param[in]  id  BEM command ID
		 \return     String description
		 *******************************************************************************/
		[[nodiscard]] inline std::string bemCommandIdToString(BemCommandId id) {
			switch (id) {
				/* Device Control Commands */
				case BemCommandId::ReInitMainApp:
					return "ReInitMainApp";
				case BemCommandId::CommitToEeprom:
					return "CommitToEeprom";
				case BemCommandId::CommitToFlash:
					return "CommitToFlash";
				/* Device Information Commands */
				case BemCommandId::GetSetOperatingMode:
					return "GetSetOperatingMode";
				case BemCommandId::GetSetTotalTime:
					return "GetSetTotalTime";
				case BemCommandId::Echo:
					return "Echo";
				/* Port Configuration Commands */
				case BemCommandId::GetSetPortPCode:
					return "GetSetPortPCode";
				case BemCommandId::GetSetPortBaudrate:
					return "GetSetPortBaudrate";
				/* NMEA 2000 Product Information Commands */
				case BemCommandId::GetSupportedPgnList:
					return "GetSupportedPgnList";
				case BemCommandId::GetProductInfo:
					return "GetProductInfo";
				case BemCommandId::GetSetCanConfig:
					return "GetSetCanConfig";
				case BemCommandId::GetSetCanInfoField1:
					return "GetSetCanInfoField1";
				case BemCommandId::GetSetCanInfoField2:
					return "GetSetCanInfoField2";
				case BemCommandId::GetCanInfoField3:
					return "GetCanInfoField3";
				/* PGN Enable Commands */
				case BemCommandId::GetSetRxPgnEnable:
					return "GetSetRxPgnEnable";
				case BemCommandId::GetSetTxPgnEnable:
					return "GetSetTxPgnEnable";
				/* PGN List Management Commands */
				case BemCommandId::GetSetRxPgnEnableListF1:
					return "GetSetRxPgnEnableListF1";
				case BemCommandId::GetSetTxPgnEnableListF1:
					return "GetSetTxPgnEnableListF1";
				case BemCommandId::DeletePgnEnableLists:
					return "DeletePgnEnableLists";
				case BemCommandId::ActivatePgnEnableLists:
					return "ActivatePgnEnableLists";
				case BemCommandId::DefaultPgnEnableList:
					return "DefaultPgnEnableList";
				case BemCommandId::ParamsPgnEnableLists:
					return "ParamsPgnEnableLists";
				case BemCommandId::GetSetRxPgnEnableListF2:
					return "GetSetRxPgnEnableListF2";
				case BemCommandId::GetSetTxPgnEnableListF2:
					return "GetSetTxPgnEnableListF2";
				/* Unsolicited Messages */
				case BemCommandId::StartupStatus:
					return "StartupStatus";
				case BemCommandId::ErrorReport:
					return "ErrorReport";
				case BemCommandId::SystemStatus:
					return "SystemStatus";
				case BemCommandId::NegativeAck:
					return "NegativeAck";
				default:
					return "BEM-0x" + std::to_string(static_cast<unsigned>(id));
			}
		}

		/**************************************************************************/ /**
		 \brief      Check if BEM command ID is an unsolicited message type
		 \param[in]  id  BEM command ID
		 \return     True if this is an unsolicited message (F0-FF range)
		 *******************************************************************************/
		[[nodiscard]] constexpr bool isBemUnsolicited(BemCommandId id) noexcept {
			return static_cast<uint8_t>(id) >= 0xF0;
		}

		/* BEM Header Size Constants */
		static constexpr std::size_t kBemResponseHeaderSize = 14; ///< BEM GP header size
		static constexpr std::size_t kBemCommandHeaderSize = 3;	  ///< BEM PG header size

		/* BEM Response Field Offsets (from BST payload start) */
		static constexpr std::size_t kBemGP_OffBemId = 0;
		static constexpr std::size_t kBemGP_OffSeqId = 1;
		static constexpr std::size_t kBemGP_OffModelId = 2;
		static constexpr std::size_t kBemGP_OffSerial = 4;
		static constexpr std::size_t kBemGP_OffError = 8;
		static constexpr std::size_t kBemGP_OffData = 12;

		/* BEM Command Field Offsets (from BST payload start) */
		static constexpr std::size_t kBemPG_OffBemId = 0;
		static constexpr std::size_t kBemPG_OffData = 1;

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_BEM_COMMANDS_BEM_COMMANDS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
