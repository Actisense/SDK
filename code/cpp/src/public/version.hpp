#ifndef __ACTISENSE_SDK_VERSION_HPP
#define __ACTISENSE_SDK_VERSION_HPP

/*==============================================================================
\file       version.hpp
\author     (Created) Phil Whitehurst
\date       (Created) 02/01/2026
\brief      Actisense SDK version information
\details    Compile-time version constants and runtime version query

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
==============================================================================*/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>
#include <string>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      SDK version numbers (compile-time constants)
		 *******************************************************************************/
		/* 1.0.0 is the initial public, SemVer-stable release: from here the
		   src/public/ API is under Semantic Versioning. It carries the GIT-115
		   ABI baseline (Session and RemoteDevice as final, non-polymorphic pimpl
		   handles) — future verbs append symbols, not vtable slots — and any
		   public-breaking change now requires a new major version. */
		constexpr int VERSION_MAJOR = 1;
		constexpr int VERSION_MINOR = 0;
		constexpr int VERSION_PATCH = 0;

		/**************************************************************************/ /**
		 \brief      Version structure for runtime queries
		 *******************************************************************************/
		struct Version
		{
			int major;
			int minor;
			int patch;

			/**************************************************************************/ /**
			 \brief      Convert version to string (e.g., "0.1.0")
			 \return     Version string (owns its storage; no aliasing between calls)
			 *******************************************************************************/
			[[nodiscard]] std::string toString() const;
		};

		/**************************************************************************/ /**
		 \brief      Get the SDK version at runtime
		 \return     Version structure with major, minor, patch numbers
		 *******************************************************************************/
		[[nodiscard]] Version version() noexcept;

	} /* namespace Sdk */
} /* namespace Actisense */

#endif /* __ACTISENSE_SDK_VERSION_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
