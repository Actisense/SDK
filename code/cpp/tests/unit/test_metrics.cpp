/**************************************************************************//**
\file       test_metrics.cpp
\brief      Unit tests for SDK metrics infrastructure
\details    Tests metrics data structures and MetricsCollector

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <public/metrics.hpp>
#include <core/metrics_collector.hpp>

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* TransportMetrics Tests --------------------------------------------------- */

class TransportMetricsTest : public ::testing::Test
{
};

TEST_F(TransportMetricsTest, DefaultInitialization)
{
	TransportMetrics metrics;

	EXPECT_EQ(metrics.bytesSent, 0u);
	EXPECT_EQ(metrics.bytesReceived, 0u);
	EXPECT_EQ(metrics.writeCalls, 0u);
	EXPECT_EQ(metrics.readCalls, 0u);
	EXPECT_EQ(metrics.errorsCount, 0u);
	EXPECT_EQ(metrics.lastError, ErrorCode::Ok);
}

TEST_F(TransportMetricsTest, Reset)
{
	TransportMetrics metrics;
	metrics.bytesSent = 100;
	metrics.bytesReceived = 200;
	metrics.errorsCount = 5;
	metrics.lastError = ErrorCode::TransportIo;

	metrics.reset();

	EXPECT_EQ(metrics.bytesSent, 0u);
	EXPECT_EQ(metrics.bytesReceived, 0u);
	EXPECT_EQ(metrics.errorsCount, 0u);
	EXPECT_EQ(metrics.lastError, ErrorCode::Ok);
}

/* ProtocolMetrics Tests ---------------------------------------------------- */

class ProtocolMetricsTest : public ::testing::Test
{
};

TEST_F(ProtocolMetricsTest, DefaultInitialization)
{
	ProtocolMetrics metrics;

	EXPECT_EQ(metrics.framesReceived, 0u);
	EXPECT_EQ(metrics.framesDropped, 0u);
	EXPECT_EQ(metrics.checksumErrors, 0u);
	EXPECT_EQ(metrics.framingErrors, 0u);
	EXPECT_EQ(metrics.bst93Count, 0u);
	EXPECT_EQ(metrics.bst94Count, 0u);
	EXPECT_EQ(metrics.bst95Count, 0u);
	EXPECT_EQ(metrics.bstD0Count, 0u);
	EXPECT_EQ(metrics.bemCount, 0u);
}

TEST_F(ProtocolMetricsTest, Reset)
{
	ProtocolMetrics metrics;
	metrics.framesReceived = 100;
	metrics.checksumErrors = 5;
	metrics.bst93Count = 50;

	metrics.reset();

	EXPECT_EQ(metrics.framesReceived, 0u);
	EXPECT_EQ(metrics.checksumErrors, 0u);
	EXPECT_EQ(metrics.bst93Count, 0u);
}

/* BemMetrics Tests --------------------------------------------------------- */

class BemMetricsTest : public ::testing::Test
{
};

TEST_F(BemMetricsTest, DefaultInitialization)
{
	BemMetrics metrics;

	EXPECT_EQ(metrics.commandsSent, 0u);
	EXPECT_EQ(metrics.responsesReceived, 0u);
	EXPECT_EQ(metrics.timeouts, 0u);
	EXPECT_EQ(metrics.deviceErrors, 0u);
	EXPECT_EQ(metrics.totalLatencyMs, 0u);
	EXPECT_EQ(metrics.maxLatencyMs, 0u);
}

TEST_F(BemMetricsTest, AvgLatencyZeroWhenNoResponses)
{
	BemMetrics metrics;
	EXPECT_EQ(metrics.avgLatencyMs(), 0u);
}

TEST_F(BemMetricsTest, AvgLatencyCalculation)
{
	BemMetrics metrics;
	metrics.responsesReceived = 4;
	metrics.totalLatencyMs = 100;

	EXPECT_EQ(metrics.avgLatencyMs(), 25u);
}

TEST_F(BemMetricsTest, RecordLatency)
{
	BemMetrics metrics;

	metrics.recordLatency(100);
	EXPECT_EQ(metrics.totalLatencyMs, 100u);
	EXPECT_EQ(metrics.maxLatencyMs, 100u);
	EXPECT_EQ(metrics.minLatencyMs, 100u);

	metrics.recordLatency(50);
	EXPECT_EQ(metrics.totalLatencyMs, 150u);
	EXPECT_EQ(metrics.maxLatencyMs, 100u);
	EXPECT_EQ(metrics.minLatencyMs, 50u);

	metrics.recordLatency(200);
	EXPECT_EQ(metrics.totalLatencyMs, 350u);
	EXPECT_EQ(metrics.maxLatencyMs, 200u);
	EXPECT_EQ(metrics.minLatencyMs, 50u);
}

