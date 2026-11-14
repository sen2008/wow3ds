#!/usr/bin/env python3
"""Send a .3dsx to a 3DS waiting in the Homebrew Launcher's netloader (press Y there) and start it.
Same protocol as devkitPro's 3dslink; needs only Python 3, no other packages.

    python3 send3ds.py w3dproto-patched.3dsx              # finds the 3DS on the local network
    python3 send3ds.py w3dproto-patched.3dsx 192.168.1.50  # or give its IP (shown on the 3DS screen)

The file is saved on the SD card as sdmc:/3ds/<name>.3dsx (the name below), next to your
existing install, so the Homebrew Launcher lists it as its own entry.
"""
import os, socket, struct, sys, zlib

PORT = 17491
CHUNK = 16 * 1024            # the 3DS reads at most 16 KiB per chunk
NAME = "w3dproto_test.3dsx"  # sdmc:/3ds/w3dproto_test.3dsx: does not overwrite sdmc:/3ds/w3dproto/w3dproto.3dsx


def find_3ds(tries=20):
    b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    b.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    r = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    r.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    r.bind(("", PORT))
    r.settimeout(0.15)
    try:
        for _ in range(tries):
            b.sendto(b"3dsboot", ("255.255.255.255", PORT))
            try:
                data, addr = r.recvfrom(256)
                if data.startswith(b"boot3ds"):
                    return addr[0]
            except socket.timeout:
                pass
    finally:
        b.close(); r.close()
    return None


def recv_i32(s):
    buf = b""
    while len(buf) < 4:
        part = s.recv(4 - len(buf))
        if not part:
            raise ConnectionError("the 3DS closed the connection")
        buf += part
    return struct.unpack("<i", buf)[0]


def send(path, host):
    data = open(path, "rb").read()
    name = NAME.encode()
    s = socket.create_connection((host, PORT), timeout=30)
    s.sendall(struct.pack("<I", len(name)) + name + struct.pack("<I", len(data)))
    resp = recv_i32(s)
    if resp != 0:
        raise SystemExit({-1: "the 3DS could not create the file", -2: "not enough space on the SD card",
                          -3: "not enough memory on the 3DS"}.get(resp, "the 3DS refused (%d)" % resp))
    z = zlib.compressobj()
    sent = 0

    def out(blob):
        for i in range(0, len(blob), CHUNK):
            piece = blob[i:i + CHUNK]
            s.sendall(struct.pack("<I", len(piece)) + piece)

    for i in range(0, len(data), CHUNK):
        out(z.compress(data[i:i + CHUNK]))
        sent = min(i + CHUNK, len(data))
        if sent % (1024 * 1024) < CHUNK or sent == len(data):
            print("\r%d / %d KiB" % (sent // 1024, len(data) // 1024), end="", flush=True)
    out(z.flush())
    print()
    if recv_i32(s) != 0:
        raise SystemExit("the 3DS reported an error while saving")
    argv0 = b"sdmc:/3ds/" + name + b"\0"
    s.sendall(struct.pack("<I", len(argv0)) + argv0)
    s.close()
    print("Sent. The 3DS saves it as sdmc:/3ds/%s and starts it." % NAME)


if __name__ == "__main__":
    if len(sys.argv) < 2 or not os.path.isfile(sys.argv[1]):
        raise SystemExit(__doc__)
    host = sys.argv[2] if len(sys.argv) > 2 else find_3ds()
    if not host:
        raise SystemExit("No 3DS answered. Open the Homebrew Launcher, press Y, and give the IP it shows.")
    print("3DS at", host)
    send(sys.argv[1], host)
