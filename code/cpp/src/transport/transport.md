# Transport Design Overview

## Asynchronous Receive Pattern

- Uses a **dedicated background read thread** that continuously polls the serial port (default 10ms interval)
- The thread calls `readSync()` which blocks briefly waiting for data (either via Windows overlapped I/O or POSIX `select()`)
- Data received is placed into a **message-oriented ring buffer** as complete message blocks
- When async operations are pending, complete messages are dequeued and passed to completion handlers
- Async recv requests queue up if no messages are immediately available; they're satisfied as the background thread enqueues messages

## Message-Oriented Buffer Design

### MessageRingBuffer

The transport layer uses `MessageRingBuffer<std::vector<uint8_t>>` instead of a byte-oriented ring buffer. This stores complete message blocks rather than individual bytes.

```cpp
template <typename T = std::vector<uint8_t>>
class MessageRingBuffer
{
  std::deque<T> messages_;           // Queue of message buffers
  std::size_t maxMessages_;          // Capacity in messages
  std::mutex mutex_;
  std::condition_variable dataAvailable_;
  
public:
  bool enqueue(T&& message);                    // Move message into buffer
  bool enqueue(std::span<const uint8_t> data);  // Copy from span
  std::optional<T> dequeue();                   // Move message out
  void reset(std::size_t maxMessages);          // Clear and set new capacity
  void clear();
  void notifyAll();                             // Wake waiting threads
};
```

### Key Advantages

- ✓ Eliminates byte-at-a-time copying
- ✓ Each message stored at exact payload size (not fixed buffer size)
- ✓ Single completion per message (not per-byte)
- ✓ Better burst handling: each read = one enqueue
- ✓ Easy backpressure via message count check
- ✓ Thread-safe with mutex protection and condition variable for blocking waits

## Serial Transport

### Read Thread Operation

1. Read from serial port into temporary buffer (configurable, default 512 bytes)
2. Create right-sized `std::vector<uint8_t>` containing only the bytes actually read
3. `std::move` the message into the ring buffer via `enqueue()`
4. Process any pending async receive operations

```cpp
void SerialTransport::readThreadFunc() {
    std::vector<uint8_t> tempBuffer(tempBufferSize_);
    
    while (!stopRequested_ && isOpen()) {
        const auto bytesRead = readSync(tempBuffer.data(), tempBuffer.size(), 100);
        
        if (bytesRead > 0) {
            // Create right-sized message
            std::vector<uint8_t> message(tempBuffer.begin(), tempBuffer.begin() + bytesRead);
            
            // Move into ring buffer
            if (!messageBuffer_.enqueue(std::move(message))) {
                // Buffer overflow - message dropped
            }
            
            processAsyncOperations();
        }
    }
}
```

### Configuration

```cpp
struct SerialTransportConfig {
    std::string port;                  // Port name (e.g., "COM7", "/dev/ttyUSB0")
    unsigned baud = 115200;            // Baud rate
    unsigned dataBits = 8;             // Data bits (5-8)
    char parity = 'N';                 // Parity: 'N'=None, 'E'=Even, 'O'=Odd
    unsigned stopBits = 1;             // Stop bits (1 or 2)
    std::size_t readBufferSize = 512;  // Temp buffer size for serial reads
    std::size_t writeBufferSize = 4096;
    unsigned readTimeoutMs = 10;       // Read timeout/poll interval in milliseconds
    std::size_t maxPendingMessages = 16; // Max messages in ring buffer
};
```

### Public Configuration (SerialConfig)

```cpp
struct SerialConfig {
    std::string port;
    unsigned baud = 115200;
    unsigned dataBits = 8;
    char parity = 'N';
    unsigned stopBits = 1;
    unsigned readBufferSize = 512;       // Temp buffer size
    unsigned readTimeoutMs = 10;         // Poll interval in milliseconds
    unsigned maxPendingMessages = 16;    // Ring buffer capacity
};
```

## Loopback Transport

The `LoopbackTransport` also uses `MessageRingBuffer` for testing. Each `asyncSend()` call enqueues a complete message that can be received via `asyncRecv()`.

```cpp
class LoopbackTransport {
    MessageRingBuffer<std::vector<uint8_t>> messageBuffer_;  // Capacity: 64 messages
    
    std::size_t messagesAvailable() const;  // Number of pending messages
    std::size_t bytesAvailable() const;     // Total bytes across all messages
};
```

## Async Receive Behavior

With message-oriented buffering, `asyncRecv()` passes complete messages directly to the callback:

```cpp
// New API: data passed via callback, no caller-provided buffer needed
using RecvCompletionHandler = std::function<void(ErrorCode code, ConstByteSpan data)>;

void asyncRecv(RecvCompletionHandler completion);
```

- The callback receives the complete message as a `ConstByteSpan`
- One `asyncRecv()` call = one message (not partial bytes)
- No buffer size concerns - the transport owns the data and passes it efficiently
- Span is only valid during the callback; copy if needed later

This differs from byte-oriented APIs where callers provide buffers and receive byte counts.

## Poll Interval Analysis

The `readTimeoutMs` configuration controls the polling interval:

| Interval | Worst Latency | Thread Wakeups/sec | CPU Impact |
|----------|---------------|-------------------|-----------|
| 100ms    | ~100ms        | 10                | Minimal   |
| 50ms     | ~50ms         | 20                | Low       |
| 10ms     | ~10ms         | 100               | Moderate  |
| 1ms      | ~1ms          | 1000              | High      |

**Default**: 10ms (good balance of latency vs. CPU usage)

**Recommendations**:
- Use 100ms for low-power/battery deployments
- Use 10ms or lower for low-latency applications
- The poll interval also affects message batching - longer intervals may batch more data per message
