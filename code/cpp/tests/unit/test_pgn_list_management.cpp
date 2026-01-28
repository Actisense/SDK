/**************************************************************************//**
\file       test_pgn_list_management.cpp
\brief      Unit tests for PGN List Management BEM commands
\details    Tests encode/decode for Delete PGN Enable Lists (0x4A),
            Activate PGN Enable Lists (0x4B), Default PGN Enable List (0x4C),
            Params PGN Enable Lists (0x4D), and PGN Enable List F1/F2 (0x48-0x4F)

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
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
};

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

TEST_F(PgnListManagementTest, DefaultPgnEnableList_EncodeRequest)
{
	std::vector<uint8_t> data;
	encodeDefaultPgnEnableListRequest(data);

	EXPECT_EQ(data.size(), kDefaultPgnEnableListRequestSize);
	EXPECT_TRUE(data.empty());
}

TEST_F(PgnListManagementTest, DefaultPgnEnableList_Builder)
{
	EXPECT_TRUE(m_protocol.buildDefaultPgnEnableList(m_frame, m_error));
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

/* PGN Index Encoding Tests ------------------------------------------------- */

TEST_F(PgnListManagementTest, PgnIndex_StandardPgnToPgnIndex)
{
	uint16_t index;

	/* Standard PGN 0 -> index 1 */
	EXPECT_TRUE(pgnToPgnIndex(0, index));
	EXPECT_EQ(index, 1u);

	/* Standard PGN 254 -> index 255 */
	EXPECT_TRUE(pgnToPgnIndex(254, index));
	EXPECT_EQ(index, 255u);

	/* Invalid standard PGN 255 - not in valid range */
	EXPECT_FALSE(pgnToPgnIndex(255, index));
}

TEST_F(PgnListManagementTest, PgnIndex_ProprietaryPgnToPgnIndex)
{
	uint16_t index;

	/* Proprietary PGN 0xFF000000 -> index 256 */
	EXPECT_TRUE(pgnToPgnIndex(0xFF000000, index));
	EXPECT_EQ(index, 256u);

	/* Proprietary PGN 0xFF0001FF -> index 767 */
	EXPECT_TRUE(pgnToPgnIndex(0xFF0001FF, index));
	EXPECT_EQ(index, 767u);

	/* Invalid proprietary PGN 0xFF000200 - out of range */
	EXPECT_FALSE(pgnToPgnIndex(0xFF000200, index));
}

TEST_F(PgnListManagementTest, PgnIndex_IndexToStandardPgn)
{
	uint32_t pgn;

	/* Index 1 -> PGN 0 */
	EXPECT_TRUE(pgnIndexToPgn(1, pgn));
	EXPECT_EQ(pgn, 0u);

	/* Index 255 -> PGN 254 */
	EXPECT_TRUE(pgnIndexToPgn(255, pgn));
	EXPECT_EQ(pgn, 254u);

	/* Invalid index 0 */
	EXPECT_FALSE(pgnIndexToPgn(0, pgn));
}

TEST_F(PgnListManagementTest, PgnIndex_IndexToProprietaryPgn)
{
	uint32_t pgn;

	/* Index 256 -> PGN 0xFF000000 */
	EXPECT_TRUE(pgnIndexToPgn(256, pgn));
	EXPECT_EQ(pgn, 0xFF000000u);

	/* Index 767 -> PGN 0xFF0001FF */
	EXPECT_TRUE(pgnIndexToPgn(767, pgn));
	EXPECT_EQ(pgn, 0xFF0001FFu);

	/* Invalid index 768 */
	EXPECT_FALSE(pgnIndexToPgn(768, pgn));
}

/* Rx PGN Enable List F2 (0x4E) Tests --------------------------------------- */

TEST_F(PgnListManagementTest, RxPgnEnableListF2_EncodeGetRequest)
{
	std::vector<uint8_t> data;
	encodeRxPgnEnableListF2GetRequest(data);

	EXPECT_EQ(data.size(), kRxPgnEnableListF2GetRequestSize);
	EXPECT_TRUE(data.empty());
}

