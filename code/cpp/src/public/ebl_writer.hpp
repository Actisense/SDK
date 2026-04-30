#ifndef __ACTISENSE_SDK_EBL_WRITER_HPP
#define __ACTISENSE_SDK_EBL_WRITER_HPP

/**************************************************************************/ /**
 \file       ebl_writer.hpp
 \brief      EBL (Enhanced Binary Log) record writer
 \details    Customer-facing helper that emits Actisense EBL log records to a
             user-supplied byte sink. EBL is a self-describing binary format
             carrying timestamp markers, direction markers and raw byte data;
             files written by this class are readable by the EBL Reader tool
             and any other consumer of the Actisense EBL format.

             This is a port of the embedded-side EBL writer (CommonLib's
             EblEmbedded and DesktopLib's EBLDocExportEBL) into the modern
             C++20 SDK so that customers no longer need to reference the
             private internal libraries to capture or replay traffic.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

#include "public/wire_trace.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      EBL framing control codes
		 \details    Records take the form ESC SOH <tag> <ESC-stuffed payload> ESC LF.
					 Any 0x1B byte inside the payload (or in the tag byte) is
					 doubled (0x1B 0x1B) on output so it is not mistaken for the
					 control code.
		 *******************************************************************************/
		inline constexpr uint8_t kEblEscapeCode = 0x1B; ///< ESC: escape introducer
		inline constexpr uint8_t kEblStartCode = 0x01;	///< SOH: start of record
		inline constexpr uint8_t kEblEndCode = 0x0A;	///< LF: end of record

		/**************************************************************************/ /**
		 \brief      Current EBL format version emitted in the Version tag
		 \details    Reader displays this divided by 1000, so 1002 = "1.002".
		 *******************************************************************************/
		inline constexpr uint32_t kEblVersionU32 = 1002u;

		/**************************************************************************/ /**
		 \brief      EBL tag identifiers
		 *******************************************************************************/
		enum class EblTag : uint8_t
		{
			Invalid = 0x00,
			Version = 0x01,			///< 4-byte LE u32 version code
			Description = 0x02,		///< Variable-length text (free-form)
			TimeUtc = 0x03,			///< 8-byte LE u64, Windows FILETIME (100ns since 1601)
			ElementType = 0x04,		///< 1-byte EBLElement classifier
			DirectionMarker = 0x05, ///< 1-byte: 0x00=Rx, 0x01=Tx
			BstRawFrame = 0x07		///< Raw BST message bytes, no DLE framing
		};

		/**************************************************************************/ /**
		 \brief      EBL Direction Marker payload values
		 \details    These are the wire values written into the direction-marker
					 record - distinct from WireTraceDirection's enum integers.
		 *******************************************************************************/
		inline constexpr uint8_t kEblDirRx = 0x00;
		inline constexpr uint8_t kEblDirTx = 0x01;

		/**************************************************************************/ /**
		 \brief      EBL Writer
		 \details    Emits EBL records to a user-supplied byte sink. The class
					 holds no state of its own beyond the sink; callers compose
					 records (preamble, time marker, direction marker, raw stream
					 or BST frame) in whatever order best suits the use case.

					 Typical usage for a wire-trace capture file:
					 \code
					 EblWriter w;
					 w.setSink([&](std::span<const uint8_t> bytes) {
						 file.write(reinterpret_cast<const char*>(bytes.data()),
									bytes.size());
					 });
					 w.writeVersion();
					 w.writeTimeUtc(std::chrono::system_clock::now());
					 // ... per wire event:
					 w.writeDirectionMarker(WireTraceDirection::Tx);
					 w.writeRawStream(payload);
					 \endcode
		 *******************************************************************************/
		class EblWriter
		{
		public:
			/**************************************************************************/ /**
			 \brief      Sink invoked once per record (or once per chunk within a record)
			 *******************************************************************************/
			using ByteSink = std::function<void(std::span<const uint8_t>)>;

			/**************************************************************************/ /**
			 \brief      Default-construct without a sink (no-op until setSink)
			 *******************************************************************************/
			EblWriter() noexcept = default;

			/**************************************************************************/ /**
			 \brief      Construct with a sink installed
			 *******************************************************************************/
			explicit EblWriter(ByteSink sink) noexcept;

			/**************************************************************************/ /**
			 \brief      Replace the current sink. Pass an empty std::function to
						 disable; subsequent writes become no-ops.
			 *******************************************************************************/
			void setSink(ByteSink sink) noexcept;

			/**************************************************************************/ /**
			 \brief      True when a sink is installed
			 *******************************************************************************/
			[[nodiscard]] bool hasSink() const noexcept { return static_cast<bool>(sink_); }

			/**************************************************************************/ /**
			 \brief      Emit an EBLT_Version record (kEblVersionU32, LE u32)
			 *******************************************************************************/
			void writeVersion();

			/**************************************************************************/ /**
			 \brief      Emit an EBLT_Description record with free-form UTF-8 text
			 *******************************************************************************/
			void writeDescription(std::string_view text);

			/**************************************************************************/ /**
			 \brief      Emit an EBLT_TimeUtc record from a chrono time-point
			 \details    Time is converted to Windows FILETIME ticks
						 (100-ns intervals since 1601-01-01 UTC) and emitted as a
						 little-endian uint64_t.
			 *******************************************************************************/
			void writeTimeUtc(std::chrono::system_clock::time_point tp);

			/**************************************************************************/ /**
			 \brief      Emit an EBLT_TimeUtc record from raw FILETIME ticks
			 *******************************************************************************/
			void writeTimeUtcRaw(uint64_t fileTimeTicks);

			/**************************************************************************/ /**
			 \brief      Emit an EBLT_DirectionMarker record (Rx=0x00, Tx=0x01)
			 *******************************************************************************/
			void writeDirectionMarker(WireTraceDirection dir);

			/**************************************************************************/ /**
			 \brief      Emit raw stream bytes outside any tag (with ESC stuffing)
			 \details    Use this for serial/TCP-style byte streams that already
						 contain their own framing (e.g. BDTP DLE/STX/ETX). The
						 reader will display the bytes alongside the most recent
						 timestamp and direction markers.
			 *******************************************************************************/
			void writeRawStream(std::span<const uint8_t> bytes);

			/**************************************************************************/ /**
			 \brief      Emit a single EBLT_BstRawFrame record carrying unframed BST
			 \details    Use this for BST messages that have already been
						 reassembled (i.e. DLE/STX/ETX framing stripped).
			 *******************************************************************************/
			void writeBstRawFrame(std::span<const uint8_t> bst_payload);

			/* Static helpers ------------------------------------------------------- */

			/**************************************************************************/ /**
			 \brief      Convert a chrono system_clock time-point to FILETIME ticks
			 \details    100-nanosecond intervals since January 1, 1601 (UTC),
						 the standard Windows FILETIME epoch used by EBL.
			 \note       Assumes std::chrono::system_clock uses the Unix epoch
						 (guaranteed by C++20).
			 *******************************************************************************/
			[[nodiscard]] static uint64_t toFileTimeTicks(
				std::chrono::system_clock::time_point tp) noexcept;

			/**************************************************************************/ /**
			 \brief      Inverse of toFileTimeTicks()
			 *******************************************************************************/
			[[nodiscard]] static std::chrono::system_clock::time_point fromFileTimeTicks(
				uint64_t ticks) noexcept;

		private:
			/**************************************************************************/ /**
			 \brief      Emit a complete EBL record (ESC SOH tag <data> ESC LF)
						 with payload ESC-stuffing.
			 *******************************************************************************/
			void writeRecord(EblTag tag, std::span<const uint8_t> payload);

			/**************************************************************************/ /**
			 \brief      Append `byte` to `out`, doubling the byte if it equals ESC.
			 *******************************************************************************/
			static void appendStuffed(std::vector<uint8_t>& out, uint8_t byte);

			ByteSink sink_;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_EBL_WRITER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
