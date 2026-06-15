/**************************************************************************//**
\file       test_debug_log.cpp
\brief      Unit tests for the DebugLog asynchronous file flush (GIT-118)
\details    Verifies that file output is written off-thread in emission order,
			that flush() deterministically drains the queue to disk (no sleeps),
			that concurrent logging neither deadlocks nor loses lines, and that
			console/callback/level/logHex behaviour is preserved.

\copyright  <h2>&copy; COPYRIGHT 2026 Active Research Limited<br>ALL RIGHTS RESERVED</h2>
*******************************************************************************/

/* Dependent includes ------------------------------------------------------- */
#include <util/debug_log.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Actisense
{
namespace Sdk
{
namespace Test
{

/* Fixture ------------------------------------------------------------------ */

/**
 * @brief      Resets the DebugLog singleton to a known state around each test
 *             and provides a fresh temporary log file path.
 */
class DebugLogTest : public ::testing::Test
{
protected:
	void SetUp() override {
		auto& log = DebugLog::instance();
		log.setLevel(LogLevel::None);
		log.setConsoleOutput(nullptr);

		const ::testing::TestInfo* info =
			::testing::UnitTest::GetInstance()->current_test_info();
		log_path_ = std::filesystem::temp_directory_path() /
					(std::string("actisense_debuglog_") + info->test_suite_name() + "_" +
					 info->name() + ".log");

		/* setLogFile() appends, so clear any residue from a previous run. */
		std::error_code ec;
		std::filesystem::remove(log_path_, ec);
	}

	void TearDown() override {
		auto& log = DebugLog::instance();
		log.setLogFile("");
		log.setConsoleOutput(nullptr);
		log.setLevel(LogLevel::None);

		std::error_code ec;
		std::filesystem::remove(log_path_, ec);
	}

	/* Closes the log file (so the stream is released) and returns its lines. */
	[[nodiscard]] std::vector<std::string> readLoggedLines() {
		DebugLog::instance().setLogFile("");

		std::vector<std::string> lines;
		std::ifstream in(log_path_);
		std::string line;
		while (std::getline(in, line)) {
			lines.push_back(line);
		}
		return lines;
	}

	std::string filePath() const { return log_path_.string(); }

	std::filesystem::path log_path_;
};

/* Tests -------------------------------------------------------------------- */

/* File lines are written in emission order and flush() guarantees they are on
   disk before it returns — no sleeps anywhere. */
TEST_F(DebugLogTest, FlushWritesAllLinesInOrder)
{
	auto& log = DebugLog::instance();
	log.setFileLevel(LogLevel::Trace);
	log.setLogFile(filePath());

	constexpr int kCount = 200;
	for (int i = 0; i < kCount; ++i) {
		log.log(LogLevel::Info, "TEST", "line " + std::to_string(i));
	}
	log.flush();

	const std::vector<std::string> lines = readLoggedLines();
	ASSERT_EQ(lines.size(), static_cast<std::size_t>(kCount));
	for (int i = 0; i < kCount; ++i) {
		EXPECT_NE(lines[i].find("line " + std::to_string(i)), std::string::npos)
			<< "at index " << i << ": " << lines[i];
		EXPECT_NE(lines[i].find("[INFO ]"), std::string::npos);
		EXPECT_NE(lines[i].find("[TEST]"), std::string::npos);
	}
}

/* A large burst enqueued faster than the worker can drain must be fully
   written by flush() — exercises batched drain (the same path the destructor
   relies on at shutdown). */
TEST_F(DebugLogTest, FlushDrainsLargeBacklog)
{
	auto& log = DebugLog::instance();
	log.setFileLevel(LogLevel::Trace);
	log.setLogFile(filePath());

	constexpr int kCount = 5000;
	for (int i = 0; i < kCount; ++i) {
		log.log(LogLevel::Debug, "BURST", "msg");
	}
	log.flush();

	EXPECT_EQ(readLoggedLines().size(), static_cast<std::size_t>(kCount));
}

/* Concurrent loggers must neither deadlock nor lose lines. Count is
   deterministic via flush(); cross-thread ordering is not asserted. */
TEST_F(DebugLogTest, ConcurrentLoggingLosesNothing)
{
	auto& log = DebugLog::instance();
	log.setFileLevel(LogLevel::Trace);
	log.setLogFile(filePath());

	constexpr int kThreads = 8;
	constexpr int kPerThread = 250;

	std::vector<std::thread> threads;
	threads.reserve(kThreads);
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&log, t] {
			for (int i = 0; i < kPerThread; ++i) {
				log.log(LogLevel::Info, "MT", "t" + std::to_string(t) + "#" +
												  std::to_string(i));
			}
		});
	}
	for (auto& th : threads) {
		th.join();
	}
	log.flush();

	EXPECT_EQ(readLoggedLines().size(),
			  static_cast<std::size_t>(kThreads * kPerThread));
}

