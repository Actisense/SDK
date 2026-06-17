/*********************************************************************//**
\file       test_bem_device_error_mapping.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/06/2026
\brief      Unit tests for BEM device-error mapping in the correlator (GIT-127).
\details    A BEM response carrying a non-zero ARL error code in its header must
            be surfaced to the caller as ErrorCode::BemDeviceError (not the
            historic catch-all ErrorCode::UnsupportedOperation, which masked
            every device rejection). The signed ARL value is recovered from the
            unsigned 32-bit header field and, with its human-readable
            description, placed in the error message — the same way the Negative
            Ack path surfaces its rejection reason (GIT-100).

            The reported trigger (GIT-127) was getRxPgnEnable for PGNs 60928 /
            126996 on an NGT-1 whose Rx enable list has those PGNs disabled: the
            firmware correctly returns ES9_N2000_PGN_NOT_ON_LIST (-995) or
            ES9_N2000_PGN_NOT_IN_LIBRARY (-997), which the SDK previously
            collapsed to UnsupportedOperation.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_protocol.hpp"
#include "public/error.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace Actisense
{
	namespace Sdk
	{
		namespace Test
		{
			namespace
			{
				/* Build a one-shot BEM response for the given command carrying a
				   raw ARL error code in its header (as the device would send it:
				   an unsigned little-endian 32-bit field). */
				BemResponse makeErrorResponse(BemCommandId bemId, uint32_t rawErrorCode)
				{
					BemResponse r;
					r.header.bstId = BstId::Bem_GP_A0;
					r.header.storeLength = 12;
					r.header.bemId = static_cast<uint8_t>(bemId);
					r.header.sequenceId = 0;
					r.header.modelId = 0x000E; /* NGT-1 */
					r.header.serialNumber = 0;
					r.header.errorCode = rawErrorCode;
					return r;
				}

				/* Drive a single GetSetRxPgnEnable request/response cycle through
				   the correlator and capture what the callback received. */
				struct Captured
				{
					bool fired = false;
					ErrorCode ec = ErrorCode::Ok;
					std::string msg;
				};

				Captured correlateWithError(uint32_t rawErrorCode)
				{
					BemProtocol bem;
					Captured cap;
					bem.registerRequest(
						BemCommandId::GetSetRxPgnEnable, BstId::Bem_PG_A1,
						std::chrono::seconds(1),
						[&](const std::optional<BemResponse>&, ErrorCode ec,
							std::string_view msg) {
							cap.fired = true;
							cap.ec = ec;
							cap.msg = std::string(msg);
						});

					const auto rsp =
						makeErrorResponse(BemCommandId::GetSetRxPgnEnable, rawErrorCode);
					EXPECT_TRUE(bem.correlateResponse(rsp));
					EXPECT_EQ(bem.pendingRequestCount(), 0u);
					return cap;
				}
			} /* namespace */

			/* ES9_N2000_PGN_NOT_ON_LIST = -995 arrives in the unsigned header as
			   0xFFFFFC1D. The customer's NGT-1 returns this for getRxPgnEnable on
			   a PGN it has Rx-disabled (e.g. 60928). It must surface as
			   BemDeviceError with the signed code and description in the message —
			   NOT UnsupportedOperation. */
			TEST(BemDeviceErrorMapping, PgnNotOnList_SurfacesAsBemDeviceError)
			{
				const auto cap = correlateWithError(static_cast<uint32_t>(-995));
				EXPECT_TRUE(cap.fired);
				EXPECT_EQ(cap.ec, ErrorCode::BemDeviceError);
				EXPECT_NE(cap.ec, ErrorCode::UnsupportedOperation);
				EXPECT_NE(cap.msg.find("-995"), std::string::npos)
					<< "Expected the raw ARL code in the message; got: " << cap.msg;
				EXPECT_NE(cap.msg.find("not on enable list"), std::string::npos)
					<< "Expected the ARL description in the message; got: " << cap.msg;
			}

			/* ES9_N2000_PGN_NOT_IN_LIBRARY = -997 — returned for a PGN absent from
			   the device's NMEA 2000 library (e.g. 126996 on a reduced build). */
			TEST(BemDeviceErrorMapping, PgnNotInLibrary_SurfacesAsBemDeviceError)
			{
				const auto cap = correlateWithError(static_cast<uint32_t>(-997));
				EXPECT_TRUE(cap.fired);
				EXPECT_EQ(cap.ec, ErrorCode::BemDeviceError);
				EXPECT_NE(cap.msg.find("-997"), std::string::npos) << cap.msg;
				EXPECT_NE(cap.msg.find("not in NMEA 2000 library"), std::string::npos)
					<< cap.msg;
			}

			/* An ARL code with no specific description still surfaces as
			   BemDeviceError, with the raw code and the generic fallback text. */
			TEST(BemDeviceErrorMapping, UnknownArlCode_SurfacesAsBemDeviceError)
			{
				const auto cap = correlateWithError(static_cast<uint32_t>(-12345));
				EXPECT_EQ(cap.ec, ErrorCode::BemDeviceError);
				EXPECT_NE(cap.msg.find("-12345"), std::string::npos) << cap.msg;
			}

			/* Regression guard: a success response (errorCode == 0) must still
			   resolve as ErrorCode::Ok with no spurious device-error promotion. */
			TEST(BemDeviceErrorMapping, ZeroErrorCode_ResolvesOk)
			{
				const auto cap = correlateWithError(0);
				EXPECT_TRUE(cap.fired);
				EXPECT_EQ(cap.ec, ErrorCode::Ok);
				EXPECT_TRUE(cap.msg.empty());
			}

		} /* namespace Test */
	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
