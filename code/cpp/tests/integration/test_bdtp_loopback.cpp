/**************************************************************************//**
\file       test_bdtp_loopback.cpp
\brief      Integration test for BDTP over loopback transport
\details    End-to-end test of BDTP frames sent via loopback and parsed

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/loopback/loopback_transport.hpp"
#include "protocols/bdtp/bdtp_protocol.hpp"

#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <span>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class BdtpLoopbackIntegrationTest : public ::testing::Test
{
protected:
	LoopbackTransport m_transport;
	BdtpProtocol      m_protocol;
	TransportConfig   m_config;

	std::vector<ParsedMessageEvent> m_receivedMessages;
	std::vector<std::pair<ErrorCode, std::string>> m_errors;

	void SetUp() override
	{
		m_config.kind = TransportKind::Loopback;
		m_receivedMessages.clear();
		m_errors.clear();

		/* Open transport */
		m_transport.asyncOpen(m_config, [](ErrorCode ec)
		{
			ASSERT_EQ(ec, ErrorCode::Ok);
		});
	}

	MessageEmitter messageEmitter()
	{
		return [this](const ParsedMessageEvent& event)
		{
			m_receivedMessages.push_back(event);
		};
	}

	ErrorEmitter errorEmitter()
	{
		return [this](ErrorCode code, std::string_view message)
		{
			m_errors.emplace_back(code, std::string(message));
		};
	}

	/* Helper: Send data via transport and read back for parsing */
	void sendAndReceive(ConstByteSpan data)
	{
		/* Send via transport */
		bool sendComplete = false;
		m_transport.asyncSend(data, [&](ErrorCode ec, std::size_t)
		{
			EXPECT_EQ(ec, ErrorCode::Ok);
			sendComplete = true;
		});
		EXPECT_TRUE(sendComplete);

		/* Read from transport */
		std::vector<uint8_t> recvBuffer;
		bool recvComplete = false;
		m_transport.asyncRecv([&](ErrorCode ec, ConstByteSpan recvData)
		{
			EXPECT_EQ(ec, ErrorCode::Ok);
			recvBuffer.assign(recvData.begin(), recvData.end());
			recvComplete = true;
		});
		EXPECT_TRUE(recvComplete);

		/* Parse received data */
		m_protocol.parse(ConstByteSpan(recvBuffer), messageEmitter(), errorEmitter());
	}
};

/* Integration Tests -------------------------------------------------------- */

TEST_F(BdtpLoopbackIntegrationTest, SendReceiveSingleFrame)
{
	/* Create BST datagram */
	BstDatagram datagram;
	datagram.bstId = 0x93;
	datagram.storeLength = 3;
	datagram.data = {0x11, 0x22, 0x33};

	/* Encode to BDTP frame */
	std::vector<uint8_t> frame;
	BdtpProtocol::encodeBst(datagram, frame);

	/* Send via loopback and parse */
	sendAndReceive(frame);

	/* Verify received message */
	ASSERT_EQ(m_receivedMessages.size(), 1u);
	EXPECT_TRUE(m_errors.empty());

	const auto& msg = m_receivedMessages[0];
	EXPECT_EQ(msg.protocol, "bdtp");
	EXPECT_EQ(msg.messageType, "BST_147");

	const auto& received = std::any_cast<const BstDatagram&>(msg.payload);
	EXPECT_EQ(received.bstId, datagram.bstId);
	EXPECT_EQ(received.data, datagram.data);
}

TEST_F(BdtpLoopbackIntegrationTest, SendReceiveMultipleFrames)
{
	/* Create and send multiple datagrams */
	for (uint8_t i = 0; i < 5; ++i)
	{
		BstDatagram datagram;
		datagram.bstId = 0x90 + i;
		datagram.storeLength = 2;
		datagram.data = {i, static_cast<uint8_t>(i * 2)};

		std::vector<uint8_t> frame;
		BdtpProtocol::encodeBst(datagram, frame);

		sendAndReceive(frame);
	}

	/* Verify all messages received */
	ASSERT_EQ(m_receivedMessages.size(), 5u);
	EXPECT_TRUE(m_errors.empty());

	for (uint8_t i = 0; i < 5; ++i)
	{
		const auto& received = std::any_cast<const BstDatagram&>(m_receivedMessages[i].payload);
		EXPECT_EQ(received.bstId, 0x90 + i);
	}
}

