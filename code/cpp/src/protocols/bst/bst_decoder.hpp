#ifndef __ACTISENSE_SDK_BST_DECODER_HPP
#define __ACTISENSE_SDK_BST_DECODER_HPP

/**************************************************************************//**
\file       bst_decoder.hpp
\brief      BST frame decoders
\details    Decoders for BST-93, BST-94, BST-95, BST-D0 message formats

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bst/bst_types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>

namespace Actisense
{
namespace Sdk
{
	/* Type Aliases --------------------------------------------------------- */
	using ConstByteSpan = std::span<const uint8_t>;

	/* Type Definitions ----------------------------------------------------- */

	/**************************************************************************//**
	\brief      Variant holding any decoded BST frame type
	*******************************************************************************/
	using BstFrameVariant = std::variant<Bst93Frame, Bst94Frame, Bst95Frame, BstD0Frame>;

	/**************************************************************************//**
	\brief      Result of BST frame decoding
	*******************************************************************************/
	struct BstDecodeResult
	{
		bool             success = false;    ///< True if decode succeeded
		std::string      error;              ///< Error message if failed
		BstFrameVariant  frame;              ///< Decoded frame (valid if success)
	};

	/**************************************************************************//**
	\brief      BST frame decoder
	\details    Stateless decoder for BST message formats. Feed it raw BST
	            payloads (after BDTP framing is removed) and it produces
	            structured frame data.
	*******************************************************************************/
	class BstDecoder
	{
	public:
		/**************************************************************************//**
		\brief      Decode a raw BST payload
		\param[in]  data  Raw BST data (starts with BST ID byte)
		\return     Decode result with frame or error
		\details    Automatically dispatches to correct decoder based on BST ID
		*******************************************************************************/
		[[nodiscard]] BstDecodeResult decode(ConstByteSpan data) const;

		/**************************************************************************//**
		\brief      Decode BST-93 frame (Gateway→PC NMEA 2000)
		\param[in]  data  Raw BST-93 payload
		\return     Decoded frame or nullopt on error
		*******************************************************************************/
		[[nodiscard]] std::optional<Bst93Frame> decode93(
			ConstByteSpan data,
			std::string& outError) const;

		/**************************************************************************//**
		\brief      Decode BST-94 frame (PC→Gateway NMEA 2000)
		\param[in]  data  Raw BST-94 payload
		\return     Decoded frame or nullopt on error
		*******************************************************************************/
		[[nodiscard]] std::optional<Bst94Frame> decode94(
			ConstByteSpan data,
			std::string& outError) const;

		/**************************************************************************//**
		\brief      Decode BST-95 frame (CAN Frame)
		\param[in]  data  Raw BST-95 payload
		\return     Decoded frame or nullopt on error
		*******************************************************************************/
		[[nodiscard]] std::optional<Bst95Frame> decode95(
			ConstByteSpan data,
			std::string& outError) const;

		/**************************************************************************//**
		\brief      Decode BST-D0 frame (Latest NMEA 2000)
		\param[in]  data  Raw BST-D0 payload
		\return     Decoded frame or nullopt on error
		*******************************************************************************/
		[[nodiscard]] std::optional<BstD0Frame> decodeD0(
			ConstByteSpan data,
			std::string& outError) const;

		/**************************************************************************//**
		\brief      Calculate PGN from PDU fields
		\param[in]  pduf      PDU Format byte
		\param[in]  pdus      PDU Specific byte
		\param[in]  dataPage  Data page (0-3)
		\return     18-bit PGN value
		*******************************************************************************/
		[[nodiscard]] static uint32_t calculatePgn(
			uint8_t pduf,
			uint8_t pdus,
			uint8_t dataPage) noexcept;

		/**************************************************************************//**
		\brief      Extract PDU fields from a PGN
		\param[in]  pgn       18-bit PGN value
		\param[out] pduf      PDU Format byte
		\param[out] pdus      PDU Specific byte (0 for PDU1)
		\param[out] dataPage  Data page (0-3)
		*******************************************************************************/
		static void extractPduFields(
			uint32_t pgn,
			uint8_t& pduf,
			uint8_t& pdus,
			uint8_t& dataPage) noexcept;

	private:
		/**************************************************************************//**
		\brief      Read little-endian 16-bit value
		*******************************************************************************/
		[[nodiscard]] static uint16_t readU16LE(const uint8_t* p) noexcept;

		/**************************************************************************//**
		\brief      Read little-endian 32-bit value
		*******************************************************************************/
		[[nodiscard]] static uint32_t readU32LE(const uint8_t* p) noexcept;
	};

	/**************************************************************************//**
	\brief      BST frame encoder
	\details    Encodes structured frames into raw BST payloads
	*******************************************************************************/
	class BstEncoder
	{
	public:
		/**************************************************************************//**
		\brief      Encode BST-94 frame for transmission
		\param[in]  frame     Frame data to encode
		\param[out] outData   Encoded BST-94 payload (without BDTP framing)
		\return     True on success
		*******************************************************************************/
		[[nodiscard]] bool encode94(
			const Bst94Frame& frame,
			std::vector<uint8_t>& outData,
			std::string& outError) const;

		/**************************************************************************//**
		\brief      Encode BST-D0 frame for transmission
		\param[in]  frame     Frame data to encode
		\param[out] outData   Encoded BST-D0 payload (without BDTP framing)
		\return     True on success
		*******************************************************************************/
		[[nodiscard]] bool encodeD0(
			const BstD0Frame& frame,
			std::vector<uint8_t>& outData,
			std::string& outError) const;

	private:
		/**************************************************************************//**
		\brief      Write little-endian 16-bit value
		*******************************************************************************/
		static void writeU16LE(uint8_t* p, uint16_t value) noexcept;

		/**************************************************************************//**
		\brief      Write little-endian 32-bit value
		*******************************************************************************/
		static void writeU32LE(uint8_t* p, uint32_t value) noexcept;
	};

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_BST_DECODER_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
