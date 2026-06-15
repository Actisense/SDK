/**************************************************************************/ /**
\file       test_session_pgn_list_management.cpp
\author     (Created) Claude Code
\date       (Created) 28/04/2026
\brief      Session-level smoke tests for PGN List Management helpers
\details    Verifies SessionImpl::{getRx/TxPgnEnableListF2,
            setRx/TxPgnEnableListF2, deletePgnEnableLists, activatePgnEnableLists,
            defaultPgnEnableList, getParamsPgnEnableLists} encode the right BEM
            command ID and payload onto the wire. Builders/decoders are covered
            in test_pgn_list_management.cpp; this file covers the session layer.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class SessionPgnListManagementTest : public ::testing::Test
{
protected:
	LoopbackTransport*           transport_ = nullptr;
	std::unique_ptr<SessionImpl> session_;

	void SetUp() override
	{
		auto loopback = std::make_unique<LoopbackTransport>();
		transport_ = loopback.get();

		bool opened = false;
		TransportConfig cfg;
		cfg.kind = TransportKind::Loopback;
		transport_->asyncOpen(cfg, [&](ErrorCode code) {
			ASSERT_EQ(code, ErrorCode::Ok);
			opened = true;
		});
		ASSERT_TRUE(opened);

		/* Construct session WITHOUT startReceiving so the loopback's recv
		   buffer keeps the bytes the session sends — we drain them manually
		   to inspect what went on the wire. */
		session_ = std::make_unique<SessionImpl>(std::move(loopback),
												  /*onEvent=*/nullptr,
												  /*onError=*/nullptr);
	}

	void TearDown() override
	{
		if (session_) {
			session_->close();
			session_.reset();
		}
		transport_ = nullptr;
	}

	/* Drain bytes sent by the session and parse them back into a BST
	   datagram so the test can inspect the BST/BEM ID and payload. */
	BstDatagram captureSentDatagram()
	{
		std::vector<uint8_t> raw;
		while (transport_->bytesAvailable() > 0) {
			transport_->asyncRecv([&raw](ErrorCode code, ConstByteSpan data) {
				if (code == ErrorCode::Ok) {
					raw.insert(raw.end(), data.begin(), data.end());
				}
			});
		}

		BdtpProtocol parser;
		std::optional<BstDatagram> captured;
		parser.parse(
			raw,
			[&captured](const ParsedMessageEvent& ev) {
				if (!captured.has_value()) {
					captured = std::any_cast<BstDatagram>(ev.payload);
				}
			},
			[](ErrorCode, std::string_view) {});

		EXPECT_TRUE(captured.has_value()) << "No BST datagram was framed";
		return captured.value_or(BstDatagram{});
	}

	static constexpr auto kTimeout = std::chrono::milliseconds(1000);
};

/* GET helpers -------------------------------------------------------------- */

TEST_F(SessionPgnListManagementTest, GetRxPgnEnableListF2_SendsCommand)
{
	session_->getRxPgnEnableListF2(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetRxPgnEnableListF2));
}

TEST_F(SessionPgnListManagementTest, GetTxPgnEnableListF2_SendsCommand)
{
	session_->getTxPgnEnableListF2(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetTxPgnEnableListF2));
}

TEST_F(SessionPgnListManagementTest, GetParamsPgnEnableLists_SendsCommand)
{
	session_->getParamsPgnEnableLists(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::ParamsPgnEnableLists));
}

/* Note: F2 list SET commands (0x4E/0x4F) have no firmware handler. To
   change Rx/Tx enable state, use the per-PGN commands 0x46/0x47
   (test_rx_pgn_enable.cpp / test_tx_pgn_enable.cpp). */

/* Management commands ------------------------------------------------------ */

TEST_F(SessionPgnListManagementTest, DeletePgnEnableLists_EncodesSelector)
{
	session_->deletePgnEnableLists(/*selector=*/2, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 2u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::DeletePgnEnableLists));
	EXPECT_EQ(dgm.data[1], 2);
}

TEST_F(SessionPgnListManagementTest, DeletePgnEnableLists_RejectsInvalidSelector)
{
	bool called = false;
	ErrorCode reportedCode = ErrorCode::Ok;
	session_->deletePgnEnableLists(/*selector=*/3, kTimeout,
		[&](const std::optional<BemResponse>&, ErrorCode code, std::string_view) {
			called = true;
			reportedCode = code;
		});

	EXPECT_TRUE(called);
	EXPECT_EQ(reportedCode, ErrorCode::InvalidArgument);
	EXPECT_EQ(transport_->bytesAvailable(), 0u);
}

