#ifndef __ACTISENSE_SDK_BEM_SYSTEM_STATUS_HPP
#define __ACTISENSE_SDK_BEM_SYSTEM_STATUS_HPP

/**************************************************************************//**
\file       system_status.hpp
\author     (Created) Copilot
\date       (Created) 09/01/2026
\brief      System Status unsolicited message structures
\details    Structures for decoding BEM F2H System Status unsolicited messages.
            This message is sent regularly from devices configured to report status.
            BST Response Id = A0H, BEM Id = F2H

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace Actisense
{
namespace Sdk
{
	/* Definitions ---------------------------------------------------------- */

	/**************************************************************************//**
	\brief      Individual Buffer statistics
	\details    Statistics for each individual buffer (Rx/Tx channel)
	*******************************************************************************/
	struct IndividualBufferStats
	{
		uint8_t rx_bandwidth_;    ///< Receive bandwidth usage (%)
		uint8_t rx_loading_;      ///< Receive loading (%)
		uint8_t rx_filtered_;     ///< Receive filtered packets (%)
		uint8_t rx_dropped_;      ///< Receive dropped packets (%)
		uint8_t tx_bandwidth_;    ///< Transmit bandwidth usage (%)
		uint8_t tx_loading_;      ///< Transmit loading (%)
	};

	/**************************************************************************//**
	\brief      Unified Buffer statistics
	\details    Statistics for each unified buffer
	*******************************************************************************/
	struct UnifiedBufferStats
	{
		uint8_t bandwidth_;        ///< Buffer bandwidth usage (%)
		uint8_t deleted_;          ///< Deleted packets (%)
		uint8_t loading_;          ///< Buffer loading (%)
		uint8_t pointer_loading_;  ///< Pointer queue loading (%)
	};

	/**************************************************************************//**
	\brief      CAN Extended Status (optional)
	\details    CAN bus error counters and status flags
	*******************************************************************************/
	struct CanExtendedStatus
	{
		uint8_t rx_error_count_;  ///< CAN bus receive error count
		uint8_t tx_error_count_;  ///< CAN bus transmit error count
		uint8_t can_status_;      ///< CAN bus status flags
	};

	/**************************************************************************//**
	\brief      System Status message data
	\details    Decoded system status from BEM F2H unsolicited message
	*******************************************************************************/
	struct SystemStatusData
	{
		std::vector<IndividualBufferStats> individual_buffers_;  ///< Individual buffer stats
		std::vector<UnifiedBufferStats>    unified_buffers_;     ///< Unified buffer stats
		std::optional<CanExtendedStatus>   can_status_;          ///< Optional CAN status
		std::optional<uint16_t>            operating_mode_;      ///< Optional operating mode
	};

	/**************************************************************************//**
	\brief      Decode System Status from BEM data block
	\param[in]  data       Raw BEM data block (after header)
	\param[in]  data_size  Size of the data block
	\param[out] out_error  Error message if decoding fails
	\return     Decoded system status or nullopt on error
	*******************************************************************************/
	[[nodiscard]] inline std::optional<SystemStatusData> decodeSystemStatus(
		const uint8_t* data,
		std::size_t data_size,
		std::string& out_error)
	{
		if (data_size < 1) {
			out_error = "System status data too short";
			return std::nullopt;
		}

		SystemStatusData status;
		std::size_t offset = 0;

		/* Parse Individual Buffer count and entries */
		const uint8_t num_individual_buffers = data[offset++];
		if (num_individual_buffers < 1 || num_individual_buffers > 16) {
			out_error = "Invalid individual buffer count: " + std::to_string(num_individual_buffers);
			return std::nullopt;
		}

		const std::size_t indi_bytes_needed = static_cast<std::size_t>(num_individual_buffers) * 6;
		if (offset + indi_bytes_needed > data_size) {
			out_error = "Data too short for individual buffers";
			return std::nullopt;
		}

		status.individual_buffers_.reserve(num_individual_buffers);
		for (uint8_t i = 0; i < num_individual_buffers; ++i) {
			IndividualBufferStats buf{};
			buf.rx_bandwidth_ = data[offset++];
			buf.rx_loading_   = data[offset++];
			buf.rx_filtered_  = data[offset++];
			buf.rx_dropped_   = data[offset++];
			buf.tx_bandwidth_ = data[offset++];
			buf.tx_loading_   = data[offset++];
			status.individual_buffers_.push_back(buf);
		}

		/* Check if we have unified buffer header */
		if (offset >= data_size) {
			/* No unified buffers or extended data */
			return status;
		}

		/* Parse Unified Buffer count and entries */
		const uint8_t num_unified_buffers = data[offset++];
		if (num_unified_buffers > 8) {
			out_error = "Invalid unified buffer count: " + std::to_string(num_unified_buffers);
			return std::nullopt;
		}

		const std::size_t uni_bytes_needed = static_cast<std::size_t>(num_unified_buffers) * 4;
		if (offset + uni_bytes_needed > data_size) {
			out_error = "Data too short for unified buffers";
			return std::nullopt;
		}

		status.unified_buffers_.reserve(num_unified_buffers);
		for (uint8_t j = 0; j < num_unified_buffers; ++j) {
			UnifiedBufferStats buf{};
			buf.bandwidth_       = data[offset++];
			buf.deleted_         = data[offset++];
			buf.loading_         = data[offset++];
			buf.pointer_loading_ = data[offset++];
			status.unified_buffers_.push_back(buf);
		}

		/* Check for CAN Extended Status (3 bytes) */
		const std::size_t remaining = data_size - offset;
		if (remaining >= 3) {
			CanExtendedStatus can{};
			can.rx_error_count_ = data[offset++];
			can.tx_error_count_ = data[offset++];
			can.can_status_     = data[offset++];
			status.can_status_  = can;
		}

		/* Check for Operating Mode (2 bytes after CAN fields) */
		const std::size_t remaining_after_can = data_size - offset;
		if (remaining_after_can >= 2) {
			const uint16_t mode = static_cast<uint16_t>(data[offset]) |
			                      (static_cast<uint16_t>(data[offset + 1]) << 8);
			status.operating_mode_ = mode;
		}

		return status;
	}

	/**************************************************************************//**
	\brief      Format System Status as human-readable string
	\param[in]  status  Decoded system status
	\return     Formatted string representation
	*******************************************************************************/
	[[nodiscard]] inline std::string formatSystemStatus(const SystemStatusData& status)
	{
		std::string result;
		result.reserve(512);

		result += "System Status:\n";
		result += "  Individual Buffers (" + std::to_string(status.individual_buffers_.size()) + "):\n";
		
		for (std::size_t i = 0; i < status.individual_buffers_.size(); ++i) {
			const auto& buf = status.individual_buffers_[i];
			result += "    [" + std::to_string(i) + "] Rx: BW=" + std::to_string(buf.rx_bandwidth_) + 
			          "% Load=" + std::to_string(buf.rx_loading_) +
			          "% Filt=" + std::to_string(buf.rx_filtered_) +
			          "% Drop=" + std::to_string(buf.rx_dropped_) +
			          "% | Tx: BW=" + std::to_string(buf.tx_bandwidth_) +
			          "% Load=" + std::to_string(buf.tx_loading_) + "%\n";
		}

		result += "  Unified Buffers (" + std::to_string(status.unified_buffers_.size()) + "):\n";
		for (std::size_t j = 0; j < status.unified_buffers_.size(); ++j) {
			const auto& buf = status.unified_buffers_[j];
			result += "    [" + std::to_string(j) + "] BW=" + std::to_string(buf.bandwidth_) +
			          "% Del=" + std::to_string(buf.deleted_) +
			          "% Load=" + std::to_string(buf.loading_) +
			          "% PtrLoad=" + std::to_string(buf.pointer_loading_) + "%\n";
		}

		if (status.can_status_.has_value()) {
			const auto& can = *status.can_status_;
			result += "  CAN Status: RxErr=" + std::to_string(can.rx_error_count_) +
			          " TxErr=" + std::to_string(can.tx_error_count_) +
			          " Status=0x" + std::to_string(can.can_status_) + "\n";
		}

		if (status.operating_mode_.has_value()) {
			result += "  Operating Mode: 0x" + std::to_string(*status.operating_mode_) + "\n";
		}

		return result;
	}

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BEM_SYSTEM_STATUS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
