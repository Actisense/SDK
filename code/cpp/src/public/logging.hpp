#ifndef __ACTISENSE_SDK_LOGGING_HPP
#define __ACTISENSE_SDK_LOGGING_HPP

/**************************************************************************/ /**
 \file       logging.hpp
 \brief      Logging interface and configuration for Actisense SDK
 \details    Defines pluggable logging interface with log levels and categories

 \copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
 *******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <string_view>

namespace Actisense
{
	namespace Sdk
	{
		/* Definitions ---------------------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Log severity levels
		 \details    Ordered from most severe (Error) to most verbose (Trace)
		 *******************************************************************************/
		enum class LogLevel
		{
			Error = 0, ///< Errors that affect operation
			Warn,	   ///< Warnings that may indicate problems
			Info,	   ///< Informational messages (connection events, etc.)
			Debug,	   ///< Debug information for troubleshooting
			Trace	   ///< Verbose tracing (frame-level detail)
		};

		/**************************************************************************/ /**
		 \brief      Log message categories
		 \details    Used for filtering logs by SDK component
		 *******************************************************************************/
		enum class LogCategory
		{
			General = 0, ///< General SDK messages
			Transport,	 ///< Transport layer (serial, TCP, UDP)
			Protocol,	 ///< Protocol parsing (BDTP, BST)
			Bem,		 ///< BEM command/response
			Session,	 ///< Session lifecycle
			Metrics		 ///< Metrics and diagnostics
		};

		/**************************************************************************/ /**
		 \brief      Abstract logger interface
		 \details    Implement this interface to provide custom logging backend.
		             SDK provides StderrLogger and NullLogger implementations.
		 *******************************************************************************/
		class ILogger
		{
		public:
			virtual ~ILogger() = default;

			/**************************************************************************/ /**
			 \brief      Log a message
			 \param[in]  level     Severity level
			 \param[in]  category  Message category
			 \param[in]  message   Log message
			 \param[in]  file      Source file (optional, may be empty)
			 \param[in]  line      Source line (optional, 0 if not provided)
			 *******************************************************************************/
			virtual void log(LogLevel level, LogCategory category, std::string_view message,
							 std::string_view file = {}, int line = 0) = 0;

			/**************************************************************************/ /**
			 \brief      Check if logging is enabled for level/category
			 \details    Allows fast-path skip of expensive message formatting.
			             Called before constructing log message.
			 \param[in]  level     Severity level to check
			 \param[in]  category  Category to check
			 \return     True if a message at this level/category would be logged
			 *******************************************************************************/
			[[nodiscard]] virtual bool isEnabled(LogLevel level,
												 LogCategory category) const noexcept = 0;

			/**************************************************************************/ /**
			 \brief      Flush any buffered output
			 \details    Call to ensure all log messages have been written
			 *******************************************************************************/
			virtual void flush() = 0;

		protected:
			ILogger() = default;
			ILogger(const ILogger&) = default;
			ILogger& operator=(const ILogger&) = default;
		};

		/* Global Logger Management --------------------------------------------- */

		/**************************************************************************/ /**
		 \brief      Set the global logger instance
		 \param[in]  logger  Logger to use (nullptr to reset to default NullLogger)
		 \details    Not thread-safe; call before creating sessions or from main thread.
		             The SDK does not take ownership - caller must ensure logger
		             lifetime exceeds SDK usage.
		 *******************************************************************************/
		void setLogger(ILogger* logger);

		/**************************************************************************/ /**
		 \brief      Get the current global logger
		 \return     Reference to current logger (never null; returns NullLogger if unset)
		 *******************************************************************************/
		[[nodiscard]] ILogger& logger() noexcept;

		/**************************************************************************/ /**
		 \brief      Set global log level threshold
		 \param[in]  level  Minimum level to log (messages below this are discarded)
		 *******************************************************************************/
		void setLogLevel(LogLevel level);

		/**************************************************************************/ /**
		 \brief      Set log level for a specific category
		 \param[in]  category  Category to configure
		 \param[in]  level     Minimum level for this category
		 *******************************************************************************/
		void setLogLevel(LogCategory category, LogLevel level);

		/**************************************************************************/ /**
		 \brief      Get the current global log level
		 \return     Current minimum log level
		 *******************************************************************************/
		[[nodiscard]] LogLevel logLevel() noexcept;

		/**************************************************************************/ /**
		 \brief      Get log level name as string
		 \param[in]  level  Log level
		 \return     String representation (e.g., "ERROR", "WARN")
		 *******************************************************************************/
		[[nodiscard]] std::string_view logLevelName(LogLevel level) noexcept;

		/**************************************************************************/ /**
		 \brief      Get log category name as string
		 \param[in]  category  Log category
		 \return     String representation (e.g., "Transport", "Protocol")
		 *******************************************************************************/
		[[nodiscard]] std::string_view logCategoryName(LogCategory category) noexcept;

	}; /* namespace Sdk */
}; /* namespace Actisense */

#endif /* __ACTISENSE_SDK_LOGGING_HPP */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
