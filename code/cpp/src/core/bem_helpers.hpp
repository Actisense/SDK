#ifndef __ACTISENSE_SDK_CORE_BEM_HELPERS
#define __ACTISENSE_SDK_CORE_BEM_HELPERS

/*==============================================================================
\file       bem_helpers.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 15/06/2026
\brief      Shared BEM request/response helpers for Session and RemoteDevice
\details    Consolidates the small helpers that were duplicated between
			session (local gateway) and remote-device (PGN 126720) code paths
			(GIT-116): the A1 command builder, the ResponseOrigin stamper, and
			the wrapAck / wrapTyped adapters that turn a raw BemResponseCallback
			into a typed ack / get-result callback.

			Origin handling is generalised via makeOrigin(), which uses the
			kLocalSrcAddr (0xFF) sentinel to pick makeLocalOrigin() for the
			local gateway path and makeRemoteOrigin(srcAddr) otherwise — the
			same selector the aggregated-reply registration already used. This
			lets both the local Session verbs and the remote RemoteDevice verbs
			share one implementation.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/session_impl.hpp"
#include "protocols/bem/bem_types.hpp"

namespace Actisense
{
	namespace Sdk
	{
		namespace detail
		{
			/**************************************************************************/ /**
			 \brief      Build an empty A1-page BEM command for the given command id.
			 \details    All A1 BEM commands share the same BstId; callers append
						 the per-command payload afterwards.
			 *******************************************************************************/
			inline BemCommand makeBemA1(BemCommandId id) {
				BemCommand cmd;
				cmd.bstId = BstId::Bem_PG_A1;
				cmd.bemId = id;
				return cmd;
			}

			/**************************************************************************/ /**
			 \brief      Select the ResponseOrigin for a reply, by source address.
			 \details    kLocalSrcAddr (0xFF) means the reply came back over the
						 local gateway path (origin path = Local); any other address
						 is a device reached via the PGN 126720 wrap (path = Remote).
						 0xFF is the NMEA 2000 null address and is never a valid
						 remote target, so the sentinel cannot collide with a real
						 device.
			 *******************************************************************************/
			inline ResponseOrigin makeOrigin(Session::Impl& session, uint8_t srcAddr) {
				return srcAddr == kLocalSrcAddr ? session.makeLocalOrigin()
												: session.makeRemoteOrigin(srcAddr);
			}

			/**************************************************************************/ /**
			 \brief      Populate the responder-identity fields on @p origin from a
						 BEM reply header.
			 *******************************************************************************/
			inline void stampOriginFromResponse(ResponseOrigin& origin,
												const BemResponse& response) noexcept {
				origin.modelId = response.header.modelId;
				origin.serialNumber = response.header.serialNumber;
			}

			/**************************************************************************/ /**
			 \brief      Adapt a typed "ack" callback to a raw BemResponseCallback.
			 \details    Maps a device error code (header.errorCode != 0) to
						 ErrorCode::MalformedFrame so callers see a non-Ok result,
						 and synthesises the ResponseOrigin via makeOrigin().
			 *******************************************************************************/
			template <typename AckCallback>
			BemResponseCallback wrapAck(Session::Impl& session, uint8_t srcAddr,
										AckCallback callback) {
				return BemResponseCallback{[&session, srcAddr, cb = std::move(callback)](
											   const std::optional<BemResponse>& response,
											   ErrorCode code, std::string_view errorMsg) {
					if (!cb) {
						return;
					}
					ResponseOrigin origin = makeOrigin(session, srcAddr);
					if (response) {
						stampOriginFromResponse(origin, *response);
					}
					if (code != ErrorCode::Ok || !response) {
						cb(code, errorMsg, std::move(origin));
						return;
					}
					if (response->header.errorCode != 0) {
						cb(ErrorCode::MalformedFrame, "Device returned BEM error code",
						   std::move(origin));
						return;
					}
					cb(ErrorCode::Ok, {}, std::move(origin));
				}};
			}

			/**************************************************************************/ /**
			 \brief      Adapt a typed get-result callback to a raw BemResponseCallback.
			 \details    Decodes the reply payload via @p decoder and invokes the
						 typed callback with (code, errMsg, optional<value>, origin).
			 *******************************************************************************/
			template <typename DecodedT, typename Decoder, typename TypedCallback>
			BemResponseCallback wrapTyped(Session::Impl& session, uint8_t srcAddr, Decoder decoder,
										  TypedCallback callback) {
				return BemResponseCallback{[&session, srcAddr, decoder, cb = std::move(callback)](
											   const std::optional<BemResponse>& response,
											   ErrorCode code, std::string_view errorMsg) {
					if (!cb) {
						return;
					}
					ResponseOrigin origin = makeOrigin(session, srcAddr);
					if (response) {
						stampOriginFromResponse(origin, *response);
					}
					if (code != ErrorCode::Ok || !response) {
						cb(code, errorMsg, std::nullopt, std::move(origin));
						return;
					}
					if (response->header.errorCode != 0) {
						cb(ErrorCode::MalformedFrame, "Device returned BEM error code",
						   std::nullopt, std::move(origin));
						return;
					}
					DecodedT decoded;
					std::string decodeError;
					if (!decoder(response->data, decoded, decodeError)) {
						cb(ErrorCode::MalformedFrame, decodeError, std::nullopt, std::move(origin));
						return;
					}
					cb(ErrorCode::Ok, {}, std::make_optional(std::move(decoded)),
					   std::move(origin));
				}};
			}

		} /* namespace detail */
	}	  /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_CORE_BEM_HELPERS */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
