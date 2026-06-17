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
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
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
		[&](ErrorCode ec, std::string_view msg, std::optional<OperatingMode> m,
		    ResponseOrigin) {
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

	/* Synthesise a remote device reply: OperatingMode = NgTransferNormalMode (1). */
	const uint16_t mode = static_cast<uint16_t>(OperatingMode::NgTransferNormalMode);
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
	EXPECT_EQ(*decoded, OperatingMode::NgTransferNormalMode);
}

TEST_F(RemoteBemLoopbackTest, SetOperatingMode_AckedReply)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::pair<ErrorCode, std::string>> promise;
	remote->setOperatingMode(
		OperatingMode::NgTransferNormalMode, std::chrono::seconds(2),
		[&](ErrorCode ec, std::string_view msg, ResponseOrigin) {
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
		[&](ErrorCode ec, std::string_view msg, std::optional<OperatingMode> m,
		    ResponseOrigin) {
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
	/* The correlator surfaces any non-zero ARL error code as
	   ErrorCode::BemDeviceError, with the raw code in the message (GIT-127;
	   was the catch-all UnsupportedOperation). The typed wrapper forwards that
	   error directly. */
	EXPECT_EQ(ec, ErrorCode::BemDeviceError)
		<< "got " << static_cast<int>(ec) << " (" << errMsg << ")";
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
			std::optional<OperatingMode> /*m*/, ResponseOrigin /*origin*/) { fired.store(true); });

	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value());

	/* Reply from a *different* remote address (0x43) — must not satisfy the
	   request bound for 0x42. */
	const uint16_t mode = static_cast<uint16_t>(OperatingMode::NgTransferNormalMode);
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

/* ----------------------------------------------------------------------- */
/* GIT-90: Aggregated PGN-list verbs over PGN 126720                       */
/* ----------------------------------------------------------------------- */

namespace
{
	/* Build the post-BEM-header payload for one Rx F2 (0x4E) reply message. */
	std::vector<uint8_t> rxF2Payload(uint8_t xid, uint8_t total, uint8_t first,
									  std::span<const RxPgnEnableEntry> entries)
	{
		std::vector<uint8_t> out;
		out.reserve(kRxPgnEnableListF2ResponseHeaderSize + 2 * entries.size());
		out.push_back(xid);
		/* SVID 0x00001101 LE */
		out.push_back(0x01);
		out.push_back(0x11);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(total);
		out.push_back(first);
		out.push_back(static_cast<uint8_t>(entries.size()));
		for (const auto& e : entries) {
			out.push_back(e.pgnIndex);
			out.push_back(e.rxMask);
		}
		return out;
	}

	/* Build the post-BEM-header payload for a Tx F2 (0x4F) standard-variant
	   reply message. */
	std::vector<uint8_t> txF2StdPayload(uint8_t xid, uint8_t total, uint8_t first,
										 std::span<const TxPgnEnableEntry> entries)
	{
		std::vector<uint8_t> out;
		out.reserve(kTxPgnEnableListF2StdHeaderSize + 4 * entries.size());
		out.push_back(xid);
		/* SVID 0x00001102 LE */
		out.push_back(0x02);
		out.push_back(0x11);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(total);
		out.push_back(first);
		out.push_back(static_cast<uint8_t>(entries.size()));
		for (const auto& e : entries) {
			out.push_back(e.pgnIndex);
			out.push_back(e.priority);
			out.push_back(static_cast<uint8_t>(e.rateMs & 0xFF));
			out.push_back(static_cast<uint8_t>((e.rateMs >> 8) & 0xFF));
		}
		return out;
	}

	/* Build the post-BEM-header payload for a Tx F2 (0x4F) proprietary-variant
	   reply message — the trailing message that signals end-of-train. */
	std::vector<uint8_t> txF2PropPayload(uint8_t xid, std::span<const uint8_t> dp0,
										  std::span<const uint8_t> dp1)
	{
		std::vector<uint8_t> out;
		out.reserve(kTxPgnEnableListF2PropHeaderSize + 2 + dp0.size() + dp1.size());
		out.push_back(xid);
		/* SVID 0x00001103 LE */
		out.push_back(0x03);
		out.push_back(0x11);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(static_cast<uint8_t>(dp0.size()));
		out.insert(out.end(), dp0.begin(), dp0.end());
		out.push_back(static_cast<uint8_t>(dp1.size()));
		out.insert(out.end(), dp1.begin(), dp1.end());
		return out;
	}