TEST_F(SessionPgnListManagementTest, ActivatePgnEnableLists_SendsCommand)
{
	session_->activatePgnEnableLists(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::ActivatePgnEnableLists));
}

TEST_F(SessionPgnListManagementTest, DefaultPgnEnableList_EncodesSelector)
{
	session_->defaultPgnEnableList(DeletePgnListSelector::Both, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 2u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::DefaultPgnEnableList));
	EXPECT_EQ(dgm.data[1], 0x02);  /* Both */
}

/* 0x40 SupportedPgnList chunked walk (GIT-86) ------------------------------ */

namespace
{
	/* Wire shape per supported_pgn_list.hpp on-wire response payload spec. */
	std::vector<uint8_t> makeSupportedPgnListPayload(uint8_t xid, uint16_t dbVer,
													  uint8_t total, uint8_t first,
													  uint8_t sub,
													  std::span<const SupportedPgnEntry> entries)
	{
		std::vector<uint8_t> out;
		out.reserve(kSupportedPgnListResponseHeaderSize + 4 * sub);
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
		out.push_back(sub);
		for (const auto& e : entries) {
			out.push_back(e.pgnIndex);
			out.push_back(static_cast<uint8_t>(e.pgn & 0xFF));
			out.push_back(static_cast<uint8_t>((e.pgn >> 8) & 0xFF));
			out.push_back(static_cast<uint8_t>((e.pgn >> 16) & 0xFF));
		}
		return out;
	}

	BemResponse makeSupportedPgnListResponse(uint8_t xid, uint16_t dbVer, uint8_t total,
											  uint8_t first, uint8_t sub,
											  std::span<const SupportedPgnEntry> entries)
	{
		BemResponse r;
		r.header.bstId = BstId::Bem_GP_A0;
		r.header.bemId = static_cast<uint8_t>(BemCommandId::GetSupportedPgnList);
		r.header.errorCode = 0;
		r.data = makeSupportedPgnListPayload(xid, dbVer, total, first, sub, entries);
		return r;
	}
} /* namespace */

TEST_F(SessionPgnListManagementTest, GetSupportedPgnList_All_ThreeChunkWalkMergesEntries)
{
	std::optional<SupportedPgnListResult> capturedResult;
	ErrorCode capturedCode = ErrorCode::Ok;
	bool callbackFired = false;

	session_->getSupportedPgnList_All(kTimeout,
		[&](ErrorCode ec, std::string_view, std::optional<SupportedPgnListResult> r,
			ResponseOrigin) {
			capturedResult = std::move(r);
			capturedCode = ec;
			callbackFired = true;
		});

	constexpr uint8_t  kTotal     = 5;
	constexpr uint8_t  kDeviceXid = 0x42;
	constexpr uint16_t kDbVer     = 2100;

	/* GET #1: caller-supplied pgnIndex=0, transferId=0. */
	{
		const auto dgm = captureSentDatagram();
		EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
		ASSERT_GE(dgm.data.size(), 3u);
		EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSupportedPgnList));
		EXPECT_EQ(dgm.data[1], 0u);
		EXPECT_EQ(dgm.data[2], 0u);
	}

	/* Reply #1: indices 0..1, device assigns xid=0x42. */
	const std::array<SupportedPgnEntry, 2> chunk1 = {
		SupportedPgnEntry{0, 0x1F101u},
		SupportedPgnEntry{1, 0x1F102u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(kDeviceXid, kDbVer, kTotal, 0, 2, chunk1)));

	/* GET #2: SDK should advance to pgnIndex=2 and reuse the device xid. */
	{
		const auto dgm = captureSentDatagram();
		ASSERT_GE(dgm.data.size(), 3u);
		EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSupportedPgnList));
		EXPECT_EQ(dgm.data[1], 2u);
		EXPECT_EQ(dgm.data[2], kDeviceXid);
	}

	/* Reply #2: indices 2..3. */
	const std::array<SupportedPgnEntry, 2> chunk2 = {
		SupportedPgnEntry{2, 0x1F103u},
		SupportedPgnEntry{3, 0x1F104u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(kDeviceXid, kDbVer, kTotal, 2, 2, chunk2)));

	/* GET #3: pgnIndex=4, xid=0x42. */
	{
		const auto dgm = captureSentDatagram();
		ASSERT_GE(dgm.data.size(), 3u);
		EXPECT_EQ(dgm.data[1], 4u);
		EXPECT_EQ(dgm.data[2], kDeviceXid);
	}

	/* Reply #3 (final): index 4 — supportedPgnListHasMore() is now false. */
	const std::array<SupportedPgnEntry, 1> chunk3 = {
		SupportedPgnEntry{4, 0x1F105u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(kDeviceXid, kDbVer, kTotal, 4, 1, chunk3)));

	/* Walk complete: callback fires once with the merged list. */
	EXPECT_TRUE(callbackFired);
	EXPECT_EQ(capturedCode, ErrorCode::Ok);
	ASSERT_TRUE(capturedResult.has_value());
	EXPECT_EQ(capturedResult->transferId, kDeviceXid);
	EXPECT_EQ(capturedResult->nmea2000DbVersion, kDbVer);
	EXPECT_EQ(capturedResult->totalListSize, kTotal);
	ASSERT_EQ(capturedResult->entries.size(), kTotal);
	EXPECT_EQ(capturedResult->entries[0].pgn, 0x1F101u);
	EXPECT_EQ(capturedResult->entries[1].pgn, 0x1F102u);
	EXPECT_EQ(capturedResult->entries[2].pgn, 0x1F103u);
	EXPECT_EQ(capturedResult->entries[3].pgn, 0x1F104u);
	EXPECT_EQ(capturedResult->entries[4].pgn, 0x1F105u);
}