/* File level gating still drops messages below the configured threshold. */
TEST_F(DebugLogTest, FileLevelGatingFiltersOutput)
{
	auto& log = DebugLog::instance();
	log.setConsoleLevel(LogLevel::None);
	log.setFileLevel(LogLevel::Warn);
	log.setLogFile(filePath());

	log.log(LogLevel::Info, "GATE", "should be dropped");
	log.log(LogLevel::Error, "GATE", "should be kept");
	log.log(LogLevel::Warn, "GATE", "should also be kept");
	log.flush();

	const std::vector<std::string> lines = readLoggedLines();
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_NE(lines[0].find("should be kept"), std::string::npos);
	EXPECT_NE(lines[1].find("should also be kept"), std::string::npos);
}

/* The console callback is still invoked synchronously (no flush needed) and
   receives formatted lines — the console path is unchanged by GIT-118. */
TEST_F(DebugLogTest, ConsoleCallbackInvokedSynchronously)
{
	auto& log = DebugLog::instance();
	log.setConsoleLevel(LogLevel::Trace);
	log.setFileLevel(LogLevel::None);

	std::mutex mtx;
	std::vector<std::string> captured;
	log.setConsoleOutput([&mtx, &captured](LogLevel, std::string_view msg) {
		std::lock_guard<std::mutex> lock(mtx);
		captured.emplace_back(msg);
	});

	log.log(LogLevel::Info, "CB", "hello");
	log.log(LogLevel::Error, "CB", "world");

	std::lock_guard<std::mutex> lock(mtx);
	ASSERT_EQ(captured.size(), 2u);
	EXPECT_NE(captured[0].find("hello"), std::string::npos);
	EXPECT_NE(captured[0].find("[CB]"), std::string::npos);
	EXPECT_NE(captured[1].find("world"), std::string::npos);
}

/* logHex still emits a header plus the byte values through the async file path. */
TEST_F(DebugLogTest, LogHexWritesBytesToFile)
{
	auto& log = DebugLog::instance();
	log.setFileLevel(LogLevel::Trace);
	log.setLogFile(filePath());

	const uint8_t data[] = {0x01, 0x02, 0xAB};
	log.logHex(LogLevel::Debug, "HEX", "payload", data, sizeof(data));
	log.flush();

	const std::vector<std::string> lines = readLoggedLines();
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_NE(lines[0].find("payload"), std::string::npos);
	EXPECT_NE(lines[0].find("[3 bytes]"), std::string::npos);
	EXPECT_NE(lines[0].find("01"), std::string::npos);
	EXPECT_NE(lines[0].find("02"), std::string::npos);
	EXPECT_NE(lines[0].find("ab"), std::string::npos);
}

/* flush() with no file configured must return immediately (no hang). */
TEST_F(DebugLogTest, FlushWithoutFileIsNoOp)
{
	auto& log = DebugLog::instance();
	log.setConsoleLevel(LogLevel::Info);
	log.setFileLevel(LogLevel::Info);
	log.log(LogLevel::Info, "NOFILE", "console only");
	log.flush(); /* must not block */
	SUCCEED();
}

} /* namespace Test */
} /* namespace Sdk */
} /* namespace Actisense */

/**************** (C) COPYRIGHT Active Research Limited  ** END OF FILE **/