TEST_F(BdtpLoopbackIntegrationTest, SendReceiveWithDleInPayload)
{
	/* Data with multiple DLE bytes that need escaping */
	BstDatagram datagram;
	datagram.bstId = 0x94;
	datagram.storeLength = 5;
	datagram.data = {0x10, 0x02, 0x10, 0x03, 0x10};  /* DLE, STX, DLE, ETX, DLE */

	std::vector<uint8_t> frame;
	BdtpProtocol::encodeBst(datagram, frame);

	sendAndReceive(frame);

	ASSERT_EQ(m_receivedMessages.size(), 1u);
	const auto& received = std::any_cast<const BstDatagram&>(m_receivedMessages[0].payload);
	EXPECT_EQ(received.data, datagram.data);
}

TEST_F(BdtpLoopbackIntegrationTest, DataInjectionSimulatesDevice)
{
	/* Simulate device sending data by injecting into transport */
	BstDatagram datagram;
	datagram.bstId = 0x95;
	datagram.storeLength = 4;
	datagram.data = {0xDE, 0xAD, 0xBE, 0xEF};

	std::vector<uint8_t> frame;
	BdtpProtocol::encodeBst(datagram, frame);

	/* Inject as if device sent it */
	const auto injected = m_transport.injectData(frame);
	EXPECT_EQ(injected, frame.size());

	/* Read and parse */
	std::vector<uint8_t> recvBuffer;
	m_transport.asyncRecv([&](ErrorCode ec, ConstByteSpan data)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		recvBuffer.assign(data.begin(), data.end());
	});

	m_protocol.parse(recvBuffer, messageEmitter(), errorEmitter());

	ASSERT_EQ(m_receivedMessages.size(), 1u);
	const auto& received = std::any_cast<const BstDatagram&>(m_receivedMessages[0].payload);
	EXPECT_EQ(received.data, datagram.data);
}

TEST_F(BdtpLoopbackIntegrationTest, ChunkedReceive)
{
	/* Send complete frame and receive as complete message, then parse progressively */
	BstDatagram datagram;
	datagram.bstId = 0x96;
	datagram.storeLength = 10;
	datagram.data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	std::vector<uint8_t> frame;
	BdtpProtocol::encodeBst(datagram, frame);

	/* Send complete frame */
	m_transport.asyncSend(frame, [](ErrorCode ec, std::size_t)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
	});

	/* Receive complete message (message-oriented buffer returns full message) */
	std::vector<uint8_t> recvBuffer;
	
	m_transport.asyncRecv([&](ErrorCode ec, ConstByteSpan data)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		recvBuffer.assign(data.begin(), data.end());
	});

	EXPECT_EQ(recvBuffer.size(), frame.size());

	/* Parse progressively in small chunks to test BDTP parser's chunked parsing */
	std::size_t offset = 0;
	constexpr std::size_t chunkSize = 3;
	while (offset < recvBuffer.size())
	{
		std::size_t bytesToParse = std::min(chunkSize, recvBuffer.size() - offset);
		m_protocol.parse(ConstByteSpan(recvBuffer.data() + offset, bytesToParse),
		                 messageEmitter(), errorEmitter());
		offset += bytesToParse;
	}

	/* Should still receive complete message */
	ASSERT_EQ(m_receivedMessages.size(), 1u);
	const auto& received = std::any_cast<const BstDatagram&>(m_receivedMessages[0].payload);
	EXPECT_EQ(received.data, datagram.data);
}

