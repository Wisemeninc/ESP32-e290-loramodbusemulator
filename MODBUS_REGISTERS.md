# Modbus Register Map - Implementation Reference

## Holding Registers (0-11) - Function Code 0x03/0x06

| Addr | Description | Access | Implementation | Update |
|------|-------------|--------|----------------|--------|
| 0 | Sequential counter | R/W | ✅ `holding_regs.sequential_counter` <br>Increments on every read | On access |
| 1 | Random number (0-65535) | R | ✅ `holding_regs.random_number` <br>Updated in `updateHoldingRegisters()` | Every 5s |
| 2 | Uptime seconds (low 16-bit) | R | ✅ `holding_regs.uptime_seconds` <br>Low 16 bits of `millis()/1000` | Every 2s |
| 3 | Free heap KB (low word) | R | ✅ `holding_regs.free_heap_kb_low` <br>`ESP.getFreeHeap() / 1024` | Every 2s |
| 4 | Free heap KB (high word) | R | ✅ `holding_regs.free_heap_kb_high` <br>`(ESP.getFreeHeap() / 1024) >> 16` | Every 2s |
| 5 | Min free heap KB | R | ✅ `holding_regs.min_heap_kb` <br>`ESP.getMinFreeHeap() / 1024` | Every 2s |
| 6 | CPU frequency MHz | R | ✅ `holding_regs.cpu_freq_mhz` <br>`ESP.getCpuFreqMHz()` | Every 2s |
| 7 | FreeRTOS task count | R | ✅ `holding_regs.task_count` <br>`uxTaskGetNumberOfTasks()` | Every 2s |
| 8 | Temperature × 10 | R | ✅ `holding_regs.temperature_x10` <br>`temperatureRead() * 10` | Every 2s |
| 9 | CPU cores | R | ✅ `holding_regs.cpu_cores` <br>Fixed value: 2 | Static |
| 10 | WiFi AP enabled (0/1) | R | ✅ `holding_regs.wifi_enabled` <br>`wifi_ap_active ? 1 : 0` | On change |
| 11 | WiFi clients (0-4) | R | ✅ `holding_regs.wifi_clients` <br>`WiFi.softAPgetStationNum()` | On change |

**Notes:**
- Register 0 is the ONLY writable holding register
- Registers 3-4 form 32-bit heap value: `heap_kb = (reg4 << 16) | reg3`
- All registers update automatically in background tasks

## Input Registers (0-8) - Function Code 0x04 (Read Only)

| Addr | Description | Range | Scale | Units | Implementation |
|------|-------------|-------|-------|-------|----------------|
| 0 | SF₆ density | 1500-3500 | ×0.01 | kg/m³ | ✅ `input_regs.sf6_density` <br>25.50 kg/m³ → 2550 |
| 1 | SF₆ pressure @20°C | 3000-8000 | ×0.1 | kPa | ✅ `input_regs.sf6_pressure_20c` <br>550.0 kPa → 5500 |
| 2 | SF₆ temperature | 2880-3030 | ×0.1 | K | ✅ `input_regs.sf6_temperature` <br>293.0 K → 2930 |
| 3 | SF₆ pressure variance | 3000-8000 | ×0.1 | kPa | ✅ `input_regs.sf6_pressure_var` <br>Mirrors register 1 |
| 4 | Slave ID | 1-247 | 1 | - | ✅ `input_regs.slave_id` <br>Matches configured slave ID |
| 5 | Serial number Hi | 0x0000-0xFFFF | - | Hex | ✅ `input_regs.serial_hi` <br>Fixed: 0x1234 |
| 6 | Serial number Lo | 0x0000-0xFFFF | - | Hex | ✅ `input_regs.serial_lo` <br>Fixed: 0x5678 |
| 7 | Software version | 10-50 | ×0.1 | - | ✅ `input_regs.sw_release` <br>10 = v1.0 |
| 8 | Quartz frequency | 14500-15500 | ×0.01 | Hz | ✅ `input_regs.quartz_freq` <br>14750 = 147.50 Hz |

**Notes:**
- All input registers are READ ONLY
- SF₆ values update every 3 seconds with realistic drift
- Serial number: `0x12345678` (registers 5-6)
- Values include small random variations for realism

## Modbus Function Codes Supported

| Code | Function | Registers | Status |
|------|----------|-----------|--------|
| 0x03 | Read Holding Registers | 0-11 | ✅ Implemented |
| 0x04 | Read Input Registers | 0-8 | ✅ Implemented |
| 0x06 | Write Single Register | 0 only | ✅ Implemented |

## Callbacks Implementation

**Read Holding Register:**
```cpp
cbReadHoldingRegister(TRegister* reg, uint16_t val)
- Auto-increments register 0 on every read
- Returns live system values for all registers
- Tracked in modbus_request_count and modbus_read_count
```

**Read Input Register:**
```cpp
cbReadInputRegister(TRegister* reg, uint16_t val)
- Returns SF₆ sensor emulation values
- Returns static device info (serial, version)
- Tracked in modbus_request_count and modbus_read_count
```

**Write Holding Register:**
```cpp
cbWriteHoldingRegister(TRegister* reg, uint16_t val)
- Only allows writes to register 0
- Logs write operations to serial
- Tracked in modbus_request_count and modbus_write_count
```

## Testing Commands

### Read All Holding Registers (0-11)
```bash
mbpoll -a 1 -0 -r 0 -c 12 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0
```

### Read All Input Registers (0-8)
```bash
mbpoll -a 1 -0 -r 0 -c 9 -t 3 -b 9600 -P none -s 1 /dev/ttyUSB0
```

### Write to Sequential Counter (Register 0)
```bash
mbpoll -a 1 -0 -r 0 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0 500
```

### Read Specific SF₆ Values (Input Registers 0-3)
```bash
mbpoll -a 1 -0 -r 0 -c 4 -t 3 -b 9600 -P none -s 1 /dev/ttyUSB0
```

## Status: ✅ COMPLETE

All registers from the documentation are fully implemented and match the specification exactly.
