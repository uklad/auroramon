#!/usr/bin/env python3
"""Simulate Aurora inverters on a pseudo-terminal, for testing without hardware.

    tests/fake_inverter.py --link /tmp/ttyAurora --addr 2 --addr 3 [--log requests.txt] [--clock-error 60]

--log records each request as "time address opcode parameter".
--clock-error starts the inverter's clock this many seconds from local time.
--clock-drift makes the clock gain this many seconds per second.
--fail-sets ignores this many requests to set the clock.  The program sends each
set up to 3 times, so 3 of these make one failed set.
--lose-set-replies makes this many requests to set the clock work, with no reply.
Like a real inverter its clock knows nothing about daylight saving, so it will
differ from local time by an hour after a change, until it's set.

Set the Serial Port in auroramon's Setup dialog to the --link path. Only the
listed addresses respond, so leave one out to simulate a dead inverter.
"""
import argparse
import os
import pty
import select
import struct
import termios
import time
import tty

TIME_BASE = 946684800

DSP = {1: 240.0, 2: 6.2, 3: 1500.0, 4: 50.0, 21: 41.5, 22: 36.5,
       23: 300.0, 25: 3.0, 26: 295.0, 27: 2.2, 35: 2100.0}
ENERGY_WH = {0: 5000, 1: 30000, 3: 120000, 4: 900000, 5: 4200000, 6: 1000000}

# inverter memory areas for retrieved data
TENSEC_BASE = 0x000a
DAILY_BASE = 0x438c
DAILY_TODAY_SLOT = 10
RETRIEVED_POWER = 100    # raw 10 second value
STARTED = int(time.time()) - 3600
clock_error = time.localtime().tm_gmtoff   # the inverter's clock minus UTC, seconds
clock_drift = 0
DRIFT_START = time.time()


def stored_words(address):
    """The two 16 bit words which the inverter stores at this address."""
    if address >= DAILY_BASE:
        slot = (address - DAILY_BASE) // 4
        if slot > DAILY_TODAY_SLOT:
            return 0xffff, 0xffff   # unused
        return 20000 - (DAILY_TODAY_SLOT - slot), 3000 + slot   # date code, Wh
    # 10 second data: a 0xffff marker and a timestamp, then readings
    stamp = STARTED - TIME_BASE
    if address == TENSEC_BASE:
        return 0xffff, stamp >> 16
    if address == TENSEC_BASE + 4:
        return stamp & 0xffff, RETRIEVED_POWER
    return RETRIEVED_POWER, RETRIEVED_POWER


def crc16(data):
    crc = 0xffff
    for byte in data:
        for _ in range(8):
            if (crc ^ byte) & 1:
                crc = (crc >> 1) ^ 0x8408
            else:
                crc >>= 1
            byte >>= 1
    return ~crc & 0xffff


def reply(request):
    global clock_error, DRIFT_START
    op, param = request[1], request[2]
    if op == 50:     # state: global=run, inverter=run, dc/dc=mppt, no alarm
        body = bytes([0, 6, 2, 2, 2, 0])
    elif op == 59:   # DSP measurement
        body = bytes([0, 6]) + struct.pack(">f", DSP.get(param, 0.0))
    elif op == 78:   # cumulated energy
        # the energy totals increase, 1 Wh per second
        body = bytes([0, 6]) + struct.pack(">I", ENERGY_WH.get(param, 0) + int(time.time()) - STARTED)
    elif op == 70:   # time
        drift = int((time.time() - DRIFT_START) * clock_drift)
        body = bytes([0, 6]) + struct.pack(">I", int(time.time()) + clock_error + drift - TIME_BASE)
    elif op == 71:   # set time
        clock_error = struct.unpack(">I", request[2:6])[0] + TIME_BASE - int(time.time())
        DRIFT_START = time.time()
        body = bytes([0, 6, 0, 0, 0, 0])
    elif op == 58:   # version
        body = bytes([0, 6]) + b"4UNN"
    elif op == 63:   # serial number, with no status byte
        body = b"123456"
    elif op == 52:   # part number, with no status byte
        body = b"-3G97-"
    elif op == 72:   # firmware
        body = bytes([0, 6]) + b"C016"
    elif op == 65:   # manufacture date
        body = bytes([0, 6]) + b"1012"
    elif op == 77:   # configuration
        body = bytes([0, 6, 0, 0, 0, 0])
    elif op == 75:   # start address of the stored data, param 1=10 second, 2=daily
        start = TENSEC_BASE if param == 1 else DAILY_BASE + DAILY_TODAY_SLOT * 4
        body = bytes([0, 6]) + struct.pack(">HH", start, 0)
    elif op == 79:   # read stored data
        body = bytes([0, 6]) + struct.pack(">HH", *stored_words((param << 8) | request[3]))
    elif op == 80:   # counters
        body = bytes([0, 6]) + struct.pack(">I", 3600000)
    else:
        body = bytes([0, 6, 0, 0, 0, 0])
    crc = crc16(body)
    return body + bytes([crc & 0xff, crc >> 8])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--link", required=True, help="symlink to create for the serial device")
    parser.add_argument("--addr", type=int, action="append", required=True, help="address which responds")
    parser.add_argument("--log", help="file to record the requests")
    parser.add_argument("--clock-error", type=int, default=0, help="seconds between the inverter's clock and local time")
    parser.add_argument("--clock-drift", type=float, default=0, help="seconds gained by the clock per second")
    parser.add_argument("--fail-sets", type=int, default=0, help="requests to set the clock to ignore")
    parser.add_argument("--lose-set-replies", type=int, default=0, help="requests to set the clock which work, with no reply")
    args = parser.parse_args()
    global clock_error, clock_drift
    clock_error += args.clock_error
    clock_drift = args.clock_drift
    log = open(args.log, "a", buffering=1) if args.log else None

    master, slave = pty.openpty()   # keep slave open, so the master survives auroramon closing it
    tty.setraw(slave, termios.TCSANOW)
    if os.path.lexists(args.link):
        os.unlink(args.link)
    os.symlink(os.ttyname(slave), args.link)
    print("device", os.ttyname(slave), "linked at", args.link, flush=True)

    buffer = b""
    while True:
        readable, _, _ = select.select([master], [], [], 1)
        if not readable:
            continue
        buffer += os.read(master, 256)
        while len(buffer) >= 10:
            request, buffer = buffer[:10], buffer[10:]
            good_crc = crc16(request[:8]) == request[8] | (request[9] << 8)
            if good_crc and request[0] in args.addr:
                if log:
                    log.write(f"{time.time():.1f} {request[0]} {request[1]} {request[2]} {request[2:6].hex()}\n")
                if request[1] == 71 and args.fail_sets > 0:
                    args.fail_sets -= 1
                    continue
                response = reply(request)
                if request[1] == 71 and args.lose_set_replies > 0:
                    args.lose_set_replies -= 1
                    continue
                os.write(master, response)
            elif not good_crc:
                buffer = b""   # lost framing, resynchronise on the next request


if __name__ == "__main__":
    main()
