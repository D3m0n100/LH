# F2812 Bridge Integration (MVP)

This document defines the minimum contract for:
- PC software (this repo)
- Modbus controller bridge firmware
- Target F2812 firmware

## 1. Topology

`PC (Modbus RTU master) -> Controller -> F2812 target`

Addressing mode in software:
- `TargetAsSlaveId` (default): target id is used as Modbus slave id.

## 2. Serial and Modbus settings

- Baud rate: `115200`
- Data bits: `8`
- Parity: `None`
- Stop bits: `1`
- Response timeout: `300 ms`
- Retry count: `3`

## 3. Required bridge config keys

The download module expects these keys under `comm`:
- `protocol=MODBUS`
- `mode=RTU`
- `type=Master`
- `port`, `baudRate`, `dataBits`, `parity`, `stopBits`
- `address`, `responseTimeout`, `retryCount`
- `enableBridge=true`
- `bridge.*`

## 4. F2812 register contract (MVP)

All addresses are holding registers.

- `0x0400` (`1024`): enter boot command, write `0x5A5A`
- `0x0401` (`1025`): boot state
  - `0`: Idle
  - `1`: Ready
  - `2`: Busy
  - `3`: Done
  - `4`: Error
- `0x0410` (`1040`): packet index (optional header)
- `0x0411` (`1041`): packet byte length (optional header)
- `0x0412` (`1042`): packet CRC16 (optional header)
- `0x0413` (`1043`): packet byte offset (optional header)
- `0x0420` (`1056`): packet data buffer start (`60 words`)
- `0x0402` (`1026`): finalize command, write `0xA5A5`
- `0x0403` (`1027`): result code (`0` means success)
- `0x0404` (`1028`): error detail code (`0` means none)

## 5. Download flow

1. `enter`: write `0x5A5A` to `0x0400`
2. `poll`: wait `0x0401 == 1`
3. `sendChunk`: write packet header registers (if configured), then write data to `0x0420`
4. `finalize`: write `0xA5A5` to `0x0402`
5. `poll`: wait `0x0401 == 3`
6. `queryResult`: read `0x0403..0x0404`, expect `[0, 0]`

## 6. Notes

- The software now supports cancellation during poll/chunk transfer.
- `sendChunk` supports optional per-packet metadata registers:
  - `packetIndexAddress`
  - `packetLengthAddress`
  - `packetCrcAddress`
  - `packetOffsetAddress`
- `queryResult` supports strict checking via `expected`.
