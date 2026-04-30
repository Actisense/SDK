#ifndef __ACTISENSE_SDK_BST_DECODER_HPP
#define __ACTISENSE_SDK_BST_DECODER_HPP

/**************************************************************************/ /**
 \file       bst_decoder.hpp
 \brief      BST frame decoder/encoder
 \details    Stateless dispatcher that wraps raw BST payloads into BstFrame and
			 back out again. All field decoding lives in BstFrame itself; this
			 file only handles the boundary between raw byte streams and the
			 unified BstFrame type.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "protocols/bst/bst_frame.hpp"
#include "protocols/bst/bst_types.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* Type Aliases --------------------------------------------------------- */
		using ConstByteSpan = std::span<const uint8_t>;

		/**************************************************************************/ /**
		 \brief      BST frame decoder
		 \details    Stateless dispatcher: feeds raw BST payloads (after BDTP
					 framing is removed) into a BstFrame for unified access.
		 *******************************************************************************/
		class BstDecoder
		{
		public:
			/**************************************************************************/ /**
			 \brief      Decode a raw BST payload into a BstFrame
			 \param[in]  data      Raw BST data (starts with BST ID byte)
			 \param[out] outError  Populated with a description on failure
			 \return     Decoded frame, or nullopt if the payload is malformed
						 or the BST ID is unsupported
			 *******************************************************************************/
			[[nodiscard]] std::optional<BstFrame> decode(ConstByteSpan data,
														 std::string& outError) const;

			/**************************************************************************/ /**
			 \brief      Calculate PGN from PDU fields
			 \param[in]  pduf      PDU Format byte
			 \param[in]  pdus      PDU Specific byte
			 \param[in]  dataPage  Data page (0-3)
			 \return     18-bit PGN value
			 *******************************************************************************/
			[[nodiscard]] static uint32_t calculatePgn(uint8_t pduf, uint8_t pdus,
													   uint8_t dataPage) noexcept;

			/**************************************************************************/ /**
			 \brief      Extract PDU fields from a PGN
			 \param[in]  pgn       18-bit PGN value
			 \param[out] pduf      PDU Format byte
			 \param[out] pdus      PDU Specific byte (0 for PDU1)
			 \param[out] dataPage  Data page (0-3)
			 *******************************************************************************/
			static void extractPduFields(uint32_t pgn, uint8_t& pduf, uint8_t& pdus,
										 uint8_t& dataPage) noexcept;
		};

		/**************************************************************************/ /**
		 \brief      BST frame encoder
		 \details    Produces the raw BST payload (without BDTP framing) for a
					 BstFrame built via one of its create*() factories.
		 *******************************************************************************/
		class BstEncoder
		{
		public:
			/**************************************************************************/ /**
			 \brief      Encode a BstFrame for transmission
			 \param[in]  frame     Frame to encode (typically built via
								   BstFrame::create94 / createD0 / etc.)
			 \param[out] outData   Encoded BST payload (without BDTP framing)
			 \param[out] outError  Populated with a description on failure
			 \return     True on success
			 *******************************************************************************/
			[[nodiscard]] bool encode(const BstFrame& frame, std::vector<uint8_t>& outData,
									  std::string& outError) const;
		};

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BST_DECODER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
