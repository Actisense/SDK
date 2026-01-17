/**************************************************************************//**
\file       test_loopback_transport.cpp
\brief      Unit tests for loopback transport
\details    Tests in-memory transport for data round-trip and injection

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "transport/loopback/loopback_transport.hpp"

#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <span>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class LoopbackTransportTest : public ::testing::Test
{
protected:
	LoopbackTransport m_transport;
	TransportConfig   m_config;

	void SetUp() override
	{
		m_config.kind = TransportKind::Loopback;
	}

	void openTransport()
	{
		bool opened = false;
		m_transport.asyncOpen(m_config, [&](ErrorCode ec)
		{
			EXPECT_EQ(ec, ErrorCode::Ok);
			opened = true;
		});
		EXPECT_TRUE(opened);
		EXPECT_TRUE(m_transport.isOpen());
	}
};

/* Tests -------------------------------------------------------------------- */

TEST_F(LoopbackTransportTest, InitialState)
{
	EXPECT_FALSE(m_transport.isOpen());
	EXPECT_EQ(m_transport.kind(), TransportKind::Loopback);
	EXPECT_EQ(m_transport.bytesAvailable(), 0u);
	EXPECT_EQ(m_transport.messagesAvailable(), 0u);
	EXPECT_EQ(m_transport.bytesSent(), 0u);
}

TEST_F(LoopbackTransportTest, OpenClose)
{
	openTransport();
	
	m_transport.close();
	EXPECT_FALSE(m_transport.isOpen());
}

TEST_F(LoopbackTransportTest, DoubleOpenFails)
{
	openTransport();

	bool errorReceived = false;
	m_transport.asyncOpen(m_config, [&](ErrorCode ec)
	{
		EXPECT_EQ(ec, ErrorCode::AlreadyConnected);
		errorReceived = true;
	});
	EXPECT_TRUE(errorReceived);
}

TEST_F(LoopbackTransportTest, SendReceiveRoundTrip)
{
	openTransport();

	const std::array<uint8_t, 5> sendData = {0x10, 0x02, 0xAA, 0x10, 0x03};
	bool sendComplete = false;
	std::size_t sentBytes = 0;

	m_transport.asyncSend(sendData, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		sendComplete = true;
		sentBytes = bytes;
	});

	EXPECT_TRUE(sendComplete);
	EXPECT_EQ(sentBytes, 5u);
	EXPECT_EQ(m_transport.bytesSent(), 5u);
	EXPECT_EQ(m_transport.bytesAvailable(), 5u);

	/* Receive the data back */
	std::array<uint8_t, 5> recvData = {};
	bool recvComplete = false;
	std::size_t recvBytes = 0;

	m_transport.asyncRecv(recvData, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		recvComplete = true;
		recvBytes = bytes;
	});

	EXPECT_TRUE(recvComplete);
	EXPECT_EQ(recvBytes, 5u);
	EXPECT_EQ(recvData, sendData);
	EXPECT_EQ(m_transport.bytesAvailable(), 0u);
}

TEST_F(LoopbackTransportTest, DataInjection)
{
	openTransport();

	const std::array<uint8_t, 4> injectData = {0xDE, 0xAD, 0xBE, 0xEF};
	const auto injected = m_transport.injectData(injectData);
	EXPECT_EQ(injected, 4u);
	EXPECT_EQ(m_transport.bytesAvailable(), 4u);

	std::array<uint8_t, 4> recvData = {};
	m_transport.asyncRecv(recvData, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		EXPECT_EQ(bytes, 4u);
	});

	EXPECT_EQ(recvData, injectData);
}

TEST_F(LoopbackTransportTest, PendingReceive)
{
	openTransport();

	/* Queue a receive before data is available */
	std::array<uint8_t, 4> recvData = {};
	bool recvComplete = false;

	m_transport.asyncRecv(recvData, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		EXPECT_EQ(bytes, 4u);
		recvComplete = true;
	});

	/* Receive should not complete yet */
	EXPECT_FALSE(recvComplete);

	/* Inject data - should trigger pending receive */
	const std::array<uint8_t, 4> injectData = {1, 2, 3, 4};
	m_transport.injectData(injectData);

	EXPECT_TRUE(recvComplete);
	EXPECT_EQ(recvData, injectData);
}

