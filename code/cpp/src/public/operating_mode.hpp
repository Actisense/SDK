#ifndef __ACTISENSE_SDK_OPERATING_MODE_HPP
#define __ACTISENSE_SDK_OPERATING_MODE_HPP

/**************************************************************************/ /**
 \file       operating_mode.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 09/01/2026
 \brief      Operating modes as an enum
 \details    Each Actisense device has an Operating Mode that determines how
			 it behaves. The current mode can be queried or changed via the
			 BEM Get/Set Operating Mode command (see Session::getOperatingMode
			 / Session::setOperatingMode).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstddef>
#include <cstdint>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief    Operating Mode ID enumerations.
		 \details  Each device has an Operating Mode that determines how it behaves.
				 The current mode can be requested via rest API or
				 a BST-BEM Command message.

				 Each instrument has standard operating modes that
				 are defined in files or other non-volatile storage means
				 An operating mode has an enumerated mode number
				 which is stored in EEPROM or other non-volatile store
				 so that the mode can be selected on power-up.

				 If mode information is lost, or device is new,
				 the device will select its default mode.

				 Devices have a table of correct operating modes, so if a
				 mode is requested which is not available, the device
				 will return an error code and remain in the same mode.
		 *******************************************************************************/
		enum class OperatingMode : uint16_t
		{
			/**********************************************************************/ /**
			 \brief    	Undefined Operating Mode.
			 \details  	Use ARLModel ID to initialise correctly.
			 \note		Enum value 0.
			 ***************************************************************************/
			UndefinedMode = 0,

			/* NGT-1 / NGX Operating Modes (1 to 3) -------------------------------- */

			/**********************************************************************/ /**
			 \brief    	NGT: Normal Rx & Tx Transfers using BST Protocol.
			 \details  	Rx & Tx PGN Enable Lists are active.
			 \note		Enum value 1.
			 ***************************************************************************/
			NgTransferNormalMode = 1,

			/**********************************************************************/ /**
			 \brief    	Rx Transfer All & Normal Tx Transfers using BST Protocol.
			 \details  	NGT: Rx PGN Enable List is inactive - all PGNs in NMEA 2000 database
						 are enabled for reception (and Transfer to PC). Tx PGN Enable
						 List is active.
						 Transfer method is BST93/94
			 \note		Enum value 2.
			 ***************************************************************************/
			NgTransferRxAllMode = 2,

			/**********************************************************************/ /**
			 \brief    	Legacy / spare transfer mode slot (mode 3).
			 \details  	Historically labelled "Raw Rx & Tx Transfers". NGT never
						 implemented mode 3, and the NGX no longer uses it for raw
						 CAN — raw CAN-frame transfer moved to CanPacket (5) /
						 CanPacketAscii (6), matching firmware OperatingModeCodes.h
						 where mode 3 is now OM_NGTransferSpareMode. The enum value is
						 retained (not removed) to preserve the public API surface for
						 existing clients; new code should target CanPacket instead.
			 \note		Enum value 3.
			 ***************************************************************************/
			NgTransferRawMode = 3,

			/* NGW-1 & NGX Operating Modes (4 to 4) -------------------------------- */
			/**********************************************************************/ /**
			 \brief    	Normal Rx & Tx conversions using NMEA 0183 Protocol.
			 \details  	Rx & Tx PGN Enable Lists are active.
						 NGW: It's the normal mode for an NGW
						 NGX: Switches device to NGW mode and translates NMEA 2000 to 0183
			 \note		Enum value 4.
			 ***************************************************************************/
			NgConvertNormalMode = 4,

			/* NGX / Gateway raw CAN Operating Modes (5 to 6) --------------------- */
			/**********************************************************************/ /**
			 \brief    	CAN Packet mode — raw CAN frames via BST-95.
			 \details  	Rx & Tx PGN Enable Lists are inactive. All CAN packets are
						 transferred to / from the PC as raw BST-95 CAN frames, in
						 both directions. No NMEA 2000 processing is performed, so the
						 mode is agnostic to the higher-level protocol on the bus —
						 ideal as a low-level CAN analyser.
						 NGT: not implemented.
						 NGX: switches the device to this mode over the serial host
						 link; only BEM commands are otherwise processed, making it a
						 serial-wired CAN-analyser mode.
			 \note		Enum value 5. Matches firmware OperatingModeCodes.h OM_CanPacket.
			 ***************************************************************************/
			CanPacket = 5,

			/**********************************************************************/ /**
			 \brief    	CAN Packet mode — raw CAN frames in ASCII.
			 \details  	As CanPacket, but CAN frames are transferred in ASCII
						 format rather than as binary BST-95 frames.
			 \note		Enum value 6. Matches firmware OperatingModeCodes.h
						 OM_CanPacketASCII.
			 ***************************************************************************/
			CanPacketAscii = 6,


			/* ---  Enum values 7 to 15 are reserved for use by Gateway products  --- */

			/* Buffer/Combiner Operating Modes  ------------------------------------- */
			/**********************************************************************/ /**
			 \brief    	Buffer Mode 1.
			 \details  	Input 1 -> Outputs 1 to 12. Output Baudrate = Input baudrate.
			 \note		Enum value 16.
			 ***************************************************************************/
			Buffer1 = 16,
			/**********************************************************************/ /**
			 \brief    	Buffer Mode 2.
			 \details  	Input 2 -> Outputs 1 to 12.	Output Baudrate = Input baudrate.
			 \note		Enum value 17.
			 ***************************************************************************/
			Buffer2,
			/**********************************************************************/ /**
			 \brief    	Buffer Mode 3.
			 \details  	Input 1 -> Outputs 1 to 6, Input 2 -> Output 7 to 12.
						 Output Baudrate = Input baudrate.
			 \note		Enum value 18.
			 ***************************************************************************/
			Buffer3,
			/**********************************************************************/ /**
			 \brief    	Autoswitch Direct 'Simple' Mode (1)
			 \details  	Simple Autoswitch - signal detection only.
						 Replaced by all-smart autoswitch
						 This code is now deprecated - only the original
						 PROBUF1 code supports this mode.
						 DO NOT USE FOR NEW DESIGNS!
			 \note		Enum value 19.
			 ***************************************************************************/
			AutoswitchDirect,
			/**********************************************************************/ /**
			 \brief    	Autoswitch 'Smart' Mode (2)
			 \details  	SMART Autoswitch - full sentence and deep inspection.
						 User can select the level of deep inspection available
						 if they set up a user mode to use this setting
						 Default SMART settings are logically base upon inspecting
						 the input stream.
						 At the most basic level, failure to receive data is the
						 switch criteria.  See the auto switch receiver special
						 functions.
			 \note		Enum value 20.
			 ***************************************************************************/
			AutoswitchSmart,
			/**********************************************************************/ /**
			 \brief    	Combiner / Multiplexer 'Slow' Mode (1)
			 \details  	Output baud rate = slowest of all inputs
						 Any binary (AIS or other) data will be stripped if data rates
						 exceeded. Data will be removed using smart duplicate deletion
						 algorithm if output data rate exceeds input.
			 \note		Enum value 21.
			 ***************************************************************************/
			Combine1,
			/**********************************************************************/ /**
			 \brief    	Combiner / Multiplexer 'Fast' Mode (2).
			 \details  	Output baud rate = fastest of all inputs.
						 AIS data preserved.	Duplicate deletion will occur only if two
						 4800 baud streams exceed input rate or if too much 38400 data
						 is present.
			 \note		Enum value 22.
			 ***************************************************************************/
			Combine2,
			/**********************************************************************/ /**
			 \brief    	Test Mode 1.
			 \details  	Instrument dependent "test" mode. Programmer can use this to
						 initialise unit into a non-standard test mode such as simulator.
			 \note		Enum value 23.
			 ***************************************************************************/
			Test1,
			/**********************************************************************/ /**
			 \brief    	NSI Mode 1.
			 \details  	Reserved for NSI mode.
			 \note		Enum value 24.
			 ***************************************************************************/
			NsiMode1 = 24, /* Reserved for NSI mode */

			/* All "Standard" modes are less than this number */
			LastStandard = 253,

			/* General device/instrument Operating Modes  -------------------------- */
			/**********************************************************************/ /**
			 \brief    	"Normal" mode
			 \details  	This mode is transitioned to instruments which generally
						 have a single "standard" or "normal" operating mode.
						 Such instruments as W2K-1 and EMU-1 use this to transition
						 to before startup.
						 Any other modes supported will be special modes used to
						 change operation or put into tests
						 When such a device has started and initialised correctly, it
						 will transition from UndefinedMode to Normal
			 ***************************************************************************/
			Normal = 512,

			/* EMU/Perception Specific (Special) Operating Modes  -------------------------- */
			/* none defined yet */

			/* W2K Specific (Special) Operating Modes  -------------------------- */
			/* none defined yet */

			/* Predefined Operating Modes  -------------------------- */
			/**********************************************************************/ /**
			 \brief    	Numeric enumerations of predefined instrument modes
			 \details  	Some devices support predefined modes that are
						 not user configurable. These modes are defined
						 here in the reserved range 40000 to 40255.
						 PredefinedMode1 is the predefined default mode
			 \note		Enum start value 40000
			 ***************************************************************************/
			PredefinedMode1 = 40000,
			PredefinedMode2 = 40001,

			/* Max value of predefined mode reserved range */
			PredefinedModeEnd = 40255,

			/* User Operating Modes  ------------------------------------------------ */
			/**********************************************************************/ /**
			 \brief    	User Mode area.
			 \details  	Instrument in one of a range of "User" configured modes.
						 These enumerations are used generically for all instruments
						 that possess user configs or have both manual and user
						 configured modes. Generic user modes depend upon user setting
						 configurations or files, so user mode can be e.g. 50000 for
						 user mode 1, 50001 for user mode 2 etc.
						 These modes allow modes to be managed by enumerated
						 value from the BEM or WEB interface - the instrument
						 must support a valid mapped mode for the user mode selected.

						 e.g. on a Crosspoint system, there must be a config
						 file for each user defined mode

			 \note		Enum value range 50000 to 59999 - to offer 9999 user modes!
			 *******************************************************************************/
			UserStart = 50000,
			User1 = UserStart,
			User2,
			User3,
			User4,
			User5,
			UserLastDefined,
			//* User modes can be defined up to UserEnd
			UserEnd = 59999,
			Null = 65535

		};

		static constexpr std::size_t kBemGP_Off_OperatingMode = 12;

		/**************************************************************************/ /**
		 \brief    Get string representation of an OperatingMode
		 \details  Converts an OperatingMode enum value to its human-readable name
		 \param    mode The OperatingMode enum value to convert
		 \return   Pointer to a constant string containing the mode name
		 *******************************************************************************/
		const char* OperatingModeName(OperatingMode mode);

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_OPERATING_MODE_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
