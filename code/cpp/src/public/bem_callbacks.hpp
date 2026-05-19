#ifndef __ACTISENSE_SDK_PUBLIC_BEM_CALLBACKS
#define __ACTISENSE_SDK_PUBLIC_BEM_CALLBACKS

/*==============================================================================
\file       bem_callbacks.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 18/05/2026
\brief      Public typed callback aliases for BEM commands.
\details    Centralises the callback signatures shared between Session
			(local gateway) and RemoteDevice (gateway-relayed remote
			device). Every typed callback carries a trailing
			ResponseOrigin describing which device sent the reply, via
			which session/transport, through what wrapping path, and
			when it landed.

			The decoded result types referenced here live alongside
			their wire-format helpers in
			src/protocols/bem/bem_commands/ headers. Those headers are
			pure data + decoders in the Actisense::Sdk namespace with
			no internal-only dependencies, and form part of the public
			API contract — applications including this header will
			transitively pull them in.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <functional>
#include <optional>
#include <string_view>

#include "protocols/bem/bem_commands/can_config.hpp"
#include "protocols/bem/bem_commands/can_info_fields.hpp"
#include "protocols/bem/bem_commands/delete_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/echo.hpp"
#include "protocols/bem/bem_commands/params_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/port_baudrate.hpp"
#include "protocols/bem/bem_commands/port_pcode.hpp"
#include "protocols/bem/bem_commands/product_info.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/total_time.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "public/error.hpp"
#include "public/hardware_info.hpp"
#include "public/operating_mode.hpp"
#include "public/response_origin.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Generic callbacks ------------------------------------------------ */

		/**************************************************************************/ /**
		 \brief      Generic BEM result callback (acknowledgement-only verbs)
		 \param[in]  code      ErrorCode::Ok on success, otherwise the failure
		 \param[in]  errorMsg  Human-readable error description (empty on success)
		 \param[in]  origin    Origin metadata for the reply
		 *******************************************************************************/
		using BemResultCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg, ResponseOrigin origin)>;

		/* Get-verb callbacks: (code, errorMsg, optional<value>, origin) ----- */

		/**************************************************************************/ /**
		 \brief      Operating-mode callback (Session/RemoteDevice::getOperatingMode)
		 *******************************************************************************/
		using OperatingModeCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<OperatingMode> mode, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Hardware-info callback (Session/RemoteDevice::getHardwareInfo)
		 \details    Decoded from the device's NMEA 2000 Product Information
					 (model, software/model versions, serial number, etc.).
		 *******************************************************************************/
		using HardwareInfoCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   const std::optional<HardwareInfo>& info, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Product-info callback (raw Product Information response,
					 prior to HardwareInfo mapping).
		 *******************************************************************************/
		using ProductInfoCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<ProductInfoResponse> info, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Port-baudrate callback (Get/Set Port Baudrate replies).
		 *******************************************************************************/
		using PortBaudrateCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg, std::optional<PortBaudrateResponse> response,
			ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Port P-Code callback (Get/Set Port P-Code replies).
		 *******************************************************************************/
		using PortPCodeCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<PortPCodeResponse> response, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable callback (Get/Set Rx PGN Enable single-PGN replies).
		 *******************************************************************************/
		using RxPgnEnableCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<RxPgnEnableResponse> response, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable callback (Get/Set Tx PGN Enable single-PGN replies).
		 *******************************************************************************/
		using TxPgnEnableCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<TxPgnEnableResponse> response, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Rx PGN Enable List F2 result callback — full aggregated walk.
		 *******************************************************************************/
		using RxPgnEnableListF2ResultCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg,
			std::optional<RxPgnEnableListF2Result> result, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Tx PGN Enable List F2 result callback — full aggregated walk.
		 *******************************************************************************/
		using TxPgnEnableListF2ResultCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg,
			std::optional<TxPgnEnableListF2Result> result, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Supported-PGN List result callback — full aggregated walk.
		 *******************************************************************************/
		using SupportedPgnListResultCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg, std::optional<SupportedPgnListResult> result,
			ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Total-time callback (Get/Set Total Time replies).
		 *******************************************************************************/
		using TotalTimeCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<TotalTimeResponse> response, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Echo callback (Echo reply, including the echoed payload).
		 *******************************************************************************/
		using EchoCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<EchoResponse> response, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      CAN Config callback (Get/Set CAN Config replies — NAME + SA).
		 *******************************************************************************/
		using CanConfigCallback =
			std::function<void(ErrorCode code, std::string_view errorMsg,
							   std::optional<CanConfigResponse> response, ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      CAN Info Field callback (Get/Set CAN Info Field 1/2/3 replies).
		 *******************************************************************************/
		using CanInfoFieldCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg, std::optional<CanInfoFieldResponse> response,
			ResponseOrigin origin)>;

		/**************************************************************************/ /**
		 \brief      Params PGN Enable Lists callback (status query reply).
		 *******************************************************************************/
		using ParamsPgnEnableListsCallback = std::function<void(
			ErrorCode code, std::string_view errorMsg,
			std::optional<ParamsPgnEnableListsResponse> response, ResponseOrigin origin)>;

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_CALLBACKS */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
