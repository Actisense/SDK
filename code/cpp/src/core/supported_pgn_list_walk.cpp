/**************************************************************************/ /**
 \file       supported_pgn_list_walk.cpp
 \brief      Explicit state-machine driver for the 0x40 SupportedPgnList walk
 \details    Implementation of SupportedPgnListWalk (GIT-117). Replaces the
			 shared_ptr<std::function> self-recursion that previously lived in
			 SessionImpl::runSupportedPgnListWalk. The branch logic in onReply()
			 mirrors the old perResponseCallback exactly; the lifetime model is
			 inverted relative to the old code — instead of breaking a reference
			 cycle by nulling a captured std::function on every terminal path,
			 each registered BEM request owns a shared_ptr to the walk and the
			 walk is destroyed naturally once the last pending callback is
			 released.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/supported_pgn_list_walk.hpp"

#include <span>
#include <string>
#include <utility>

#include "core/bem_helpers.hpp"

namespace Actisense
{
	namespace Sdk
	{
		using detail::makeBemA1;

		SupportedPgnListWalk::SupportedPgnListWalk(BemProtocol& bem,
												   std::chrono::milliseconds perGetTimeout,
												   uint8_t srcAddr,
												   SupportedPgnListResultCallback callback,
												   SubmitFn submitFn, OriginFn makeOrigin)
			: bem_(bem)
			, user_callback_(std::move(callback))
			, submit_fn_(std::move(submitFn))
			, make_origin_(std::move(makeOrigin))
			, per_get_timeout_(perGetTimeout)
			, src_addr_(srcAddr)
		{
		}

		void SupportedPgnListWalk::start() {
			if (state_ != WalkState::Idle) {
				return; /* start() is one-shot. */
			}
			state_ = WalkState::AwaitingFirstReply;
			/* Device assigns transferId on reply #1 when the caller passes 0. */
			sendGet(0, 0);
		}

		void SupportedPgnListWalk::sendGet(uint8_t pgnIndex, uint8_t transferId) {
			BemCommand cmd = makeBemA1(BemCommandId::GetSupportedPgnList);
			encodeSupportedPgnListGetRequest(pgnIndex, transferId, cmd.data);

			/* The registered callback owns a shared_ptr back to this walk, so the
			   walk outlives the in-flight request (and reply #2 cannot land on
			   freed memory). Register BEFORE submitting so a reply can never beat
			   the pending entry into existence. */
			auto self = shared_from_this();
			bem_.registerRequest(
				cmd.bemId, cmd.bstId, per_get_timeout_,
				[self](const std::optional<BemResponse>& response, ErrorCode ec,
					   std::string_view errMsg) { self->onReply(response, ec, errMsg); },
				src_addr_);
			submit_fn_(cmd);
		}

		void SupportedPgnListWalk::onReply(const std::optional<BemResponse>& response, ErrorCode ec,
										   std::string_view errMsg) {
			if (delivered_) {
				return;
			}

			if (ec != ErrorCode::Ok || !response.has_value()) {
				std::optional<SupportedPgnListResult> partial;
				if (accumulator_.initialised()) {
					partial = accumulator_.result();
				}
				finish(std::move(partial), ec, errMsg);
				return;
			}

			SupportedPgnListResponse decoded;
			std::string decodeError;
			if (!decodeSupportedPgnListResponse(
					std::span<const uint8_t>(response->data.data(), response->data.size()), decoded,
					decodeError)) {
				finish(std::nullopt, ErrorCode::InvalidArgument, decodeError);
				return;
			}

			std::string feedError;
			const auto status = accumulator_.feed(decoded, feedError);
			if (status == PgnListAccumulatorStatus::Mismatch) {
				finish(std::nullopt, ErrorCode::InvalidArgument, feedError);
				return;
			}
			if (status == PgnListAccumulatorStatus::Done) {
				finish(accumulator_.result(), ErrorCode::Ok, std::string_view{});
				return;
			}

			/* Continue: issue next GET using the device-set transferId and the
			   next pgnIndex past the sub-list we just absorbed. Guard against a
			   non-advancing index (subCount == 0) or an 8-bit overflow of
			   firstSubIdx + subCount: either would re-request an index the device
			   keeps answering, looping until the inactivity timeout. */
			const unsigned nextIdxWide =
				static_cast<unsigned>(decoded.firstSubIdx) + decoded.subCount;
			if (nextIdxWide <= decoded.firstSubIdx || nextIdxWide > 0xFF) {
				finish(std::nullopt, ErrorCode::MalformedFrame,
					   "Supported PGN list walk made no progress (subCount=0 or index overflow)");
				return;
			}
			state_ = WalkState::AwaitingReply;
			const uint8_t nextIdx = static_cast<uint8_t>(nextIdxWide);
			sendGet(nextIdx, decoded.transferId);
		}

		void SupportedPgnListWalk::finish(std::optional<SupportedPgnListResult> result, ErrorCode ec,
										  std::string_view msg) {
			if (delivered_) {
				return;
			}
			delivered_ = true;
			state_ = (ec == ErrorCode::Ok) ? WalkState::Complete : WalkState::Failed;
			if (user_callback_) {
				user_callback_(ec, msg, std::move(result), make_origin_());
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
