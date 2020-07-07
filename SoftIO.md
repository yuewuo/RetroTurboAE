# SoftIO Design

## Introduction

In order to fully control MCU with data streaming, a traditional packet design would introduce redundant code to interpret those messages from host PC. We design a Soft-IO that works on the abstract of memory synchronization. 

PC and MCU are unaware of how data is transferred by just setting the data structure and sync them. For optimization of large data streaming, we also enable highly efficient function that push and pull streams between PC and MCU.

The underneath interface required is serial communication with **no packet loss**.  A basic example is running on STM32 with USB CDC.

## Packet Structure

The request packet structure is 4 byte control word, followed by data and 8bit checksum (for write operations). The maximum memory space is `1MB`. The control word is defined as below:

```
|-|-|-|-|-|-|-|-| |-|-|-|-|-|-|-|-| |-|-|-|-|-|-|-|-| |-|-|-|-|-|-|-|-|
|--type-|-----------------start address-------------| |----length-----|
     4 bit                   20 bit                        8 bit ( 1<=length<=254)
```

the possible `type` fields are:

- 0x0: read remote memory to local, return `0x1 + 8bit length + data + 8bit checksum`
- 0x2: write local memory to remote, return `0x3 + 8bit length` for success otherwise error
- 0x4: read remote fifo, may return less than wanted and that's fine, return `0x5 + 8bit length + data + 8bit checksum`
- 0x6: write remote fifo, return the byte that write successfully, return `0x7 + 8bit length` for successfully written length. Note that write more data than wanted to a fifo is bad behaved, and should be killed then.
- 0x8: clear fifo, lock-free thread safety when the slave is the writer of this fifo, success return `0x9`.
- 0xA: reset fifo, not thread safe but ensure read pointer and write pointer are set to 0 (this is mainly for memory alignment required for DMA operations), return `0xB` for success.
- 0xC: soft reset MCU. if you send 260 bytes of `0xC`, the MCU will surely reset after 1 second. This is fault recovery since badly behaved host program will kill the Soft-IO mechanism. You'll not receive any feed back by doing this and you can expect that several seconds later the MCU is reset.

### Implementation

In the previous version, we found this mechanism easy to program but has rather low performance due to the blocking IO and also limited resource of memory on MCU. The problem is tackled in this version by symmetric operation on both side, enabling not only PC-MCU control but also MCU-MCU or even MCU-PC control. The library provides memory synchronization without knowing the specific structure. For simplicity, all sanity check is done with `assert`.

Before starting, you need to set the proper function pointer of `SoftIO_t` **after initialize** it. This prototype is defined below:

- `char read_blocking()`

Transaction FIFO is used to implement this. Several API is defined as below:

- `softio_delay_read(softio, var)` / `softio_blocking_read(softio, var)`
  - try to start a transaction to read the memory, so in most cases it will only send the message (without flush called) but not waiting for the reply. You should not expect the memory is read after call this function. However, if the transaction queue is full, it will be a blocking call.
- `softio_delay_read_between(softio, var1, var2)`
  - sync all variables between `var1` and `var2` (including both).
- `softio_try_handle_one(softio)` / `softio_try_handle_all(softio)`
  - try to handle the incoming messages, either respond to requests or receive return values.
- `softio_wait_one(softio)` / `softio_wait_all(softio)`
  - wait for the oldest transaction to be done, if existed, otherwise return immediately. The latter will wait for all transaction done.
- `softio_delay_write_fifo(softio, var)`
  - this is easy to fail

### Memory Sync

### Streaming

## Testing

To develop and test Soft-IO implementation, a tester program `Tester/DebugTest/SoftIOtest.cpp` is developed. See it for more details. (also as basic example of this library)