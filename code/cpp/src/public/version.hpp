#ifndef __ACTISENSE_SDK_VERSION_HPP
#define __ACTISENSE_SDK_VERSION_HPP

/**************************************************************************//**
\file       version.hpp
\brief      Actisense SDK version information
\details    Compile-time version constants and runtime version query

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <cstdint>

namespace Actisense
{
namespace Sdk
{
	/* Definitions ---------------------------------------------------------- */

	/**************************************************************************//**
	\brief      SDK version numbers (compile-time constants)
	*******************************************************************************/
	constexpr int VERSION_MAJOR = 0;
	constexpr int VERSION_MINOR = 1;
	constexpr int VERSION_PATCH = 0;

	/**************************************************************************//**
	\brief      Version structure for runtime queries
	*******************************************************************************/
	struct Version
	{
		int major;
		int minor;
		int patch;

		/**************************************************************************//**
		\brief      Convert version to string (e.g., "0.1.0")
		\return     Version string
		*******************************************************************************/
		[[nodiscard]] const char* toString() const noexcept;
	};

	/**************************************************************************//**
	\brief      Get the SDK version at runtime
	\return     Version structure with major, minor, patch numbers
	*******************************************************************************/
	[[nodiscard]] Version version() noexcept;

}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_VERSION_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
