/**************************************************************************/ /**
 \file       session_remote.cpp
 \brief      Session remote-device (PGN 126720) BEM plumbing
 \details    Split from session_impl.cpp by concern (GIT-116). Holds the
			 PGN-126720 wrap/send helpers: buildRemoteBemFrame,
			 sendBemCommandRemote and sendBemCommandRemoteMultiReply. The
			 aggregated *Remote verbs live in session_receive.cpp alongside the
			 registerAggregatedReply template they instantiate.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/session_impl.hpp"
#include "protocols/bem/bem_wrap_126720.hpp"
#include "protocols/bst/bst_frame.hpp"

namespace Actisense
{
	namespace Sdk
	{
		std::optional<BstFrame> Session::Impl::buildRemoteBemFrame(uint8_t targetN2kSourceAddress,
																 const BemCommand& command,
																 std::string& outEncodeError) {
			std::vector<uint8_t> innerBst;
			if (!bem_.encodeCommandInnerBst(command, innerBst, outEncodeError)) {
				return std::nullopt;
			}

			std::vector<uint8_t> wrappedPayload;
			wrapBemInPgn126720(innerBst, wrappedPayload);

			constexpr uint8_t kPriority = 3;
			return BstFrame::create94(kPgn126720, targetN2kSourceAddress, wrappedPayload,
									  kPriority);
		}

		void Session::Impl::sendBemCommandRemote(uint8_t targetN2kSourceAddress,
											   const BemCommand& command,
											   std::chrono::milliseconds timeout,
											   BemResponseCallback callback) {
			std::string encodeError;
			auto frame = buildRemoteBemFrame(targetN2kSourceAddress, command, encodeError);
			if (!frame) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument, encodeError);
				}
				return;
			}

			bem_.registerRequest(command.bemId, command.bstId, timeout, std::move(callback),
								 targetN2kSourceAddress);
			metricsCollector_.recordBemCommand();

			asyncSend(SendProtocol::Bst, frame->rawData(), [this](ErrorCode code) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send remote BEM command");
				}
			});
		}

		void Session::Impl::sendBemCommandRemoteMultiReply(
			uint8_t targetN2kSourceAddress, const BemCommand& command,
			std::chrono::milliseconds inactivityTimeout,
			std::function<bool(const BemResponse&)> isComplete, BemResponseCallback callback) {
			std::string encodeError;
			auto frame = buildRemoteBemFrame(targetN2kSourceAddress, command, encodeError);
			if (!frame) {
				if (callback) {
					callback(std::nullopt, ErrorCode::InvalidArgument, encodeError);
				}
				return;
			}

			bem_.registerMultiReplyRequest(command.bemId, command.bstId, inactivityTimeout,
										   std::move(isComplete), std::move(callback),
										   targetN2kSourceAddress);
			metricsCollector_.recordBemCommand();

			asyncSend(SendProtocol::Bst, frame->rawData(), [this](ErrorCode code) {
				if (code != ErrorCode::Ok && errorCallback_) {
					errorCallback_(code, "Failed to send remote BEM command (multi-reply)");
				}
			});
		}


	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
