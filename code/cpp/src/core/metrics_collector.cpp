/**************************************************************************/ /**
 \file       metrics_collector.cpp
 \brief      MetricsCollector implementation
 \details    Thread-safe metrics collection with atomic counters

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "core/metrics_collector.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Public Method Definitions -------------------------------------------- */

		MetricsCollector::MetricsCollector()
			: startTime_(std::chrono::steady_clock::now()) {
		}

		/* Transport metrics ---------------------------------------------------- */

		void MetricsCollector::recordBytesSent(uint64_t count) noexcept {
			bytesSent_.fetch_add(count, std::memory_order_relaxed);
		}

		void MetricsCollector::recordBytesReceived(uint64_t count) noexcept {
			bytesReceived_.fetch_add(count, std::memory_order_relaxed);
		}

		void MetricsCollector::recordWriteCall() noexcept {
			writeCalls_.fetch_add(1, std::memory_order_relaxed);
		}

		void MetricsCollector::recordReadCall() noexcept {
			readCalls_.fetch_add(1, std::memory_order_relaxed);
		}

		void MetricsCollector::recordTransportError(ErrorCode code) noexcept {
			transportErrors_.fetch_add(1, std::memory_order_relaxed);
			lastTransportError_.store(static_cast<int>(code), std::memory_order_relaxed);
		}

		/* Protocol metrics ----------------------------------------------------- */

		void MetricsCollector::recordFrameParsed(uint8_t bstId) noexcept {
			framesReceived_.fetch_add(1, std::memory_order_relaxed);

			// Update per-type counter
			switch (bstId) {
				case 0x93:
					bst93Count_.fetch_add(1, std::memory_order_relaxed);
					break;
				case 0x94:
					bst94Count_.fetch_add(1, std::memory_order_relaxed);
					break;
				case 0x95:
					bst95Count_.fetch_add(1, std::memory_order_relaxed);
					break;
				case 0xD0:
					bstD0Count_.fetch_add(1, std::memory_order_relaxed);
					break;
				case 0xA0: // BEM response types
				case 0xA2:
				case 0xA3:
				case 0xA5:
					bemCount_.fetch_add(1, std::memory_order_relaxed);
					break;
				default:
					// Unknown type - still counted in framesReceived
					break;
			}
		}

		void MetricsCollector::recordFrameDropped() noexcept {
			framesDropped_.fetch_add(1, std::memory_order_relaxed);
		}

		void MetricsCollector::recordChecksumError() noexcept {
			checksumErrors_.fetch_add(1, std::memory_order_relaxed);
		}

		void MetricsCollector::recordFramingError() noexcept {
			framingErrors_.fetch_add(1, std::memory_order_relaxed);
		}

		/* BEM metrics ---------------------------------------------------------- */

		void MetricsCollector::recordBemCommand() noexcept {
			bemCommandsSent_.fetch_add(1, std::memory_order_relaxed);
		}

		void MetricsCollector::recordBemResponse(uint32_t latencyMs) noexcept {
			bemResponsesReceived_.fetch_add(1, std::memory_order_relaxed);

			// Latency tracking requires mutex for min/max updates
			std::lock_guard<std::mutex> lock(latencyMutex_);
			totalLatencyMs_ += latencyMs;
			if (latencyMs > maxLatencyMs_) {
				maxLatencyMs_ = latencyMs;
			}
			if (latencyMs < minLatencyMs_) {
				minLatencyMs_ = latencyMs;
			}
		}

		void MetricsCollector::recordBemTimeout() noexcept {
			bemTimeouts_.fetch_add(1, std::memory_order_relaxed);
		}

		void MetricsCollector::recordBemDeviceError() noexcept {
			bemDeviceErrors_.fetch_add(1, std::memory_order_relaxed);
		}

		/* Session metrics ------------------------------------------------------ */

		void MetricsCollector::recordReconnect() noexcept {
			reconnectCount_.fetch_add(1, std::memory_order_relaxed);
		}

		/* Snapshot and reset --------------------------------------------------- */

		SessionMetrics MetricsCollector::snapshot() const {
			SessionMetrics metrics;

			// Capture timestamp
			const auto now = std::chrono::steady_clock::now();
			metrics.capturedAt = now;

			// Calculate uptime
			const auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
				now - startTime_);
			metrics.uptimeMs = static_cast<uint64_t>(uptime.count());

			// Transport metrics (relaxed loads are safe for counters)
			metrics.transport.bytesSent = bytesSent_.load(std::memory_order_relaxed);
			metrics.transport.bytesReceived = bytesReceived_.load(std::memory_order_relaxed);
			metrics.transport.writeCalls = writeCalls_.load(std::memory_order_relaxed);
			metrics.transport.readCalls = readCalls_.load(std::memory_order_relaxed);
			metrics.transport.errorsCount = transportErrors_.load(std::memory_order_relaxed);
			metrics.transport.lastError = static_cast<ErrorCode>(
				lastTransportError_.load(std::memory_order_relaxed));

			// Protocol metrics
			metrics.protocol.framesReceived = framesReceived_.load(std::memory_order_relaxed);
			metrics.protocol.framesDropped = framesDropped_.load(std::memory_order_relaxed);
			metrics.protocol.checksumErrors = checksumErrors_.load(std::memory_order_relaxed);
			metrics.protocol.framingErrors = framingErrors_.load(std::memory_order_relaxed);
			metrics.protocol.bst93Count = bst93Count_.load(std::memory_order_relaxed);
			metrics.protocol.bst94Count = bst94Count_.load(std::memory_order_relaxed);
			metrics.protocol.bst95Count = bst95Count_.load(std::memory_order_relaxed);
			metrics.protocol.bstD0Count = bstD0Count_.load(std::memory_order_relaxed);
			metrics.protocol.bemCount = bemCount_.load(std::memory_order_relaxed);

			// BEM metrics
			metrics.bem.commandsSent = bemCommandsSent_.load(std::memory_order_relaxed);
			metrics.bem.responsesReceived = bemResponsesReceived_.load(std::memory_order_relaxed);
			metrics.bem.timeouts = bemTimeouts_.load(std::memory_order_relaxed);
			metrics.bem.deviceErrors = bemDeviceErrors_.load(std::memory_order_relaxed);

			// Latency needs mutex
			{
				std::lock_guard<std::mutex> lock(latencyMutex_);
				metrics.bem.totalLatencyMs = totalLatencyMs_;
				metrics.bem.maxLatencyMs = maxLatencyMs_;
				metrics.bem.minLatencyMs = minLatencyMs_;
			}

			// Session metrics
			metrics.reconnectCount = reconnectCount_.load(std::memory_order_relaxed);

			return metrics;
		}

		void MetricsCollector::reset() noexcept {
			// Reset start time
			startTime_ = std::chrono::steady_clock::now();

			// Reset all atomic counters
			bytesSent_.store(0, std::memory_order_relaxed);
			bytesReceived_.store(0, std::memory_order_relaxed);
			writeCalls_.store(0, std::memory_order_relaxed);
			readCalls_.store(0, std::memory_order_relaxed);
			transportErrors_.store(0, std::memory_order_relaxed);
			lastTransportError_.store(0, std::memory_order_relaxed);

			framesReceived_.store(0, std::memory_order_relaxed);
			framesDropped_.store(0, std::memory_order_relaxed);
			checksumErrors_.store(0, std::memory_order_relaxed);
			framingErrors_.store(0, std::memory_order_relaxed);
			bst93Count_.store(0, std::memory_order_relaxed);
			bst94Count_.store(0, std::memory_order_relaxed);
			bst95Count_.store(0, std::memory_order_relaxed);
			bstD0Count_.store(0, std::memory_order_relaxed);
			bemCount_.store(0, std::memory_order_relaxed);

			bemCommandsSent_.store(0, std::memory_order_relaxed);
			bemResponsesReceived_.store(0, std::memory_order_relaxed);
			bemTimeouts_.store(0, std::memory_order_relaxed);
			bemDeviceErrors_.store(0, std::memory_order_relaxed);

			// Reset latency with mutex
			{
				std::lock_guard<std::mutex> lock(latencyMutex_);
				totalLatencyMs_ = 0;
				maxLatencyMs_ = 0;
				minLatencyMs_ = std::numeric_limits<uint32_t>::max();
			}

			reconnectCount_.store(0, std::memory_order_relaxed);
		}

	}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
