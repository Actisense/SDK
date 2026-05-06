/**************************************************************************/ /**
\file       test_session_send_pgn.cpp
\brief      Session-level test for SessionImpl::sendPgn on-the-wire framing
\details    Regression test for GIT-81 follow-up: verifies that bytes
            produced by SessionImpl::sendPgn carry a valid zero-sum BST
            checksum. The earlier code path framed the BST raw bytes via
            BdtpProtocol::encodeFrame without first appending the checksum,
            so the receiving device's BDTP parser silently dropped every
            outbound PGN. This test rebuilds a SessionImpl over the
            loopback transport, calls sendPgn, then re-parses the bytes
            with BdtpProtocol — which validates the checksum and only
            emits the datagram when the frame is well-formed.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bst/bst_frame.hpp"
#include "protocols/bst/bst_types.hpp"
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>

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

class SessionSendPgnTest : public ::testing::Test
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

		EXPECT_TRUE(captured.has_value())
			<< "BdtpProtocol parser rejected the framed bytes — likely missing or invalid checksum";
		return captured.value_or(BstDatagram{});
	}
};

TEST_F(SessionSendPgnTest, SendPgn_FramesWithValidChecksum)
{
	/* PGN 127250 (Vessel Heading), broadcast, default priority. The exact
	   payload value is not important for this test — we only care that the
	   on-wire bytes carry a valid BST checksum so the parser accepts them. */
	const std::vector<uint8_t> payload{0x00, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};

	session_->sendPgn(127250, payload, /*destination=*/0xFF, /*priority=*/6, /*completion=*/{});

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Nmea2000_PCToGateway));
}

TEST_F(SessionSendPgnTest, SendPgn_RoundTripsPgnAndPayload)
{
	const std::vector<uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};

	session_->sendPgn(127250, payload, /*destination=*/0xFF, /*priority=*/6, /*completion=*/{});

	const auto dgm = captureSentDatagram();
	ASSERT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Nmea2000_PCToGateway));

	/* Reconstruct a BstFrame from the parsed datagram so we can decode the
	   N2K header without re-implementing the BST-94 layout here. The
	   BstFrame constructor wants the leading ID + length + payload bytes. */
	std::vector<uint8_t> raw;
	raw.reserve(2 + dgm.data.size());
	raw.push_back(dgm.bstId);
	raw.push_back(static_cast<uint8_t>(dgm.data.size()));
	raw.insert(raw.end(), dgm.data.begin(), dgm.data.end());

	auto frame = BstFrame::fromRawData(raw);
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->pgn(), 127250u);
	EXPECT_EQ(frame->priority(), 6u);
	EXPECT_EQ(frame->destination(), 0xFFu);

	const auto recoveredPayload = frame->data();
	ASSERT_EQ(recoveredPayload.size(), payload.size());
	for (std::size_t i = 0; i < payload.size(); ++i) {
		EXPECT_EQ(recoveredPayload[i], payload[i]) << "mismatch at byte " << i;
	}
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