	/* Build the post-BEM-header payload for one 0x40 SupportedPgnList reply. */
	std::vector<uint8_t> supportedPgnListPayload(uint8_t xid, uint16_t dbVer,
												  uint8_t total, uint8_t first,
												  std::span<const SupportedPgnEntry> entries)
	{
		std::vector<uint8_t> out;
		out.reserve(kSupportedPgnListResponseHeaderSize + 4 * entries.size());
		out.push_back(xid);
		/* SVID 0x00001100 LE */
		out.push_back(0x00);
		out.push_back(0x11);
		out.push_back(0x00);
		out.push_back(0x00);
		out.push_back(static_cast<uint8_t>(dbVer & 0xFF));
		out.push_back(static_cast<uint8_t>((dbVer >> 8) & 0xFF));
		out.push_back(total);
		out.push_back(first);
		out.push_back(static_cast<uint8_t>(entries.size()));
		for (const auto& e : entries) {
			out.push_back(e.pgnIndex);
			out.push_back(static_cast<uint8_t>(e.pgn & 0xFF));
			out.push_back(static_cast<uint8_t>((e.pgn >> 8) & 0xFF));
			out.push_back(static_cast<uint8_t>((e.pgn >> 16) & 0xFF));
		}
		return out;
	}
} /* namespace */

TEST_F(RemoteBemLoopbackTest, GetRxPgnEnableListF2_AggregatesMultiReplyTrain)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::tuple<ErrorCode, std::optional<RxPgnEnableListF2Result>>> promise;
	remote->getRxPgnEnableListF2(std::chrono::seconds(2),
		[&](ErrorCode ec, std::string_view, std::optional<RxPgnEnableListF2Result> r,
		    ResponseOrigin) {
			promise.set_value({ec, std::move(r)});
		});

	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value()) << "Session did not send anything";

	constexpr uint8_t kXid   = 0x77;
	constexpr uint8_t kTotal = 5;

	/* Reply #1: indices 0..1 */
	const std::array<RxPgnEnableEntry, 2> chunk1 = {
		RxPgnEnableEntry{0, kRxPgnMaskEnabled},
		RxPgnEnableEntry{1, kRxPgnMaskDisabled}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetRxPgnEnableListF2,
		0, 0, 0, 0, rxF2Payload(kXid, kTotal, 0, chunk1)));

	/* Reply #2: indices 2..3 */
	const std::array<RxPgnEnableEntry, 2> chunk2 = {
		RxPgnEnableEntry{2, kRxPgnMaskEnabled},
		RxPgnEnableEntry{3, kRxPgnMaskEnabled}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetRxPgnEnableListF2,
		0, 0, 0, 0, rxF2Payload(kXid, kTotal, 2, chunk2)));

	/* Reply #3: index 4 — sub-count sum reaches totalListSize, signals Done. */
	const std::array<RxPgnEnableEntry, 1> chunk3 = {
		RxPgnEnableEntry{4, kRxPgnMaskDisabled}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetRxPgnEnableListF2,
		0, 0, 0, 0, rxF2Payload(kXid, kTotal, 4, chunk3)));

	auto fut = promise.get_future();
	ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	auto [ec, result] = fut.get();
	EXPECT_EQ(ec, ErrorCode::Ok);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->transferId, kXid);
	EXPECT_EQ(result->totalListSize, kTotal);
	ASSERT_EQ(result->entries.size(), kTotal);
	EXPECT_EQ(result->entries[0].rxMask, kRxPgnMaskEnabled);
	EXPECT_EQ(result->entries[3].rxMask, kRxPgnMaskEnabled);
	EXPECT_EQ(result->entries[4].rxMask, kRxPgnMaskDisabled);
}

TEST_F(RemoteBemLoopbackTest, GetTxPgnEnableListF2_TerminatesOnProprietaryMessage)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::tuple<ErrorCode, std::optional<TxPgnEnableListF2Result>>> promise;
	remote->getTxPgnEnableListF2(std::chrono::seconds(2),
		[&](ErrorCode ec, std::string_view, std::optional<TxPgnEnableListF2Result> r,
		    ResponseOrigin) {
			promise.set_value({ec, std::move(r)});
		});

	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value());

	constexpr uint8_t kXid      = 0x33;
	constexpr uint8_t kStdTotal = 2;
	/* Simulate an NGX-1 responder so the SDK accumulator's model gate
	   (supportsProprietaryEnableListF2) expects the proprietary trailer. */
	constexpr uint16_t kModelId = static_cast<uint16_t>(ArlModelId::NGX1);

	/* Std-variant reply #1: index 0 with one entry. */
	const std::array<TxPgnEnableEntry, 1> std1 = {
		TxPgnEnableEntry{0, /*priority=*/3, /*rateMs=*/100}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetTxPgnEnableListF2,
		0, kModelId, 0, 0, txF2StdPayload(kXid, kStdTotal, 0, std1)));

	/* Std-variant reply #2: index 1 with one entry. */
	const std::array<TxPgnEnableEntry, 1> std2 = {
		TxPgnEnableEntry{1, /*priority=*/6, /*rateMs=*/500}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetTxPgnEnableListF2,
		0, kModelId, 0, 0, txF2StdPayload(kXid, kStdTotal, 1, std2)));

	/* Proprietary-variant trailer: one bit set in DP0 byte 0 (PGN 0xFF00). */
	const std::array<uint8_t, 1> dp0{0x01};
	const std::array<uint8_t, 0> dp1{};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetTxPgnEnableListF2,
		/*seq=*/2, kModelId, 0, 0, txF2PropPayload(kXid, dp0, dp1)));

	auto fut = promise.get_future();
	ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	auto [ec, result] = fut.get();
	EXPECT_EQ(ec, ErrorCode::Ok);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->transferId, kXid);
	EXPECT_EQ(result->totalListSize, kStdTotal);
	ASSERT_EQ(result->entries.size(), kStdTotal);
	EXPECT_EQ(result->entries[0].rateMs, 100u);
	EXPECT_EQ(result->entries[1].rateMs, 500u);
	EXPECT_TRUE(result->proprietaryReceived);
	ASSERT_EQ(result->proprietary.enabledPgns.size(), 1u);
	EXPECT_EQ(result->proprietary.enabledPgns[0], kTxPgnPropDp0Base);
}