TEST_F(LoopbackTransportTest, SendWhenNotConnected)
{
	const std::array<uint8_t, 3> data = {1, 2, 3};
	bool errorReceived = false;

	m_transport.asyncSend(data, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::NotConnected);
		EXPECT_EQ(bytes, 0u);
		errorReceived = true;
	});

	EXPECT_TRUE(errorReceived);
}

TEST_F(LoopbackTransportTest, RecvWhenNotConnected)
{
	std::array<uint8_t, 3> data = {};
	bool errorReceived = false;

	m_transport.asyncRecv(data, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::NotConnected);
		EXPECT_EQ(bytes, 0u);
		errorReceived = true;
	});

	EXPECT_TRUE(errorReceived);
}

TEST_F(LoopbackTransportTest, LoopbackDisabled)
{
	openTransport();
	m_transport.setLoopbackEnabled(false);
	EXPECT_FALSE(m_transport.isLoopbackEnabled());

	const std::array<uint8_t, 5> sendData = {1, 2, 3, 4, 5};
	m_transport.asyncSend(sendData, [](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		EXPECT_EQ(bytes, 5u);
	});

	/* Data should be sent but not looped back */
	EXPECT_EQ(m_transport.bytesSent(), 5u);
	EXPECT_EQ(m_transport.bytesAvailable(), 0u);
}

TEST_F(LoopbackTransportTest, ClearBuffers)
{
	openTransport();

	const std::array<uint8_t, 10> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	m_transport.asyncSend(data, [](ErrorCode, std::size_t) {});
	EXPECT_EQ(m_transport.bytesAvailable(), 10u);

	m_transport.clearBuffers();
	EXPECT_EQ(m_transport.bytesAvailable(), 0u);
}

TEST_F(LoopbackTransportTest, CloseWithPendingReceives)
{
	openTransport();

	std::array<uint8_t, 10> data = {};
	bool canceled = false;

	m_transport.asyncRecv(data, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Canceled);
		EXPECT_EQ(bytes, 0u);
		canceled = true;
	});

	EXPECT_FALSE(canceled);
	m_transport.close();
	EXPECT_TRUE(canceled);
}

TEST_F(LoopbackTransportTest, CreateFactory)
{
	auto transport = createLoopbackTransport();
	EXPECT_NE(transport, nullptr);
	EXPECT_EQ(transport->kind(), TransportKind::Loopback);
}

TEST_F(LoopbackTransportTest, MessageOrientedBuffer)
{
	openTransport();

	/* Send multiple messages */
	const std::array<uint8_t, 3> msg1 = {1, 2, 3};
	const std::array<uint8_t, 4> msg2 = {4, 5, 6, 7};
	const std::array<uint8_t, 2> msg3 = {8, 9};

	m_transport.asyncSend(msg1, [](ErrorCode, std::size_t) {});
	m_transport.asyncSend(msg2, [](ErrorCode, std::size_t) {});
	m_transport.asyncSend(msg3, [](ErrorCode, std::size_t) {});

	EXPECT_EQ(m_transport.messagesAvailable(), 3u);
	EXPECT_EQ(m_transport.bytesAvailable(), 9u);  /* 3 + 4 + 2 */

	/* Receive each message separately */
	std::array<uint8_t, 10> recvBuffer = {};

	m_transport.asyncRecv(recvBuffer, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		EXPECT_EQ(bytes, 3u);  /* First message only */
	});

	EXPECT_EQ(m_transport.messagesAvailable(), 2u);

	m_transport.asyncRecv(recvBuffer, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		EXPECT_EQ(bytes, 4u);  /* Second message */
	});

	EXPECT_EQ(m_transport.messagesAvailable(), 1u);

	m_transport.asyncRecv(recvBuffer, [&](ErrorCode ec, std::size_t bytes)
	{
		EXPECT_EQ(ec, ErrorCode::Ok);
		EXPECT_EQ(bytes, 2u);  /* Third message */
	});

	EXPECT_EQ(m_transport.messagesAvailable(), 0u);
	EXPECT_EQ(m_transport.bytesAvailable(), 0u);
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
