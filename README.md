<img width="547" height="471" alt="image" src="https://github.com/user-attachments/assets/84b0591b-50be-4699-8277-041738c57550" />


# dxs238xw — ESPHome component for DDS238-4W / DTS238-7W energy meters

[![CI](https://github.com/kaboom748/dxs238xw/actions/workflows/ci.yml/badge.svg)](https://github.com/kaboom748/dxs238xw/actions/workflows/ci.yml)

Local control of Hiking / ENGELEC WiFi smart energy meters sold under the **WISEN**
brand — **no cloud, no internet**.

| Model | Type |
|---|---|
| **DDS238-4W** | single phase, DIN rail, 5(60) A |
| **DTS238-7W** | three phase, DIN rail |

These meters ship with a TYWE3S / ESP8285 WiFi module that can be reflashed with
ESPHome, or replaced with any ESP8266/ESP32 board. The measuring board speaks the
HEKR protocol over UART at 9600 8N1.

> [!WARNING]
> These meters are wired to mains voltage. Disconnect the installation before
> opening the meter, and never work on it while it is energised.

## Installation

```yaml
external_components:
  - source: github://kaboom748/dxs238xw
    components: [dxs238xw]
    refresh: 1d

uart:
  id: uart_bus
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 9600

logger:
  baud_rate: 0          # the log would collide with the meter on the same pins

dxs238xw:
  id: meter

sensor:
  - platform: dxs238xw
    active_power_total:
      name: "Active Power"
    total_energy:
      name: "Total Energy"

binary_sensor:
  - platform: dxs238xw
    meter_state:
      name: "Meter State"

switch:
  - platform: dxs238xw
    meter_state:
      name: "Relay"
```

`restore_from_flash: true` is required for the kWh price, the starting index, the
delay and the energy purchase to survive a reboot.

A complete configuration exposing all 57 entities is in
[`example/smartmeter.yaml`](example/smartmeter.yaml).

Full reference documentation: [`docs/dxs238xw.mdx`](docs/dxs238xw.mdx).

## What makes this component different

### It never blocks

The state machine runs inside `loop()` and always returns immediately. No
`while(true) + yield()`, no waiting in `setup()`, no recursion. On an ESP8266 that
also runs WiFi and the API, a blocking serial read is what causes disconnections
and `Component took a long time` warnings.

### It matches the datasheet

Checked against the *DDS238-4W Technical Datasheet* (ENGELEC) and the *DTS238-7
WIFI User Manual*:

| Point | Datasheet | This component |
|---|---|---|
| Current protection | **65 A default**, settable 1-63 A (1-80 A on the 80 A variant) | **1-80 A** |
| Delay | "time delay control **on/off**", **1 min to 24 h** | 1-1440 min |
| Total energy | "negative energy **accumulated into** positive energy" → `total = \|import\| + \|export\|` | `total_increasing` |
| Export | "the reverse active energy, such as solar power generation" | **positive** accumulator |
| Voltage limits | "high limit must be bigger than low limit" | cross-validated |

### Sign correction is exact

The meter encodes negative (export) values as offset binary, with an offset of
**1,000,000 raw units** — the same for current and power. Correcting the sign
*after* scaling means computing `1000.0f - 1000.5f`, and the float spacing at that
magnitude is 6e-5, which destroys the precision of the small result (**-0.50006**
instead of **-0.5**). This component corrects on the raw integer:
`1000500 - 1000000 = 500`, exact.

### It does not fight the meter's firmware

When the delay timer elapses, the meter's own firmware drives the relay — on this
hardware it switches it **on**. Sending a command at that moment means fighting the
meter. Verified on a DTS238-7W: the meter also **never clears its armed flag**, so
`delay_state` stays set forever with `remaining = 0`.

Consequences, all handled:

- ESPHome sends **nothing** when the timer elapses (`delay_end_action: none`).
- The `delay_state` switch reports whether a delay is *running*
  (`delay_state && remaining > 0`), not the raw flag, so it returns to off at `0m`.
- `warning_off_by_end_delay` fires on the **transition** to zero, not on the steady
  state — otherwise a manual switch-off months later would still be blamed on the
  delay.

| `delay_end_action` | Effect |
|---|---|
| `none` *(default)* | ESPHome observes. The meter decides. |
| `disarm` | disarms the timer, without touching the relay |
| `force_off` | forces the relay off — discouraged |

## Known hardware limitation

The manual is explicit: *"the meter do not have time clock internal, the time
control is decide to cloud serve"*. **Scheduling** (fixed-time on/off) depends on
the WISEN cloud and is not reachable from any local component. Only the **delay**
(a relative timer) is local, and it is implemented here. Use Home Assistant
automations or ESPHome's `time` component for scheduling.

## Entities

57 entities across 6 platforms: 32 `sensor`, 8 `binary_sensor`, 4 `text_sensor`,
9 `number`, 3 `switch`, 1 `button`, plus the `meter_state_on` / `meter_state_off` /
`meter_state_toggle` / `hex_message` actions. See [`docs/dxs238xw.mdx`](docs/dxs238xw.mdx).

## Testing without hardware

[`tools/meter_sim.py`](tools/meter_sim.py) simulates a HEKR meter on a virtual
serial port:

```bash
# 1. create a pair of linked virtual serial ports
socat -d -d pty,raw,echo=0,link=/tmp/meter pty,raw,echo=0,link=/tmp/esp

# 2. run the simulator on one end
python tools/meter_sim.py /tmp/meter

# 3. point an ESPHome `host` platform build at the other end: /tmp/esp
```

## Quality checks

| Check | Result |
|---|---|
| Google Test unit tests (`tests/components/dxs238xw/`) | 11/11 |
| `esphome config`, esp8266-ard / esp32-ard / esp32-idf / rp2040-ard | 4/4 |
| ESPHome `ci-custom.py` | 0 |
| `ruff check` / `ruff format` | 0 |
| `clang-format` (ESPHome config) | 0 |
| `-Wall -Wextra -Wpedantic`, 3 log levels | 0 warnings |

The layout mirrors the official ESPHome repository (`esphome/components/…`,
`tests/components/…`), so it also works directly as a `github://` source.

## Credits

The HEKR protocol was reverse engineered by **[rodgon81](https://github.com/rodgon81)**,
whose [`dxs238xw` component](https://github.com/rodgon81/esphome) is the origin of
this work — see [esphome/esphome#3750](https://github.com/esphome/esphome/pull/3750).
This component keeps his protocol map and frame layout, and departs from his
implementation on the points described above.

Also relevant: [esphome/esphome#10207](https://github.com/esphome/esphome/pull/10207)
by [Gaudi111](https://github.com/Gaudi111).

## License

Same model as ESPHome: the C++ / runtime code (`.cpp`, `.h`) is **GPLv3**, the
Python code and everything else is **MIT**. See [LICENSE](LICENSE).
