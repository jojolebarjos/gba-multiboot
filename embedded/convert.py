
import argparse


# Parse arguments
parser = argparse.ArgumentParser()
parser.add_argument("rom_path")
parser.add_argument("source_path")
args = parser.parse_args()

# Get ROM bytes
with open(args.rom_path, "rb") as file:
    rom = file.read()
assert 192 <= len(rom) <= 0x40000

# Make sure size is a multiple of 16 bytes
while len(rom) % 16 != 0:
    rom = rom + b"\0"

# Write C header
with open(args.source_path, "w", encoding="ascii") as file:
    file.write("static char const ROM[] = {")
    num_values_per_row = 16
    num_rows = (len(rom) - 1) // num_values_per_row + 1
    for row in range(num_rows):
        if row > 0:
            file.write(",")
        file.write("\n  ")
        for i in range(num_values_per_row):
            if i > 0:
                file.write(", ")
            file.write(f"{rom[row * num_values_per_row + i]}")
    file.write("\n};\n")
