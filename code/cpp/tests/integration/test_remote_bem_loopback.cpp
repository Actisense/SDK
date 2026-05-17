/*********************************************************************//**
\file       test_remote_bem_loopback.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/05/2026
\brief      End-to-end integration test for remote BEM commands wrapped in
            PGN 126720 (GIT-88).
\details    Uses a hermetic test-only ITransport implementation
            (FakeWrappedGateway) that:
              - Captures the BDTP-framed bytes the session sends and parses
                them into a BST-94 PGN 126720 frame, so the test can assert
                wire format and target N2K address;
              - Synthesises a wrapped BST-93 PGN 126720 reply from a fake
                remote device's source address and pushes it back into the
                session's Rx side via injectRx.

            This exercises the full RemoteDevice → wrap → asyncSend →
            asyncRecv → unwrap → BEM correlator path without hardware.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/remote_device_impl.hpp"
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_wrap_126720.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "transport/transport.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* ----------------------------------------------------------------------- */
/* FakeWrappedGateway transport                                            */
/* ----------------------------------------------------------------------- */

class FakeWrappedGateway final : public ITransport
{
public:
	void asyncOpen(const TransportConfig& /*config*/,
				   std::function<void(ErrorCode)> completion) override
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			is_open_ = true;
		}
		if (completion) {
			completion(ErrorCode::Ok);
		}
	}

	void close() override
	{
		std::vector<RecvCompletionHandler> pending;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			is_open_ = false;
			while (!pending_recvs_.empty()) {
				pending.push_back(std::move(pending_recvs_.front()));
				pending_recvs_.pop_front();
			}
		}
		cv_.notify_all();
		for (auto& c : pending) {
			if (c) {
				c(ErrorCode::Canceled, {});
			}
		}
	}

	[[nodiscard]] bool isOpen() const noexcept override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return is_open_;
	}

	void asyncSend(ConstByteSpan data, SendCompletionHandler completion) override
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			sent_.emplace_back(data.begin(), data.end());
		}
		cv_.notify_all();
		if (completion) {
			completion(ErrorCode::Ok, data.size());
		}
	}

	void asyncRecv(RecvCompletionHandler completion) override
	{
		std::vector<uint8_t> immediate;
		bool deliver = false;
		ErrorCode ec = ErrorCode::Ok;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!is_open_) {
				deliver = true;
				ec = ErrorCode::NotConnected;
			} else if (!rx_queue_.empty()) {
				immediate = std::move(rx_queue_.front());
				rx_queue_.pop_front();
				deliver = true;
			} else {
				pending_recvs_.push_back(std::move(completion));
			}
		}
		if (deliver && completion) {
			completion(ec, immediate);
		}
	}

	[[nodiscard]] TransportKind kind() const noexcept override
	{
		return TransportKind::Loopback;
	}

	/* Test-side controls --------------------------------------------------- */

	/// Push pre-framed bytes into the session's Rx side.
	void injectRx(std::span<const uint8_t> bytes)
	{
		RecvCompletionHandler waiter;
		std::vector<uint8_t> copy(bytes.begin(), bytes.end());
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!pending_recvs_.empty()) {
				waiter = std::move(pending_recvs_.front());
				pending_recvs_.pop_front();
			} else {
				rx_queue_.push_back(std::move(copy));
				return;
			}
		}
		if (waiter) {
			waiter(ErrorCode::Ok, copy);
		}
	}

	/// Block until at least one outbound frame is available, or the timeout
	/// elapses. Returns the captured frame, or std::nullopt on timeout.
	std::optional<std::vector<uint8_t>>
	waitForSent(std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		const bool got =
			cv_.wait_for(lock, timeout, [this] { return !sent_.empty(); });
		if (!got) {
			return std::nullopt;
		}
		auto frame = std::move(sent_.front());
		sent_.pop_front();
		return frame;
	}

