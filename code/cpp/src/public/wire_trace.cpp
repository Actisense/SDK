/*********************************************************************/ /**
 \file       wire_trace.cpp
 \brief      Hex-dump formatter for the SDK wire-trace facility
 \details    See wire_trace.hpp for the format specification.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/wire_trace.hpp"

#include <array>
#include <ctime>
#include <format>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		namespace
		{
			constexpr std::array<char, 16> kHexDigits = {'0', '1', '2', '3', '4', '5', '6', '7',
														 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

			constexpr std::size_t kLocalTsWidth = 12; /* HH:MM:SS.mmm */
			constexpr std::size_t kIsoTsWidth = 24;	  /* YYYY-MM-DDTHH:MM:SS.mmmZ */

			/**************************************************************************/ /**
			 \brief    Format the leading-line timestamp
			 *******************************************************************************/
			std::string renderTimestamp(std::chrono::system_clock::time_point tp,
										bool absoluteTimestamps) {
				const auto t = std::chrono::system_clock::to_time_t(tp);
				const auto subsec = std::chrono::duration_cast<std::chrono::milliseconds>(
									   tp.time_since_epoch())
									   .count() %
								   1000;

				std::tm tm{};
#ifdef _WIN32
				if (absoluteTimestamps) {
					gmtime_s(&tm, &t);
				} else {
					localtime_s(&tm, &t);
				}
#else
				if (absoluteTimestamps) {
					gmtime_r(&t, &tm);
				} else {
					localtime_r(&t, &tm);
				}
#endif

				if (absoluteTimestamps) {
					return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
									   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
									   tm.tm_min, tm.tm_sec, static_cast<int>(subsec));
				}
				return std::format("{:02d}:{:02d}:{:02d}.{:03d}", tm.tm_hour, tm.tm_min, tm.tm_sec,
								   static_cast<int>(subsec));
			}

			/**************************************************************************/ /**
			 \brief    Build the fixed-width hex region for one line
			 \details  Always renders to (3 * bytesPerLine - 1 + halfwayExtra) chars,
					   padding short lines with spaces so the ASCII gutter stays
					   aligned. `halfwayExtra` is 1 when bytesPerLine >= 16, else 0.
			 *******************************************************************************/
			std::string renderHexRegion(std::span<const uint8_t> bytes, std::size_t bytesPerLine) {
				const bool halfway = (bytesPerLine >= 16);
				const std::size_t halfwayPos = bytesPerLine / 2;

				std::string out;
				out.reserve(3 * bytesPerLine + (halfway ? 1 : 0));

				for (std::size_t i = 0; i < bytesPerLine; ++i) {
					if (i > 0) {
						out += ' ';
						if (halfway && i == halfwayPos) {
							out += ' ';
						}
					}
					if (i < bytes.size()) {
						out += kHexDigits[(bytes[i] >> 4) & 0x0F];
						out += kHexDigits[bytes[i] & 0x0F];
					} else {
						out += "  ";
					}
				}
				return out;
			}

			/**************************************************************************/ /**
			 \brief    Render the ASCII gutter for the bytes on this line
			 *******************************************************************************/
			std::string renderAscii(std::span<const uint8_t> bytes) {
				std::string out;
				out.reserve(bytes.size() + 2);
				out += '|';
				for (uint8_t b : bytes) {
					out += (b >= 0x20 && b <= 0x7E) ? static_cast<char>(b) : '.';
				}
				out += '|';
				return out;
			}

			constexpr char dirChar(WireTraceDirection dir) noexcept {
				return dir == WireTraceDirection::Tx ? '>' : '<';
			}

		} /* anonymous namespace */

		void formatHexDumpEvent(const WireTraceConfig& config, WireTraceDirection dir,
								std::span<const uint8_t> data,
								std::chrono::system_clock::time_point timestamp,
								const WireTraceLineSink& lineSink) {
			if (config.format != WireTraceFormat::Hex) {
				return; /* EBL mode is reserved for a follow-up ticket */
			}
			if (!lineSink || data.empty() || config.bytesPerLine == 0) {
				return;
			}

			const std::string leadingTs = renderTimestamp(timestamp, config.absoluteTimestamps);
			const std::size_t tsWidth = config.absoluteTimestamps ? kIsoTsWidth : kLocalTsWidth;
			const std::string blankTs(tsWidth, ' ');
			const char dir_char = dirChar(dir);

			std::size_t offset = 0;
			bool firstLine = true;

			while (offset < data.size()) {
				const std::size_t take = std::min(config.bytesPerLine, data.size() - offset);
				const std::span<const uint8_t> chunk = data.subspan(offset, take);

				std::string line;
				line.reserve(tsWidth + 4 + 3 * config.bytesPerLine + 2 + take + 3);

				line += firstLine ? leadingTs : blankTs;
				line += ' ';
				line += dir_char;
				line += ' ';
				line += renderHexRegion(chunk, config.bytesPerLine);

				if (config.includeAscii) {
					line += "  ";
					line += renderAscii(chunk);
				}

				line += '\n';
				lineSink(line);

				offset += take;
				firstLine = false;
			}
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
