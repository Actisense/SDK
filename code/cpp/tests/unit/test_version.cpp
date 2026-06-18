/**************************************************************************//**
\file       test_version.cpp
\brief      Unit tests for the SDK version constants and runtime query
\details    Locks the public SDK version at 1.0.0 (the initial public,
            SemVer-stable release — GIT-123): the compile-time constants in
            public/version.hpp, the runtime Api::version() query, and the
            Version::toString() formatting must all agree on 1.0.0.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/api.hpp"
#include "public/version.hpp"

#include <gtest/gtest.h>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Compile-time version constants ------------------------------------------- */

TEST(VersionTest, CompileTimeConstantsAreOneZeroZero)
{
	EXPECT_EQ(VERSION_MAJOR, 1);
	EXPECT_EQ(VERSION_MINOR, 0);
	EXPECT_EQ(VERSION_PATCH, 0);
}

/* Runtime query ------------------------------------------------------------ */

TEST(VersionTest, RuntimeVersionMatchesConstants)
{
	const Version v = Api::version();
	EXPECT_EQ(v.major, VERSION_MAJOR);
	EXPECT_EQ(v.minor, VERSION_MINOR);
	EXPECT_EQ(v.patch, VERSION_PATCH);
}

TEST(VersionTest, RuntimeVersionIsOneZeroZero)
{
	const Version v = Api::version();
	EXPECT_EQ(v.major, 1);
	EXPECT_EQ(v.minor, 0);
	EXPECT_EQ(v.patch, 0);
}

/* String formatting -------------------------------------------------------- */

TEST(VersionTest, ToStringIsOneZeroZero)
{
	EXPECT_EQ(Api::version().toString(), "1.0.0");
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
