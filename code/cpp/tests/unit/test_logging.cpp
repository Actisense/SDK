/**************************************************************************//**
\file       test_logging.cpp
\brief      Unit tests for SDK logging infrastructure
\details    Tests ILogger interface, StderrLogger, NullLogger, and log macros

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <public/logging.hpp>
#include <util/stderr_logger.hpp>
#include <util/log_macros.hpp>

#include <gtest/gtest.h>
#include <sstream>
#include <thread>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Test Logger -------------------------------------------------------------- */

/**
 * @brief      Test logger that captures messages for verification
 */
class TestLogger final : public ILogger
{
public:
	struct LogEntry
	{
		LogLevel level;
		LogCategory category;
		std::string message;
		std::string file;
		int line;
	};

	void log(LogLevel level, LogCategory category, std::string_view message,
			 std::string_view file, int line) override {
		std::lock_guard<std::mutex> lock(mutex_);
		entries_.push_back({level, category, std::string(message),
							std::string(file), line});
	}

	[[nodiscard]] bool isEnabled(LogLevel level, LogCategory /*category*/) const noexcept override {
		return level <= threshold_;
	}

	void flush() override {}

	void setThreshold(LogLevel level) { threshold_ = level; }

	[[nodiscard]] std::vector<LogEntry> entries() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return entries_;
	}

	void clear() {
		std::lock_guard<std::mutex> lock(mutex_);
		entries_.clear();
	}

private:
	mutable std::mutex mutex_;
	std::vector<LogEntry> entries_;
	LogLevel threshold_ = LogLevel::Trace;
};

/* Logging Tests ------------------------------------------------------------ */

class LoggingTest : public ::testing::Test
{
protected:
	void SetUp() override {
		/* Save current logger and install test logger */
		savedLogger_ = &logger();
		testLogger_ = std::make_unique<TestLogger>();
		setLogger(testLogger_.get());
	}

	void TearDown() override {
		/* Restore original logger */
		setLogger(savedLogger_ == &logger() ? nullptr : savedLogger_);
	}

	TestLogger& testLogger() { return *testLogger_; }

private:
	ILogger* savedLogger_ = nullptr;
	std::unique_ptr<TestLogger> testLogger_;
};

TEST_F(LoggingTest, LogLevelNames)
{
	EXPECT_EQ(logLevelName(LogLevel::Error), "ERROR");
	EXPECT_EQ(logLevelName(LogLevel::Warn), "WARN");
	EXPECT_EQ(logLevelName(LogLevel::Info), "INFO");
	EXPECT_EQ(logLevelName(LogLevel::Debug), "DEBUG");
	EXPECT_EQ(logLevelName(LogLevel::Trace), "TRACE");
}

TEST_F(LoggingTest, LogCategoryNames)
{
	EXPECT_EQ(logCategoryName(LogCategory::General), "General");
	EXPECT_EQ(logCategoryName(LogCategory::Transport), "Transport");
	EXPECT_EQ(logCategoryName(LogCategory::Protocol), "Protocol");
	EXPECT_EQ(logCategoryName(LogCategory::Bem), "Bem");
	EXPECT_EQ(logCategoryName(LogCategory::Session), "Session");
	EXPECT_EQ(logCategoryName(LogCategory::Metrics), "Metrics");
}

TEST_F(LoggingTest, BasicLogging)
{
	logger().log(LogLevel::Info, LogCategory::General, "Test message", "", 0);

	const auto entries = testLogger().entries();
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_EQ(entries[0].level, LogLevel::Info);
	EXPECT_EQ(entries[0].category, LogCategory::General);
	EXPECT_EQ(entries[0].message, "Test message");
}

TEST_F(LoggingTest, LogWithFileAndLine)
{
	logger().log(LogLevel::Debug, LogCategory::Transport, "Debug msg", "test.cpp", 42);

	const auto entries = testLogger().entries();
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_EQ(entries[0].file, "test.cpp");
	EXPECT_EQ(entries[0].line, 42);
}

TEST_F(LoggingTest, LevelFiltering)
{
	testLogger().setThreshold(LogLevel::Warn);

	/* Should be logged (at or below threshold) */
	logger().log(LogLevel::Error, LogCategory::General, "Error", "", 0);
	logger().log(LogLevel::Warn, LogCategory::General, "Warning", "", 0);

	/* Should be filtered (above threshold) */
	if (logger().isEnabled(LogLevel::Info, LogCategory::General)) {
		logger().log(LogLevel::Info, LogCategory::General, "Info", "", 0);
	}
	if (logger().isEnabled(LogLevel::Debug, LogCategory::General)) {
		logger().log(LogLevel::Debug, LogCategory::General, "Debug", "", 0);
	}

	const auto entries = testLogger().entries();
	EXPECT_EQ(entries.size(), 2u);
}

TEST_F(LoggingTest, IsEnabledCheck)
{
	testLogger().setThreshold(LogLevel::Info);

	EXPECT_TRUE(logger().isEnabled(LogLevel::Error, LogCategory::General));
	EXPECT_TRUE(logger().isEnabled(LogLevel::Warn, LogCategory::General));
	EXPECT_TRUE(logger().isEnabled(LogLevel::Info, LogCategory::General));
	EXPECT_FALSE(logger().isEnabled(LogLevel::Debug, LogCategory::General));
	EXPECT_FALSE(logger().isEnabled(LogLevel::Trace, LogCategory::General));
}

