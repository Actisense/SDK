/*********************************************************************//**
\file       test_bem_wrap_126720.cpp
\author     (Created) Phil Whitehurst
\date       (Created) 17/05/2026
\brief      Unit tests for the PGN 126720 BEM wrap/unwrap helpers (GIT-88)

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "protocols/bem/bem_wrap_126720.hpp"

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <vector>

namespace Actisense
{
	namespace Sdk
	{
		namespace Test
		{
			TEST(BemWrap126720, HeaderConstantsMatchActisenseManufacturer)
			{
				/* manufacturer 273 -> 0x11, industry 4 + reserved 3 + (273>>8) -> 0x99 */
				EXPECT_EQ(kActisenseManufacturerByte1, 0x11);
				EXPECT_EQ(kActisenseManufacturerByte2, 0x99);
				EXPECT_EQ(kPgn126720, 0x1EF00u);
			}

			TEST(BemWrap126720, WrapPrependsTwoHeaderBytes)
			{
				const std::array<uint8_t, 4> inner{0xA1, 0x01, 0x11, 0x00};
				std::vector<uint8_t> out;
				wrapBemInPgn126720(inner, out);

				ASSERT_EQ(out.size(), inner.size() + 2);
				EXPECT_EQ(out[0], kActisenseManufacturerByte1);
				EXPECT_EQ(out[1], kActisenseManufacturerByte2);
				EXPECT_EQ(out[2], 0xA1);
				EXPECT_EQ(out[3], 0x01);
				EXPECT_EQ(out[4], 0x11);
				EXPECT_EQ(out[5], 0x00);
			}

			TEST(BemWrap126720, WrapClearsExistingBufferContents)
			{
				std::vector<uint8_t> out{0xDE, 0xAD, 0xBE, 0xEF};
				const std::array<uint8_t, 2> inner{0xA1, 0x00};
				wrapBemInPgn126720(inner, out);

				ASSERT_EQ(out.size(), 4u);
				EXPECT_EQ(out[0], kActisenseManufacturerByte1);
				EXPECT_EQ(out[1], kActisenseManufacturerByte2);
			}

			TEST(BemWrap126720, WrapHandlesEmptyInnerPayload)
			{
				std::vector<uint8_t> out;
				wrapBemInPgn126720({}, out);

				ASSERT_EQ(out.size(), 2u);
				EXPECT_EQ(out[0], kActisenseManufacturerByte1);
				EXPECT_EQ(out[1], kActisenseManufacturerByte2);
			}

			TEST(BemWrap126720, UnwrapRoundTripsExactBytes)
			{
				const std::array<uint8_t, 5> inner{0xA0, 0x02, 0x11, 0x00, 0x42};
				std::vector<uint8_t> wrapped;
				wrapBemInPgn126720(inner, wrapped);

				std::span<const uint8_t> recovered;
				ASSERT_TRUE(tryUnwrapBemFromPgn126720(wrapped, recovered));
				ASSERT_EQ(recovered.size(), inner.size());
				for (std::size_t i = 0; i < inner.size(); ++i) {
					EXPECT_EQ(recovered[i], inner[i]) << "byte " << i;
				}
			}

			TEST(BemWrap126720, UnwrapRejectsWrongManufacturerByte1)
			{
				const std::array<uint8_t, 4> bad{0x12, 0x99, 0xA0, 0x00};
				std::span<const uint8_t> out;
				EXPECT_FALSE(tryUnwrapBemFromPgn126720(bad, out));
			}

			TEST(BemWrap126720, UnwrapRejectsWrongManufacturerByte2)
			{
				const std::array<uint8_t, 4> bad{0x11, 0x9A, 0xA0, 0x00};
				std::span<const uint8_t> out;
				EXPECT_FALSE(tryUnwrapBemFromPgn126720(bad, out));
			}

			TEST(BemWrap126720, UnwrapRejectsPayloadWithNoInnerBytes)
			{
				/* Just the two header bytes, no inner BST payload. */
				const std::array<uint8_t, 2> headerOnly{kActisenseManufacturerByte1,
													  kActisenseManufacturerByte2};
				std::span<const uint8_t> out;
				EXPECT_FALSE(tryUnwrapBemFromPgn126720(headerOnly, out));
			}

			TEST(BemWrap126720, UnwrapRejectsTooShortPayload)
			{
				const std::array<uint8_t, 1> tiny{kActisenseManufacturerByte1};
				std::span<const uint8_t> out;
				EXPECT_FALSE(tryUnwrapBemFromPgn126720(tiny, out));

				EXPECT_FALSE(tryUnwrapBemFromPgn126720({}, out));
			}

		} /* namespace Test */
	} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
