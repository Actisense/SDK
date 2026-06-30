/*********************************************************************//**
\file       test_bem_callbacks_standalone.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 15/06/2026
\brief      Standalone-compile check for the public BEM callbacks header.
\details    GIT-112 split the public response/result data structures out of
			the internal protocols/bem/bem_commands/ headers into
			src/public/bem_responses/. This translation unit includes ONLY
			public/bem_callbacks.hpp and exercises the response data types and
			callback aliases reachable through it. If bem_callbacks.hpp ever
			regrows a dependency on an internal header, the companion
			public_headers_no_protocols_include guard test fails; if a moved
			struct goes missing from the public headers, this TU fails to
			compile.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/bem_callbacks.hpp"

#include <gtest/gtest.h>

namespace
{
	using namespace Actisense::Sdk;

	TEST(BemCallbacksStandalone, PublicResponseTypesVisible) {
		ProductInfoResponse productInfo{};
		EXPECT_EQ(productInfo.nmea2000Version, 0);

		PortBaudrateResponse baud{};
		baud.protocol = HardwareProtocol::CanNmea2000;
		EXPECT_EQ(baud.protocol, HardwareProtocol::CanNmea2000);

		CanConfigResponse canConfig{};
		canConfig.name.setManufacturerCode(1857);
		EXPECT_EQ(canConfig.name.manufacturerCode(), 1857);

		CanInfoFieldResponse canInfo{};
		EXPECT_EQ(canInfo.field, CanInfoField::InstallationDesc1);

		RxPgnEnableResponse rx{};
		EXPECT_EQ(rx.enable, RxPgnEnableFlag::Disabled);

		TxPgnEnableResponse tx{};
		EXPECT_EQ(tx.enable, TxPgnEnableFlag::Disabled);

		ParamsPgnEnableListsResponse params{};
		EXPECT_TRUE(params.isSynced());

		SupportedPgnListResult supported{};
		EXPECT_TRUE(supported.entries.empty());

		RxPgnEnableListF2Result rxF2{};
		EXPECT_FALSE(rxF2.proprietaryReceived);

		TxPgnEnableListF2Result txF2{};
		EXPECT_FALSE(txF2.proprietaryReceived);
	}

	TEST(BemCallbacksStandalone, CallbackAliasesUsable) {
		EchoCallback echoCb = [](ErrorCode, std::string_view, std::optional<EchoResponse>,
								 ResponseOrigin) {};
		EXPECT_TRUE(static_cast<bool>(echoCb));

		PortBaudrateCallback baudCb = [](ErrorCode, std::string_view,
										 std::optional<PortBaudrateResponse>, ResponseOrigin) {};
		EXPECT_TRUE(static_cast<bool>(baudCb));

		SupportedPgnListResultCallback supportedCb =
			[](ErrorCode, std::string_view, std::optional<SupportedPgnListResult>,
			   ResponseOrigin) {};
		EXPECT_TRUE(static_cast<bool>(supportedCb));
	}
} // namespace

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
