/*********************************************************************//**
\file       test_api_umbrella.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 30/06/2026
\brief      Compile-time proof that public/api.hpp is a complete umbrella.
\details    GIT-131 made public/api.hpp aggregate the entire public surface so
			that a single #include "public/api.hpp" gives an external consumer
			the whole SDK. This translation unit includes ONLY public/api.hpp
			(no other public or internal header) and then names one type from
			every top-level public header. If api.hpp ever stops aggregating a
			public header, a name below becomes unresolved and this TU fails to
			compile.

			This TU is built against a public-only include path (it links
			actisense_sdk, whose downstream BUILD_INTERFACE include exposes only
			public/). That simultaneously proves api.hpp reaches no internal
			header. The companion static guards are public_api_umbrella_complete
			(every public header is aggregated) and public_headers_no_internal_include.

			When adding a new public header: add it to api.hpp's include list and
			add a line here naming a symbol from it.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"

#include <gtest/gtest.h>

namespace
{
	using namespace Actisense::Sdk;

	// One representative type per top-level public header, all reachable through
	// public/api.hpp alone. sizeof requires the type to be complete (or, for the
	// abstract/pimpl ones, at least fully declared), so this is a pure
	// compile-time reachability check that needs no linking of definitions.
	TEST(ApiUmbrella, EveryPublicHeaderIsReachableThroughApiHpp) {
		EXPECT_GT(sizeof(Api), 0u);                  // api.hpp
		EXPECT_GT(sizeof(ProductInfoResponse), 0u);  // bem_callbacks.hpp
		EXPECT_GT(sizeof(PortBaudrateResponse), 0u); // bem_responses/* (via bem_callbacks.hpp)
		EXPECT_GT(sizeof(SerialConfig), 0u);         // config.hpp
		EXPECT_GT(sizeof(EblWriter), 0u);            // ebl_writer.hpp
		EXPECT_GT(sizeof(ErrorCode), 0u);            // error.hpp
		EXPECT_GT(sizeof(ParsedMessageEvent), 0u);   // events.hpp
		EXPECT_GT(sizeof(HardwareInfo), 0u);         // hardware_info.hpp
		EXPECT_GT(sizeof(ILogger), 0u);              // logging.hpp
		EXPECT_GT(sizeof(SessionMetrics), 0u);       // metrics.hpp
		EXPECT_GT(sizeof(OperatingMode), 0u);        // operating_mode.hpp
		EXPECT_GT(sizeof(HeadingReference), 0u);     // pgn_encoders.hpp
		EXPECT_GT(sizeof(ReceivedFrame), 0u);        // received_frame.hpp
		EXPECT_GT(sizeof(RemoteDevice), 0u);         // remote_device.hpp
		EXPECT_GT(sizeof(ResponseOrigin), 0u);       // response_origin.hpp
		EXPECT_GT(sizeof(SerialDeviceInfo), 0u);     // serial_device_info.hpp
		EXPECT_GT(sizeof(Session), 0u);              // session.hpp
		EXPECT_GT(sizeof(ITransport), 0u);           // transport.hpp
		EXPECT_GT(sizeof(Version), 0u);              // version.hpp
		EXPECT_GT(sizeof(WireTraceConfig), 0u);      // wire_trace.hpp
	}

	// GIT-136: DeletePgnListSelector must be *complete* through public/api.hpp
	// alone, not merely declared.
	//
	// Note this deliberately names enumerators rather than taking sizeof(). The
	// bug being pinned was that public/remote_device.hpp carried only an opaque
	// declaration (`enum class DeletePgnListSelector : uint8_t;`) — and because an
	// opaque enum has a fixed underlying type, sizeof() compiles happily against
	// it. A public-only consumer could therefore name the parameter type but never
	// construct a value to pass to defaultPgnEnableList(). Naming an enumerator is
	// what actually requires the definition, so it is what guards the fix.
	TEST(ApiUmbrella, PgnEnableListSelectorIsCompleteThroughApiHpp) {
		EXPECT_EQ(static_cast<uint8_t>(DeletePgnListSelector::RxList), 0x00u);
		EXPECT_EQ(static_cast<uint8_t>(DeletePgnListSelector::TxList), 0x01u);
		EXPECT_EQ(static_cast<uint8_t>(DeletePgnListSelector::Both), 0x02u);
	}

	// GIT-136: the PGN enable-list verbs must be callable on the public Session.
	// This TU is the only one compiled against a public-only include path, so it
	// is the only place that can prove a consumer with nothing but api.hpp can
	// reach them. Naming each member-function pointer forces the declaration to
	// exist with exactly this signature; the loopback integration test covers the
	// runtime behaviour.
	TEST(ApiUmbrella, SessionPgnEnableVerbsAreReachableThroughApiHpp) {
		EXPECT_NE(&Session::getRxPgnEnable, nullptr);
		EXPECT_NE(&Session::setRxPgnEnable, nullptr);
		EXPECT_NE(&Session::setRxPgnEnableWithMask, nullptr);
		EXPECT_NE(&Session::getTxPgnEnable, nullptr);
		EXPECT_NE(&Session::setTxPgnEnable, nullptr);
		EXPECT_NE(&Session::setTxPgnEnableWithRate, nullptr);
		EXPECT_NE(&Session::activatePgnEnableLists, nullptr);
		EXPECT_NE(&Session::defaultPgnEnableList, nullptr);
		EXPECT_NE(&Session::getSupportedPgnList_All, nullptr);
	}
} // namespace

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
