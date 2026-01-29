#ifndef __ACTISENSE_SDK_METRICS_HPP
#define __ACTISENSE_SDK_METRICS_HPP

/**************************************************************************/ /**
 \file       metrics.hpp
 \brief      Metrics data structures for Actisense SDK
 \details    Defines structures for collecting operational metrics and statistics

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstdint>
#include <limits>

#include "public/error.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Transport layer metrics
		 \details    Counters and statistics for transport operations
		 *******************************************************************************/
		struct TransportMetrics
		{
			uint64_t bytesSent = 0;				 ///< Total bytes written to transport
			uint64_t bytesReceived = 0;			 ///< Total bytes read from transport
			uint64_t writeCalls = 0;			 ///< Number of write operations
			uint64_t readCalls = 0;				 ///< Number of read operations
			uint32_t errorsCount = 0;			 ///< Total transport errors encountered
			ErrorCode lastError = ErrorCode::Ok; ///< Most recent error code

			/**************************************************************************/ /**
			 \brief      Reset all counters to zero
			 *******************************************************************************/
			void reset() noexcept {
				bytesSent = 0;
				bytesReceived = 0;
				writeCalls = 0;
				readCalls = 0;
				errorsCount = 0;
				lastError = ErrorCode::Ok;
			}
		};

		/**************************************************************************/ /**
		 \brief      Protocol layer metrics
		 \details    Counters for protocol parsing and frame statistics
		 *******************************************************************************/
		struct ProtocolMetrics
		{
			uint64_t framesReceived = 0; ///< Valid frames successfully parsed
			uint64_t framesDropped = 0;	 ///< Frames discarded (invalid/unsupported)
			uint32_t checksumErrors = 0; ///< Checksum validation failures
			uint32_t framingErrors = 0;	 ///< BDTP framing errors

			// Per-BST-type counters
			uint32_t bst93Count = 0; ///< BST-93 frames received
			uint32_t bst94Count = 0; ///< BST-94 frames received
			uint32_t bst95Count = 0; ///< BST-95 frames received
			uint32_t bstD0Count = 0; ///< BST-D0 frames received
			uint32_t bemCount = 0;	 ///< BEM messages received

			/**************************************************************************/ /**
			 \brief      Reset all counters to zero
			 *******************************************************************************/
			void reset() noexcept {
				framesReceived = 0;
				framesDropped = 0;
				checksumErrors = 0;
				framingErrors = 0;
				bst93Count = 0;
				bst94Count = 0;
				bst95Count = 0;
				bstD0Count = 0;
				bemCount = 0;
			}
		};

		/**************************************************************************/ /**
		 \brief      BEM command/response metrics
		 \details    Counters and latency tracking for BEM operations
		 *******************************************************************************/
		struct BemMetrics
		{
			uint32_t commandsSent = 0;		///< Commands transmitted
			uint32_t responsesReceived = 0; ///< Responses received
			uint32_t timeouts = 0;			///< Commands that timed out
			uint32_t deviceErrors = 0;		///< Device-returned error responses

			// Latency tracking (milliseconds)
			uint64_t totalLatencyMs = 0; ///< Sum of all response latencies
			uint32_t maxLatencyMs = 0;	 ///< Maximum observed latency
			uint32_t minLatencyMs = (std::numeric_limits<uint32_t>::max)(); ///< Minimum latency

			/**************************************************************************/ /**
			 \brief      Calculate average latency
			 \return     Average latency in milliseconds (0 if no responses)
			 *******************************************************************************/
			[[nodiscard]] uint32_t avgLatencyMs() const noexcept {
				return responsesReceived > 0
						   ? static_cast<uint32_t>(totalLatencyMs / responsesReceived)
						   : 0;
			}

			/**************************************************************************/ /**
			 \brief      Record a response latency measurement
			 \param[in]  latencyMs  Response latency in milliseconds
			 *******************************************************************************/
			void recordLatency(uint32_t latencyMs) noexcept {
				totalLatencyMs += latencyMs;
				if (latencyMs > maxLatencyMs) {
					maxLatencyMs = latencyMs;
				}
				if (latencyMs < minLatencyMs) {
					minLatencyMs = latencyMs;
				}
			}

			/**************************************************************************/ /**
			 \brief      Reset all counters to zero
			 *******************************************************************************/
			void reset() noexcept {
				commandsSent = 0;
				responsesReceived = 0;
				timeouts = 0;
				deviceErrors = 0;
				totalLatencyMs = 0;
				maxLatencyMs = 0;
				minLatencyMs = (std::numeric_limits<uint32_t>::max)();
			}
		};

		/**************************************************************************/ /**
		 \brief      Aggregated session metrics
		 \details    Contains all metrics for a session, including transport,
					 protocol, and BEM statistics
		 *******************************************************************************/
		struct SessionMetrics
		{
			TransportMetrics transport; ///< Transport layer metrics
			ProtocolMetrics protocol;	///< Protocol layer metrics
			BemMetrics bem;				///< BEM command/response metrics

			uint64_t uptimeMs = 0;		 ///< Time since session opened (milliseconds)
			uint32_t reconnectCount = 0; ///< Number of reconnection attempts

			/// Timestamp when this snapshot was captured
			std::chrono::steady_clock::time_point capturedAt;

			/**************************************************************************/ /**
			 \brief      Reset all metrics to initial state
			 *******************************************************************************/
			void reset() noexcept {
				transport.reset();
				protocol.reset();
				bem.reset();
				uptimeMs = 0;
				reconnectCount = 0;
			}

			/**************************************************************************/ /**
			 \brief      Get total frames processed (received + dropped)
			 \return     Total frame count
			 *******************************************************************************/
			[[nodiscard]] uint64_t totalFrames() const noexcept {
				return protocol.framesReceived + protocol.framesDropped;
			}

			/**************************************************************************/ /**
			 \brief      Get frame success rate as percentage
			 \return     Success rate (0.0 to 100.0), or 100.0 if no frames
			 *******************************************************************************/
			[[nodiscard]] double frameSuccessRate() const noexcept {
				const auto total = totalFrames();
				if (total == 0) {
					return 100.0;
				}
				return (static_cast<double>(protocol.framesReceived) / static_cast<double>(total)) *
					   100.0;
			}

			/**************************************************************************/ /**
			 \brief      Get BEM command success rate as percentage
			 \return     Success rate (0.0 to 100.0), or 100.0 if no commands
			 *******************************************************************************/
			[[nodiscard]] double bemSuccessRate() const noexcept {
				if (bem.commandsSent == 0) {
					return 100.0;
				}
				const auto successful = bem.responsesReceived - bem.deviceErrors;
				return (static_cast<double>(successful) / static_cast<double>(bem.commandsSent)) *
					   100.0;
			}
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_METRICS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