/* SessionMetrics Tests ----------------------------------------------------- */

class SessionMetricsTest : public ::testing::Test
{
};

TEST_F(SessionMetricsTest, TotalFrames)
{
	SessionMetrics metrics;
	metrics.protocol.framesReceived = 100;
	metrics.protocol.framesDropped = 10;

	EXPECT_EQ(metrics.totalFrames(), 110u);
}

TEST_F(SessionMetricsTest, FrameSuccessRate)
{
	SessionMetrics metrics;

	/* No frames - 100% success */
	EXPECT_DOUBLE_EQ(metrics.frameSuccessRate(), 100.0);

	/* All successful */
	metrics.protocol.framesReceived = 100;
	metrics.protocol.framesDropped = 0;
	EXPECT_DOUBLE_EQ(metrics.frameSuccessRate(), 100.0);

	/* 90% success */
	metrics.protocol.framesReceived = 90;
	metrics.protocol.framesDropped = 10;
	EXPECT_DOUBLE_EQ(metrics.frameSuccessRate(), 90.0);
}

TEST_F(SessionMetricsTest, BemSuccessRate)
{
	SessionMetrics metrics;

	/* No commands - 100% success */
	EXPECT_DOUBLE_EQ(metrics.bemSuccessRate(), 100.0);

	/* All successful */
	metrics.bem.commandsSent = 10;
	metrics.bem.responsesReceived = 10;
	metrics.bem.deviceErrors = 0;
	EXPECT_DOUBLE_EQ(metrics.bemSuccessRate(), 100.0);

	/* 80% success (2 device errors out of 10) */
	metrics.bem.deviceErrors = 2;
	EXPECT_DOUBLE_EQ(metrics.bemSuccessRate(), 80.0);
}

/* MetricsCollector Tests --------------------------------------------------- */

class MetricsCollectorTest : public ::testing::Test
{
protected:
	MetricsCollector collector_;
};

TEST_F(MetricsCollectorTest, InitialSnapshot)
{
	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.transport.bytesSent, 0u);
	EXPECT_EQ(snapshot.transport.bytesReceived, 0u);
	EXPECT_EQ(snapshot.protocol.framesReceived, 0u);
	EXPECT_EQ(snapshot.bem.commandsSent, 0u);
	EXPECT_EQ(snapshot.reconnectCount, 0u);
}

TEST_F(MetricsCollectorTest, RecordTransportMetrics)
{
	collector_.recordBytesSent(100);
	collector_.recordBytesSent(50);
	collector_.recordBytesReceived(200);
	collector_.recordWriteCall();
	collector_.recordWriteCall();
	collector_.recordReadCall();

	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.transport.bytesSent, 150u);
	EXPECT_EQ(snapshot.transport.bytesReceived, 200u);
	EXPECT_EQ(snapshot.transport.writeCalls, 2u);
	EXPECT_EQ(snapshot.transport.readCalls, 1u);
}

TEST_F(MetricsCollectorTest, RecordTransportError)
{
	collector_.recordTransportError(ErrorCode::TransportIo);
	collector_.recordTransportError(ErrorCode::Timeout);

	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.transport.errorsCount, 2u);
	EXPECT_EQ(snapshot.transport.lastError, ErrorCode::Timeout);
}

TEST_F(MetricsCollectorTest, RecordFrameParsed)
{
	collector_.recordFrameParsed(0x93);
	collector_.recordFrameParsed(0x93);
	collector_.recordFrameParsed(0x94);
	collector_.recordFrameParsed(0xD0);
	collector_.recordFrameParsed(0xA0); // BEM response

	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.protocol.framesReceived, 5u);
	EXPECT_EQ(snapshot.protocol.bst93Count, 2u);
	EXPECT_EQ(snapshot.protocol.bst94Count, 1u);
	EXPECT_EQ(snapshot.protocol.bstD0Count, 1u);
	EXPECT_EQ(snapshot.protocol.bemCount, 1u);
}

TEST_F(MetricsCollectorTest, RecordFrameErrors)
{
	collector_.recordFrameDropped();
	collector_.recordFrameDropped();
	collector_.recordChecksumError();
	collector_.recordFramingError();
	collector_.recordFramingError();
	collector_.recordFramingError();

	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.protocol.framesDropped, 2u);
	EXPECT_EQ(snapshot.protocol.checksumErrors, 1u);
	EXPECT_EQ(snapshot.protocol.framingErrors, 3u);
}

