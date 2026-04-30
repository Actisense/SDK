/**************************************************************************/ /**
 \file       bst_decoder.cpp
 \brief      BST frame decoder/encoder implementation
 \details    Thin wrappers around BstFrame: decode() validates raw bytes via
			 BstFrame's constructor; encode() emits the bytes BstFrame already
			 holds. All field-level decoding lives in BstFrame itself.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bst/bst_decoder.hpp"

namespace Actisense
{
	namespace Sdk
	{
		/* BstDecoder ----------------------------------------------------------- */

		std::optional<BstFrame> BstDecoder::decode(ConstByteSpan data,
												   std::string& outError) const {
			if (data.empty()) {
				outError = "Empty BST data";
				return std::nullopt;
			}

			auto frame = BstFrame::fromRawData(data);
			if (!frame) {
				const auto bstId = static_cast<unsigned>(data[0]);
				outError =
					"Malformed or unsupported BST frame (ID 0x" + std::to_string(bstId) + ")";
				return std::nullopt;
			}

			return frame;
		}

		uint32_t BstDecoder::calculatePgn(uint8_t pduf, uint8_t pdus, uint8_t dataPage) noexcept {
			/* PDU2 (PDUF >= 240): PGN = (DP << 16) | (PDUF << 8) | PDUS */
			/* PDU1 (PDUF < 240):  PGN = (DP << 16) | (PDUF << 8) | 0x00 */
			if (pduf >= 240) {
				return (static_cast<uint32_t>(dataPage) << 16) |
					   (static_cast<uint32_t>(pduf) << 8) | static_cast<uint32_t>(pdus);
			}
			return (static_cast<uint32_t>(dataPage) << 16) | (static_cast<uint32_t>(pduf) << 8);
		}

		void BstDecoder::extractPduFields(uint32_t pgn, uint8_t& pduf, uint8_t& pdus,
										  uint8_t& dataPage) noexcept {
			dataPage = static_cast<uint8_t>((pgn >> 16) & 0x03);
			pduf = static_cast<uint8_t>((pgn >> 8) & 0xFF);

			if (pduf >= 240) {
				pdus = static_cast<uint8_t>(pgn & 0xFF);
			} else {
				pdus = 0; /* PDUS is destination for PDU1, not part of PGN */
			}
		}

		/* BstEncoder ----------------------------------------------------------- */

		bool BstEncoder::encode(const BstFrame& frame, std::vector<uint8_t>& outData,
								std::string& outError) const {
			if (!frame.checksumValid()) {
				outError = "BstFrame is not valid";
				outData.clear();
				return false;
			}

			const auto raw = frame.rawData();
			outData.assign(raw.begin(), raw.end());
			return true;
		}

	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
