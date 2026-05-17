/**************************************************************************//**
\file       test_pgn_list_management.cpp
\brief      Unit tests for PGN List Management BEM commands
\details    Tests encode/decode for Supported PGN List (0x40), Delete PGN
            Enable Lists (0x4A), Activate PGN Enable Lists (0x4B), Default
            PGN Enable List (0x4C), Params PGN Enable Lists (0x4D), and PGN
            Enable List F1/F2 (0x48-0x4F)

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_commands/supported_pgn_list.hpp"
#include "protocols/bem/bem_commands/delete_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/activate_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/default_pgn_enable_list.hpp"
#include "protocols/bem/bem_commands/params_pgn_enable_lists.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f1.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f1.hpp"
#include "protocols/bem/bem_commands/rx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/tx_pgn_enable_list_f2.hpp"
#include "protocols/bem/bem_commands/bem_commands.hpp"
#include "protocols/bem/bem_protocol.hpp"

#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <thread>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Fixture ------------------------------------------------------------- */

class PgnListManagementTest : public ::testing::Test
{
protected:
	BemProtocol m_protocol;
	std::vector<uint8_t> m_frame;
	std::string m_error;

	void SetUp() override
	{
		m_frame.clear();
		m_error.clear();
	}

	/* Helper to find BEM ID in frame */
	bool findBemIdInFrame(uint8_t bemId) const
	{
		for (std::size_t i = 4; i < m_frame.size() - 3; ++i) {
			if (m_frame[i] == bemId) {
				return true;
			}
		}
		return false;
	}
};

/* Supported PGN List (0x40) Tests ------------------------------------------ */

