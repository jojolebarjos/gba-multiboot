
import argparse
import time

import serial

from tqdm import tqdm


# Parse arguments
parser = argparse.ArgumentParser()
parser.add_argument("rom_path")
parser.add_argument("port_name")
args = parser.parse_args()

# Get ROM bytes
with open(args.rom_path, "rb") as file:
    rom = file.read()

# Multiboot ROM must fit the 256Ko RAM
if len(rom) > 0x40000:
    raise ValueError("file too large")

# Make sure size is a multiple of 16 bytes
while len(rom) % 16 != 0:
    rom = rom + b"\0"

# Open USB connection
with serial.Serial(args.port_name) as port:

    # Exchange 32 bits with GBA
    def xfer32(output):

        # Send bytes to Arduino, which will exchange using SPI
        output_bytes = bytes((
            output & 0xFF,
            (output >> 8) & 0xFF,
            (output >> 16) & 0xFF,
            (output >> 24) & 0xFF,
        ))
        port.write(output_bytes)
        port.flush()

        # Get back the reply
        input_bytes = port.read(4)
        input = input_bytes[0] | input_bytes[1] << 8 | input_bytes[2] << 16 | input_bytes[3] << 24
        return input

    # In many cases, we only exchange 16 bits at a time, the remaining bytes being garbage
    def xfer16(output):

        # Upper bits sent by master are zero
        input = xfer32(output & 0xffff)

        # Lower bits sent by slave are the same as previously sent by master, ignored
        return input >> 16

    # Send connection request, wait for slave to enter Normal mode
    with tqdm() as progress:
        while True:
            r = xfer16(0x6202)
            progress.set_description(f"0x{r:04x}")
            progress.update(1)

            # Check whether slave has entered correct mode, recognition okay
            if (r & 0xfff0) == 0x7200:

                # Slave replies with its ID (bit 1, 2, or 3, is set)
                # Note: in Normal mode, there can be only one slave
                x = r & 0xf
                assert x == 2
                break

            # Wait a bit before trying again
            time.sleep(1 / 16)

    # Exchange master/slave info
    r = xfer16(0x6102)
    assert r == 0x7202

    # Send header
    for i in range(0, 0xc0, 2):

        # Send 2 bytes at a time
        xxxx = rom[i] | rom[i + 1] << 8
        r = xfer16(xxxx)

        # Slave replies with index and id
        assert ((r >> 8) & 0xff) == (0xc0 - i) // 2
        assert (r & 0xff) == 2

    # Transfer is complete
    r = xfer16(0x6200)
    assert r == 2

    # Exchange master/slave info (again)
    r = xfer16(0x6202)
    assert r == 0x7202

    # Choose palette
    # Note: we really don't care about this, but it seems we could configure it?
    pp = 0xd1

    # Wait until slave is ready
    while True:
        r = xfer16(0x6300 | pp)
        if (r & 0xff00) == 0x7300:
            break

    # Random client data
    cc = r & 0xff

    # Compute handshake data
    # Note: client data for missing clients 2 and 3 are 0xff
    hh = (0x11 + cc + 0xff + 0xff) & 0xff

    # Send handshake
    # Note: client returns a random value, which is ignored
    r = xfer16(0x6400 | hh)
    assert (r & 0xff00) == 0x7300

    # Wait a bit
    time.sleep(1 / 16)

    # Send length information
    llll = (len(rom) - 0xc0) // 4 - 0x34
    r = xfer16(llll)
    assert (r & 0xff00) == 0x7300
    rr = r & 0xff

    # Send encrypted payload
    c = 0xc387
    x = 0xc37b
    k = 0x43202f2f
    m = 0xffff0000 | cc << 8 | pp
    f = 0xffff0000 | rr << 8 | hh
    for i in tqdm(range(0xc0, len(rom), 4)):

        # Send 4 bytes at a time
        data = rom[i] | rom[i + 1] << 8 | rom[i + 2] << 16 | rom[i + 3] << 24

        # Update checksum
        c ^= data
        for bit in range(32):
            carry = c & 1
            c >>= 1
            if carry:
                c ^= x

        # Encrypt and send data
        m = (0x6f646573 * m + 1) & 0xffffffff
        complement = (-0x2000000 - i) & 0xffffffff
        yyyyyyyy = data ^ complement ^ m ^ k
        r = xfer32(yyyyyyyy)

        # Client replies with lower bits of destination address
        assert r >> 16 == i & 0xffff

    # Final checksum update
    c ^= f
    for bit in range(32):
        carry = c & 1
        c >>= 1
        if carry:
            c ^= x

    # Client just acknowledged the last bits
    r = xfer16(0x0065)
    assert r == len(rom) & 0xffff

    # Wait until all slaves are ready for CRC transfer
    while True:
        r = xfer16(0x0065)
        if r == 0x0075:
            break
        assert r == 0x0074

    # Signal that CRC will follow
    r = xfer16(0x0066)
    assert r == 0x0075

    # Exchange CRC
    r = xfer16(c)
    assert r == c
