#ifndef __ACTISENSE_SDK_UTIL_ENDIAN_HPP
#define __ACTISENSE_SDK_UTIL_ENDIAN_HPP

/**************************************************************************/ /**
 \file       endian.hpp
 \brief      Little-endian read/write helpers for fixed-width integers
 \details    Internal utility used throughout the SDK to pack and unpack
			 little-endian integers into byte buffers. Replaces the
			 hand-rolled `dst[0] = v & 0xFF; dst[1] = (v >> 8) & 0xFF; ...`
			 pattern that was previously duplicated in session_impl.cpp,
			 pgn_encoders.cpp and ebl_writer.cpp.

			 Signed integers are encoded via their two's-complement
			 representation (guaranteed by C++20) so callers can pass
			 int16_t / int32_t / int64_t directly.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		/* Helpers -------------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Write a fixed-width integer to a byte buffer in little-endian
					 order.
		 \param[out] dst    Destination pointer; must have at least sizeof(T) bytes.
		 \param[in]  value  Value to encode. Signed types use two's complement.
		 *******************************************************************************/
		template <typename T>
			requires std::is_integral_v<T>
		void writeLe(uint8_t* dst, T value) noexcept {
			using U = std::make_unsigned_t<T>;
			auto u = static_cast<U>(value);
			for (std::size_t i = 0; i < sizeof(T); ++i) {
				dst[i] = static_cast<uint8_t>(u & 0xFFu);
				u >>= 8;
			}
		}

		/**************************************************************************/ /**
		 \brief      Append a fixed-width integer to a byte vector in little-endian
					 order.
		 \param[in,out] out    Vector to append to.
		 \param[in]     value  Value to encode. Signed types use two's complement.
		 *******************************************************************************/
		template <typename T>
			requires std::is_integral_v<T>
		void appendLe(std::vector<uint8_t>& out, T value) {
			using U = std::make_unsigned_t<T>;
			auto u = static_cast<U>(value);
			for (std::size_t i = 0; i < sizeof(T); ++i) {
				out.push_back(static_cast<uint8_t>(u & 0xFFu));
				u >>= 8;
			}
		}

		/**************************************************************************/ /**
		 \brief      Read a fixed-width integer from a byte buffer in little-endian
					 order.
		 \param[in]  src  Source pointer; must have at least sizeof(T) readable bytes.
		 \return     The decoded value. Signed types are reinterpreted from two's
					 complement.
		 *******************************************************************************/
		template <typename T>
			requires std::is_integral_v<T>
		[[nodiscard]] T readLe(const uint8_t* src) noexcept {
			using U = std::make_unsigned_t<T>;
			U u = 0;
			for (std::size_t i = 0; i < sizeof(T); ++i) {
				u |= static_cast<U>(src[i]) << (i * 8);
			}
			return static_cast<T>(u);
		}

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_UTIL_ENDIAN_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