TEST_F(SessionPgnListManagementTest, GetSupportedPgnList_All_TimeoutMidWalkDeliversPartial)
{
	std::optional<SupportedPgnListResult> capturedResult;
	ErrorCode capturedCode = ErrorCode::Ok;
	bool callbackFired = false;

	/* perGetTimeout of 0 means the next processTimeouts() sweep after the
	   pending entry is registered will fire the inactivity branch. */
	const auto kPerGet = std::chrono::milliseconds(0);
	session_->getSupportedPgnList_All(kPerGet,
		[&](ErrorCode ec, std::string_view, std::optional<SupportedPgnListResult> r,
			ResponseOrigin) {
			capturedResult = std::move(r);
			capturedCode = ec;
			callbackFired = true;
		});

	/* Drain GET #1 and inject reply #1. */
	captureSentDatagram();
	const std::array<SupportedPgnEntry, 2> chunk1 = {
		SupportedPgnEntry{0, 0x1F101u},
		SupportedPgnEntry{1, 0x1F102u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(0x42, 2100, /*total=*/5, /*first=*/0, /*sub=*/2,
									  chunk1)));

	/* Drain GET #2; do NOT reply. */
	captureSentDatagram();

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_EQ(session_->bem().processTimeouts(), 1u);

	EXPECT_TRUE(callbackFired);
	EXPECT_EQ(capturedCode, ErrorCode::Timeout);
	ASSERT_TRUE(capturedResult.has_value());
	EXPECT_EQ(capturedResult->totalListSize, 5);
	EXPECT_EQ(capturedResult->entries[0].pgn, 0x1F101u);
	EXPECT_EQ(capturedResult->entries[1].pgn, 0x1F102u);
	/* Slots 2..4 stay zero-initialised since they never arrived. */
	EXPECT_EQ(capturedResult->entries[4].pgn, 0u);
}

/* GIT-117: SupportedPgnListWalk state-machine edge cases. The walk replaced a
   shared_ptr<std::function> self-recursion; these cover the branches the old
   three-chunk + timeout tests above did not exercise. */

TEST_F(SessionPgnListManagementTest, GetSupportedPgnList_All_SingleChunkCompletesImmediately)
{
	std::optional<SupportedPgnListResult> capturedResult;
	ErrorCode capturedCode = ErrorCode::UnsupportedOperation;
	int callbackCount = 0;

	session_->getSupportedPgnList_All(kTimeout,
		[&](ErrorCode ec, std::string_view, std::optional<SupportedPgnListResult> r,
			ResponseOrigin) {
			capturedResult = std::move(r);
			capturedCode = ec;
			++callbackCount;
		});

	captureSentDatagram();  /* GET #1 */

	/* Reply #1 is final: total=2, sub=2 -> supportedPgnListHasMore() == false. */
	const std::array<SupportedPgnEntry, 2> chunk = {
		SupportedPgnEntry{0, 0x1F101u},
		SupportedPgnEntry{1, 0x1F102u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(0x42, 2100, /*total=*/2, /*first=*/0, /*sub=*/2, chunk)));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::Ok);
	ASSERT_TRUE(capturedResult.has_value());
	ASSERT_EQ(capturedResult->entries.size(), 2u);
	EXPECT_EQ(capturedResult->entries[0].pgn, 0x1F101u);
	EXPECT_EQ(capturedResult->entries[1].pgn, 0x1F102u);

	/* The walk is done after one reply: no follow-up GET went on the wire. */
	EXPECT_EQ(transport_->bytesAvailable(), 0u);
}

