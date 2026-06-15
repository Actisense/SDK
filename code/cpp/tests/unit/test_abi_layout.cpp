/**************************************************************************/ /**
 \file       test_abi_layout.cpp
 \brief      Compile-time ABI guards for the Session / RemoteDevice handles
 \details    GIT-115 converted Session and RemoteDevice from pure-abstract
			 interfaces into final, non-polymorphic, move-only pimpl handles so
			 the MIT binary SDK has a stable ABI: growth appends member symbols
			 instead of mutating a vtable. These static_asserts fail the build
			 if that property regresses — e.g. if a virtual sneaks back in (which
			 would add a vtable pointer and break sizeof), or if the handle gains
			 a second data member.

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include "public/remote_device.hpp"
#include "public/session.hpp"

namespace
{
	using Actisense::Sdk::RemoteDevice;
	using Actisense::Sdk::Session;

	/* The handle owns exactly one std::unique_ptr<Impl>; use a unique_ptr<int>
	   as the reference size (all default-deleter unique_ptr are one pointer). */
	using PtrSize = std::unique_ptr<int>;

	/* Session ------------------------------------------------------------- */
	static_assert(std::is_final_v<Session>, "Session must be final");
	static_assert(!std::is_polymorphic_v<Session>, "Session must have no vtable");
	static_assert(sizeof(Session) == sizeof(PtrSize), "Session must be exactly one pointer");
	static_assert(std::is_nothrow_move_constructible_v<Session>, "Session must be nothrow-movable");
	static_assert(std::is_nothrow_move_assignable_v<Session>, "Session must be nothrow-move-assign");
	static_assert(!std::is_copy_constructible_v<Session>, "Session must be move-only");
	static_assert(!std::is_copy_assignable_v<Session>, "Session must be move-only");

	/* RemoteDevice -------------------------------------------------------- */
	static_assert(std::is_final_v<RemoteDevice>, "RemoteDevice must be final");
	static_assert(!std::is_polymorphic_v<RemoteDevice>, "RemoteDevice must have no vtable");
	static_assert(sizeof(RemoteDevice) == sizeof(PtrSize),
				  "RemoteDevice must be exactly one pointer");
	static_assert(std::is_nothrow_move_constructible_v<RemoteDevice>,
				  "RemoteDevice must be nothrow-movable");
	static_assert(std::is_nothrow_move_assignable_v<RemoteDevice>,
				  "RemoteDevice must be nothrow-move-assign");
	static_assert(!std::is_copy_constructible_v<RemoteDevice>, "RemoteDevice must be move-only");
	static_assert(!std::is_copy_assignable_v<RemoteDevice>, "RemoteDevice must be move-only");
} /* anonymous namespace */

/* A runtime case so the executable has a test to register; the real coverage
   is the static_asserts above, which are checked at compile time. */
TEST(AbiLayout, HandlesAreStableNonPolymorphicPimpl) {
	SUCCEED();
}

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
