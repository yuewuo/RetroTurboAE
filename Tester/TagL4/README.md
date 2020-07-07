# TagL4

This folder contains utilities to manipulate tag device, specifically the tag device based on STM32L432 MCU and PCB design of `SoftTag-L432`. The prefix of this directory is `T4_`, which means the compiled binary has a prefix of `T4_`, and also has a suffix of `.exe` on Windows.

## Dump.cpp

To dump the status of a device, run `./T4_Dump <portname>`. `portname` is mostly like `COMx` on Windows and `/dev/ttyx` on Linux. A sample output is here

```ini
opening device "COM8"
--- reading 68 bytes (excuting "softio_blocking" consumes 0 us)
[basic information]
  1. command: 0x44 [none]
  2. flags: 0x00 [ ]
  3. verbose_level: 0x80
  4. mode: 0x00 [none]
  5. status: 0x00 [init]
  6. pid: 0x5210
  7. version: 0x19101900
  8. mem_size: 33924
  9. PIN_EN9: 0
 10. PIN_PWSEL: 0
 11. PIN_D0: 0
 12. PIN_RXEN: 0
 13. NLCD: 16
[statistics information]
  1. siorx_overflow: 0
  2. lpuart1_tx_overflow: 0
[Tx Information]
(excuting "softio_blocking" consumes 0 us)
  1. sample_rate: period = 3
             frequency = 8.000000 kHz
  2. count: 0, count_add: 0
  3. tx_underflow: 0
  4. default sample: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

You wouldn't have to understand all the variables above to start off, since the programs below can show you how to manipulate the device well enough.

## SetEN9.cpp

To enable the power chip and to set driving voltage of LCD, run `./T4_SetEN9 <portname> <0|1(EN9)> <0|1(PWSEL)>`. `EN9` should be `1` to enable the power output. `PWSEL` should be `1` to select the 9V voltage, otherwise it would be 4.5V. **Note that MCU is booted up without setting the `EN9`, so that you wouldn't get expected flickering if you try to send the LCD transmit sequence directly.** The equivalent methods in TurboHost are `tag_set_en9` and `tag_set_pwsel`, each takes a integer value as input.

## RawTx.cpp

Run `./T4_RawTx <portname>`. You should first modify the source code for your own transmit sequence and sample rate before calling this executable. The original source code contains a blink sequence. The default method is giving a `vector<Tag_Sample_t>` as input, then the driver layer would transmit that sequence properly. If you want a callback API, then refer to `LCD8421Tester.cpp`, which is more complicated using C++ lambda expression as callback function. It would print out lots of streaming state, which you might not want in deployment, then just set the `verbose` to `false` in source code.

## Blink.cpp

To let all LCDs blink for a specific duration with a specific frequency, run `./T4_Blink <portname> <duration:s(=5.)> <frequency:Hz(=1.)>`. All the LCDs will be blinking, mostly used for debugging.

## AlwaysBlink.cpp

It is allowed that the LCDs always blink at a specific frequency. `./T4_AlwaysBlink <portname> <frequency:Hz(=1.)>`.

## CompressedTx.cpp

You can send compressed Tx sequence also, which requires a modification to the source code. The original source code is for the preamble of `8:4:2:1` LCDs. The equivalent method in TurboHost is `tag_send`.

## LCD8421Tester.cpp

Test `8:4:2:1` LCDs by transmitting a sequence to test the individual flicking, `./T4_LCD8421Tester [target_idx(=-1)] [frequency=Hz(=2.)] [multi(=1)]`. You can run `./T4_LCD8421Tester` for more usage information. Briefly saying, you can test a specific LCD or all of them, which is useful when trying to determine the order of all LCDs.

## TestIfReboot.cpp

This does nothing with LCD, but just to test whether MCU has rebooted by reading and writing an unused variable. The story was, we developed a tag device for MobiCom'19 demo program, which has a battery and totally wireless. We tried to let the tag run after plugging off the USB cable, to keep flickering at a given sequence. The result is optimistic, so that we plugin the USB cable again, the variable remains the same value.