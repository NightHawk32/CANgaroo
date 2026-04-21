# CANgaroo

CAN bus analyzer and trace tool built with Qt6 and C++.

## Build System

- **qmake** (not CMake) with `.pro` / `.pri` files
- Entry point: `src/src.pro`
- Requires **Qt 6** (Widgets, Xml, Charts, SerialPort, SerialBus, Network)
- Version is defined in `src/src.pro` via `VERSION = x.y.z`

### Build (Linux)

```bash
cd src && qmake6 && make -j$(nproc)
```

### Optional driver flags

```bash
qmake6 CONFIG+=kvaser CONFIG+=peakcan
```

- `CONFIG+=peakcan` — enables PeakCAN driver (Windows only, needs PCAN-Basic SDK)
- `CONFIG+=kvaser` — enables Kvaser driver (needs CANlib SDK)

### Platform-specific drivers

| Driver        | Platform | Notes                                      |
|---------------|----------|--------------------------------------------|
| SocketCAN     | Linux    | libnl-3, libnl-route-3                     |
| SLCAN         | All      | Serial port based                          |
| GrIP          | All      | Serial port based                          |
| CANBlaster    | All      | UDP based                                  |
| CandleAPI     | Windows  | gs_usb devices                             |
| PeakCAN       | Windows  | Requires `CONFIG+=peakcan`                 |
| Kvaser        | All      | Requires `CONFIG+=kvaser` + CANlib SDK     |
| Vector        | All      | Qt SerialBus plugin (`vectorcan`)          |
| TinyCAN       | All      | Qt SerialBus plugin (`tinycan`)            |

## Architecture

### Driver pattern

- `CanDriver` — discovers and owns `BusInterface` instances
- `BusInterface` (QObject) — abstract interface for send/receive
- `BusListener` — runs in a dedicated QThread, calls `readMessage()` in a loop and feeds `BusTrace`
- Driver constructors use `reinterpret_cast<CanDriver*>(driver)`
- TX messages: appended to a mutex-protected `_txMsgList` in `sendMessage()`, dequeued in `readMessage()`
- Qt SerialBus plugins (Vector, TinyCAN): check `QCanBus::instance()->plugins().contains()` before use
- Drivers with enable/disable toggle (CANBlaster, TinyCAN): follow settings pattern in `mainwindow.cpp`

### Key classes

- `Backend` — singleton, owns `BusTrace`, measurement setup, drivers
- `BusTrace` — thread-safe message store, supports save to candump/ASC/PCAP/PCAPng
- `CanMessage` — value type representing a single CAN frame (registered as Qt metatype)

### File layout

```
src/
  core/          — Backend, BusTrace, CanMessage, CanDb, Log, MeasurementSetup
  driver/        — BusInterface, BusListener, CanDriver + per-driver subdirectories
  parser/        — dbc/ (DBC parser), ldf/ (LIN Description File parser, header-only)
  decoders/      — protocol decoders (UDS, J1939)
  window/        — UI windows (TraceWindow, SetupDialog, TxGeneratorWindow, ReplayWindow, ...)
  helpers/       — utility code
  mainwindow.*   — application shell, driver registration, menu actions
```

### Adding a new driver

1. Create `src/driver/NewDriver/` with `NewDriver.h/.cpp`, `NewInterface.h/.cpp`, `NewDriver.pri`
2. In `.pri`: list HEADERS/SOURCES, add `QT += serialbus` if using Qt plugin
3. Include the `.pri` from `src/src.pro` (use `win32:` or `unix:` prefix if platform-specific)
4. Register the driver in `mainwindow.cpp` (with optional settings-based enable/disable)
5. TX reporting: add `QMutex _txMutex` + `QList<CanMessage> _txMsgList` to the interface

## Code Conventions

- Use modern C++ (C++20 and above):
- `auto` and structured bindings where they improve readability
- Range-based for loops, `std::as_const()` for const iteration
- `constexpr` and `if constexpr` where applicable
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw owning pointers
- `std::chrono` for all time-related code
- Concepts and constraints for template interfaces
- Designated initializers for aggregate types
- `std::optional`, `std::variant`, `std::string_view` where appropriate
- `[[nodiscard]]`, `[[maybe_unused]]` attributes
- `noexcept` on move constructors, destructors, and non-throwing functions
- Scoped enums (`enum class`) over unscoped enums
- Allman brace style (opening brace on its own line)
- Qt6 API only (no deprecated Qt5 patterns)
- `override` instead of `Q_DECL_OVERRIDE`
- `QString::arg()` / `QString::number()` instead of `QString::asprintf()`
- `static_cast` / `reinterpret_cast` instead of C-style casts
- `QProcess::start()` + `waitForFinished()` instead of `QProcess::execute()`
- Include order: own header, C++ standard, Qt, project, platform
- Separate include groups with blank lines

## CI

- GitHub Actions workflow: `.github/workflows/cmake.yml`
- MSYS2 for Windows builds with built-in caching (`cache: true` in setup-msys2)
- Version extracted from `src/src.pro` (not git describe)
