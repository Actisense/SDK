/*********************************************************************//**
\file       test_bem_correlation_src_addr.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/05/2026
\brief      Unit tests for BEM correlator's srcAddr-keyed pending requests
            (GIT-88).
\details    Two pending requests for the same (BST ID, BEM ID) but different
            N2K source addresses must not collide; replies from one source
            must not satisfy a request bound for another.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_protocol.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string_view>

namespace Actisense
{
	namespace Sdk
	{
		namespace Test
		{
			namespace
			{
				BemResponse makeBemResponse(BemCommandId bemId, uint16_t modelId = 0x003B,
											uint32_t serial = 0)
				{
					BemResponse r;
					r.header.bstId = BstId::Bem_GP_A0;
					r.header.storeLength = 12;
					r.header.bemId = static_cast<uint8_t>(bemId);
					r.header.sequenceId = 0;
					r.header.modelId = modelId;
					r.header.serialNumber = serial;
					r.header.errorCode = 0;
					return r;
				}
			} /* namespace */

			TEST(BemCorrelationSrcAddr, DefaultRegisterTreatsResponseAsLocal)
			{
				BemProtocol bem;
				int fired = 0;
				bem.registerRequest(
					BemCommandId::GetSetOperatingMode, BstId::Bem_PG_A1,
					std::chrono::seconds(1),
					[&](const std::optional<BemResponse>&, ErrorCode,
						std::string_view) { ++fired; });

				const auto rsp = makeBemResponse(BemCommandId::GetSetOperatingMode);
				EXPECT_TRUE(bem.correlateResponse(rsp));
				EXPECT_EQ(fired, 1);
				EXPECT_EQ(bem.pendingRequestCount(), 0u);
			}

			TEST(BemCorrelationSrcAddr, RemoteResponseDoesNotSatisfyLocalRequest)
			{
				BemProtocol bem;
				int fired = 0;
				bem.registerRequest(
					BemCommandId::GetSetOperatingMode, BstId::Bem_PG_A1,
					std::chrono::seconds(1),
					[&](const std::optional<BemResponse>&, ErrorCode,
						std::string_view) { ++fired; });

				const auto rsp = makeBemResponse(BemCommandId::GetSetOperatingMode);
				/* A reply from remote 0x42 must not consume the local request. */
				EXPECT_FALSE(bem.correlateResponse(rsp, /*srcAddr=*/0x42));
				EXPECT_EQ(fired, 0);
				EXPECT_EQ(bem.pendingRequestCount(), 1u);

				/* The local reply still completes the request. */
				EXPECT_TRUE(bem.correlateResponse(rsp));
				EXPECT_EQ(fired, 1);
				EXPECT_EQ(bem.pendingRequestCount(), 0u);
			}

			TEST(BemCorrelationSrcAddr, TwoRemoteRequestsForSameVerbResolveIndependently)
			{
				BemProtocol bem;
				int firedA = 0;
				int firedB = 0;

				bem.registerRequest(
					BemCommandId::GetSetOperatingMode, BstId::Bem_PG_A1,
					std::chrono::seconds(1),
					[&](const std::optional<BemResponse>&, ErrorCode,
						std::string_view) { ++firedA; },
					/*srcAddr=*/0x42);

				bem.registerRequest(
					BemCommandId::GetSetOperatingMode, BstId::Bem_PG_A1,
					std::chrono::seconds(1),
					[&](const std::optional<BemResponse>&, ErrorCode,
						std::string_view) { ++firedB; },
					/*srcAddr=*/0x43);

				EXPECT_EQ(bem.pendingRequestCount(), 2u);

				const auto rsp = makeBemResponse(BemCommandId::GetSetOperatingMode);

				/* Reply from 0x43 — only the B request should resolve. */
				EXPECT_TRUE(bem.correlateResponse(rsp, 0x43));
				EXPECT_EQ(firedA, 0);
				EXPECT_EQ(firedB, 1);
				EXPECT_EQ(bem.pendingRequestCount(), 1u);

				/* Now reply from 0x42 — the A request resolves. */
				EXPECT_TRUE(bem.correlateResponse(rsp, 0x42));
				EXPECT_EQ(firedA, 1);
				EXPECT_EQ(firedB, 1);
				EXPECT_EQ(bem.pendingRequestCount(), 0u);
			}

			TEST(BemCorrelationSrcAddr, MultiReplyRequestKeyedBySrcAddr)
			{
				BemProtocol bem;
				int firedLocal = 0;
				int firedRemote = 0;
				int predicateRemote = 0;

				auto isComplete = [&](const BemResponse&) {
					return ++predicateRemote == 2;
				};

				bem.registerMultiReplyRequest(
					BemCommandId::GetSetOperatingMode, BstId::Bem_PG_A1,
					std::chrono::seconds(1), /*isComplete=*/{},
					[&](const std::optional<BemResponse>&, ErrorCode,
						std::string_view) { ++firedLocal; });

				bem.registerMultiReplyRequest(
					BemCommandId::GetSetOperatingMode, BstId::Bem_PG_A1,
					std::chrono::seconds(1), isComplete,
					[&](const std::optional<BemResponse>&, ErrorCode,
						std::string_view) { ++firedRemote; },
					/*srcAddr=*/0x42);

				EXPECT_EQ(bem.pendingRequestCount(), 2u);

				const auto rsp = makeBemResponse(BemCommandId::GetSetOperatingMode);

				/* Remote reply 1: remote callback fires, neither completes yet
				   (local has no predicate so it auto-completes on first reply). */
				EXPECT_TRUE(bem.correlateResponse(rsp, 0x42));
				EXPECT_EQ(firedRemote, 1);
				EXPECT_EQ(firedLocal, 0);
				EXPECT_EQ(bem.pendingRequestCount(), 2u);

				/* Local reply: local entry resolves and is erased. */
				EXPECT_TRUE(bem.correlateResponse(rsp));
				EXPECT_EQ(firedLocal, 1);
				EXPECT_EQ(bem.pendingRequestCount(), 1u);

				/* Remote reply 2: predicate returns true, remote entry erased. */
				EXPECT_TRUE(bem.correlateResponse(rsp, 0x42));
				EXPECT_EQ(firedRemote, 2);
				EXPECT_EQ(bem.pendingRequestCount(), 0u);
			}

		} /* namespace Test */
	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
