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

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