TEST_F(BdtpLoopbackIntegrationTest, LargePayload)
{
	/* Test with maximum BST payload (255 bytes) */
	BstDatagram datagram;
	datagram.bstId = 0x97;
	datagram.storeLength = 200;  /* Large but not maximum */
	datagram.data.resize(200);
	for (int i = 0; i < 200; ++i)
	{
		datagram.data[i] = static_cast<uint8_t>(i);
	}

	std::vector<uint8_t> frame;
	BdtpProtocol::encodeBst(datagram, frame);

	sendAndReceive(frame);

	ASSERT_EQ(m_receivedMessages.size(), 1u);
	const auto& received = std::any_cast<const BstDatagram&>(m_receivedMessages[0].payload);
	EXPECT_EQ(received.data.size(), 200u);
	EXPECT_EQ(received.data, datagram.data);
}

TEST_F(BdtpLoopbackIntegrationTest, BiDirectionalCommunication)
{
	/* Simulate bidirectional communication */
	
	/* "Device" sends to "Host" */
	BstDatagram deviceMessage;
	deviceMessage.bstId = 0x93;
	deviceMessage.storeLength = 2;
	deviceMessage.data = {0xAA, 0xBB};

	std::vector<uint8_t> deviceFrame;
	BdtpProtocol::encodeBst(deviceMessage, deviceFrame);
	m_transport.injectData(deviceFrame);

	/* "Host" receives and parses */
	std::vector<uint8_t> hostBuffer;
	m_transport.asyncRecv([&](ErrorCode, ConstByteSpan data) {
		hostBuffer.assign(data.begin(), data.end());
	});
	m_protocol.parse(hostBuffer, messageEmitter(), errorEmitter());

	ASSERT_EQ(m_receivedMessages.size(), 1u);

	/* "Host" sends response */
	BstDatagram hostResponse;
	hostResponse.bstId = 0x94;
	hostResponse.storeLength = 2;
	hostResponse.data = {0xCC, 0xDD};

	std::vector<uint8_t> hostFrame;
	BdtpProtocol::encodeBst(hostResponse, hostFrame);

	/* Disable loopback to simulate sending to device (one-way) */
	m_transport.setLoopbackEnabled(false);
	m_transport.asyncSend(hostFrame, [](ErrorCode ec, std::size_t)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
	});

	/* Verify data was "sent" (byte count) */
	EXPECT_GE(m_transport.bytesSent(), hostFrame.size());
}

TEST_F(BdtpLoopbackIntegrationTest, ErrorRecoveryAfterCorruptedFrame)
{
	/* Send corrupted frame followed by valid frame */
	
	/* Corrupted frame (bad checksum) */
	std::vector<uint8_t> corruptFrame = {
		BdtpChars::DLE, BdtpChars::STX,
		0x93, 0x02, 0xAA, 0xBB, 0xFF,  /* Bad checksum */
		BdtpChars::DLE, BdtpChars::ETX
	};

	/* Valid frame */
	BstDatagram validDatagram;
	validDatagram.bstId = 0x94;
	validDatagram.storeLength = 1;
	validDatagram.data = {0x42};

	std::vector<uint8_t> validFrame;
	BdtpProtocol::encodeBst(validDatagram, validFrame);

	/* Send both */
	std::vector<uint8_t> combined;
	combined.insert(combined.end(), corruptFrame.begin(), corruptFrame.end());
	combined.insert(combined.end(), validFrame.begin(), validFrame.end());

	m_transport.injectData(combined);

	std::vector<uint8_t> recvBuffer;
	m_transport.asyncRecv([&](ErrorCode, ConstByteSpan data) {
		recvBuffer.assign(data.begin(), data.end());
	});
	m_protocol.parse(recvBuffer, messageEmitter(), errorEmitter());

	/* Should have one error and one valid message */
	EXPECT_EQ(m_errors.size(), 1u);
	ASSERT_EQ(m_receivedMessages.size(), 1u);
	
	const auto& received = std::any_cast<const BstDatagram&>(m_receivedMessages[0].payload);
	EXPECT_EQ(received.bstId, 0x94);
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
