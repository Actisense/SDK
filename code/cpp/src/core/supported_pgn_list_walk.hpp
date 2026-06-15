#ifndef __ACTISENSE_SDK_CORE_SUPPORTED_PGN_LIST_WALK
#define __ACTISENSE_SDK_CORE_SUPPORTED_PGN_LIST_WALK

/*==============================================================================
\file       supported_pgn_list_walk.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 15/06/2026
\brief      Explicit state-machine driver for the 0x40 SupportedPgnList walk
\details    The 0x40 SupportedPgnList protocol is N-GETs / N-replies: each GET
			returns one sub-list and (unless the list is exhausted) the caller
			must issue a follow-up GET advanced past the entries just received,
			reusing the device-assigned transfer id.

			This class replaces the previous shared_ptr<std::function>
			self-recursion in runSupportedPgnListWalk (GIT-117). The walk owns
			itself for its lifetime via std::enable_shared_from_this: every BEM
			request it registers captures a shared_ptr back to the walk, so the
			walk stays alive until the terminal reply (or timeout) releases the
			last pending-request callback. There is no reference cycle to break
			by hand, and the protocol now has nameable states.

			Behaviour is identical to the lambda-recursion it replaces — the two
			callers (local getSupportedPgnList_All, remote
			getSupportedPgnList_AllRemote) differ only in the submit function
			and origin selector handed to the factory.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_protocol.hpp"
#include "protocols/bem/bem_types.hpp"
#include "public/bem_callbacks.hpp"
#include "public/response_origin.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/**************************************************************************/ /**
		 \brief      Sequencer for the 0x40 SupportedPgnList chunked walk.
		 \details    Construct via std::make_shared and call start(). The walk
					 issues a GET, feeds each reply through a
					 SupportedPgnListAccumulator, and either issues a follow-up
					 GET (Continue), delivers the merged result (Done), or
					 delivers a partial result on error/timeout. Exactly one user
					 callback fires per walk.

					 Replies arrive on the receive thread; onReply() is the only
					 mutating entry point and is single-threaded per walk because
					 each follow-up GET is registered before the next reply can be
					 correlated.
		 *******************************************************************************/
		class SupportedPgnListWalk : public std::enable_shared_from_this<SupportedPgnListWalk>
		{
		public:
			/// Origin selector: yields the ResponseOrigin stamped onto the result.
			using OriginFn = std::function<ResponseOrigin()>;

			/// Submit step: encodes + sends @p cmd (local raw, or remote 126720 wrap).
			using SubmitFn = std::function<void(const BemCommand&)>;

			/**************************************************************************/ /**
			 \brief      Named states of a walk, for diagnostics and re-entry guards.
			 *******************************************************************************/
			enum class WalkState : uint8_t
			{
				Idle,				///< Constructed, not yet started.
				AwaitingFirstReply, ///< First GET sent; awaiting reply #1.
				AwaitingReply,		///< Follow-up GET sent; awaiting next sub-list.
				Complete,			///< Full list delivered (Ok).
				Failed				///< Partial/empty delivered (error or timeout).
			};

			SupportedPgnListWalk(BemProtocol& bem, std::chrono::milliseconds perGetTimeout,
								 uint8_t srcAddr, SupportedPgnListResultCallback callback,
								 SubmitFn submitFn, OriginFn makeOrigin);

			/**************************************************************************/ /**
			 \brief      Kick off the walk with the first GET (pgnIndex 0,
						 transferId 0 — the device assigns the id on reply #1).
			 *******************************************************************************/
			void start();

			[[nodiscard]] WalkState state() const noexcept { return state_; }

		private:
			/**************************************************************************/ /**
			 \brief      Encode + register + submit a GET for @p pgnIndex /
						 @p transferId. Registers a one-shot request whose callback
						 owns a shared_ptr to this walk before submitting, so the
						 walk outlives the in-flight request.
			 *******************************************************************************/
			void sendGet(uint8_t pgnIndex, uint8_t transferId);

			/**************************************************************************/ /**
			 \brief      Single per-reply handler: error/timeout -> finish partial;
						 decode-fail / Mismatch -> InvalidArgument; Done -> Ok;
						 Continue -> next GET.
			 *******************************************************************************/
			void onReply(const std::optional<BemResponse>& response, ErrorCode ec,
						 std::string_view errMsg);

			/**************************************************************************/ /**
			 \brief      Deliver the (single) user callback and latch the terminal
						 state. Idempotent via delivered_.
			 *******************************************************************************/
			void finish(std::optional<SupportedPgnListResult> result, ErrorCode ec,
						std::string_view msg);

			BemProtocol& bem_;
			SupportedPgnListAccumulator accumulator_;
			SupportedPgnListResultCallback user_callback_;
			SubmitFn submit_fn_;
			OriginFn make_origin_;
			std::chrono::milliseconds per_get_timeout_{0};
			uint8_t src_addr_ = kLocalSrcAddr;
			WalkState state_ = WalkState::Idle;
			bool delivered_ = false;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_CORE_SUPPORTED_PGN_LIST_WALK */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