TEST_F(RemoteBemLoopbackTest, GetSupportedPgnList_All_WalksChunksOverWrappedRoundTrip)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::tuple<ErrorCode, std::optional<SupportedPgnListResult>>> promise;
	remote->getSupportedPgnList_All(std::chrono::seconds(2),
		[&](ErrorCode ec, std::string_view, std::optional<SupportedPgnListResult> r,
		    ResponseOrigin) {
			promise.set_value({ec, std::move(r)});
		});

	constexpr uint8_t  kXid     = 0x55;
	constexpr uint8_t  kTotal   = 3;
	constexpr uint16_t kDbVer   = 2100;

	/* GET #1 should be wrapped to kRemoteAddr. */
	auto get1 = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(get1.has_value());

	/* Reply #1: indices 0..1. */
	const std::array<SupportedPgnEntry, 2> chunk1 = {
		SupportedPgnEntry{0, 0x1F101u},
		SupportedPgnEntry{1, 0x1F102u}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSupportedPgnList,
		0, 0, 0, 0, supportedPgnListPayload(kXid, kDbVer, kTotal, 0, chunk1)));

	/* SDK should now send GET #2 (wrapped + addressed to kRemoteAddr). */
	auto get2 = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(get2.has_value()) << "SDK did not issue follow-up GET for remote walk";

	/* Reply #2 (final): index 2. */
	const std::array<SupportedPgnEntry, 1> chunk2 = {
		SupportedPgnEntry{2, 0x1F103u}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSupportedPgnList,
		0, 0, 0, 0, supportedPgnListPayload(kXid, kDbVer, kTotal, 2, chunk2)));

	auto fut = promise.get_future();
	ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
	auto [ec, result] = fut.get();
	EXPECT_EQ(ec, ErrorCode::Ok);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->transferId, kXid);
	EXPECT_EQ(result->totalListSize, kTotal);
	ASSERT_EQ(result->entries.size(), kTotal);
	EXPECT_EQ(result->entries[0].pgn, 0x1F101u);
	EXPECT_EQ(result->entries[2].pgn, 0x1F103u);
}

TEST_F(RemoteBemLoopbackTest, GetRxPgnEnableListF2_TimeoutDeliversPartialResult)
{
	auto remote = session_->openRemote(kRemoteAddr);

	std::promise<std::tuple<ErrorCode, std::optional<RxPgnEnableListF2Result>>> promise;
	remote->getRxPgnEnableListF2(std::chrono::milliseconds(100),
		[&](ErrorCode ec, std::string_view, std::optional<RxPgnEnableListF2Result> r,
		    ResponseOrigin) {
			promise.set_value({ec, std::move(r)});
		});

	auto sent = gateway_->waitForSent(std::chrono::seconds(1));
	ASSERT_TRUE(sent.has_value());

	/* Inject only the first sub-list; nothing more arrives within the
	   100ms inactivity window. */
	constexpr uint8_t kXid   = 0x99;
	constexpr uint8_t kTotal = 5;
	const std::array<RxPgnEnableEntry, 2> chunk1 = {
		RxPgnEnableEntry{0, kRxPgnMaskEnabled},
		RxPgnEnableEntry{1, kRxPgnMaskEnabled}};
	gateway_->injectRx(buildWrappedReply(
		kRemoteAddr, BstId::Bem_GP_A0, BemCommandId::GetSetRxPgnEnableListF2,
		0, 0, 0, 0, rxF2Payload(kXid, kTotal, 0, chunk1)));

	/* The receive thread fires processTimeouts on its tick — the inactivity
	   window expiry should produce a partial result. Allow generous slack to
	   absorb scheduler jitter on CI. */
	auto fut = promise.get_future();
	ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
	auto [ec, result] = fut.get();
	EXPECT_EQ(ec, ErrorCode::Timeout);
	ASSERT_TRUE(result.has_value()) << "Expected partial result on inactivity timeout";
	EXPECT_EQ(result->transferId, kXid);
	EXPECT_EQ(result->totalListSize, kTotal);
	EXPECT_EQ(result->entries[0].rxMask, kRxPgnMaskEnabled);
	EXPECT_EQ(result->entries[1].rxMask, kRxPgnMaskEnabled);
	/* Slots 2..4 are zero-initialised; never received. */
	EXPECT_EQ(result->entries[4].rxMask, 0);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
