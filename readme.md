# Miocasa's Mechanical Keyboard Firmware

This is a keyboard firmware based on the qmk_firmware keycodes system and recreate all functions, that must work on all Arduino and TinyUsb compatible microcontrollers.

### ⚠️ Now tested only on xiao seeed nrf52480 sense ️⚠️   

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Framework-blue?logo=platformio)](https://platformio.org)
[![Arduino](https://img.shields.io/badge/Arduino-Core-blue?logo=arduino)](https://arduino.cc)
[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-red.svg)](LICENSE)

## Table of Contents

- [Project Status](#project-status)
- [Project Structure](#project-structure)
- [Building the Firmware](#building-the-firmware)
- [Debugging](#debugging)
- [Hardware Pinout & Matrix Layout](#hardware-pinout--matrix-layout)
- [Configuration](#configuration)
- [Contributing](#contributing)
- [Changelog](CHANGELOG.md)


## Project Status

- Now we have part of features of qmk/tmk like
  - Layers and layers change keycodes
  - NKRO by default, now here no boot keyboard descriptor
  - Consumer input
  - Mouse input


## Refactoring & Development Targets
- Ble connection
- Split keyboard (wired. Maybe ble also, but it too complicated for me now)
- Add macros
- Add combos
- Tap dance
- Ble connection
- Create configuration protocol and app like vial


Detailed change history -> [CHANGELOG.md](CHANGELOG.md)

## Project Structure

```text
.
├── lib
│   ├── qmk
│   │   ├── keycodes.h   // reference from qmk
│   │   ├── qmk_engine.h // input and keycodes proceed code
│   │   ├── qmk.h
│   │   └── quantum_keycodes.h // reference from qmk
│   └── UnifiedHid       // hid service (Uses TinyUsb)
│       ├── UnifiedHid.cpp 
│       └── UnifiedHid.h
├── LICENSE
├── platformio.ini       // libs and microcontroller configuration 
├── readme.md            
└── src
    ├── config.h         // in future will be file with global configurations
    ├── debug.h          // Debug functions
    └── main.cpp         // entry point
```

## Building the Firmware

Requires **PlatformIO**.

```bash
# Build only
pio run
# or
platformio run

# Build + upload
pio run --target upload
# or
platformio run --target upload

# Clean build artifacts
pio run --target clean
```

## Debugging

Conditional debug output is controlled via the `DEBUG` macro. (Temporally we use Serial port)

```cpp
// debug.h fragment
#ifdef DEBUG
    #define D_PRINTF(...)      Serial.printf(__VA_ARGS__)
    #define D_PRINT(...)       Serial.print(__VA_ARGS__)
    #define D_PRINTLN(...)     Serial.println(__VA_ARGS__)
    // more variants...
#else
    #define D_PRINTF(...)      ((void)0)
    #define D_PRINT(...)       ((void)0)
    #define D_PRINTLN(...)     ((void)0)
#endif
```

Usage example:

```cpp
D_PRINTF("Button state: 0x%08lX\n", button_state);
D_PRINTLN("Scan completed");
```

Code blocks wrapped in `#ifdef DEBUG` are completely removed in release builds.

## Hardware Pinout & Matrix Layout
Depend on your configuration     
3×3 key matrix (standard Perkins-style Braille input)

**Rows** (outputs): D2, D1, D0  
**Columns** (inputs with pull-up): D6, D5, D4

```
   D6    D5    D4
   ↑     ↑     ↑
+-----+-----+-----+
|  6  |  7  |  8  |  ← D2 (row 0)
+-----+-----+-----+
|  3  |  4  |  5  |  ← D1 (row 1)
+-----+-----+-----+
|  0  |  1  |  2  |  ← D0 (row 2)
+-----+-----+-----+
```


## Configuration

All important settings are define in [main.cpp](src/main.cpp) file now

- GPIO pin assignments (rows, columns)
- Matrix scan direction (row-to-column or column-to-row)
- BLE device name, pairing mode, passkey // not implemented
- Key mappings
- User code and functions 

## Contributing

### Changelog format

Every version block should follow this exact structure:

```markdown
## [x.y.z] - YYYY-MM-DD
### Id of Branch, that your code based 
### Commit by: @username

### Added
- ...

### Fixed
- ...

### Changed
- ...

### Removed
- ...
```

⚠️ **Important rule**  
All changes to `CHANGELOG.md` **must** follow the format above. Pull requests that do not comply will be rejected.

### Pull Request guidelines

- One logical change per PR
- Update `[Unreleased]` section in CHANGELOG.md
- Use clear, descriptive commit messages (Conventional Commits style is recommended)
- Include debug output or test cases when changing core logic   

> Don't be afraid to contribute; the rules are there to make the changes clear.   
Any help is appreciated
 
### Supporting
If you want to support you can star repo or by me coffee. 

