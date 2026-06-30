# Changelog

All notable changes to the Actisense C++ SDK are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Public unsolicited BEM payload headers (GIT-130).** The four typed
  unsolicited payloads and their enums now live in dedicated public headers —
  `public/bem_responses/system_status.hpp` (`SystemStatusData` +
  `IndividualBufferStats` / `UnifiedBufferStats` / `CanExtendedStatus`),
  `startup_status.hpp` (`StartupStatusData`, `StartupStatusFormat`),
  `error_report.hpp` (`ErrorReportData`, `ErrorReportVariant`) and
  `negative_ack.hpp` (`NegativeAckData`) — plus an aggregate
  `public/bem_responses/unsolicited.hpp`. A pure-public-API consumer can now
  name the payload type delivered with a typed unsolicited `ParsedMessageEvent`
  without reaching into internal `protocols/bem/bem_commands/*` headers. The
  wire-format decode/format helpers stay internal; **type names and namespaces
  are unchanged — only the include path is new** (mirrors the GIT-112
  relocation of the correlated-response structs).
- **`ParsedMessageEvent::origin` (GIT-130).** Typed unsolicited BEM events
  (`messageType` "StartupStatus" / "ErrorReport" / "SystemStatus" /
  "NegativeAck", plus the generic "BEM_Response_*" fallback) now carry an
  optional `ResponseOrigin` describing the responding device — `modelId` /
  `serialNumber` (from the BEM reply header) and the receive path
  (`n2kSourceAddress` / `TransportPath`). This restores the device identity that
  the typed payload structs omit, so a consumer of e.g. `SystemStatusData` can
  still tell *which* device produced it. `std::nullopt` for BST / NMEA 2000
  events.

### Changed

- **`HardwareProtocol` enum now carries the real on-wire codes.**
  The enumerators were renamed and renumbered to mirror the firmware
  `HardwareProtocol_e` wire values emitted in the Port Baudrate (0x17) response:
  `SerialNmea0183 = 0`, `SerialBst = 1`, `CanNmea2000 = 32`, `CanJ1939 = 33`,
  `EthernetBst = 64`, `EthernetNmea0183 = 65`, `EthernetOneNet = 66` (previously
  `Bst`/`Nmea0183`/`Nmea2000`/`Ipv4`/`Ipv6`/`RawAscii`/`N2kAscii` with invented
  values 0–6). The decoder previously surfaced every real device port as
  "Unknown" (e.g. an NGX-1 CAN port reports 0x20) or mislabelled it; it now
  decodes correctly. **Source-breaking rename of the public enumerators** —
  shipped in the 1.x line as a bug-fix correction, since the prior values never
  matched any device and so could not have been relied upon.
- **`public/api.hpp` is now a true umbrella header (GIT-131).** A single
  `#include "public/api.hpp"` now exposes the *entire* public surface — it
  aggregates every `src/public/*.hpp`, including the previously-omitted
  `pgn_encoders.hpp`, `remote_device.hpp`, `logging.hpp` and `ebl_writer.hpp` —
  matching the README's "single include gives access to the entire SDK" promise.
  A `public_api_umbrella_complete` guard test and a `test_api_umbrella`
  compile-proof keep it complete as new public headers are added.
- **Installed headers keep their `public/` path prefix (GIT-131).** The install
  step now maps `src/public/` → `include/public/` (previously
  `include/actisense/`), so an installed consumer adds `<prefix>/include` to the
  include path and writes `#include "public/api.hpp"` exactly as documented, and
  the headers' own `#include "public/…"` cross-references resolve against the
  install tree. *(Install-tree layout change — consumers that hard-coded
  `include/actisense/` must adjust; no released consumer existed.)*

### Removed

- **`actisense_console` example removed (GIT-131).** The console demo was built
  against the private `SessionImpl` interface and internal `core/`, `protocols/`
  and `util/` headers, so it could not be reproduced by an external consumer and
  broke silently when public callback signatures changed. Its demonstrable
  surface (frame display, Get/Set operating mode, EBL capture) is already covered
  by the public-API-only `nmea_reader_console` and `pgn_transmitter` examples,
  which are now the canonical references. (Supersedes the unreleased GIT-130
  rewrite of the same example.)

### Guardrails