TEST_F(MetricsCollectorTest, RecordBemMetrics)
{
	collector_.recordBemCommand();
	collector_.recordBemCommand();
	collector_.recordBemResponse(100);
	collector_.recordBemResponse(50);
	collector_.recordBemTimeout();
	collector_.recordBemDeviceError();

	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.bem.commandsSent, 2u);
	EXPECT_EQ(snapshot.bem.responsesReceived, 2u);
	EXPECT_EQ(snapshot.bem.timeouts, 1u);
	EXPECT_EQ(snapshot.bem.deviceErrors, 1u);
	EXPECT_EQ(snapshot.bem.totalLatencyMs, 150u);
	EXPECT_EQ(snapshot.bem.maxLatencyMs, 100u);
	EXPECT_EQ(snapshot.bem.minLatencyMs, 50u);
}

TEST_F(MetricsCollectorTest, RecordReconnect)
{
	collector_.recordReconnect();
	collector_.recordReconnect();
	collector_.recordReconnect();

	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.reconnectCount, 3u);
}

TEST_F(MetricsCollectorTest, UptimeIsPositive)
{
	/* Small delay to ensure uptime > 0 */
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	auto snapshot = collector_.snapshot();

	EXPECT_GT(snapshot.uptimeMs, 0u);
}

TEST_F(MetricsCollectorTest, Reset)
{
	/* Record some metrics */
	collector_.recordBytesSent(100);
	collector_.recordFrameParsed(0x93);
	collector_.recordBemCommand();
	collector_.recordReconnect();

	/* Verify they were recorded */
	auto before = collector_.snapshot();
	EXPECT_GT(before.transport.bytesSent, 0u);

	/* Reset */
	collector_.reset();

	/* Verify all reset */
	auto after = collector_.snapshot();
	EXPECT_EQ(after.transport.bytesSent, 0u);
	EXPECT_EQ(after.protocol.framesReceived, 0u);
	EXPECT_EQ(after.bem.commandsSent, 0u);
	EXPECT_EQ(after.reconnectCount, 0u);
}

TEST_F(MetricsCollectorTest, SnapshotTimestamp)
{
	auto snapshot = collector_.snapshot();

	/* capturedAt should be recent */
	auto now = std::chrono::steady_clock::now();
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - snapshot.capturedAt);

	EXPECT_LT(diff.count(), 100); /* Within 100ms */
}

/* Thread Safety Tests ------------------------------------------------------ */

class MetricsThreadSafetyTest : public ::testing::Test
{
protected:
	MetricsCollector collector_;
};

TEST_F(MetricsThreadSafetyTest, ConcurrentUpdates)
{
	const int numThreads = 4;
	const int incrementsPerThread = 1000;

	std::vector<std::thread> threads;
	threads.reserve(numThreads);

	for (int t = 0; t < numThreads; ++t) {
		threads.emplace_back([this, incrementsPerThread]() {
			for (int i = 0; i < incrementsPerThread; ++i) {
				collector_.recordBytesSent(1);
				collector_.recordFrameParsed(0x93);
				collector_.recordBemCommand();
			}
		});
	}

	for (auto& thread : threads) {
		thread.join();
	}

	auto snapshot = collector_.snapshot();

	/* All increments should be accounted for */
	EXPECT_EQ(snapshot.transport.bytesSent, static_cast<uint64_t>(numThreads * incrementsPerThread));
	EXPECT_EQ(snapshot.protocol.framesReceived, static_cast<uint64_t>(numThreads * incrementsPerThread));
	EXPECT_EQ(snapshot.bem.commandsSent, static_cast<uint32_t>(numThreads * incrementsPerThread));
}

TEST_F(MetricsThreadSafetyTest, ConcurrentLatencyRecording)
{
	const int numThreads = 4;
	const int recordsPerThread = 100;
	const uint32_t latencyValue = 10;

	std::vector<std::thread> threads;
	threads.reserve(numThreads);

	for (int t = 0; t < numThreads; ++t) {
		threads.emplace_back([this, recordsPerThread, latencyValue]() {
			for (int i = 0; i < recordsPerThread; ++i) {
				collector_.recordBemResponse(latencyValue);
			}
		});
	}

	for (auto& thread : threads) {
		thread.join();
	}

	auto snapshot = collector_.snapshot();

	EXPECT_EQ(snapshot.bem.responsesReceived, static_cast<uint32_t>(numThreads * recordsPerThread));
	EXPECT_EQ(snapshot.bem.totalLatencyMs, static_cast<uint64_t>(numThreads * recordsPerThread * latencyValue));
	EXPECT_EQ(snapshot.bem.minLatencyMs, latencyValue);
	EXPECT_EQ(snapshot.bem.maxLatencyMs, latencyValue);
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
