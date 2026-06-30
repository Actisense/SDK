#ifndef __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_UNSOLICITED
#define __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_UNSOLICITED

/**************************************************************************/ /**
 \file       unsolicited.hpp
 \author     (Created) Phil Whitehurst
 \date       (Created) 30/06/2026
 \brief      Convenience aggregate of the unsolicited BEM payload structures
 \details    Single include pulling in the four typed unsolicited payloads
			 (Startup Status, Error Report, System Status, Negative Ack)
			 delivered as the payload of a typed ParsedMessageEvent. A
			 consumer dispatching on ParsedMessageEvent::messageType can
			 include this one header rather than the four individually
			 (GIT-130).

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/bem_responses/error_report.hpp"
#include "public/bem_responses/negative_ack.hpp"
#include "public/bem_responses/startup_status.hpp"
#include "public/bem_responses/system_status.hpp"

#endif /* __ACTISENSE_SDK_PUBLIC_BEM_RESPONSES_UNSOLICITED */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