TEST_F(LoggingTest, LogMacros)
{
	SDK_LOG_ERROR(LogCategory::Transport, "Error message");
	SDK_LOG_WARN(LogCategory::Protocol, "Warning message");
	SDK_LOG_INFO(LogCategory::Session, "Info message");

	const auto entries = testLogger().entries();
	ASSERT_GE(entries.size(), 3u);

	EXPECT_EQ(entries[0].level, LogLevel::Error);
	EXPECT_EQ(entries[0].category, LogCategory::Transport);

	EXPECT_EQ(entries[1].level, LogLevel::Warn);
	EXPECT_EQ(entries[1].category, LogCategory::Protocol);

	EXPECT_EQ(entries[2].level, LogLevel::Info);
	EXPECT_EQ(entries[2].category, LogCategory::Session);
}

TEST_F(LoggingTest, ShorthandMacros)
{
	SDK_LOG_TRANSPORT_INFO("Transport info");
	SDK_LOG_PROTOCOL_DEBUG("Protocol debug");
	SDK_LOG_BEM_ERROR("BEM error");
	SDK_LOG_SESSION_WARN("Session warning");

	const auto entries = testLogger().entries();
	ASSERT_GE(entries.size(), 4u);

	EXPECT_EQ(entries[0].category, LogCategory::Transport);
	EXPECT_EQ(entries[1].category, LogCategory::Protocol);
	EXPECT_EQ(entries[2].category, LogCategory::Bem);
	EXPECT_EQ(entries[3].category, LogCategory::Session);
}

/* NullLogger Tests --------------------------------------------------------- */

class NullLoggerTest : public ::testing::Test
{
};

TEST_F(NullLoggerTest, IsNeverEnabled)
{
	NullLogger nullLogger;

	EXPECT_FALSE(nullLogger.isEnabled(LogLevel::Error, LogCategory::General));
	EXPECT_FALSE(nullLogger.isEnabled(LogLevel::Trace, LogCategory::General));
}

TEST_F(NullLoggerTest, LogDoesNothing)
{
	NullLogger nullLogger;

	/* Should not crash */
	nullLogger.log(LogLevel::Error, LogCategory::General, "Test", "file.cpp", 1);
	nullLogger.flush();
}

/* StderrLogger Tests ------------------------------------------------------- */

class StderrLoggerTest : public ::testing::Test
{
};

TEST_F(StderrLoggerTest, DefaultThresholdIsInfo)
{
	StderrLogger stderrLogger;

	EXPECT_TRUE(stderrLogger.isEnabled(LogLevel::Error, LogCategory::General));
	EXPECT_TRUE(stderrLogger.isEnabled(LogLevel::Warn, LogCategory::General));
	EXPECT_TRUE(stderrLogger.isEnabled(LogLevel::Info, LogCategory::General));
	EXPECT_FALSE(stderrLogger.isEnabled(LogLevel::Debug, LogCategory::General));
	EXPECT_FALSE(stderrLogger.isEnabled(LogLevel::Trace, LogCategory::General));
}

TEST_F(StderrLoggerTest, ThresholdCanBeChanged)
{
	StderrLogger stderrLogger;

	stderrLogger.setThreshold(LogLevel::Error);
	EXPECT_TRUE(stderrLogger.isEnabled(LogLevel::Error, LogCategory::General));
	EXPECT_FALSE(stderrLogger.isEnabled(LogLevel::Warn, LogCategory::General));

	stderrLogger.setThreshold(LogLevel::Trace);
	EXPECT_TRUE(stderrLogger.isEnabled(LogLevel::Trace, LogCategory::General));
}

TEST_F(StderrLoggerTest, CategoryThresholds)
{
	StderrLogger stderrLogger(LogLevel::Info);

	/* Set Transport to Debug level */
	stderrLogger.setCategoryThreshold(LogCategory::Transport, LogLevel::Debug);

	/* Transport should allow Debug */
	EXPECT_TRUE(stderrLogger.isEnabled(LogLevel::Debug, LogCategory::Transport));

	/* Other categories should still be at Info */
	EXPECT_FALSE(stderrLogger.isEnabled(LogLevel::Debug, LogCategory::General));
}

/* Thread Safety Tests ------------------------------------------------------ */

class ThreadSafetyTest : public ::testing::Test
{
protected:
	TestLogger testLogger_;
};

TEST_F(ThreadSafetyTest, ConcurrentLogging)
{
	const int numThreads = 4;
	const int logsPerThread = 100;

	std::vector<std::thread> threads;
	threads.reserve(numThreads);

	for (int t = 0; t < numThreads; ++t) {
		threads.emplace_back([this, t, logsPerThread]() {
			for (int i = 0; i < logsPerThread; ++i) {
				testLogger_.log(LogLevel::Info, LogCategory::General,
							   "Thread " + std::to_string(t) + " log " + std::to_string(i),
							   "", 0);
			}
		});
	}

	for (auto& thread : threads) {
		thread.join();
	}

	/* All logs should be captured */
	EXPECT_EQ(testLogger_.entries().size(), static_cast<size_t>(numThreads * logsPerThread));
}

}; /* namespace Test */
}; /* namespace Sdk */
}; /* namespace Actisense */
