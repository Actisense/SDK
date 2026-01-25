#ifndef __ACTISENSE_SDK_LOG_MACROS_HPP
#define __ACTISENSE_SDK_LOG_MACROS_HPP

/**************************************************************************/ /**
 \file       log_macros.hpp
 \brief      Convenience logging macros for Actisense SDK
 \details    Provides SDK_LOG_* macros for efficient logging with file/line info

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include "public/logging.hpp"

/**************************************************************************/ /**
 \brief      Internal logging macro implementation
 \details    Checks isEnabled() before formatting to avoid overhead when disabled
 *******************************************************************************/
#define SDK_LOG_IMPL(level, category, msg)                                    \
	do {                                                                      \
		auto& _sdk_log = ::Actisense::Sdk::logger();                          \
		if (_sdk_log.isEnabled(level, category)) {                            \
			_sdk_log.log(level, category, msg, __FILE__, __LINE__);           \
		}                                                                     \
	} while (0)

/**************************************************************************/ /**
 \brief      Log an error message
 \param[in]  category  LogCategory enum value
 \param[in]  msg       Message string (string_view compatible)
 *******************************************************************************/
#define SDK_LOG_ERROR(category, msg) \
	SDK_LOG_IMPL(::Actisense::Sdk::LogLevel::Error, category, msg)

/**************************************************************************/ /**
 \brief      Log a warning message
 \param[in]  category  LogCategory enum value
 \param[in]  msg       Message string (string_view compatible)
 *******************************************************************************/
#define SDK_LOG_WARN(category, msg) \
	SDK_LOG_IMPL(::Actisense::Sdk::LogLevel::Warn, category, msg)

/**************************************************************************/ /**
 \brief      Log an info message
 \param[in]  category  LogCategory enum value
 \param[in]  msg       Message string (string_view compatible)
 *******************************************************************************/
#define SDK_LOG_INFO(category, msg) \
	SDK_LOG_IMPL(::Actisense::Sdk::LogLevel::Info, category, msg)

/**************************************************************************/ /**
 \brief      Log a debug message
 \param[in]  category  LogCategory enum value
 \param[in]  msg       Message string (string_view compatible)
 *******************************************************************************/
#define SDK_LOG_DEBUG(category, msg) \
	SDK_LOG_IMPL(::Actisense::Sdk::LogLevel::Debug, category, msg)

/**************************************************************************/ /**
 \brief      Log a trace message
 \param[in]  category  LogCategory enum value
 \param[in]  msg       Message string (string_view compatible)
 \note       May be compiled out in release builds if ACTISENSE_SDK_NO_TRACE is defined
 *******************************************************************************/
#ifndef ACTISENSE_SDK_NO_TRACE
#define SDK_LOG_TRACE(category, msg) \
	SDK_LOG_IMPL(::Actisense::Sdk::LogLevel::Trace, category, msg)
#else
#define SDK_LOG_TRACE(category, msg) ((void)0)
#endif

/* Shorthand macros for common categories ----------------------------------- */

/**************************************************************************/ /**
 \brief      Log to Transport category
 *******************************************************************************/
#define SDK_LOG_TRANSPORT_ERROR(msg) SDK_LOG_ERROR(::Actisense::Sdk::LogCategory::Transport, msg)
#define SDK_LOG_TRANSPORT_WARN(msg)  SDK_LOG_WARN(::Actisense::Sdk::LogCategory::Transport, msg)
#define SDK_LOG_TRANSPORT_INFO(msg)  SDK_LOG_INFO(::Actisense::Sdk::LogCategory::Transport, msg)
#define SDK_LOG_TRANSPORT_DEBUG(msg) SDK_LOG_DEBUG(::Actisense::Sdk::LogCategory::Transport, msg)
#define SDK_LOG_TRANSPORT_TRACE(msg) SDK_LOG_TRACE(::Actisense::Sdk::LogCategory::Transport, msg)

/**************************************************************************/ /**
 \brief      Log to Protocol category
 *******************************************************************************/
#define SDK_LOG_PROTOCOL_ERROR(msg) SDK_LOG_ERROR(::Actisense::Sdk::LogCategory::Protocol, msg)
#define SDK_LOG_PROTOCOL_WARN(msg)  SDK_LOG_WARN(::Actisense::Sdk::LogCategory::Protocol, msg)
#define SDK_LOG_PROTOCOL_INFO(msg)  SDK_LOG_INFO(::Actisense::Sdk::LogCategory::Protocol, msg)
#define SDK_LOG_PROTOCOL_DEBUG(msg) SDK_LOG_DEBUG(::Actisense::Sdk::LogCategory::Protocol, msg)
#define SDK_LOG_PROTOCOL_TRACE(msg) SDK_LOG_TRACE(::Actisense::Sdk::LogCategory::Protocol, msg)

/**************************************************************************/ /**
 \brief      Log to Bem category
 *******************************************************************************/
#define SDK_LOG_BEM_ERROR(msg) SDK_LOG_ERROR(::Actisense::Sdk::LogCategory::Bem, msg)
#define SDK_LOG_BEM_WARN(msg)  SDK_LOG_WARN(::Actisense::Sdk::LogCategory::Bem, msg)
#define SDK_LOG_BEM_INFO(msg)  SDK_LOG_INFO(::Actisense::Sdk::LogCategory::Bem, msg)
#define SDK_LOG_BEM_DEBUG(msg) SDK_LOG_DEBUG(::Actisense::Sdk::LogCategory::Bem, msg)
#define SDK_LOG_BEM_TRACE(msg) SDK_LOG_TRACE(::Actisense::Sdk::LogCategory::Bem, msg)

/**************************************************************************/ /**
 \brief      Log to Session category
 *******************************************************************************/
#define SDK_LOG_SESSION_ERROR(msg) SDK_LOG_ERROR(::Actisense::Sdk::LogCategory::Session, msg)
#define SDK_LOG_SESSION_WARN(msg)  SDK_LOG_WARN(::Actisense::Sdk::LogCategory::Session, msg)
#define SDK_LOG_SESSION_INFO(msg)  SDK_LOG_INFO(::Actisense::Sdk::LogCategory::Session, msg)
#define SDK_LOG_SESSION_DEBUG(msg) SDK_LOG_DEBUG(::Actisense::Sdk::LogCategory::Session, msg)
#define SDK_LOG_SESSION_TRACE(msg) SDK_LOG_TRACE(::Actisense::Sdk::LogCategory::Session, msg)

#endif /* __ACTISENSE_SDK_LOG_MACROS_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