TEST_F(PgnListManagementTest, SupportedPgnList_EncodeGetRequest)
{
	EXPECT_TRUE(m_protocol.buildGetSupportedPgnList(0, 1, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	EXPECT_TRUE(findBemIdInFrame(0x40)) << "BEM ID 0x40 not found in frame";
}

TEST_F(PgnListManagementTest, SupportedPgnList_DecodeResponse)
{
	/* Wire format (post-BEM-header): xid + SVID + dbVer + total + firstIdx +
	   subCount + (pgnIndex u8, pgn u24 LE) × subCount.
	   This sample: xid=01, SVID=0x1100, dbVer=2100, total=10, firstIdx=0,
	   subCount=3, entries (0,0x01EF60)(1,0x01F010)(2,0x01F114). */
	const std::vector<uint8_t> data = {
		0x01,                    /* transferId */
		0x00, 0x11, 0x00, 0x00,  /* SVID LE = 0x00001100 */
		0x34, 0x08,              /* dbVer LE = 2100 */
		0x0A,                    /* totalListSize */
		0x00,                    /* firstSubIdx */
		0x03,                    /* subCount */
		0x00, 0x60, 0xEF, 0x01,  /* idx 0 -> PGN 0x01EF60 */
		0x01, 0x10, 0xF0, 0x01,  /* idx 1 -> PGN 0x01F010 */
		0x02, 0x14, 0xF1, 0x01,  /* idx 2 -> PGN 0x01F114 */
	};

	SupportedPgnListResponse response;
	EXPECT_TRUE(decodeSupportedPgnListResponse(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.transferId, 1u);
	EXPECT_EQ(response.structureVariantId, kSupportedPgnListSvId);
	EXPECT_EQ(response.nmea2000DbVersion, 2100u);
	EXPECT_EQ(response.totalListSize, 10u);
	EXPECT_EQ(response.firstSubIdx, 0u);
	EXPECT_EQ(response.subCount, 3u);
	ASSERT_EQ(response.entries.size(), 3u);
	EXPECT_EQ(response.entries[0].pgnIndex, 0u);
	EXPECT_EQ(response.entries[0].pgn, 0x01EF60u);
	EXPECT_EQ(response.entries[1].pgnIndex, 1u);
	EXPECT_EQ(response.entries[1].pgn, 0x01F010u);
	EXPECT_EQ(response.entries[2].pgnIndex, 2u);
	EXPECT_EQ(response.entries[2].pgn, 0x01F114u);
}

TEST_F(PgnListManagementTest, SupportedPgnList_HasMore)
{
	SupportedPgnListResponse r;
	r.totalListSize = 10;
	r.firstSubIdx = 0;
	r.subCount = 3;
	EXPECT_TRUE(supportedPgnListHasMore(r));

	r.firstSubIdx = 7;
	r.subCount = 3; /* consumed 10 -> equal to total, no more */
	EXPECT_FALSE(supportedPgnListHasMore(r));
}

TEST_F(PgnListManagementTest, SupportedPgnList_DecodeRejectsWrongSvId)
{
	std::vector<uint8_t> data = {
		0x01, 0xFF, 0xFF, 0xFF, 0xFF, /* xid + bad SVID */
		0x00, 0x00, 0x00, 0x00, 0x00,
	};
	SupportedPgnListResponse response;
	EXPECT_FALSE(decodeSupportedPgnListResponse(data, response, m_error));
	EXPECT_NE(m_error.find("Structure Variant"), std::string::npos);
}

TEST_F(PgnListManagementTest, SupportedPgnList_DecodeTooShort)
{
	const std::vector<uint8_t> shortData = {0x00, 0x01};

	SupportedPgnListResponse response;
	EXPECT_FALSE(decodeSupportedPgnListResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(PgnListManagementTest, SupportedPgnList_DecodeRejectsTruncatedEntries)
{
	std::vector<uint8_t> data = {
		0x01, 0x00, 0x11, 0x00, 0x00, /* xid + SVID */
		0x34, 0x08, 0x0A, 0x00, 0x03, /* dbVer + total=10 + firstIdx=0 + subCount=3 */
		0x00, 0x60, 0xEF, 0x01,       /* only one entry */
	};
	SupportedPgnListResponse response;
	EXPECT_FALSE(decodeSupportedPgnListResponse(data, response, m_error));
	EXPECT_NE(m_error.find("truncated"), std::string::npos);
}

TEST_F(PgnListManagementTest, SupportedPgnList_Constants)
{
	EXPECT_EQ(kSupportedPgnListGetRequestSize, 2u);
	EXPECT_EQ(kSupportedPgnListResponseHeaderSize, 10u);
	EXPECT_EQ(kSupportedPgnListEntrySize, 4u);
	EXPECT_EQ(kSupportedPgnListSvId, 0x00001100u);
	EXPECT_EQ(kSupportedPgnListMaxPgnsPerMessage, 48u);
}

/* Delete PGN Enable Lists (0x4A) Tests ------------------------------------- */

TEST_F(PgnListManagementTest, DeletePgnEnableLists_EncodeRequestRx)
{
	std::vector<uint8_t> data;
	encodeDeletePgnEnableListsRequest(DeletePgnListSelector::RxList, data);

	EXPECT_EQ(data.size(), kDeletePgnEnableListsRequestSize);
	EXPECT_EQ(data[0], 0x00);
}

TEST_F(PgnListManagementTest, DeletePgnEnableLists_EncodeRequestTx)
{
	std::vector<uint8_t> data;
	encodeDeletePgnEnableListsRequest(DeletePgnListSelector::TxList, data);

	EXPECT_EQ(data.size(), kDeletePgnEnableListsRequestSize);
	EXPECT_EQ(data[0], 0x01);
}

TEST_F(PgnListManagementTest, DeletePgnEnableLists_EncodeRequestBoth)
{
	std::vector<uint8_t> data;
	encodeDeletePgnEnableListsRequest(DeletePgnListSelector::Both, data);

	EXPECT_EQ(data.size(), kDeletePgnEnableListsRequestSize);
	EXPECT_EQ(data[0], 0x02);
}

TEST_F(PgnListManagementTest, DeletePgnEnableLists_BuilderValid)
{
	EXPECT_TRUE(m_protocol.buildDeletePgnEnableLists(0x00, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(PgnListManagementTest, DeletePgnEnableLists_BuilderInvalid)
{
	EXPECT_FALSE(m_protocol.buildDeletePgnEnableLists(0x03, m_frame, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(PgnListManagementTest, DeletePgnEnableLists_SelectorToString)
{
	EXPECT_STREQ(deletePgnListSelectorToString(DeletePgnListSelector::RxList), "Rx List");
	EXPECT_STREQ(deletePgnListSelectorToString(DeletePgnListSelector::TxList), "Tx List");
	EXPECT_STREQ(deletePgnListSelectorToString(DeletePgnListSelector::Both), "Both Lists");
}

/* Activate PGN Enable Lists (0x4B) Tests ----------------------------------- */

TEST_F(PgnListManagementTest, ActivatePgnEnableLists_EncodeRequest)
{
	std::vector<uint8_t> data;
	encodeActivatePgnEnableListsRequest(data);

	EXPECT_EQ(data.size(), kActivatePgnEnableListsRequestSize);
	EXPECT_TRUE(data.empty());
}

TEST_F(PgnListManagementTest, ActivatePgnEnableLists_Builder)
{
	EXPECT_TRUE(m_protocol.buildActivatePgnEnableLists(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

/* Default PGN Enable List (0x4C) Tests ------------------------------------- */

TEST_F(PgnListManagementTest, DefaultPgnEnableList_EncodeRequestRx)
{
	std::vector<uint8_t> data;
	encodeDefaultPgnEnableListRequest(DeletePgnListSelector::RxList, data);

	ASSERT_EQ(data.size(), kDefaultPgnEnableListRequestSize);
	EXPECT_EQ(data[0], 0x00);
}

TEST_F(PgnListManagementTest, DefaultPgnEnableList_EncodeRequestTx)
{
	std::vector<uint8_t> data;
	encodeDefaultPgnEnableListRequest(DeletePgnListSelector::TxList, data);

	ASSERT_EQ(data.size(), kDefaultPgnEnableListRequestSize);
	EXPECT_EQ(data[0], 0x01);
}

TEST_F(PgnListManagementTest, DefaultPgnEnableList_Builder)
{
	EXPECT_TRUE(m_protocol.buildDefaultPgnEnableList(DeletePgnListSelector::Both, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

/* Params PGN Enable Lists (0x4D) Tests ------------------------------------- */

TEST_F(PgnListManagementTest, ParamsPgnEnableLists_EncodeRequest)
{
	std::vector<uint8_t> data;
	encodeParamsPgnEnableListsRequest(data);

	EXPECT_EQ(data.size(), kParamsPgnEnableListsRequestSize);
	EXPECT_TRUE(data.empty());
}

TEST_F(PgnListManagementTest, ParamsPgnEnableLists_Builder)
{
	EXPECT_TRUE(m_protocol.buildGetParamsPgnEnableLists(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(PgnListManagementTest, ParamsPgnEnableLists_DecodeResponse)
{
	/* Construct test response data (14 bytes) */
	const std::array<uint8_t, 14> data = {
		0x00, 0x01,   /* Rx max capacity = 256 */
		0x0A, 0x00,   /* Rx session count = 10 */
		0x08, 0x00,   /* Rx active count = 8 */
		0x00, 0x03,   /* Tx max capacity = 768 */
		0x14, 0x00,   /* Tx session count = 20 */
		0x10, 0x00,   /* Tx active count = 16 */
		0x00,         /* Rx sync status = synced */
		0x01          /* Tx sync status = pending */
	};

	ParamsPgnEnableListsResponse response;
	EXPECT_TRUE(decodeParamsPgnEnableListsResponse(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.rxListMaxCapacity, 256u);
	EXPECT_EQ(response.rxListSessionCount, 10u);
	EXPECT_EQ(response.rxListActiveCount, 8u);
	EXPECT_EQ(response.txListMaxCapacity, 768u);
	EXPECT_EQ(response.txListSessionCount, 20u);
	EXPECT_EQ(response.txListActiveCount, 16u);
	EXPECT_TRUE(response.isRxSynced());
	EXPECT_FALSE(response.isTxSynced());
	EXPECT_FALSE(response.isSynced());
}

TEST_F(PgnListManagementTest, ParamsPgnEnableLists_DecodeTooShort)
{
	const std::array<uint8_t, 10> shortData = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	ParamsPgnEnableListsResponse response;
	EXPECT_FALSE(decodeParamsPgnEnableListsResponse(shortData, response, m_error));
	EXPECT_FALSE(m_error.empty());
}

TEST_F(PgnListManagementTest, ParamsPgnEnableLists_FormatOutput)
{
	ParamsPgnEnableListsResponse response;
	response.rxListMaxCapacity = 256;
	response.rxListSessionCount = 10;
	response.rxListActiveCount = 8;
	response.txListMaxCapacity = 768;
	response.txListSessionCount = 20;
	response.txListActiveCount = 16;
	response.rxSyncStatus = 0;
	response.txSyncStatus = 0;

	const std::string result = formatParamsPgnEnableLists(response);
	EXPECT_TRUE(result.find("256") != std::string::npos);
	EXPECT_TRUE(result.find("768") != std::string::npos);
	EXPECT_TRUE(result.find("Synced") != std::string::npos);
}

/* Rx PGN Enable List F2 (0x4E) Tests --------------------------------------- */

TEST_F(PgnListManagementTest, RxPgnEnableListF2_EncodeGetRequest)
{
	std::vector<uint8_t> data;
	encodeRxPgnEnableListF2GetRequest(data);
	EXPECT_TRUE(data.empty());
}

/* Note: 0x4E has no firmware SET handler; use the per-PGN 0x46 command. */

TEST_F(PgnListManagementTest, RxPgnEnableListF2_DecodeResponse)
{
	/* xid=01, SVID=0x1101, total=9, firstIdx=0, subCount=3, three entries */
	const std::vector<uint8_t> data = {
		0x01,
		0x01, 0x11, 0x00, 0x00,
		0x09, 0x00, 0x03,
		0x05, 0x00,
		0x08, 0x01,
		0x0B, 0x00,
	};

	RxPgnEnableListF2Response response;
	EXPECT_TRUE(decodeRxPgnEnableListF2Response(data, response, m_error));
	EXPECT_EQ(response.transferId, 1u);
	EXPECT_EQ(response.structureVariantId, kRxPgnEnableListF2SvId);
	EXPECT_EQ(response.totalListSize, 9u);
	EXPECT_EQ(response.firstSubIdx, 0u);
	EXPECT_EQ(response.subCount, 3u);
	ASSERT_EQ(response.entries.size(), 3u);
	EXPECT_EQ(response.entries[0].pgnIndex, 0x05u);
	EXPECT_EQ(response.entries[0].rxMask, 0x00u);
	EXPECT_EQ(response.entries[1].pgnIndex, 0x08u);
	EXPECT_EQ(response.entries[1].rxMask, 0x01u);
}

TEST_F(PgnListManagementTest, RxPgnEnableListF2_DecodeRejectsWrongSvId)
{
	std::vector<uint8_t> data = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0};
	RxPgnEnableListF2Response response;
	EXPECT_FALSE(decodeRxPgnEnableListF2Response(data, response, m_error));
	EXPECT_NE(m_error.find("Structure Variant"), std::string::npos);
}

TEST_F(PgnListManagementTest, RxPgnEnableListF2_Builder)
{
	EXPECT_TRUE(m_protocol.buildGetRxPgnEnableListF2(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

/* Tx PGN Enable List F2 (0x4F) Tests --------------------------------------- */

TEST_F(PgnListManagementTest, TxPgnEnableListF2_EncodeGetRequest)
{
	std::vector<uint8_t> data;
	encodeTxPgnEnableListF2GetRequest(data);
	EXPECT_TRUE(data.empty());
}

TEST_F(PgnListManagementTest, TxPgnEnableListF2_DecodeStdResponse)
{
	/* xid=01, SVID=0x1102, total=2, firstIdx=0, subCount=2, two entries */
	const std::vector<uint8_t> data = {
		0x01,
		0x02, 0x11, 0x00, 0x00,
		0x02, 0x00, 0x02,
		0x03, 0x06, 0xE8, 0x03,  /* idx 3, prio 6, rate 1000ms */
		0x04, 0x07, 0xFF, 0xFF,  /* idx 4, prio 7, rate disabled */
	};

	TxPgnEnableListF2Response response;
	EXPECT_TRUE(decodeTxPgnEnableListF2Response(data, response, m_error));
	EXPECT_EQ(response.variant, TxPgnEnableListF2Variant::Standard);
	EXPECT_EQ(response.transferId, 1u);
	EXPECT_EQ(response.structureVariantId, kTxPgnEnableListF2StdSvId);
	EXPECT_EQ(response.stdTotalListSize, 2u);
	EXPECT_EQ(response.stdFirstSubIdx, 0u);
	EXPECT_EQ(response.stdSubCount, 2u);
	ASSERT_EQ(response.stdEntries.size(), 2u);
	EXPECT_EQ(response.stdEntries[0].pgnIndex, 0x03u);
	EXPECT_EQ(response.stdEntries[0].priority, 6u);
	EXPECT_EQ(response.stdEntries[0].rateMs, 1000u);
	EXPECT_EQ(response.stdEntries[1].rateMs, kTxPgnRateDisabled);
}

TEST_F(PgnListManagementTest, TxPgnEnableListF2_DecodeProprietaryResponse)
{
	/* xid=02, SVID=0x1103, dp0Size=2, dp0=[01,00], dp1Size=1, dp1=[20] */
	const std::vector<uint8_t> data = {
		0x02,
		0x03, 0x11, 0x00, 0x00,
		0x02, 0x01, 0x00,
		0x01, 0x20,
	};

	TxPgnEnableListF2Response response;
	EXPECT_TRUE(decodeTxPgnEnableListF2Response(data, response, m_error));
	EXPECT_EQ(response.variant, TxPgnEnableListF2Variant::Proprietary);
	ASSERT_EQ(response.propDp0Bitmap.size(), 2u);
	EXPECT_EQ(response.propDp0Bitmap[0], 0x01u);
	EXPECT_EQ(response.propDp0Bitmap[1], 0x00u);
	ASSERT_EQ(response.propDp1Bitmap.size(), 1u);
	EXPECT_EQ(response.propDp1Bitmap[0], 0x20u);
}

TEST_F(PgnListManagementTest, TxPgnEnableListF2_DecodeRejectsUnknownSvId)
{
	std::vector<uint8_t> data = {0x01, 0xFF, 0xFF, 0xFF, 0xFF};
	TxPgnEnableListF2Response response;
	EXPECT_FALSE(decodeTxPgnEnableListF2Response(data, response, m_error));
}

/* Note: 0x4F has no firmware SET handler; use the per-PGN 0x47 command. */

TEST_F(PgnListManagementTest, TxPgnEnableListF2_Builder)
{
	EXPECT_TRUE(m_protocol.buildGetTxPgnEnableListF2(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

/* Rx PGN Enable List F1 (0x48) Tests --------------------------------------- */

TEST_F(PgnListManagementTest, RxPgnEnableListF1_EncodeGetRequest)
{
	std::vector<uint8_t> data;
	encodeRxPgnEnableListF1GetRequest(0, data);

	EXPECT_EQ(data.size(), kRxPgnEnableListF1GetRequestSize);
	EXPECT_EQ(data[0], 0u);
}

TEST_F(PgnListManagementTest, RxPgnEnableListF1_DecodeResponse)
{
	/* Response: message 0, 2 PGNs */
	const std::array<uint8_t, 10> data = {
		0x00,              /* Message index 0 */
		0x02,              /* PGN count = 2 */
		0x10, 0x27, 0x00, 0x00,  /* PGN 10000 */
		0x20, 0x4E, 0x00, 0x00   /* PGN 20000 */
	};

	RxPgnEnableListF1Response response;
	EXPECT_TRUE(decodeRxPgnEnableListF1Response(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.messageIndex, 0u);
	EXPECT_EQ(response.pgnCount, 2u);
	EXPECT_TRUE(response.isFirstMessage());
	EXPECT_FALSE(response.isLastMessage());
	EXPECT_EQ(response.pgns[0], 10000u);
	EXPECT_EQ(response.pgns[1], 20000u);
}

/* The buildGet*PgnEnableListF1 protocol helpers are [[deprecated]] in favour
   of the F2 variants, but the underlying codec still works and these tests
   are the only thing guarding it against regression until removal. Silence
   the deprecation warning locally so the suite still builds clean. */
#if defined(__GNUC__) || defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#	pragma warning(push)
#	pragma warning(disable : 4996)
#endif

TEST_F(PgnListManagementTest, RxPgnEnableListF1_BuilderValid)
{
	EXPECT_TRUE(m_protocol.buildGetRxPgnEnableListF1(0, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());

	m_frame.clear();
	EXPECT_TRUE(m_protocol.buildGetRxPgnEnableListF1(1, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
}

TEST_F(PgnListManagementTest, RxPgnEnableListF1_BuilderInvalid)
{
	EXPECT_FALSE(m_protocol.buildGetRxPgnEnableListF1(2, m_frame, m_error));
	EXPECT_FALSE(m_error.empty());
}

/* Tx PGN Enable List F1 (0x49) Tests --------------------------------------- */

TEST_F(PgnListManagementTest, TxPgnEnableListF1_EncodeGetRequest)
{
	std::vector<uint8_t> data;
	encodeTxPgnEnableListF1GetRequest(2, data);

	EXPECT_EQ(data.size(), kTxPgnEnableListF1GetRequestSize);
	EXPECT_EQ(data[0], 2u);
}

TEST_F(PgnListManagementTest, TxPgnEnableListF1_DecodeResponsePgnList)
{
	/* Response: message 0 (PGN list), 2 PGNs */
	const std::array<uint8_t, 10> data = {
		0x00,              /* Message index 0 (PGN list) */
		0x02,              /* Entry count = 2 */
		0x10, 0x27, 0x00, 0x00,  /* PGN 10000 */
		0x20, 0x4E, 0x00, 0x00   /* PGN 20000 */
	};

	TxPgnEnableListF1Response response;
	EXPECT_TRUE(decodeTxPgnEnableListF1Response(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.messageIndex, 0u);
	EXPECT_EQ(response.entryCount, 2u);
	EXPECT_TRUE(response.isPgnListMessage());
	EXPECT_EQ(response.pgns.size(), 2u);
	EXPECT_EQ(response.pgns[0], 10000u);
	EXPECT_EQ(response.pgns[1], 20000u);
}

TEST_F(PgnListManagementTest, TxPgnEnableListF1_DecodeResponseRatePriority)
{
	/* Response: message 1 (rate/priority), 2 entries */
	const std::array<uint8_t, 6> data = {
		0x01,         /* Message index 1 (rate/priority) */
		0x02,         /* Entry count = 2 */
		0x05, 0x03,   /* Rate 5, Priority 3 */
		0x0A, 0x06    /* Rate 10, Priority 6 */
	};

	TxPgnEnableListF1Response response;
	EXPECT_TRUE(decodeTxPgnEnableListF1Response(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.messageIndex, 1u);
	EXPECT_EQ(response.entryCount, 2u);
	EXPECT_TRUE(response.isRatePriorityMessage());
	EXPECT_EQ(response.ratePriority.size(), 2u);
	EXPECT_EQ(response.ratePriority[0].first, 5u);
	EXPECT_EQ(response.ratePriority[0].second, 3u);
	EXPECT_EQ(response.ratePriority[1].first, 10u);
	EXPECT_EQ(response.ratePriority[1].second, 6u);
}

TEST_F(PgnListManagementTest, TxPgnEnableListF1_BuilderValid)
{
	for (uint8_t i = 0; i <= 3; ++i) {
		m_frame.clear();
		m_error.clear();
		EXPECT_TRUE(m_protocol.buildGetTxPgnEnableListF1(i, m_frame, m_error))
			<< "Failed for message index " << static_cast<int>(i);
		EXPECT_TRUE(m_error.empty());
	}
}

TEST_F(PgnListManagementTest, TxPgnEnableListF1_BuilderInvalid)
{
	EXPECT_FALSE(m_protocol.buildGetTxPgnEnableListF1(4, m_frame, m_error));
	EXPECT_FALSE(m_error.empty());
}

#if defined(__GNUC__) || defined(__clang__)
#	pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#	pragma warning(pop)
#endif

/* Constants Tests ---------------------------------------------------------- */

TEST_F(PgnListManagementTest, Constants)
{
	EXPECT_EQ(kDeletePgnEnableListsRequestSize, 1u);
	EXPECT_EQ(kActivatePgnEnableListsRequestSize, 0u);
	EXPECT_EQ(kDefaultPgnEnableListRequestSize, 1u);
	EXPECT_EQ(kParamsPgnEnableListsResponseSize, 14u);
	EXPECT_EQ(kRxPgnEnableListF2MaxEntriesPerSubList, 96u);
	EXPECT_EQ(kTxPgnEnableListF2StdMaxEntriesPerSubList, 48u);
	EXPECT_EQ(kRxPgnEnableListF1MaxPgns, 50u);
	EXPECT_EQ(kTxPgnEnableListF1MaxPgns, 50u);
}

/* BEM Command ID String Tests ---------------------------------------------- */

TEST_F(PgnListManagementTest, BemCommandIdToString)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSupportedPgnList), "GetSupportedPgnList");
#if defined(__GNUC__) || defined(__clang__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#	pragma warning(push)
#	pragma warning(disable : 4996)
#endif
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetRxPgnEnableListF1), "GetSetRxPgnEnableListF1");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetTxPgnEnableListF1), "GetSetTxPgnEnableListF1");
#if defined(__GNUC__) || defined(__clang__)
#	pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#	pragma warning(pop)
#endif
	EXPECT_EQ(bemCommandIdToString(BemCommandId::DeletePgnEnableLists), "DeletePgnEnableLists");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::ActivatePgnEnableLists), "ActivatePgnEnableLists");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::DefaultPgnEnableList), "DefaultPgnEnableList");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::ParamsPgnEnableLists), "ParamsPgnEnableLists");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetRxPgnEnableListF2), "GetSetRxPgnEnableListF2");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetTxPgnEnableListF2), "GetSetTxPgnEnableListF2");
}

/* Accumulator helpers ------------------------------------------------------ */

namespace
{
	RxPgnEnableListF2Response makeRxSubList(uint8_t xid, uint8_t total, uint8_t first,
											std::vector<RxPgnEnableEntry> entries)
	{
		RxPgnEnableListF2Response msg;
		msg.transferId = xid;
		msg.structureVariantId = kRxPgnEnableListF2SvId;
		msg.totalListSize = total;
		msg.firstSubIdx = first;
		msg.subCount = static_cast<uint8_t>(entries.size());
		msg.entries = std::move(entries);
		return msg;
	}

	TxPgnEnableListF2Response makeTxStd(uint8_t xid, uint8_t total, uint8_t first,
										std::vector<TxPgnEnableEntry> entries)
	{
		TxPgnEnableListF2Response msg;
		msg.transferId = xid;
		msg.structureVariantId = kTxPgnEnableListF2StdSvId;
		msg.variant = TxPgnEnableListF2Variant::Standard;
		msg.stdTotalListSize = total;
		msg.stdFirstSubIdx = first;
		msg.stdSubCount = static_cast<uint8_t>(entries.size());
		msg.stdEntries = std::move(entries);
		return msg;
	}

	TxPgnEnableListF2Response makeTxProp(uint8_t xid, std::vector<uint8_t> dp0,
										 std::vector<uint8_t> dp1)
	{
		TxPgnEnableListF2Response msg;
		msg.transferId = xid;
		msg.structureVariantId = kTxPgnEnableListF2PropSvId;
		msg.variant = TxPgnEnableListF2Variant::Proprietary;
		msg.propDp0Bitmap = std::move(dp0);
		msg.propDp1Bitmap = std::move(dp1);
		return msg;
	}
} /* namespace */

/* Rx F2 accumulator -------------------------------------------------------- */

TEST_F(PgnListManagementTest, RxAccumulator_SingleMessageFullList)
{
	RxPgnEnableListF2Accumulator acc;
	const auto msg = makeRxSubList(5, 2, 0, {{0x01, 0x01}, {0x02, 0x01}});
	EXPECT_EQ(acc.feed(msg, m_error), PgnListAccumulatorStatus::Done);
	EXPECT_TRUE(m_error.empty());
	EXPECT_EQ(acc.result().transferId, 5u);
	EXPECT_EQ(acc.result().totalListSize, 2u);
	ASSERT_EQ(acc.result().entries.size(), 2u);
	EXPECT_EQ(acc.result().entries[0].pgnIndex, 0x01u);
	EXPECT_EQ(acc.result().entries[1].pgnIndex, 0x02u);
}

TEST_F(PgnListManagementTest, RxAccumulator_ThreeSubListTrain)
{
	RxPgnEnableListF2Accumulator acc;
	std::vector<RxPgnEnableEntry> e1(96), e2(96), e3(8);
	for (std::size_t i = 0; i < e1.size(); ++i) e1[i] = {static_cast<uint8_t>(i), 1};
	for (std::size_t i = 0; i < e2.size(); ++i)
		e2[i] = {static_cast<uint8_t>(96 + i), 1};
	for (std::size_t i = 0; i < e3.size(); ++i)
		e3[i] = {static_cast<uint8_t>(192 + i), 1};

	EXPECT_EQ(acc.feed(makeRxSubList(7, 200, 0, e1), m_error),
			  PgnListAccumulatorStatus::Continue);
	EXPECT_EQ(acc.feed(makeRxSubList(7, 200, 96, e2), m_error),
			  PgnListAccumulatorStatus::Continue);
	EXPECT_EQ(acc.feed(makeRxSubList(7, 200, 192, e3), m_error),
			  PgnListAccumulatorStatus::Done);
	EXPECT_TRUE(m_error.empty());
	EXPECT_EQ(acc.result().entries.size(), 200u);
	EXPECT_EQ(acc.result().entries[0].pgnIndex, 0x00u);
	EXPECT_EQ(acc.result().entries[199].pgnIndex, 199u);
}

TEST_F(PgnListManagementTest, RxAccumulator_TransferIdMismatch)
{
	RxPgnEnableListF2Accumulator acc;
	(void)acc.feed(makeRxSubList(1, 4, 0, {{0x10, 1}, {0x11, 1}}), m_error);
	EXPECT_EQ(acc.feed(makeRxSubList(2, 4, 2, {{0x12, 1}, {0x13, 1}}), m_error),
			  PgnListAccumulatorStatus::Mismatch);
	EXPECT_NE(m_error.find("transferId"), std::string::npos);
}

TEST_F(PgnListManagementTest, RxAccumulator_RepeatedSubListDoesNotDoubleCount)
{
	RxPgnEnableListF2Accumulator acc;
	const auto sub = makeRxSubList(3, 4, 0, {{0xAA, 1}, {0xBB, 1}});
	EXPECT_EQ(acc.feed(sub, m_error), PgnListAccumulatorStatus::Continue);
	EXPECT_EQ(acc.feed(sub, m_error), PgnListAccumulatorStatus::Continue);
	EXPECT_EQ(acc.feed(makeRxSubList(3, 4, 2, {{0xCC, 1}, {0xDD, 1}}), m_error),
			  PgnListAccumulatorStatus::Done);
}

/* Tx F2 accumulator -------------------------------------------------------- */

TEST_F(PgnListManagementTest, TxAccumulator_StdOnlyNeverDone)
{
	TxPgnEnableListF2Accumulator acc;
	EXPECT_EQ(acc.feed(makeTxStd(4, 1, 0, {{0x01, 3, 100}}), m_error),
			  PgnListAccumulatorStatus::Continue);
	EXPECT_FALSE(acc.result().proprietaryReceived);
}

TEST_F(PgnListManagementTest, TxAccumulator_EmptyStdPlusEmptyProp)
{
	TxPgnEnableListF2Accumulator acc;
	EXPECT_EQ(acc.feed(makeTxStd(9, 0, 0, {}), m_error),
			  PgnListAccumulatorStatus::Continue);
	EXPECT_EQ(acc.feed(makeTxProp(9, std::vector<uint8_t>(32, 0),
								  std::vector<uint8_t>(32, 0)),
					   m_error),
			  PgnListAccumulatorStatus::Done);
	EXPECT_TRUE(acc.result().proprietaryReceived);
	EXPECT_TRUE(acc.result().proprietary.enabledPgns.empty());
}

TEST_F(PgnListManagementTest, TxAccumulator_DP0LutByte0_0x05_yields_FF00_and_FF02)
{
	TxPgnEnableListF2Accumulator acc;
	(void)acc.feed(makeTxStd(2, 0, 0, {}), m_error);
	std::vector<uint8_t> dp0(32, 0);
	dp0[0] = 0x05; /* bits 0 and 2 */
	EXPECT_EQ(acc.feed(makeTxProp(2, dp0, std::vector<uint8_t>(32, 0)), m_error),
			  PgnListAccumulatorStatus::Done);
	const auto& pgns = acc.result().proprietary.enabledPgns;
	ASSERT_EQ(pgns.size(), 2u);
	EXPECT_EQ(pgns[0], 0x0000FF00u);
	EXPECT_EQ(pgns[1], 0x0000FF02u);
}

TEST_F(PgnListManagementTest, TxAccumulator_DP1LutByte1Bit7_yields_1FF0F)
{
	TxPgnEnableListF2Accumulator acc;
	(void)acc.feed(makeTxStd(8, 0, 0, {}), m_error);
	std::vector<uint8_t> dp1(32, 0);
	dp1[1] = 0x80; /* bit 7 of byte 1 → offset 15 */
	EXPECT_EQ(acc.feed(makeTxProp(8, std::vector<uint8_t>(32, 0), dp1), m_error),
			  PgnListAccumulatorStatus::Done);
	const auto& pgns = acc.result().proprietary.enabledPgns;
	ASSERT_EQ(pgns.size(), 1u);
	EXPECT_EQ(pgns[0], 0x0001FF0Fu);
}

TEST_F(PgnListManagementTest, TxAccumulator_BothPagesSortedDp0ThenDp1)
{
	TxPgnEnableListF2Accumulator acc;
	(void)acc.feed(makeTxStd(11, 0, 0, {}), m_error);
	std::vector<uint8_t> dp0(32, 0), dp1(32, 0);
	dp0[0] = 0x02;       /* PGN 0xFF01 */
	dp0[31] = 0x80;      /* PGN 0xFFFF */
	dp1[0] = 0x01;       /* PGN 0x1FF00 */
	(void)acc.feed(makeTxProp(11, dp0, dp1), m_error);
	const auto& pgns = acc.result().proprietary.enabledPgns;
	ASSERT_EQ(pgns.size(), 3u);
	EXPECT_EQ(pgns[0], 0x0000FF01u);
	EXPECT_EQ(pgns[1], 0x0000FFFFu);
	EXPECT_EQ(pgns[2], 0x0001FF00u);
}

TEST_F(PgnListManagementTest, TxAccumulator_RawLutsPreserved)
{
	TxPgnEnableListF2Accumulator acc;
	(void)acc.feed(makeTxStd(1, 0, 0, {}), m_error);
	std::vector<uint8_t> dp0(32, 0), dp1(32, 0);
	dp0[5] = 0xA5;
	dp1[10] = 0x42;
	(void)acc.feed(makeTxProp(1, dp0, dp1), m_error);
	EXPECT_EQ(acc.result().proprietary.dp0RawLut[5], 0xA5u);
	EXPECT_EQ(acc.result().proprietary.dp1RawLut[10], 0x42u);
}

TEST_F(PgnListManagementTest, TxAccumulator_TransferIdMismatchOnProp)
{
	TxPgnEnableListF2Accumulator acc;
	(void)acc.feed(makeTxStd(5, 1, 0, {{0x01, 3, 100}}), m_error);
	m_error.clear();
	EXPECT_EQ(acc.feed(makeTxProp(6, std::vector<uint8_t>(32, 0),
								  std::vector<uint8_t>(32, 0)),
					   m_error),
			  PgnListAccumulatorStatus::Mismatch);
	EXPECT_NE(m_error.find("transferId"), std::string::npos);
}

/* Multi-reply correlator --------------------------------------------------- */

namespace
{
	BemResponse makeBemResponse(BemCommandId cmd, std::vector<uint8_t> data = {})
	{
		BemResponse r;
		r.header.bstId = BstId::Bem_GP_A0;
		r.header.bemId = static_cast<uint8_t>(cmd);
		r.header.errorCode = 0;
		r.data = std::move(data);
		return r;
	}
} /* namespace */

TEST_F(PgnListManagementTest, MultiReplyCorrelator_PredicateFalseThenTrue)
{
	BemProtocol bem;
	int callbackCount = 0;
	int predicateCount = 0;

	auto isComplete = [&predicateCount](const BemResponse&) {
		return ++predicateCount == 2;
	};
	auto callback = [&callbackCount](const std::optional<BemResponse>&, ErrorCode,
									 std::string_view) { ++callbackCount; };

	bem.registerMultiReplyRequest(BemCommandId::GetSetRxPgnEnableListF2, BstId::Bem_PG_A1,
								  std::chrono::seconds(10), isComplete, callback);

	const auto rsp = makeBemResponse(BemCommandId::GetSetRxPgnEnableListF2);
	EXPECT_TRUE(bem.correlateResponse(rsp));
	EXPECT_EQ(callbackCount, 1);
	EXPECT_EQ(predicateCount, 1);
	EXPECT_EQ(bem.pendingRequestCount(), 1u);

	EXPECT_TRUE(bem.correlateResponse(rsp));
	EXPECT_EQ(callbackCount, 2);
	EXPECT_EQ(predicateCount, 2);
	EXPECT_EQ(bem.pendingRequestCount(), 0u);
}

TEST_F(PgnListManagementTest, MultiReplyCorrelator_TimeoutWhileWaitingFiresCallback)
{
	BemProtocol bem;
	int timeoutCount = 0;
	auto isComplete = [](const BemResponse&) { return false; };
	auto callback = [&timeoutCount](const std::optional<BemResponse>& rsp, ErrorCode ec,
									std::string_view) {
		if (ec == ErrorCode::Timeout && !rsp.has_value()) {
			++timeoutCount;
		}
	};

	bem.registerMultiReplyRequest(BemCommandId::GetSetRxPgnEnableListF2, BstId::Bem_PG_A1,
								  std::chrono::milliseconds(0), isComplete, callback);

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	const auto fired = bem.processTimeouts();
	EXPECT_EQ(fired, 1u);
	EXPECT_EQ(timeoutCount, 1);
	EXPECT_EQ(bem.pendingRequestCount(), 0u);
}

TEST_F(PgnListManagementTest, MultiReplyCorrelator_ResponseRefreshesInactivityWindow)
{
	BemProtocol bem;
	auto isComplete = [](const BemResponse&) { return false; };
	auto callback = [](const std::optional<BemResponse>&, ErrorCode, std::string_view) {};

	/* Long enough that the second response, delivered after a short pause,
	   refreshes sentAt so the immediately-following timeout sweep finds the
	   request still inside its window. */
	bem.registerMultiReplyRequest(BemCommandId::GetSetRxPgnEnableListF2, BstId::Bem_PG_A1,
								  std::chrono::milliseconds(50), isComplete, callback);

	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	const auto rsp = makeBemResponse(BemCommandId::GetSetRxPgnEnableListF2);
	EXPECT_TRUE(bem.correlateResponse(rsp));   /* refreshes sentAt */
	EXPECT_EQ(bem.processTimeouts(), 0u);      /* still within window */
	EXPECT_EQ(bem.pendingRequestCount(), 1u);
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
