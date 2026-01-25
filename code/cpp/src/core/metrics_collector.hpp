#ifndef __ACTISENSE_SDK_METRICS_COLLECTOR_HPP
#define __ACTISENSE_SDK_METRICS_COLLECTOR_HPP

/**************************************************************************/ /**
 \file       metrics_collector.hpp
 \brief      Thread-safe metrics collection for Actisense SDK
 \details    Internal class for collecting and aggregating session metrics

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <atomic>
#include <chrono>
#include <mutex>

#include "public/error.hpp"
#include "public/metrics.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Thread-safe metrics collector
		 \details    Provides lock-free counter updates where possible, with mutex
		             protection for snapshot operations. Used internally by Session.
		 *******************************************************************************/
		class MetricsCollector
		{
		public:
			/**************************************************************************/ /**
			 \brief      Constructor - initializes start time
			 *******************************************************************************/
			MetricsCollector();

			/* Transport metrics recording -------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Record bytes sent through transport
			 \param[in]  count  Number of bytes sent
			 *******************************************************************************/
			void recordBytesSent(uint64_t count) noexcept;

			/**************************************************************************/ /**
			 \brief      Record bytes received from transport
			 \param[in]  count  Number of bytes received
			 *******************************************************************************/
			void recordBytesReceived(uint64_t count) noexcept;

			/**************************************************************************/ /**
			 \brief      Record a write operation
			 *******************************************************************************/
			void recordWriteCall() noexcept;

			/**************************************************************************/ /**
			 \brief      Record a read operation
			 *******************************************************************************/
			void recordReadCall() noexcept;

			/**************************************************************************/ /**
			 \brief      Record a transport error
			 \param[in]  code  The error code
			 *******************************************************************************/
			void recordTransportError(ErrorCode code) noexcept;

			/* Protocol metrics recording --------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Record a successfully parsed frame
			 \param[in]  bstId  BST message type ID (0x93, 0x94, 0x95, 0xD0, or BEM)
			 *******************************************************************************/
			void recordFrameParsed(uint8_t bstId) noexcept;

			/**************************************************************************/ /**
			 \brief      Record a dropped (invalid) frame
			 *******************************************************************************/
			void recordFrameDropped() noexcept;

			/**************************************************************************/ /**
			 \brief      Record a checksum error
			 *******************************************************************************/
			void recordChecksumError() noexcept;

			/**************************************************************************/ /**
			 \brief      Record a BDTP framing error
			 *******************************************************************************/
			void recordFramingError() noexcept;

			/* BEM metrics recording -------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Record a BEM command sent
			 *******************************************************************************/
			void recordBemCommand() noexcept;

			/**************************************************************************/ /**
			 \brief      Record a BEM response received
			 \param[in]  latencyMs  Response latency in milliseconds
			 *******************************************************************************/
			void recordBemResponse(uint32_t latencyMs) noexcept;

			/**************************************************************************/ /**
			 \brief      Record a BEM command timeout
			 *******************************************************************************/
			void recordBemTimeout() noexcept;

			/**************************************************************************/ /**
			 \brief      Record a BEM device error response
			 *******************************************************************************/
			void recordBemDeviceError() noexcept;

			/* Session metrics -------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Record a reconnection attempt
			 *******************************************************************************/
			void recordReconnect() noexcept;

			/* Snapshot and reset ----------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Get a snapshot of current metrics
			 \return     Copy of all current metrics (thread-safe)
			 *******************************************************************************/
			[[nodiscard]] SessionMetrics snapshot() const;

			/**************************************************************************/ /**
			 \brief      Reset all metrics to initial state
			 *******************************************************************************/
			void reset() noexcept;

		private:
			// Start time for uptime calculation
			std::chrono::steady_clock::time_point startTime_;

			// Transport metrics (atomic for lock-free updates)
			std::atomic<uint64_t> bytesSent_{0};
			std::atomic<uint64_t> bytesReceived_{0};
			std::atomic<uint64_t> writeCalls_{0};
			std::atomic<uint64_t> readCalls_{0};
			std::atomic<uint32_t> transportErrors_{0};
			std::atomic<int> lastTransportError_{0}; // Stored as int for atomic

			// Protocol metrics
			std::atomic<uint64_t> framesReceived_{0};
			std::atomic<uint64_t> framesDropped_{0};
			std::atomic<uint32_t> checksumErrors_{0};
			std::atomic<uint32_t> framingErrors_{0};
			std::atomic<uint32_t> bst93Count_{0};
			std::atomic<uint32_t> bst94Count_{0};
			std::atomic<uint32_t> bst95Count_{0};
			std::atomic<uint32_t> bstD0Count_{0};
			std::atomic<uint32_t> bemCount_{0};

			// BEM metrics (need mutex for latency min/max)
			std::atomic<uint32_t> bemCommandsSent_{0};
			std::atomic<uint32_t> bemResponsesReceived_{0};
			std::atomic<uint32_t> bemTimeouts_{0};
			std::atomic<uint32_t> bemDeviceErrors_{0};
			mutable std::mutex latencyMutex_;
			uint64_t totalLatencyMs_ = 0;
			uint32_t maxLatencyMs_ = 0;
			uint32_t minLatencyMs_ = std::numeric_limits<uint32_t>::max();

			// Session metrics
			std::atomic<uint32_t> reconnectCount_{0};
		};

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_METRICS_COLLECTOR_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
