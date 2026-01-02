/**************************************************************************//**
\file       loopback_transport.cpp
\brief      Implementation of in-memory loopback transport
\details    Provides synchronous loopback for protocol testing

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "loopback_transport.hpp"

#include <algorithm>

namespace Actisense
{
namespace Sdk
{
	/* Public Function Definitions ------------------------------------------ */

	LoopbackTransport::LoopbackTransport()
		: mutex_()
		, buffer_()
		, is_open_(false)
		, loopback_enabled_(true)
		, total_bytes_sent_(0)
		, pending_recvs_()
	{
	}

	LoopbackTransport::~LoopbackTransport()
	{
		close();
	}

	void LoopbackTransport::asyncOpen(
		const TransportConfig& config,
		std::function<void(ErrorCode)> completion)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (is_open_)
		{
			if (completion)
			{
				completion(ErrorCode::AlreadyConnected);
			}
			return;
		}

		/* Loopback doesn't validate config - always succeeds */
		(void)config;

		is_open_ = true;
		total_bytes_sent_ = 0;
		buffer_.clear();

		if (completion)
		{
			completion(ErrorCode::Ok);
		}
	}

	void LoopbackTransport::close()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!is_open_)
		{
			return;
		}

		is_open_ = false;

		/* Cancel all pending receives */
		while (!pending_recvs_.empty())
		{
			auto pending = std::move(pending_recvs_.front());
			pending_recvs_.pop();
			
			if (pending.completion)
			{
				pending.completion(ErrorCode::Canceled, 0);
			}
		}

		buffer_.clear();
	}

	bool LoopbackTransport::isOpen() const noexcept
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return is_open_;
	}

	void LoopbackTransport::asyncSend(
		ConstByteSpan data,
		SendCompletionHandler completion)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!is_open_)
		{
			if (completion)
			{
				completion(ErrorCode::NotConnected, 0);
			}
			return;
		}

		std::size_t bytesWritten = 0;

		if (loopback_enabled_)
		{
			/* Write to ring buffer (loopback to receive side) */
			bytesWritten = buffer_.write(data);
			
			if (bytesWritten < data.size())
			{
				/* Buffer full - rate limited */
				if (completion)
				{
					completion(ErrorCode::RateLimited, bytesWritten);
				}
				return;
			}

			/* Try to complete any pending receives */
			tryCompletePendingRecvs();
		}
		else
		{
			/* Not looping back, just count the bytes */
			bytesWritten = data.size();
		}

		total_bytes_sent_ += bytesWritten;

		if (completion)
		{
			completion(ErrorCode::Ok, bytesWritten);
		}
	}

	void LoopbackTransport::asyncRecv(
		ByteSpan buffer,
		RecvCompletionHandler completion)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!is_open_)
		{
			if (completion)
			{
				completion(ErrorCode::NotConnected, 0);
			}
			return;
		}

		/* Try to read immediately if data available */
		if (!buffer_.empty())
		{
			const auto bytesRead = buffer_.read(buffer);
			if (completion)
			{
				completion(ErrorCode::Ok, bytesRead);
			}
			return;
		}

		/* No data available - queue the receive request */
		pending_recvs_.push(PendingRecv{buffer, std::move(completion)});
	}

	TransportKind LoopbackTransport::kind() const noexcept
	{
		return TransportKind::Loopback;
	}

	std::size_t LoopbackTransport::injectData(ConstByteSpan data)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (!is_open_)
		{
			return 0;
		}

		const auto bytesWritten = buffer_.write(data);
		
		/* Try to complete pending receives with injected data */
		tryCompletePendingRecvs();

		return bytesWritten;
	}

	std::size_t LoopbackTransport::bytesAvailable() const noexcept
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return buffer_.size();
	}

	std::size_t LoopbackTransport::bytesSent() const noexcept
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return total_bytes_sent_;
	}

	void LoopbackTransport::clearBuffers()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		buffer_.clear();
	}

	void LoopbackTransport::setLoopbackEnabled(bool enabled) noexcept
	{
		std::lock_guard<std::mutex> lock(mutex_);
		loopback_enabled_ = enabled;
	}

	bool LoopbackTransport::isLoopbackEnabled() const noexcept
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return loopback_enabled_;
	}

	void LoopbackTransport::tryCompletePendingRecvs()
	{
		/* Called with lock held */
		while (!pending_recvs_.empty() && !buffer_.empty())
		{
			auto pending = std::move(pending_recvs_.front());
			pending_recvs_.pop();

			const auto bytesRead = buffer_.read(pending.buffer);
			
			if (pending.completion)
			{
				pending.completion(ErrorCode::Ok, bytesRead);
			}
		}
	}

	TransportPtr createLoopbackTransport()
	{
		return std::make_unique<LoopbackTransport>();
	}

}; /* namespace Sdk */
}; /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
