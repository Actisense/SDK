/**************************************************************************/ /**
\file       test_session_device_control.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 29/04/2026
\brief      Session-level smoke tests for Device Control BEM commands
\details    Verifies SessionImpl::reInitMainApp encodes the correct BEM
            command ID (0x00) and no payload onto the wire. Encoder/decoder
            coverage lives in test_device_control.cpp; this file covers the
            session layer.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/session_impl.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
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

class SessionDeviceControlTest : public ::testing::Test
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

TEST_F(SessionDeviceControlTest, ReInitMainApp_SendsCommandWithNoPayload)
{
	session_->reInitMainApp(kTimeout, nullptr);

	const auto dgm = captureSentDatagram();
	EXPECT_EQ(dgm.bstId, static_cast<uint8_t>(BstId::Bem_PG_A1));

	/* BEM command framing for ReInitMainApp (0x00) is a single byte: the
	   BEM ID. There must be no payload after it. */
	ASSERT_EQ(dgm.data.size(), 1u);
	EXPECT_EQ(dgm.data[0], static_cast<uint8_t>(BemCommandId::ReInitMainApp));
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