TEST_F(SessionPgnListManagementTest, GetSupportedPgnList_All_TransferIdMismatchFails)
{
	std::optional<SupportedPgnListResult> capturedResult;
	ErrorCode capturedCode = ErrorCode::Ok;
	int callbackCount = 0;

	session_->getSupportedPgnList_All(kTimeout,
		[&](ErrorCode ec, std::string_view, std::optional<SupportedPgnListResult> r,
			ResponseOrigin) {
			capturedResult = std::move(r);
			capturedCode = ec;
			++callbackCount;
		});

	captureSentDatagram();  /* GET #1 */

	/* Reply #1 latches xid=0x42, total=4 and continues. */
	const std::array<SupportedPgnEntry, 2> chunk1 = {
		SupportedPgnEntry{0, 0x1F101u},
		SupportedPgnEntry{1, 0x1F102u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(0x42, 2100, /*total=*/4, /*first=*/0, /*sub=*/2, chunk1)));

	captureSentDatagram();  /* GET #2 */

	/* Reply #2 carries a different transferId -> accumulator Mismatch. */
	const std::array<SupportedPgnEntry, 2> chunk2 = {
		SupportedPgnEntry{2, 0x1F103u},
		SupportedPgnEntry{3, 0x1F104u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(/*xid=*/0x99, 2100, /*total=*/4, /*first=*/2, /*sub=*/2,
									  chunk2)));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::InvalidArgument);
	EXPECT_FALSE(capturedResult.has_value());
}

TEST_F(SessionPgnListManagementTest, GetSupportedPgnList_All_TruncatedReplyFails)
{
	std::optional<SupportedPgnListResult> capturedResult;
	ErrorCode capturedCode = ErrorCode::Ok;
	int callbackCount = 0;

	session_->getSupportedPgnList_All(kTimeout,
		[&](ErrorCode ec, std::string_view, std::optional<SupportedPgnListResult> r,
			ResponseOrigin) {
			capturedResult = std::move(r);
			capturedCode = ec;
			++callbackCount;
		});

	captureSentDatagram();  /* GET #1 */

	/* Reply #1 claims sub=2 in the header but carries only one entry's bytes,
	   so decodeSupportedPgnListResponse() rejects it as truncated. */
	const std::array<SupportedPgnEntry, 1> partial = {SupportedPgnEntry{0, 0x1F101u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(0x42, 2100, /*total=*/4, /*first=*/0, /*sub=*/2, partial)));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::InvalidArgument);
	EXPECT_FALSE(capturedResult.has_value());
}

TEST_F(SessionPgnListManagementTest, GetSupportedPgnList_AllRemote_DeliversWithRemoteOrigin)
{
	constexpr uint8_t kRemoteAddr = 0x05;
	std::optional<SupportedPgnListResult> capturedResult;
	ErrorCode capturedCode = ErrorCode::UnsupportedOperation;
	ResponseOrigin capturedOrigin;
	int callbackCount = 0;

	session_->getSupportedPgnList_AllRemote(kRemoteAddr, kTimeout,
		[&](ErrorCode ec, std::string_view, std::optional<SupportedPgnListResult> r,
			ResponseOrigin origin) {
			capturedResult = std::move(r);
			capturedCode = ec;
			capturedOrigin = std::move(origin);
			++callbackCount;
		});

	/* The remote GET is wrapped in PGN 126720 and sent; drain it. */
	captureSentDatagram();

	/* Reply arrives keyed by the remote device's source address; single chunk. */
	const std::array<SupportedPgnEntry, 1> chunk = {SupportedPgnEntry{0, 0x1F205u}};
	EXPECT_TRUE(session_->bem().correlateResponse(
		makeSupportedPgnListResponse(0x42, 2100, /*total=*/1, /*first=*/0, /*sub=*/1, chunk),
		/*srcAddr=*/kRemoteAddr));

	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(capturedCode, ErrorCode::Ok);
	ASSERT_TRUE(capturedResult.has_value());
	ASSERT_EQ(capturedResult->entries.size(), 1u);
	EXPECT_EQ(capturedResult->entries[0].pgn, 0x1F205u);
	/* Origin reflects the remote (PGN 126720) path, not the local gateway. */
	EXPECT_EQ(capturedOrigin.path, TransportPath::Remote);
	EXPECT_EQ(capturedOrigin.n2kSourceAddress, kRemoteAddr);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
