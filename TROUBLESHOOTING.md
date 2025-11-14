# Modbus Troubleshooting Guide for HW-519

## Current Status: Connection Timeout

If you're seeing "Connection timed out" errors, follow these steps:

### 1. Verify ESP32 is Running
Check the monitor output. You should see:
```
I (XXX) MB_SLAVE: Modbus slave alive - waiting for requests (poll count: XXXX)
```

If you DON'T see this message every 10 seconds:
- The firmware didn't upload correctly
- The ESP32 is stuck in a crash loop
- Check for panic/error messages in the monitor

### 2. Check Hardware Connections

#### HW-519 Wiring (CRITICAL):
```
ESP32-S3          HW-519
GPIO18 (TX) -->   TXD
GPIO16 (RX) <--   RXD
5V          -->   VCC (NOT 3.3V!)
GND         -->   GND

HW-519            RS485 Bus
A           -->   A (Data+)
B           -->   B (Data-)
```

**Common Mistakes:**
1. ❌ Using 3.3V instead of 5V for HW-519 power
2. ❌ Swapped TX/RX (TX goes to TXD, RX goes to RXD)
3. ❌ A/B reversed (try swapping if no response)
4. ❌ Missing common ground

### 3. Verify HW-519 Module

The HW-519 has some configuration jumpers/options:

1. **Check the 120Ω termination resistor**
   - Look for a jumper labeled "120R" or similar
   - For testing with short cables, termination often NOT needed
   - For long cables (>10m), enable termination on BOTH ends

2. **Verify Power LED**
   - HW-519 should have a power indicator LED
   - If not lit, check 5V power supply

3. **Check for TX/RX LEDs**
   - Some HW-519 modules have activity LEDs
   - TX LED should flicker when master sends requests
   - If TX LED flickers but RX doesn't = ESP32 not responding

### 4. Test UART Communication

Create a simple loopback test by temporarily connecting GPIO18 to GPIO16 (short TX to RX on ESP32 side):

**WARNING:** Do this ONLY with HW-519 disconnected!

This will verify:
- UART is properly configured
- Pins are correct
- Basic communication works

### 5. Check Modbus Master Configuration

Your master MUST match these settings EXACTLY:

| Setting       | Value  | Notes                           |
|---------------|--------|---------------------------------|
| Mode          | RTU    | NOT ASCII, NOT TCP              |
| Baud Rate     | 9600   | Must match exactly              |
| Data Bits     | 8      | Standard                        |
| Parity        | None   | No parity (N)                   |
| Stop Bits     | 1      | Standard                        |
| Slave Address | 1      | Default (configurable via WiFi) |

### 6. Common mbpoll Issues

If using mbpoll, the correct command is:
```bash
# Read 10 holding registers starting at address 0
mbpoll -a 1 -0 -r 0 -c 10 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0

# Breakdown:
# -a 1       : Slave address 1
# -r 0       : Start at register 0
# -c 10      : Read 10 registers
# -t 4       : Type 4 = holding registers (16-bit)
# -b 9600    : Baud rate
# -P none    : No parity
# -s 1       : 1 stop bit
# /dev/ttyUSB0 : Your serial port
```

**Common mbpoll mistakes:**
- Missing `-s 1` (stop bits)
- Using `-t 3` instead of `-t 4` (wrong register type)
- Wrong serial port

### 7. Verify Serial Port Permissions

```bash
# Check if port exists
ls -la /dev/ttyUSB* /dev/ttyACM*

# Check permissions
ls -la /dev/ttyUSB0

# Add user to dialout group (Linux)
sudo usermod -a -G dialout $USER
# Then logout/login

# Or temporarily use sudo
sudo mbpoll -a 1 -0 -r 0 -c 10 -t 4 -b 9600 -P none -s 1 /dev/ttyUSB0
```

### 8. Try Different Serial Ports

USB-to-RS485 adapters can appear as different devices:
```bash
# List all serial devices
ls /dev/tty* | grep -E 'USB|ACM'

# Try dmesg to see what was detected
dmesg | tail -20
```

### 9. Raw UART Test

If nothing works, test raw UART communication:

```bash
# Install minicom
sudo apt-get install minicom

# Configure for 9600 8N1
minicom -b 9600 -D /dev/ttyUSB0

# Type some characters - you won't see Modbus response but 
# this verifies the serial port is working
```

