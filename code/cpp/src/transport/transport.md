# Transport Design Overview

## Asynchronous Receive Pattern

- Uses a **dedicated background read thread** that continuously polls the serial port every 100ms
- The thread calls `readSync()` which blocks briefly waiting for data (either via Windows overlapped I/O or POSIX `select()`)
- Data received is placed into a shared **ring buffer** protected by a mutex
- When async operations are pending, the buffer contents are drained into those operations and their completion handlers are invoked
- Async recv requests queue up if no data is immediately available; they're satisfied as the background thread fills the ring buffer

## Buffer Methodology

- **Dynamic Ring Buffer**: Uses a power-of-2 sized circular buffer (default 4KB) with head/tail pointers and mask for fast modulo arithmetic
- **Single Producer/Consumer**: Background thread is the sole writer; consumer threads read via `asyncRecv()`
- **Thread-Safe**: Mutex protection on buffer access and separate async operation queues
- **Overflow Handling**: If ring buffer fills, incoming data is dropped with an error log; pending operations aren't satisfied until space becomes available

## Current Design Issues

1. **Fixed small buffer** (4KB default) - can drop data during bursts
2. **Polling-based** - wastes CPU polling even when no data arrives
3. **Queue-based async ops** - pending receives queue up; only first is processed per cycle
4. **No backpressure** - overflows silently logged rather than signaled
5. **Separate mutexes** - `readMutex_` and `asyncMutex_` can cause contention

## Design Improvements: Message-Oriented Ring Buffer

### Rationale

Currently, the ring buffer is byte-oriented, requiring multiple copies as data flows from the serial port → ring buffer → async operation. Since all async operaions fundamentally deal with complete message blocks (not individual bytes), a **message-oriented ring buffer** would be more efficient:

- **TCP/UDP transports**: Receive buffer size is known; each read is a complete message
- **Serial transport**: One copy from hardware to an intermediate buffer, then `std::move` into the ring buffer for zero-copy thereafter
- **Async operations**: Receive directly into their buffers via `std::move`, eliminating copies

### Implementation Plan

#### 1. New Ring Buffer Structure
```cpp
// Replace RingBuffer<uint8_t> with:
template <typename T = std::vector<uint8_t>>
class MessageRingBuffer
{
  std::deque<T> messages_;  // Ring of message buffers
  std::mutex mutex_;
  std::condition_variable dataAvailable_;
};
```

#### 2. Key Operations

**Write**: Store received message block as-is
- Serial: Read into temp buffer (512B), copy actual bytes to correctly-sized vector, `ring.enqueue(std::move(rightSizedBuffer))` - efficient memory use
- TCP/UDP: `ring.enqueue(std::move(readBuffer))` - no intermediate copy

**Read**: Dequeue message directly to async receiver
- `auto msg = ring.dequeue(); completion(Ok, msg.size()); receiver.consume(std::move(msg))`
- Zero-copy after initial hardware read

#### 3. Transport Integration

- **SerialTransport**: Read into temp buffer (512B), copy actual bytes to right-sized buffer, `std::move` into ring (efficient memory use)
- **TcpTransport/UdpTransport**: Read directly from socket, `std::move` into ring, zero-copy to consumer
- **AsyncRecv**: No buffer size limit per-message; only limited by ring capacity

#### 4. Advantages

- ✓ Eliminates byte-at-a-time copying
- ✓ Serial ring buffers only store actual payload size, not fixed temp buffer size
- ✓ Single completion per message (not per-byte)
- ✓ Better for burst handling: each read = one enqueue
- ✓ Backpressure easy: check ring before accepting new reads

#### 5. Challenges to Address

- Ring capacity now measured in messages, not bytes
- Async ops might require split messages (rare; handle gracefully)
- Need sensible defaults for message count vs. payload size
- Serial transport still requires one copy (unavoidable)

#### 6. Configuration
```cpp
struct TransportConfig {
  // Current
  size_t readBufferSize = 4096;
  
  // Add:
  size_t maxPendingMessages = 16;      // Ring capacity
  size_t messageBufferSize = 4096;     // Per-message buffer size
};
```

### Poll Interval Analysis: 100ms vs. 10ms

**Current Design (100ms)**
- `readSync()` timeout = 100ms blocking wait for data
- **Worst-case latency**: ~100ms (if data arrives just after timeout)
- **Best-case latency**: ~0ms (data available when thread checks)
- **Average latency**: ~50ms (random arrival within window)
- Fewer ring buffer enqueues, larger messages batched together

**Proposed: 10ms Interval**
- Reduces worst-case latency to ~10ms
- More frequent small messages in ring buffer
- **Trade-offs**:
  - ✓ Lower latency (good for real-time protocols)
  - ✓ Ring buffer processes smaller batches more frequently
  - ✗ 10x more thread wake-ups = higher CPU overhead
  - ✗ More context switches and mutex contention
  - ✗ Potential thrashing if messages arrive in bursts

**Recommendation**
- Make poll interval **configurable** in `SerialTransportConfig.readTimeoutMs`
- Default to 100ms for low-power deployments
- Allow reduction to 10ms (or lower) for low-latency applications
- Consider adaptive polling: increase timeout during idle, reduce during activity

**Latency vs. CPU Trade-off**
| Interval | Worst Latency | Thread Wakeups/sec | CPU Impact |
|----------|---------------|-------------------|-----------|
| 100ms    | ~100ms        | 10                | Minimal   |
| 50ms     | ~50ms         | 20                | Low       |
| 10ms     | ~10ms         | 100               | Moderate  |
| 1ms      | ~1ms          | 1000              | High      |
