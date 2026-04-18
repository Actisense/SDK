# Changelog

All notable changes to the Actisense C++ SDK are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2026-04-18

### Build

- Bumped minimum GoogleTest version to **1.17.0** (fetched via `FetchContent`
  when not available locally). Required because MSVC 19.50 (Visual Studio 2026)
  has broken test registration with GoogleTest versions earlier than 1.17.
- Switched unit-test registration from a single `add_test` per binary to
  `gtest_discover_tests`, so each `TEST`/`TEST_F` is registered individually
  with CTest. `ctest -N` now lists every test case instead of just the binary.
- Forced `gtest_force_shared_crt=ON` on Windows FetchContent builds to match
  the host project's CRT and avoid `LNK2038` when linking the tests.

### Fixed

- **Serial + loopback transport: completion callbacks no longer fire under
  the transport's internal locks.** `LoopbackTransport` (all entry points) and
  `SerialTransport::close` / `SerialTransport::processAsyncOperations` now
  collect pending completions while locked, release the lock, then invoke
  them. Removes a class of deadlocks where a user handler re-entered the
  transport (for example calling `asyncRecv` or `close` from inside a prior
  completion).
- **Serial transport (Windows): pending overlapped read is cancelled on
  read-thread exit.** If the read loop exits while a read is still in flight
  (for example because `close()` was called during a `WAIT_TIMEOUT`),
  `CancelIo` is issued and `GetOverlappedResult(..., TRUE)` is used to wait
  for the kernel to release the OVERLAPPED + buffer before the event handle
  is closed.
- **Serial transport (POSIX): `close()` can no longer hang forever.** The read
  thread now uses a bounded `select()` timeout (capped at 100 ms, with a zero
  `readTimeoutMs_` treated as "use the default poll interval"), so
  `stopRequested_` is observed promptly and `join()` returns in bounded time.
- **Serial transport: `open()` partial-failure no longer leaks the platform
  handle.** If `configurePort()` fails, the Windows `HANDLE` /
  `writeOverlapped_.hEvent` or the POSIX `fd_` is torn down directly instead
  of relying on `close()`, which previously short-circuited because
  `isOpen_` was still `false`.

### Documentation

- Added this `CHANGELOG.md`.
- Updated `docs/README.md` to document the new GoogleTest minimum version.

## [0.2.0]

Prior pre-release work; no changelog entry was kept.

## [0.1.0]

Initial pre-release.