- **Example/public-header boundary is now enforced at build time (GIT-131).**
  Four checks, run under `ctest`, prevent the two boundary defects above from
  recurring: (a) a generalized static guard (`public_headers_no_internal_include`)
  rejects any internal include — `core/`, `transport/`, `platform/`, `util/`,
  `protocols/` — from a public header; (b) the same guard
  (`examples_no_internal_include`) enforces public-only includes in `examples/`;
  (c) a per-header self-containment compile sweep
  (`public_headers_selfcontained`) compiles every public header standalone; and
  (d) examples and the sweeps build against a public-only include path so
  internal headers are physically unreachable, not merely discouraged. Broadens
  the GIT-112 `protocols/`-only guard.

### Changed (breaking)

- **`ParsedMessageEvent` gained a trailing `std::optional<ResponseOrigin>
  origin` member (GIT-130).** Source-additive — existing code compiles
  unchanged — but it changes the layout/size of `ParsedMessageEvent` and the
  `EventVariant` that wraps it, so it is a binary-ABI break for consumers
  linking a pre-built SDK binary. Bump the binary version on the next release,
  per the GIT-115 ABI-break precedent.

### Migration

- **Unsolicited BEM messages (F0/F1/F2/F4) are delivered as *typed*
  `ParsedMessageEvent`s — `e.payload` is the decoded `*Data` struct, not a raw
  `BemResponse`.** Code that does `std::any_cast<const BemResponse&>(e.payload)`
  for these throws `std::bad_any_cast` (and any surrounding `catch` may swallow
  it silently, so the message appears to vanish). Dispatch on `messageType` and
  cast to the matching public payload instead:

  ```cpp
  if (auto* msg = std::get_if<ParsedMessageEvent>(&event);
      msg && msg->protocol == "bem" && msg->messageType == "SystemStatus") {
      const auto& status = std::any_cast<const SystemStatusData&>(msg->payload);
      if (msg->origin) { /* msg->origin->modelId, ->serialNumber */ }
      for (const auto& buf : status.individual_buffers_) { /* CAN load, etc. */ }
  }
  ```

## [1.0.0] - 2026-06-18

First public, SemVer-stable release. From 1.0.0 the `src/public/` API is
covered by Semantic Versioning; this release consolidates all accumulated
pre-1.0 breaking changes. (Version bump tracked by GIT-123.)

### Added

- **Public received-frame accessor `asReceivedFrame()` (GIT-128).** New
  `public/received_frame.hpp` exposes a `ReceivedFrame` (PGN, source,
  destination, priority, length, and a `std::span<const uint8_t>` data view)
  from a `ParsedMessageEvent`, returning `std::nullopt` for non-NMEA-2000
  events. Customer code can now read received PGN fields without reaching into
  internal `protocols/` headers. The data span is valid only for the duration
  of the event callback — copy the bytes to retain them.
- **New example: `nmea_reader_console` (GIT-128).** A minimal "NMEA Reader"
  built on `asReceivedFrame()`: it connects to a serial gateway, switches it to
  Rx-All (restoring the prior mode on exit), and renders a live, in-place table
  of received PGNs — one row per PGN+source — with Src/Dst/PGN/Priority/Length/
  Data(hex) columns. Includes a framework-agnostic `PgnTableModel` designed to
  back a future Qt or native GUI.
- **`Api::openWithTransport()` lets callers supply their own transport
  (GIT-108).** The transport abstraction `ITransport` is now part of the public
  surface (`public/transport.hpp`); `openWithTransport(options, transport, …)`
  opens a session over a caller-provided `ITransport` instead of the built-in
  `TransportKind` selection, so the SDK can be driven over a custom transport
  (e.g. a test harness bridging to an emulated device). `Api::open()` now
  delegates to it. A null transport or null `onOpened` reports
  `ErrorCode::InvalidArgument`; a failed transport open propagates the failure
  code. The change is additive — existing `Api::open()` behaviour is unchanged.
- **`CanPacket` (5) and `CanPacketAscii` (6) added to the public
  `OperatingMode` enum (NGXSW-4207).** These let an SDK client request the NGX
  raw-CAN modes the firmware implements (NGXSW-4206), in which the device
  bridges `SystemNames::CAN` ↔ the serial host link as BST-95 (`CanPacket`)
  or CAN-ASCII (`CanPacketAscii`). The values match firmware
  `OperatingModeCodes.h`, and `OperatingModeName()` now returns
  `"CAN Packet Mode"` / `"CAN Packet ASCII Mode"`. The change is additive —
  existing modes are unaffected.
