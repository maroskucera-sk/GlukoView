"""Nahraj firmvér cez USB, až keď práve nebeží obnova CareLink tokenu.

Port sa otvorí bez DTR/RTS, takže doska sa pri počúvaní nereštartuje.
Obnova tokenu je v logu riadok „refresh post“. Kým po ňom nepríde koniec,
nahrávanie sa nespustí. Novší firmvér navyše opakuje „[flash] safe“, keď
je sťahovanie glykémie v pokoji.

Najprv zostav obraz, potom spusti tento skript. Medzi povolením a
esptool reštartom tak ostane len samotné nahratie.
"""

import subprocess
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("chyba pyserial")

DONE_MARKS = (
    "OK after refresh",
    "OK heap",
    "refresh net",
    "refresh invalid",
    "refresh HTTP",
    "novy refresh",
    "no new refresh",
    "refresh not resent",
)


def wait_until_safe(port_name, timeout_s):
    port = serial.Serial()
    port.port = port_name
    port.baudrate = 115200
    port.timeout = 0.2
    port.dtr = False
    port.rts = False
    port.open()
    open_refresh = False
    saw_ok = False
    saw_idle = False
    last_ok = 0.0
    last_idle = 0.0
    pending = ""
    deadline = time.time() + timeout_s
    try:
        while time.time() < deadline:
            raw = port.read(4096)
            if not raw:
                if not open_refresh and (
                    (saw_ok and (time.time() - last_ok) > 2.0)
                    or (saw_idle and (time.time() - last_idle) > 2.0)
                ):
                    return "pokoj, obnova tokenu nebezi"
                continue
            pending += raw.decode("utf-8", "replace")
            while "\n" in pending:
                line, pending = pending.split("\n", 1)
                if "refresh post" in line:
                    open_refresh = True
                    saw_ok = False
                    continue
                if any(mark in line for mark in DONE_MARKS):
                    open_refresh = False
                    if "OK heap" in line or "OK after refresh" in line:
                        saw_ok = True
                        last_ok = time.time()
                if ("[flash] safe" in line or "refresh not resent" in line) and not open_refresh:
                    saw_idle = True
                    last_idle = time.time()
    finally:
        port.close()
    return None


def main():
    if len(sys.argv) < 3:
        sys.exit("pouzitie: flash_when_safe.py <env> <COMx>")
    env, port_name = sys.argv[1], sys.argv[2]
    pio = (
        __import__("os").environ["USERPROFILE"]
        + r"\.platformio\penv\Scripts\platformio.exe"
    )
    print("zostavujem", env)
    if subprocess.call([pio, "run", "-e", env]) != 0:
        sys.exit(1)
    print("cakam na pokojny CareLink na", port_name)
    reason = wait_until_safe(port_name, 90)
    if not reason:
        sys.exit("do 90 s nebol pokojny okamih, nenahravam")
    print("nahravam:", reason)
    rc = subprocess.call(
        [pio, "run", "-e", env, "-t", "upload", "--upload-port", port_name]
    )
    sys.exit(rc)


if __name__ == "__main__":
    main()
