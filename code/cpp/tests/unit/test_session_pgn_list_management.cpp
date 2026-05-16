/**************************************************************************/ /**
\file       test_session_pgn_list_management.cpp
\author     (Created) Claude Code
\date       (Created) 28/04/2026
\brief      Session-level smoke tests for PGN List Management helpers
\details    Verifies SessionImpl::{getRx/TxPgnEnableListF1, getRx/TxPgnEnableListF2,
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
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
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

TEST_F(SessionPgnListManagementTest, GetRxPgnEnableListF1_SendsCommand)
{
	session_->getRxPgnEnableListF1(/*messageIndex=*/1, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 2u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetRxPgnEnableListF1));
	EXPECT_EQ(dgm.data[1], 1);
}

TEST_F(SessionPgnListManagementTest, GetRxPgnEnableListF1_RejectsInvalidIndex)
{
	bool called = false;
	ErrorCode reportedCode = ErrorCode::Ok;
	session_->getRxPgnEnableListF1(/*messageIndex=*/2, kTimeout,
		[&](const std::optional<BemResponse>& resp, ErrorCode code, std::string_view) {
			called = true;
			reportedCode = code;
			EXPECT_FALSE(resp.has_value());
		});

	EXPECT_TRUE(called);
	EXPECT_EQ(reportedCode, ErrorCode::InvalidArgument);
	EXPECT_EQ(transport_->bytesAvailable(), 0u);
}

TEST_F(SessionPgnListManagementTest, GetTxPgnEnableListF1_SendsCommand)
{
	session_->getTxPgnEnableListF1(/*messageIndex=*/3, kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 2u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetTxPgnEnableListF1));
	EXPECT_EQ(dgm.data[1], 3);
}

TEST_F(SessionPgnListManagementTest, GetTxPgnEnableListF1_RejectsInvalidIndex)
{
	bool called = false;
	ErrorCode reportedCode = ErrorCode::Ok;
	session_->getTxPgnEnableListF1(/*messageIndex=*/4, kTimeout,
		[&](const std::optional<BemResponse>&, ErrorCode code, std::string_view) {
			called = true;
			reportedCode = code;
		});

	EXPECT_TRUE(called);
	EXPECT_EQ(reportedCode, ErrorCode::InvalidArgument);
	EXPECT_EQ(transport_->bytesAvailable(), 0u);
}

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

/* SET helpers -------------------------------------------------------------- */

TEST_F(SessionPgnListManagementTest, SetRxPgnEnableListF2_EncodesSubList)
{
	const std::vector<RxPgnEnableEntry> entries = {
		{0x00, kRxPgnMaskEnabled},
		{0x05, kRxPgnMaskEnabled},
		{0x14, kRxPgnMaskDisabled},
	};
	session_->setRxPgnEnableListF2(/*xid=*/1, /*total=*/3, /*firstIdx=*/0, entries,
								   kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	/* Header: BEM ID + xid(1) + SVID(4) + total(1) + first(1) + sub(1) = 9 bytes,
	   then 3 entries × 2 bytes = 6, total 15. */
	ASSERT_EQ(dgm.data.size(), 1u + kRxPgnEnableListF2ResponseHeaderSize + 3 * 2);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetRxPgnEnableListF2));
	EXPECT_EQ(dgm.data[1], 1u);             /* xid */
	EXPECT_EQ(dgm.data[2], 0x01);           /* SVID LE byte 0 */
	EXPECT_EQ(dgm.data[6], 3u);             /* total */
	EXPECT_EQ(dgm.data[7], 0u);             /* firstIdx */
	EXPECT_EQ(dgm.data[8], 3u);             /* subCount */
	EXPECT_EQ(dgm.data[9], 0x00);           /* first entry pgnIdx */
	EXPECT_EQ(dgm.data[10], kRxPgnMaskEnabled);
}

TEST_F(SessionPgnListManagementTest, SetTxPgnEnableListF2_EncodesStdEntries)
{
	std::vector<TxPgnEnableEntry> entries;
	entries.push_back({/*pgnIndex=*/0x05, /*priority=*/3, /*rateMs=*/100});

	session_->setTxPgnEnableListF2(/*xid=*/1, /*total=*/1, /*firstIdx=*/0, entries,
								   kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	/* BEM ID + 8-byte header + 4-byte entry = 13 bytes. */
	ASSERT_EQ(dgm.data.size(), 1u + kTxPgnEnableListF2StdHeaderSize +
								   kTxPgnEnableListF2StdEntrySize);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::GetSetTxPgnEnableListF2));
	EXPECT_EQ(dgm.data[1], 1u);   /* xid */
	EXPECT_EQ(dgm.data[2], 0x02); /* SVID LE byte 0 — Std variant */
	EXPECT_EQ(dgm.data[3], 0x11);
	EXPECT_EQ(dgm.data[6], 1u);   /* total */
	EXPECT_EQ(dgm.data[7], 0u);   /* firstIdx */
	EXPECT_EQ(dgm.data[8], 1u);   /* subCount */

	/* Entry layout: pgnIndex(1) + priority(1) + rateMs(2 LE). */
	EXPECT_EQ(dgm.data[9], 0x05);  /* pgnIndex */
	EXPECT_EQ(dgm.data[10], 3);    /* priority */
	EXPECT_EQ(dgm.data[11], 100);  /* rate LE lo (100) */
	EXPECT_EQ(dgm.data[12], 0);    /* rate LE hi */
}

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

TEST_F(SessionPgnListManagementTest, DefaultPgnEnableList_SendsCommand)
{
	session_->defaultPgnEnableList(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));
	ASSERT_GE(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::DefaultPgnEnableList));
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