- **`NgTransferRawMode = 3` redocumented.** Mode 3 is no longer raw CAN
  (firmware now treats it as a spare/legacy slot, `OM_NGTransferSpareMode`);
  raw CAN transfer moved to `CanPacket` (5) / `CanPacketAscii` (6). The
  enum value is **retained** for public-API stability, but its documentation
  now marks it legacy — new code should target `CanPacket`.

### Changed

- **BEM device errors now report `ErrorCode::BemDeviceError` instead of
  `ErrorCode::UnsupportedOperation` (GIT-127).** When a device answers a BEM
  command with a non-zero ARL error code in the response header, the callback
  now receives `ErrorCode::BemDeviceError` with the raw signed ARL code and its
  description in the message (e.g. `"Device error -995 (PGN not on enable list
  (disabled))"`), rather than the historic catch-all `UnsupportedOperation`
  that masked every device rejection. This is what surfaced for a customer
  calling `getRxPgnEnable` for PGNs 60928 / 126996 on an NGT-1 that has those
  PGNs Rx-disabled (`ES9_N2000_PGN_NOT_ON_LIST`, -995) or absent from its
  NMEA 2000 library (`ES9_N2000_PGN_NOT_IN_LIBRARY`, -997) — both correct
  firmware responses that were being mis-reported. `bemDeviceErrorMessage()`
  gains descriptions for the PGN-enable-list ARL codes. Callers branching on
  `UnsupportedOperation` for BEM replies should switch to `BemDeviceError`.
- **`NgTransferRxAllMode` (Rx-All) documentation clarified for NGX (GIT-107).**
  Current NGX firmware (verified on fw 3.085) silently drops the ISO control
  PGNs 59904 (ISO Request) and 59392 (ISO ACK) from the bus-to-host stream in
  Rx-All mode, despite the mode being documented as forwarding "all PGNs"; an
  NGT-class gateway forwards them and ISO Address Claim (60928) is forwarded by
  both. The `OperatingMode` enum documentation and the "Receiving NMEA 2000"
  guide now call out this NGX-specific gap. A two-gateway bench
  characterisation test (`test_rxall_pgn_filter_git107`) records the behaviour.
  Documentation only — no API, wire, or behaviour change. The underlying NGX
  firmware behaviour is tracked separately.

### Changed (breaking)

- **BEM response data structures relocated to `public/bem_responses/`
  (GIT-112).** The public header `public/bem_callbacks.hpp` no longer pulls in
  any internal `protocols/bem/bem_commands/*` headers. The decoded
  response/result structs it references (`ProductInfoResponse`,
  `PortBaudrateResponse`, `RxPgnEnableListF2Result`, `EchoResponse`, …) now
  live in dedicated public headers under `public/bem_responses/`. **Type names
  and namespaces are unchanged — only the include path moves.** Code that
  includes `public/bem_callbacks.hpp`, `public/session.hpp` or
  `public/remote_device.hpp` is unaffected; only code that reached directly
  into the internal `protocols/bem/bem_commands/*` headers for those structs
  needs to switch to the matching `public/bem_responses/*` header. See the
  Upgrading guide below.
- **`OperatingMode` enumerators renamed `OM_*` → PascalCase (GIT-114).** Clean
  break — the old `OM_*` names are removed outright, with no deprecation
  aliases. The enum type (`OperatingMode`), its scoping, and every numeric
  value are unchanged, so this is a pure compile-time rename with no wire or
  ABI impact (persisted/wire numeric mode codes stay valid). The full old→new
  mapping is in the Upgrading guide below.
- **Single unified `ErrorCode` surface (GIT-113).** The separate
  `TransportErrorCode` and `ProtocolErrorCode` enums — each with its own
  `std::error_category` — have been removed. Their fine-grained diagnostics are
  appended to the single public `ErrorCode` enum in `public/error.hpp`, behind
  one `std::error_category` (`sdkErrorCategory()`). The coarse codes
  (values 0–15) keep their values; transport (`Transport*`) and protocol
  (`Bdtp*`/`Bst*`/`Bem*`) diagnostics follow, terminated by a `Count` sentinel.
  `ErrorCode` is append-only from here. Public callbacks already delivered
  `ErrorCode`, so consumers using only public headers are unaffected.
- **ABI hardening: `Session` and `RemoteDevice` are now `final`, non-polymorphic
  pimpl handles (GIT-115).** Both classes were pure-abstract interfaces (14 and
  36 pure virtuals); they are now move-only `final` classes whose only data
  member is a `std::unique_ptr<Impl>`, with non-virtual methods that forward to
  the implementation. This gives the MIT binary SDK a stable ABI — adding a verb
  appends a member symbol instead of mutating a vtable, so `sizeof` is one
  pointer and shipped-binary layout is unaffected by future growth. All public
  method signatures and the `Api` facade / `Session::openRemote` return and
  callback types are unchanged, so source compiles unmodified; this is a
  one-time intentional ABI break (binary version bumped to 0.5.0). A
  compile-time `static_assert` guard (`tests/unit/test_abi_layout.cpp`) locks the
  final / non-polymorphic / one-pointer / move-only properties in place.

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

### Changed (breaking) — GIT-104 review cleanup

- **`Session::asyncSend()` now takes a `Session::SendProtocol` enum
  (`Bst`/`Raw`) instead of a `const std::string&` selector.** The stringly-typed
  selector silently treated any unrecognised value as `"raw"`; the enum makes the
  choice explicit and typo-proof. Replace `asyncSend("bst", …)` with
  `asyncSend(Session::SendProtocol::Bst, …)` and `"raw"` with `SendProtocol::Raw`.
- **`Version::toString()` now returns `std::string` (was `const char*`).** The
  old form returned a pointer into a `thread_local` buffer that was overwritten
  by the next call on the same thread — easy to alias by accident. The owning
  string removes the hazard.
- **`setLogger()` now takes `std::shared_ptr<ILogger>` (was `ILogger*`).** The SDK
  shares ownership so an installed logger stays alive while active; callers no
  longer have to manually outlive every logging thread. The lock-free `logger()`
  read path is preserved.
- **`ErrorCode` gained an explicit `int32_t` underlying type, a new
  `BemDeviceError` enumerator (device-reported BEM error), and a trailing `Count`
  sentinel.** `BemDeviceError` was referenced in docs but missing from the enum.
- **`Api::resolveHostAsync()` is now `[[deprecated]]`.** It is an unimplemented
  stub that always reports `ErrorCode::UnsupportedOperation`; kept (not removed)
  because the symbol is compiled by internal consumers.

### Fixed — GIT-104 review cleanup

- **`Session::metrics()` now reflects real activity.** The `MetricsCollector`
  `record*` methods were never called, so the public metrics surface always read
  zero. Bytes/calls sent and received, frames parsed/dropped, BEM
  commands/responses/timeouts/device-errors, latency and framing/checksum errors
  are now recorded along the live data path (covered by `test_session_metrics`).
- **`framesReceived()` no longer under-reports.** It now counts BEM-response
  frames as well as plain BST frames.
- **Serial transport robustness.** Added a mutex around the shared Windows write
  `OVERLAPPED`; the Windows recv `WAIT_FAILED` path now cancels I/O and stops
  rather than busy-spinning; the POSIX read path now stops on peer-close
  (`read()==0`) and hard I/O errors instead of spinning; POSIX `VTIME` no longer
  truncates a sub-100 ms read timeout to 0.
- **Frame encoders reject oversized payloads** instead of silently truncating:
  `BstFrame::create93/94/95` bound the 8-bit store length and `createD0` bounds
  its 16-bit length; the BDTP Type 2 decoder sanity-bounds the 16-bit length
  field against `kBdtpMaxFrameSize`.
- **Echo response decode** rewritten to avoid a latent unsigned underflow.
- **Windows serial enumeration** pinned to the ANSI registry/SetupAPI calls so it
  compiles regardless of the project's UNICODE/MBCS setting.
- Removed a dead BEM sequence counter; documented the BEM correlator's
  single-Rx-thread invariant and the destructor's callback reentrancy contract;
  added a `Session::close()` self-join guard for callback-initiated close.

### Tests

- **Black-box NGX CAN Packet routing integration test (NGXSW-4207).** New
  `tests/integration/test_can_packet_mode.cpp` drives a real NGX into each CAN
  Packet mode via the SDK and proves the device behaviour as a black box: GET
  baseline → scope-guarded SET → GET-verify → CAN→serial (await a BST-95
  `ParsedMessageEvent` via the event callback) → serial→CAN (emit a BST-95
  frame via `asyncSend("bst", …)`) → BEM Echo (0x18) in-mode → restore the
  baseline, for both `CanPacket` and `CanPacketAscii`. Opt-in via
  `ACTISENSE_TEST_PORT` (a real NGX on a live N2K bus); skips cleanly headless
  so CI stays green. A no-hardware run against the Product Emulator is tracked
  as a DESKTOP-160 follow-up. Unit coverage for the two new modes (encode +
  `OperatingModeName`) added to `tests/unit/test_operating_mode.cpp`.

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

### Upgrading to 1.0.0

This is the SemVer baseline. If you were building against a pre-1.0 beta SDK,
apply these one-time source changes; after that the public API is stable under
SemVer.

**1. BEM response struct includes (GIT-112)** — only if you `#include`d the
internal command headers directly:

```cpp
// Before
#include "protocols/bem/bem_commands/product_info.hpp"
// After
#include "public/bem_responses/product_info.hpp"
```

Type names are unchanged (`ProductInfoResponse`, etc.). If you only include
`public/bem_callbacks.hpp` (or `session.hpp` / `remote_device.hpp`), no change
is needed.

**2. `OperatingMode` enumerator rename (GIT-114)** — replace every
`OperatingMode::OM_*` with its PascalCase name. Values are unchanged.

| Old (`OM_*`)             | New (PascalCase)       | Value      |
|--------------------------|------------------------|------------|
| `OM_UndefinedMode`       | `UndefinedMode`        | 0          |
| `OM_NGTransferNormalMode`| `NgTransferNormalMode` | 1          |
| `OM_NGTransferRxAllMode` | `NgTransferRxAllMode`  | 2          |
| `OM_NGTransferRawMode`   | `NgTransferRawMode`    | 3 (legacy) |
| `OM_NGConvertNormalMode` | `NgConvertNormalMode`  | 4          |
| `OM_CanPacket`           | `CanPacket`            | 5          |
| `OM_CanPacketASCII`      | `CanPacketAscii`       | 6          |
| `OM_BUFFER_1`            | `Buffer1`              | 16         |
| `OM_BUFFER_2`            | `Buffer2`              | 17         |
| `OM_BUFFER_3`            | `Buffer3`              | 18         |
| `OM_AUTOSWITCH_DIRECT`   | `AutoswitchDirect`     | 19         |
| `OM_AUTOSWITCH_SMART`    | `AutoswitchSmart`      | 20         |
| `OM_COMBINE_1`           | `Combine1`             | 21         |
| `OM_COMBINE_2`           | `Combine2`             | 22         |
| `OM_TEST_1`              | `Test1`                | 23         |
| `OM_NSI_MODE_1`          | `NsiMode1`             | 24         |
| `OM_LAST`                | `LastStandard`         | 253        |
| `OM_NORMAL`              | `Normal`               | 512        |
| `OM_PREDEFINED_MODE_1`   | `PredefinedMode1`      | 40000      |
| `OM_PREDEFINED_MODE_2`   | `PredefinedMode2`      | 40001      |
| `OM_PREDEFINED_MODE_END` | `PredefinedModeEnd`    | 40255      |
| `OM_USER_START`          | `UserStart`            | 50000      |
| `OM_USER_1`              | `User1`                | 50000      |
| `OM_USER_2`              | `User2`                | 50001      |
| `OM_USER_3`              | `User3`                | 50002      |
| `OM_USER_4`              | `User4`                | 50003      |
| `OM_USER_5`              | `User5`                | 50004      |
| `OM_USER_LAST_DEFINED`   | `UserLastDefined`      | 50005      |
| `OM_USER_END`            | `UserEnd`              | 59999      |
| `OM_NULL`                | `Null`                 | 65535      |

Note: `NgTransferRawMode` (3) is retained but legacy — mode 3 is no longer raw
CAN; use `CanPacket` (5) / `CanPacketAscii` (6) for raw CAN transfer.

**3. Error codes (GIT-113)** — only if you referenced the removed
`TransportErrorCode` / `ProtocolErrorCode` enums or their categories; switch to
`ErrorCode` and `sdkErrorCategory()`:

```cpp
// Before
TransportErrorCode::PortBusy
ProtocolErrorCode::BdtpFrameCorrupted
// After
ErrorCode::TransportPortBusy
ErrorCode::BdtpFrameCorrupted
```

All callbacks already deliver `ErrorCode`; no signature changes.

**4. Session / RemoteDevice ABI (GIT-115)** — rebuild your application against
the 1.0.0 headers (one-time ABI break). Idiomatic use via
`std::unique_ptr<Session>` and `->` needs no source change; do not attempt to
subclass `Session` / `RemoteDevice` (now `final`).

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