TEST_F(PgnListManagementTest, RxPgnEnableListF2_EncodeSetRequest)
{
	std::vector<uint32_t> pgns = {0, 10, 100, 254};  /* Standard PGNs */
	std::vector<uint8_t> data;

	EXPECT_TRUE(encodeRxPgnEnableListF2SetRequest(pgns, data, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_EQ(data.size(), 2 + pgns.size() * 2);  /* count + indices */

	/* Check count */
	EXPECT_EQ(data[0], 4u);
	EXPECT_EQ(data[1], 0u);
}

TEST_F(PgnListManagementTest, RxPgnEnableListF2_DecodeResponse)
{
	/* Response: 3 PGNs (indices 1, 100, 256) */
	const std::array<uint8_t, 8> data = {
		0x03, 0x00,   /* Count = 3 */
		0x01, 0x00,   /* Index 1 = PGN 0 */
		0x64, 0x00,   /* Index 100 = PGN 99 */
		0x00, 0x01    /* Index 256 = PGN 0xFF000000 */
	};

	RxPgnEnableListF2Response response;
	EXPECT_TRUE(decodeRxPgnEnableListF2Response(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.pgnCount, 3u);
	EXPECT_EQ(response.pgns.size(), 3u);
	EXPECT_EQ(response.pgns[0], 0u);
	EXPECT_EQ(response.pgns[1], 99u);
	EXPECT_EQ(response.pgns[2], 0xFF000000u);
}

TEST_F(PgnListManagementTest, RxPgnEnableListF2_Builder)
{
	EXPECT_TRUE(m_protocol.buildGetRxPgnEnableListF2(m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

TEST_F(PgnListManagementTest, RxPgnEnableListF2_SetBuilder)
{
	std::vector<uint32_t> pgns = {0, 10, 100};
	EXPECT_TRUE(m_protocol.buildSetRxPgnEnableListF2(pgns, m_frame, m_error));
	EXPECT_TRUE(m_error.empty());
	EXPECT_FALSE(m_frame.empty());
}

/* Tx PGN Enable List F2 (0x4F) Tests --------------------------------------- */

TEST_F(PgnListManagementTest, TxPgnEnableListF2_EncodeGetRequest)
{
	std::vector<uint8_t> data;
	encodeTxPgnEnableListF2GetRequest(data);

	EXPECT_EQ(data.size(), kTxPgnEnableListF2GetRequestSize);
	EXPECT_TRUE(data.empty());
}

TEST_F(PgnListManagementTest, TxPgnEnableListF2_DecodeResponse)
{
	/* Response: 2 entries */
	const std::array<uint8_t, 10> data = {
		0x02, 0x00,   /* Count = 2 */
		0x01, 0x00,   /* Index 1 = PGN 0 */
		0x05,         /* Rate */
		0x03,         /* Priority */
		0x64, 0x00,   /* Index 100 = PGN 99 */
		0x0A,         /* Rate */
		0x06          /* Priority */
	};

	TxPgnEnableListF2Response response;
	EXPECT_TRUE(decodeTxPgnEnableListF2Response(data, response, m_error));
	EXPECT_TRUE(m_error.empty());

	EXPECT_EQ(response.pgnCount, 2u);
	EXPECT_EQ(response.entries.size(), 2u);
	EXPECT_EQ(response.entries[0].pgn, 0u);
	EXPECT_EQ(response.entries[0].rate, 5u);
	EXPECT_EQ(response.entries[0].priority, 3u);
	EXPECT_EQ(response.entries[1].pgn, 99u);
	EXPECT_EQ(response.entries[1].rate, 10u);
	EXPECT_EQ(response.entries[1].priority, 6u);
}

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

/* Constants Tests ---------------------------------------------------------- */

TEST_F(PgnListManagementTest, Constants)
{
	EXPECT_EQ(kDeletePgnEnableListsRequestSize, 1u);
	EXPECT_EQ(kActivatePgnEnableListsRequestSize, 0u);
	EXPECT_EQ(kDefaultPgnEnableListRequestSize, 0u);
	EXPECT_EQ(kParamsPgnEnableListsResponseSize, 14u);
	EXPECT_EQ(kRxPgnEnableListF2MaxPgns, 255u);
	EXPECT_EQ(kTxPgnEnableListF2MaxPgns, 767u);
	EXPECT_EQ(kRxPgnEnableListF1MaxPgns, 50u);
	EXPECT_EQ(kTxPgnEnableListF1MaxPgns, 50u);
}

/* BEM Command ID String Tests ---------------------------------------------- */

TEST_F(PgnListManagementTest, BemCommandIdToString)
{
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetRxPgnEnableListF1), "GetSetRxPgnEnableListF1");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetTxPgnEnableListF1), "GetSetTxPgnEnableListF1");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::DeletePgnEnableLists), "DeletePgnEnableLists");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::ActivatePgnEnableLists), "ActivatePgnEnableLists");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::DefaultPgnEnableList), "DefaultPgnEnableList");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::ParamsPgnEnableLists), "ParamsPgnEnableLists");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetRxPgnEnableListF2), "GetSetRxPgnEnableListF2");
	EXPECT_EQ(bemCommandIdToString(BemCommandId::GetSetTxPgnEnableListF2), "GetSetTxPgnEnableListF2");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */
