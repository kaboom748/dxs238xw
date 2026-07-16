#!/usr/bin/env python3
"""DTS238-7W meter simulator (HEKR protocol) for testing without hardware.

Answers the 0x00 / 0x02 / 0x06 / 0x0A polls and the 0x03 / 0x05 / 0x09 / 0x0C / 0x0D
SET commands. Lets you script scenarios: energy export (negative values), end of
delay, energy purchase.

Usage:
    # 1. create a pair of linked virtual serial ports
    socat -d -d pty,raw,echo=0,link=/tmp/meter pty,raw,echo=0,link=/tmp/esp

    # 2. run the simulator on one end
    python tools/meter_sim.py /tmp/meter

    # 3. point ESPHome (host platform) at the other end: /tmp/esp
"""
import os
import sys
import time

HEADER = 0x48
TYPE_RECV = 0x01
TYPE_SEND = 0x02


def crc(frame):
    return sum(frame[:-1]) & 0xFF


def build(cmd, payload, seq=0):
    length = 6 + len(payload)
    f = bytearray([HEADER, length, TYPE_RECV, seq, cmd] + list(payload) + [0])
    f[-1] = crc(f)
    return bytes(f)


def u16(v):
    return [(v >> 8) & 0xFF, v & 0xFF]


def u24(v):
    return [(v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF]


def u32(v):
    return [(v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF]


class Meter:
    def __init__(self):
        self.phase_count = 3
        self.meter_state = 1
        self.total_energy = 123456       # x0.01 kWh -> 1234.56
        self.warn_code = 0
        self.over_current = 0
        self.delay_remaining = 0
        self.delay_state = 0
        self.end_purchase = 0
        self.max_voltage = 275
        self.min_voltage = 185
        self.max_current = 4050          # x0.01 A -> 40.50 A  (decimales !)
        self.purchase_balance = 5000     # x0.01 kWh -> 50.00
        self.purchase_state = 0
        self.log = []

    # ---- frame 0x01: METER_STATE (21 bytes) ----
    def meter_state_frame(self, seq):
        p = [self.phase_count, self.meter_state] + u32(self.total_energy)
        p += [self.warn_code, 0, 0, 0, self.over_current]
        p += u16(self.delay_remaining) + [self.delay_state, self.end_purchase]
        return build(0x01, p, seq)

    # ---- frame 0x08: LIMIT_AND_PURCHASE (25 bytes) ----
    def limit_frame(self, seq):
        p = u16(self.max_voltage) + u16(self.min_voltage) + u16(self.max_current)
        p += [0, 0, 0, 0]                      # 11..14
        p += u32(self.purchase_balance)        # 15..18
        p += [0, 0, 0, 0]                      # 19..22
        p += [self.purchase_state]             # 23
        return build(0x08, p, seq)

    # ---- frame 0x0B: MEASUREMENT (71 bytes) ----
    def measurement_frame(self, seq):
        # Export scenario: -0.5 A and -0.5 kW on phase 1.
        # Meter encoding: raw offset of 1,000,000 => 1000.5 after x0.001
        cur1 = 1000500   # -> 1000.5  => doit devenir -0.5 A
        cur2 = 2500      # -> 2.5 A
        cur3 = 0
        pw1 = 1005000    # x0.0001 -> 100.5 => doit devenir -0.5 kW
        pw_t = 1005000
        p = u24(cur1) + u24(cur2) + u24(cur3)          # 5..13
        p += u16(2301) + u16(2298) + u16(2305)         # 14..19 tensions x0.1
        p += u24(0) + u24(0) + u24(0) + u24(0)         # 20..31 reactif
        p += u24(pw_t) + u24(pw1) + u24(0) + u24(0)    # 32..43 actif
        p += u16(998) + u16(997) + u16(0) + u16(0)     # 44..51 cos phi
        p += u16(5001)                                 # 52..53 frequence 50.01
        p += u32(self.total_energy)                    # 54..57
        p += u32(200000)                               # 58..61 import 2000.00
        p += u32(76544)                                # 62..65 export  765.44
        return build(0x0B, p, seq)

    # ---- frame 0x07: METER_ID ----
    def meter_id_frame(self, seq):
        return build(0x07, [20, 24, 11, 12, 34, 56], seq)

    def handle(self, frame):
        cmd, seq = frame[4], frame[3]
        self.log.append(cmd)

        if cmd == 0x00:
            return self.meter_state_frame(seq)
        if cmd == 0x02:
            return self.limit_frame(seq)
        if cmd == 0x06:
            return self.meter_id_frame(seq)
        if cmd == 0x0A:
            return self.measurement_frame(seq)
        if cmd == 0x03:  # SET_LIMIT
            self.max_current = (frame[5] << 8) | frame[6]
            self.max_voltage = (frame[7] << 8) | frame[8]
            self.min_voltage = (frame[9] << 8) | frame[10]
            return self.limit_frame(seq)
        if cmd == 0x09:  # SET_METER_STATE
            self.meter_state = frame[5]
            return self.meter_state_frame(seq)
        if cmd == 0x0C:  # SET_DELAY
            self.delay_remaining = (frame[5] << 8) | frame[6]
            self.delay_state = frame[7]
            return self.meter_state_frame(seq)
        if cmd == 0x0D:  # SET_PURCHASE
            self.purchase_state = frame[13]
            if self.purchase_state:
                self.purchase_balance = (
                    (frame[5] << 24) | (frame[6] << 16) | (frame[7] << 8) | frame[8]
                )
            else:
                self.purchase_balance = 0
            return self.limit_frame(seq)
        if cmd == 0x05:  # SET_RESET
            self.total_energy = 0
            return self.measurement_frame(seq)
        return None


def main(port):
    meter = Meter()
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    buf = bytearray()
    print(f"[sim] meter listening on {port}", flush=True)

    while True:
        try:
            data = os.read(fd, 256)
        except OSError:
            time.sleep(0.01)
            continue
        if not data:
            continue
        buf.extend(data)

        while len(buf) >= 2:
            if buf[0] != HEADER:
                buf.pop(0)
                continue
            length = buf[1]
            if len(buf) < length:
                break
            frame = bytes(buf[:length])
            del buf[:length]
            if crc(frame) != frame[-1]:
                print(f"[sim] CRC KO {frame.hex()}", flush=True)
                continue
            # 1) confirmation echo (TYPE = 0x02), then 2) the response
            os.write(fd, frame)
            time.sleep(0.005)
            resp = meter.handle(frame)
            if resp:
                os.write(fd, resp)
                print(f"[sim] cmd 0x{frame[4]:02X} -> response 0x{resp[4]:02X}", flush=True)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    main(sys.argv[1])