### 10. Enable ESP32 Debug Logging

If you're getting "slave alive" messages but still timeout, there might be an issue with the Modbus stack initialization. Check for these errors in boot log:

```
E (XXX) MB_SLAVE: mb stack initialization failure
E (XXX) MB_SLAVE: mb stack set slave ID failure
```

### 11. A/B Polarity Test

RS485 A/B can be reversed (there's no universal standard):

```
Try 1: HW-519 A → Bus A, HW-519 B → Bus B
Try 2: HW-519 A → Bus B, HW-519 B → Bus A (SWAP)
```

One of these WILL work if everything else is correct.

### 12. Check for Bus Contention

If multiple devices on RS485 bus:
- Ensure only ONE device has termination enabled on each end
- Verify all devices use different slave addresses
- Check for ground loops (only one common ground point)

### 13. Voltage Levels

HW-519 needs:
- **VCC: 5V** (4.5V - 5.5V acceptable)
- ESP32 GPIO: 3.3V logic (HW-519 is compatible)

Use a multimeter to verify:
- VCC = ~5V
- GPIO18/16 when idle = ~3.3V

### 14. Last Resort: Oscilloscope/Logic Analyzer

If you have test equipment:
- Monitor GPIO18 (TX) - should see data bursts when master polls
- Monitor A/B differential voltage - should see ±2V to ±6V swings
- Verify baud rate is actually 9600 (104 μs per bit)

## Still Not Working?

Provide these details:
1. ESP32 monitor output (especially boot messages)
2. Exact mbpoll or test command used
3. USB-to-RS485 adapter model
4. Photos of wiring
5. Output of `ls -la /dev/ttyUSB*`

## Quick Test Checklist

- [ ] ESP32 boots and shows "Modbus slave alive" every 10 seconds
- [ ] HW-519 has 5V power (NOT 3.3V)
- [ ] GPIO18 → HW-519 TXD
- [ ] GPIO16 → HW-519 RXD  
- [ ] Common ground connected
- [ ] A/B connected to bus (tried both polarities)
- [ ] Master uses: 9600 baud, 8-N-1, RTU mode
- [ ] Master addresses slave ID 1
- [ ] Serial port has correct permissions
- [ ] No other program using the serial port

## E-Ink Display Issues

### Screen shows half white / half black (vertical or horizontal split)
Symptom: After calling `display_show_startup()` or a regular update, the panel displays a sharp division—one portion white, the other black—or only partial graphics render.

Confirmed Root Cause: SPI bus `max_transfer_sz` set to 4096 while full frame is 4736 bytes (296*128/8). The over-sized transaction was truncated by DMA, so only the first part of the buffer reached the E-Ink controller.

Implemented Fixes:
1. `display.c`: Increased `max_transfer_sz` to `EPD_BUFFER_SIZE + 32`.
2. `eink_driver.c`: Added chunked transfer loop (1024-byte chunks) keeping CS LOW for entire frame.
3. Added buffer diagnostics log (first, middle, last 8 bytes + checksum) to verify buffer integrity.

Expected Log Snippet:
```
EINK_DRV: Sending column-by-column: 296 columns x 16 byte-rows = 4736 bytes
EINK_DRV: Buffer diagnostics: sum=... first8=... mid8=... last8=...
```
No SPI errors should appear. If errors occur, they will show the offset at which transmission failed.

If Problem Persists:
- Ensure power pins `EINK_POWER_1` and `EINK_POWER_2` are HIGH before init.
- Check BUSY pin transitions LOW during refresh. Stuck HIGH may mean incomplete init.
- Reduce SPI clock from 6 MHz to 4 MHz inside `display.c` devcfg if signal integrity is suspect.
- Reseat the flex cable and inspect for damage.
- Confirm CS stays LOW across all chunks (manual control around loop does this).

Diagnostic Pattern:
`display_show_startup()` deliberately fills first half of buffer white and second half black. If you see a different split size than exactly half, the buffer mapping or transfer is still wrong.

Next-Level Initialization (Optional):
For stubborn artifacts, implement full controller init: Panel Setting (0x00), Power Setting (0x01), Booster Soft Start (0x06), PLL (0x30), Resolution (0x61), VCOM/LUT sequence, then data writes (0x24) + Refresh (0x20/0x12 depending on controller variant).

Reference Frame Sizes:
- Width: 296 pixels
- Height: 128 pixels
- Bytes per frame: 296 * 128 / 8 = 4736

## Web Interface Authentication Issues

### Not Being Prompted for Username/Password

**Symptoms:** When accessing http://192.168.4.1 or device IP, no login dialog appears.

**Possible Causes:**

1. **Firmware Not Uploaded**
   - Authentication was added in v1.22
   - Flash the new firmware: `platformio run -e vision-master-e290-arduino --target upload`
   - Monitor boot output: `platformio run -e vision-master-e290-arduino --target monitor`
   - Look for: `>>> Authentication: Enabled`

2. **Browser Has Cached Credentials**
   - Open a **private/incognito window**
   - Or clear browser cache/cookies for the device IP
   - Try different browser

3. **Authentication Disabled in NVS**
   - Check serial monitor output on boot for:
     ```
     >>> Loading authentication credentials...
     Authentication: Disabled
     ```
   - If disabled, access /security page to re-enable
   - Or clear NVS: Flash with "Erase Flash: All Flash Contents" option

4. **Network Issue**
   - Verify device IP address is correct
   - In AP mode: http://192.168.4.1
   - In client mode: Check display or serial output for IP
   - Ping device to verify connectivity

### Default Credentials Not Working

**Default credentials:** Username: `admin`, Password: `admin`

**If defaults don't work:**

1. **Credentials Changed Previously**
   - Try credentials you may have set before
   - Check serial monitor for authentication attempts:
     ```
     >>> Auth check: enabled=1, user=admin
     >>> Auth failed, requesting credentials
     ```

2. **Reset Credentials via NVS Erase**
   - Erase all flash and reprogram:
     ```bash
     platformio run -e vision-master-e290-arduino -t erase
     platformio run -e vision-master-e290-arduino -t upload
     ```
   - This will restore default credentials

3. **Keyboard Layout Issues**
   - Ensure keyboard is in English layout
   - Try typing credentials in notepad first, then copy/paste

### Locked Out After Changing Credentials

**If you changed credentials and forgot them:**

1. **Serial Monitor Shows Auth Failures**
   ```
   >>> Auth check: enabled=1, user=newuser
   >>> Auth failed, requesting credentials
   ```

2. **Recovery Options:**

   **Option A: Disable Auth via Serial (if you add this feature)**
   - Connect serial monitor
   - Send command to disable auth (if implemented)

   **Option B: Erase NVS Partition**
   ```bash
   # Erase all flash including NVS
   platformio run -e vision-master-e290-arduino -t erase

   # Re-upload firmware
   platformio run -e vision-master-e290-arduino -t upload
   ```
   - This restores defaults but loses all settings (WiFi, LoRaWAN, Modbus ID)

   **Option C: Disable Auth in Code (Emergency)**
   - Edit `src/main.cpp` line ~142:
     ```cpp
     bool auth_enabled = false;  // Temporarily disable
     ```
   - Upload modified firmware
   - Access /security page, change credentials
   - Re-enable auth: `auth_enabled = true;`
   - Upload again

### Authentication Works But Keeps Asking

**Symptoms:** Login dialog appears repeatedly even after entering correct credentials.

**Causes:**

1. **Browser Cookie/Session Issue**
   - Clear browser cookies
   - Try different browser
   - Disable browser extensions that might interfere

2. **HTTP vs HTTPS Confusion**
   - Use `http://` explicitly, not `https://`
   - Device only supports HTTP (no TLS/SSL)

3. **Proxy or VPN Interference**
   - Disable proxy settings
   - Disable VPN temporarily
   - Try direct connection

### Security Best Practices

**After First Login:**

1. **Change Default Credentials Immediately**
   - Navigate to /security page
   - Set strong username and password
   - Click "Update Authentication"
   - Re-login with new credentials

2. **Verify Authentication is Enabled**
   - Check serial monitor for:
     ```
     >>> Loading authentication credentials...
     Authentication: Enabled
     ```

3. **Monitor Authentication Attempts**
   - Serial monitor shows all auth attempts:
     ```
     >>> Auth check: enabled=1, user=admin
     >>> Auth success
     ```
   - Watch for unauthorized access attempts

4. **For Production Deployment**
   - Change default WiFi AP password (modbus123)
   - Use unique credentials per device
   - Consider adding HTTPS support
   - Implement rate limiting for auth attempts

