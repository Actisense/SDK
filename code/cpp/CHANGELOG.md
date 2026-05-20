# Changelog

All notable changes to the Actisense C++ SDK are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Tests

- **Remote BEM sweep: re-enabled NGX-only cases against an NGX-as-remote rig
  (GIT-94).** `test_bem_remote_device.cpp` now ports the two NGX-gated
  cases that GIT-92 deliberately deferred — `OperatingMode_NGConvertNormalMode_RoundTrip`
  (NGW-style 2000→0183 conversion-mode round-trip) and
  `SetOperatingMode_RejectsBuffer1OnNgx` (firmware must reject buffer/combiner
  modes on NGX-class hardware). The fixture gains
  `expectedRemoteHardwarePortCount()` so `GetPortBaudrate` now checks the
  remote's reported port count against the model's canonical hardware
  count (NGXSW-3623 contract); currently emits a diagnostic rather than
  asserting because NGX-as-remote hits a wrap-path mismatch tracked under
  NGXSW-4193. Both new NGX-only tests skip-gate on `remoteIsNgx()` so the
  GIT-92 baseline rig (NGT-as-remote) is unaffected. Default `ACTISENSE_TEST_PORT` is now `COM5` (the dev bench's
  local-NGX); override as before for other rigs. File-header rig
  documentation rewritten to enumerate both supported topologies.

### Removed (breaking)

- **`Session::asyncRequestResponse()` and `Session::cancel()` have been
  removed from the public interface**, along with the `RequestHandle` struct
  and `RequestCompletion` callback typedef. The generic request/response path
  was never wired up to a correlator: successful responses could not be
  matched back to the returned `RequestHandle`, and the `timeout` parameter
  was ignored. Only the cancel/close paths fired the completion (always with
  `ErrorCode::Canceled`), making the API a footgun for any caller who
  expected an actual response or timeout. Callers wanting fire-and-forget
  should use `asyncSend()`; callers needing correlated request/response
  should use the typed BEM helpers on `SessionImpl` (e.g. `getProductInfo`,
  `sendBemCommand`). The generic path may return once a real correlator is
  implemented for non-BEM protocols.

## [0.4.0] - 2026-04-19

### Changed (potentially breaking)

- **`ExtendedError::deviceMessage` and `ExtendedError::context` are now
  `std::string` instead of `std::string_view`.** They own their storage, so an
  `ExtendedError` can be safely copied out of an `ExtendedErrorCallback` and
  retained for later inspection. Previously the views could dangle the moment
  the callback returned. `ExtendedError::isError()` and `isDeviceError()` are
  no longer `constexpr` (`std::string` is not yet a literal type).

### Fixed (thread-safety)

- **`Version::toString` no longer races between threads.** The shared static
  output buffer was replaced with a `thread_local` buffer; the returned
  `const char*` is valid until the next call on the same thread.
- **Global logger state (`gLogger`, `gGlobalLogLevel`, `gCategoryLogLevels`)
  is now atomic.** `setLogger` uses release ordering, `logger()` uses acquire,
  and the level getters/setters use relaxed ordering. `setLogger` is now safe
  to call concurrently with logging from other threads (previously documented
  as not thread-safe). The supplied logger must still outlive any thread that
  may still be logging through it.
- **`StderrLogger` thresholds (`threshold_`, `categoryThresholds_[]`,
  `showLocation_`) are atomic.** `isEnabled()` is now lock-free on the hot
  path; the internal mutex is reduced to serialising the `fputs` write.

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
