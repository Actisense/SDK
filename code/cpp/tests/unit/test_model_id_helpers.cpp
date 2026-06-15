/**************************************************************************//**
\file       test_model_id_helpers.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 15/06/2026
\brief      Unit tests for the ArlModelId helper predicates in bem_types.hpp.
\details    Covers modelIdToString(), supportsProprietaryEnableListF2() and
            rewritesHostTxSidByte0(). The last is the model gate added for
            GIT-109: the Tx-PGN integration sweeps skip byte 0 (the N2K
            Sequence ID) only on NGT-1, which rewrites it on host-Tx, while
            NGX-1 / WGX preserve the host-supplied SID after NGXSW-3897 and
            so get full-payload verification. This test pins that mapping so
            a regression in the gate is caught without needing the two-device
            rig the integration tests require.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_types.hpp"

#include <gtest/gtest.h>
#include <cstdint>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* ========================================================================== */
/* rewritesHostTxSidByte0 — the GIT-109 model gate                            */
/* ========================================================================== */

TEST(ModelIdHelpersTest, Ngt1RewritesHostTxSidByte0)
{
	/* NGT-1 (both serial and USB variants) is the legacy, unfixable case:
	   it overwrites byte 0 (SID) on host-injected BST 94 / D0 frames. */
	EXPECT_TRUE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::NGT1)));
	EXPECT_TRUE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::NGT1_USB)));
}

TEST(ModelIdHelpersTest, NgxAndWgxPreserveHostTxSidByte0)
{
	/* Post-NGXSW-3897 firmware preserves the host-supplied SID, so the
	   integration sweep must compare the full payload including byte 0. */
	EXPECT_FALSE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::NGX1)));
	EXPECT_FALSE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::WGX1)));
}

TEST(ModelIdHelpersTest, OtherModelsDoNotSkipByte0)
{
	/* Every other known model and any unknown id defaults to full-payload
	   matching — only NGT-1 is exempted. */
	EXPECT_FALSE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::NGW1)));
	EXPECT_FALSE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::EMU1)));
	EXPECT_FALSE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::PRO_NDC1)));
	EXPECT_FALSE(rewritesHostTxSidByte0(static_cast<uint16_t>(ArlModelId::Unknown)));
	EXPECT_FALSE(rewritesHostTxSidByte0(0xFFFFu));
}

/* ========================================================================== */
/* supportsProprietaryEnableListF2                                            */
/* ========================================================================== */

TEST(ModelIdHelpersTest, OnlyNgxSupportsProprietaryEnableListF2)
{
	EXPECT_TRUE(supportsProprietaryEnableListF2(static_cast<uint16_t>(ArlModelId::NGX1)));
	EXPECT_FALSE(supportsProprietaryEnableListF2(static_cast<uint16_t>(ArlModelId::NGT1)));
	EXPECT_FALSE(supportsProprietaryEnableListF2(static_cast<uint16_t>(ArlModelId::WGX1)));
	EXPECT_FALSE(supportsProprietaryEnableListF2(0xFFFFu));
}

/* ========================================================================== */
/* modelIdToString                                                            */
/* ========================================================================== */

TEST(ModelIdHelpersTest, ModelIdToStringKnownAndUnknown)
{
	EXPECT_EQ(modelIdToString(static_cast<uint16_t>(ArlModelId::NGT1)), "NGT-1");
	EXPECT_EQ(modelIdToString(static_cast<uint16_t>(ArlModelId::NGT1_USB)), "NGT-1 USB");
	EXPECT_EQ(modelIdToString(static_cast<uint16_t>(ArlModelId::NGX1)), "NGX-1");
	EXPECT_EQ(modelIdToString(static_cast<uint16_t>(ArlModelId::WGX1)), "WGX");
	/* Unknown ids fall through to a hex-tagged label. */
	EXPECT_EQ(modelIdToString(0x1234u), "Model-0x" + std::to_string(0x1234u));
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