private:
	mutable std::mutex mutex_;
	std::condition_variable cv_;
	bool is_open_ = false;
	std::deque<std::vector<uint8_t>> sent_;
	std::deque<std::vector<uint8_t>> rx_queue_;
	std::deque<RecvCompletionHandler> pending_recvs_;
};

/* ----------------------------------------------------------------------- */
/* Helpers                                                                 */
/* ----------------------------------------------------------------------- */

namespace
{
	/* Strip the DLE+STX/DLE+ETX framing (and DLE-DLE escapes) from a single
	   BDTP frame. Returns the inner BST bytes (BST ID + length + data +
	   checksum). */
	std::vector<uint8_t> unframeBdtp(std::span<const uint8_t> framed)
	{
		std::vector<uint8_t> inner;
		bool inFrame = false;
		bool dle = false;
		for (uint8_t b : framed) {
			if (!inFrame) {
				if (dle && b == BdtpChars::STX) {
					inFrame = true;
					dle = false;
					continue;
				}
				dle = (b == BdtpChars::DLE);
				continue;
			}
			if (dle) {
				if (b == BdtpChars::ETX) {
					return inner;
				}
				/* DLE DLE → literal DLE */
				inner.push_back(BdtpChars::DLE);
				dle = false;
				continue;
			}
			if (b == BdtpChars::DLE) {
				dle = true;
				continue;
			}
			inner.push_back(b);
		}
		return inner;
	}

	/* BDTP-frame a raw BST datagram (ID + length + data). Appends the
	   trailing zero-sum BST checksum then DLE-frames the lot. */
	std::vector<uint8_t> bdtpFrame(std::span<const uint8_t> rawBst)
	{
		std::vector<uint8_t> withChecksum(rawBst.begin(), rawBst.end());
		const uint8_t checksum =
			static_cast<uint8_t>(-BdtpProtocol::calculateChecksum(withChecksum));
		withChecksum.push_back(checksum);
		std::vector<uint8_t> framed;
		BdtpProtocol::encodeFrame(withChecksum, framed);
		return framed;
	}

	/* Build a wrapped BEM response BST-93 frame addressed PGN 126720, ready
	   to inject as Rx bytes (DLE-framed). */
	std::vector<uint8_t> buildWrappedReply(uint8_t remoteSourceAddress,
										   BstId responseBstId, BemCommandId bemId,
										   uint8_t sequenceId, uint16_t modelId,
										   uint32_t serialNumber, uint32_t errorCode,
										   std::span<const uint8_t> bemPayload)
	{
		/* Inner BEM response BST bytes:
		   [bstId][storeLen][bemId][seqId][modelId LE16][serial LE32][err LE32][payload] */
		std::vector<uint8_t> inner;
		const uint8_t innerLen = static_cast<uint8_t>(
			1 + 1 + 2 + 4 + 4 + bemPayload.size()); /* bemId..payload */
		inner.push_back(static_cast<uint8_t>(responseBstId));
		inner.push_back(innerLen);
		inner.push_back(static_cast<uint8_t>(bemId));
		inner.push_back(sequenceId);
		inner.push_back(static_cast<uint8_t>(modelId & 0xFF));
		inner.push_back(static_cast<uint8_t>((modelId >> 8) & 0xFF));
		inner.push_back(static_cast<uint8_t>(serialNumber & 0xFF));
		inner.push_back(static_cast<uint8_t>((serialNumber >> 8) & 0xFF));
		inner.push_back(static_cast<uint8_t>((serialNumber >> 16) & 0xFF));
		inner.push_back(static_cast<uint8_t>((serialNumber >> 24) & 0xFF));
		inner.push_back(static_cast<uint8_t>(errorCode & 0xFF));
		inner.push_back(static_cast<uint8_t>((errorCode >> 8) & 0xFF));
		inner.push_back(static_cast<uint8_t>((errorCode >> 16) & 0xFF));
		inner.push_back(static_cast<uint8_t>((errorCode >> 24) & 0xFF));
		inner.insert(inner.end(), bemPayload.begin(), bemPayload.end());

		/* Wrap in Actisense-proprietary PGN 126720 envelope. */
		std::vector<uint8_t> pgnPayload;
		wrapBemInPgn126720(inner, pgnPayload);

		/* BST-93 PC<-Gateway PGN frame for 126720, source = remote device. */
		const BstFrame bst =
			BstFrame::create93(kPgn126720, remoteSourceAddress, /*dest=*/0xFF,
							   pgnPayload, /*timestamp=*/0, /*priority=*/3);
		return bdtpFrame(bst.rawData());
	}
} /* namespace */

/* ----------------------------------------------------------------------- */
/* Test fixture                                                            */
/* ----------------------------------------------------------------------- */

class RemoteBemLoopbackTest : public ::testing::Test
{
protected:
	static constexpr uint8_t kRemoteAddr = 0x42;

	std::unique_ptr<FakeWrappedGateway> transport_owner_;
	FakeWrappedGateway* gateway_ = nullptr;
	std::unique_ptr<SessionImpl> session_;

	void SetUp() override
	{
		auto t = std::make_unique<FakeWrappedGateway>();
		gateway_ = t.get();
		TransportConfig cfg;
		cfg.kind = TransportKind::Loopback;
		t->asyncOpen(cfg, [](ErrorCode ec) { ASSERT_EQ(ec, ErrorCode::Ok); });

		session_ = std::make_unique<SessionImpl>(
			std::move(t),
			[](const EventVariant& /*event*/) {},
			[](ErrorCode /*ec*/, std::string_view /*msg*/) {});
		session_->startReceiving();
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
		}
	}
};

/* ----------------------------------------------------------------------- */
/* Tests                                                                   */
/* ----------------------------------------------------------------------- */

TEST_F(RemoteBemLoopbackTest, GetOperatingMode_RoundTripsThroughPgn126720)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::tuple<ErrorCode, std::string, std::optional<OperatingMode>>>
		promise;
	remote->getOperatingMode(std::chrono::seconds(2),
		[&](ErrorCode ec, std::string_view msg, std::optional<OperatingMode> m) {
			promise.set_value({ec, std::string(msg), m});
		});

	/* Capture the outbound BST-94 PGN 126720 frame. */
	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value()) << "Session did not send anything";

	/* Decode and assert wire format. */
	const auto bstBytes = unframeBdtp(*sent);
	ASSERT_GE(bstBytes.size(), 11u);
	ASSERT_EQ(bstBytes.back(),                                        /* checksum byte */
			  static_cast<uint8_t>(-BdtpProtocol::calculateChecksum(
				  std::span<const uint8_t>(bstBytes.data(), bstBytes.size() - 1))));
	const auto bstNoChecksum =
		std::span<const uint8_t>(bstBytes.data(), bstBytes.size() - 1);
	auto frame = BstFrame::fromRawData(bstNoChecksum);
	ASSERT_TRUE(frame.has_value());
	EXPECT_TRUE(frame->is94());
	EXPECT_EQ(frame->pgn(), kPgn126720);
	EXPECT_EQ(frame->destination(), kRemoteAddr);

	auto pgnPayload = frame->data();
	ASSERT_GE(pgnPayload.size(), 2u);
	EXPECT_EQ(pgnPayload[0], kActisenseManufacturerByte1);
	EXPECT_EQ(pgnPayload[1], kActisenseManufacturerByte2);

	/* Synthesise a remote device reply: OperatingMode = OM_NGTransferNormalMode (1). */
	const uint16_t mode = static_cast<uint16_t>(OperatingMode::OM_NGTransferNormalMode);
	const std::array<uint8_t, 2> bemPayload{
		static_cast<uint8_t>(mode & 0xFF), static_cast<uint8_t>((mode >> 8) & 0xFF)};

	const auto reply = buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetOperatingMode,
		/*seq=*/0, /*modelId=*/static_cast<uint16_t>(ArlModelId::NGX1),
		/*serial=*/12345, /*err=*/0, bemPayload);
	gateway_->injectRx(reply);

	auto fut = promise.get_future();
	ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	auto [ec, errMsg, decoded] = fut.get();
	EXPECT_EQ(ec, ErrorCode::Ok) << errMsg;
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(*decoded, OperatingMode::OM_NGTransferNormalMode);
}

TEST_F(RemoteBemLoopbackTest, SetOperatingMode_AckedReply)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::pair<ErrorCode, std::string>> promise;
	remote->setOperatingMode(
		OperatingMode::OM_NGTransferNormalMode, std::chrono::seconds(2),
		[&](ErrorCode ec, std::string_view msg) {
			promise.set_value({ec, std::string(msg)});
		});

	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value());

	/* Reply with errorCode = 0 (success). No payload bytes. */
	const auto reply = buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetOperatingMode,
		0, 0, 0, 0, {});
	gateway_->injectRx(reply);

	auto fut = promise.get_future();
	ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	auto [ec, errMsg] = fut.get();
	EXPECT_EQ(ec, ErrorCode::Ok) << errMsg;
}

TEST_F(RemoteBemLoopbackTest, DeviceErrorCode_SurfacesAsError)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::tuple<ErrorCode, std::string, std::optional<OperatingMode>>>
		promise;
	remote->getOperatingMode(std::chrono::seconds(2),
		[&](ErrorCode ec, std::string_view msg, std::optional<OperatingMode> m) {
			promise.set_value({ec, std::string(msg), m});
		});

	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value());

	/* Device returns a non-zero ARL error code. */
	const std::array<uint8_t, 2> ignoredPayload{0x00, 0x00};
	const auto reply = buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetOperatingMode,
		0, 0, 0, /*errorCode=*/123, ignoredPayload);
	gateway_->injectRx(reply);

	auto fut = promise.get_future();
	ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	auto [ec, errMsg, decoded] = fut.get();
	/* The correlator promotes any non-zero ARL error code to
	   ErrorCode::UnsupportedOperation (see BemProtocol::correlateResponse).
	   The typed wrapper forwards that error directly. */
	EXPECT_NE(ec, ErrorCode::Ok);
	EXPECT_FALSE(decoded.has_value());
	EXPECT_NE(errMsg.find("123"), std::string::npos)
		<< "Expected error message to mention the ARL code (123); got: " << errMsg;
}

TEST_F(RemoteBemLoopbackTest, ReplyFromWrongSourceAddressDoesNotResolve)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::atomic<bool> fired{false};
	remote->getOperatingMode(std::chrono::milliseconds(400),
		[&](ErrorCode /*ec*/, std::string_view /*msg*/,
			std::optional<OperatingMode> /*m*/) { fired.store(true); });

	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value());

	/* Reply from a *different* remote address (0x43) — must not satisfy the
	   request bound for 0x42. */
	const uint16_t mode = static_cast<uint16_t>(OperatingMode::OM_NGTransferNormalMode);
	const std::array<uint8_t, 2> bemPayload{
		static_cast<uint8_t>(mode & 0xFF), static_cast<uint8_t>((mode >> 8) & 0xFF)};
	const auto wrongSrc = buildWrappedReply(
		/*remoteSrc=*/0x43, BstId::Bem_GP_A0, BemCommandId::GetSetOperatingMode,
		0, 0, 0, 0, bemPayload);
	gateway_->injectRx(wrongSrc);

	/* Give the receive thread a moment to process the wrong-source reply. */
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(fired.load()) << "Callback fired despite mismatched source addr";

	/* Now deliver the correct reply: callback must fire. */
	const auto right = buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetOperatingMode,
		0, 0, 0, 0, bemPayload);
	gateway_->injectRx(right);

	/* Spin briefly for the receive thread to deliver. */
	for (int i = 0; i < 50 && !fired.load(); ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	EXPECT_TRUE(fired.load());
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
