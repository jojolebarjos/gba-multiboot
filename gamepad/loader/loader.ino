
// Software SPI pins, chosen arbitrarily
// Note: we should use Arduino/mbed SPI capabilities, but there are issues with Serial on Arduino Nano 33 BLE
#define SO_PIN 2
#define SI_PIN 3
#define SC_PIN 4

// Built-in RGB LED
#define LED_RED 22
#define LED_GREEN 23
#define LED_BLUE 24

// Multiboot ROM data, generated using `convert.py`
#include "rom.h"

void setup() {

  // Configure pins
  pinMode(SO_PIN, INPUT);
  pinMode(SI_PIN, OUTPUT);
  pinMode(SC_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  // Turn off the LED
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  // Send data
  bool success = upload(ROM, sizeof(ROM));

  // Green on success, red on failure
  digitalWrite(LED_BLUE, HIGH);
  if (success)
    digitalWrite(LED_GREEN, LOW);
  else
    digitalWrite(LED_RED, LOW);

  pinMode(SO_PIN, INPUT);
  pinMode(SI_PIN, OUTPUT);
  pinMode(SC_PIN, INPUT);

  delay(2000);

  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

void loop() {

  // Wait for start bit
  while (digitalRead(SO_PIN) == HIGH);

  // Use current time as anchor
  unsigned long start = micros() + 1500000 / 9600;

  // Fetch each bit of the 8-bits payload
  uint16_t data = 0;
  for (int i = 0; i < 8; ++i) {

    // Busy wait until we are in the middle of the bit
    while (micros() < start + i * 1000000 / 9600);

    // Read it
    int b = digitalRead(SO_PIN) == HIGH ? 1 : 0;
    data |= b << i;
  }

  // TODO add, and check parity bit

  // Busy wait until we are safely out of the last bit
  while (micros() < start + 8 * 1000000 / 9600);

  if ((data & 0x80) == 0) {
    digitalWrite(LED_RED, (data & 0b01) ? HIGH : LOW);
    digitalWrite(LED_BLUE, (data & 0b10) ? HIGH : LOW);
  }

  if ((data & 0x80) != 0) {
    digitalWrite(LED_GREEN, (data & 0b1) ? HIGH : LOW);
  }
}

bool upload(char const * rom, size_t size) {
  uint16_t r;

  // Send connection request, wait for slave to enter Normal mode
  uint8_t count = 0;
  while (true) {
    r = xfer16(0x6202);

    // Check whether slave has entered correct mode, recognition okay
    if ((r & 0xfff0) == 0x7200) {
      unsigned x = r & 0xf;
      if (x != 2)
        return false;
      break;
    }

    // Blink blue while waiting
    digitalWrite(LED_BLUE, count & 8 ? HIGH : LOW);
    count += 1;

    // Wait a bit before trying again
    delay(1000 / 16);
  }

  // Blue during transmission
  digitalWrite(LED_BLUE, LOW);

  // Exchange master/slave info
  r = xfer16(0x6102);
  if (r != 0x7202)
    return false;

  // Send header
  for (size_t i = 0; i < 0xc0; i += 2) {

      // Send 2 bytes at a time
      uint16_t xxxx = (uint16_t)rom[i] | (uint16_t)rom[i + 1] << 8;
      r = xfer16(xxxx);

      // Slave replies with index and id
      if (((r >> 8) & 0xff) != (0xc0 - i) / 2)
        return false;
      if ((r & 0xff) != 2)
        return false;
  }

  // Transfer is complete
  r = xfer16(0x6200);
  if (r != 2)
    return false;

  // Exchange master/slave info (again)
  r = xfer16(0x6202);
  if (r != 0x7202)
    return false;

  // Choose palette
  uint8_t pp = 0xd1;

  // Wait until slave is ready
  while (true) {
      r = xfer16(0x6300 | pp);
      if ((r & 0xff00) == 0x7300)
          break;
  }

  // Random client data
  uint8_t cc = r & 0xff;

  // Compute handshake data
  uint8_t hh = (0x11 + cc + 0xff + 0xff) & 0xff;

  // Send handshake
  r = xfer16(0x6400 | hh);
  if ((r & 0xff00) != 0x7300)
    return false;

  // Wait a bit
  delay(1000 / 16);

  // Send length information
  uint16_t llll = (size - 0xc0) / 4 - 0x34;
  r = xfer16(llll);
  if ((r & 0xff00) != 0x7300)
    return false;
  uint8_t rr = r & 0xff;

  // Send encrypted payload
  uint32_t c = 0x0000c387;
  uint32_t x = 0x0000c37b;
  uint32_t k = 0x43202f2f;
  uint32_t m = 0xffff0000 | cc << 8 | pp;
  uint32_t f = 0xffff0000 | rr << 8 | hh;
  for (size_t i = 0xc0; i < size; i += 4) {

    // Send 4 bytes at a time
    uint32_t data = (uint32_t)rom[i] | (uint32_t)rom[i + 1] << 8 | (uint32_t)rom[i + 2] << 16 | (uint32_t)rom[i + 3] << 24;

    // Update checksum
    c ^= data;
    for (unsigned bit = 0; bit < 32; ++bit) {
      bool carry = c & 1;
      c >>= 1;
      if (carry)
        c ^= x;
    }

    // Encrypt and send data
    m = 0x6f646573 * m + 1;
    uint32_t complement = -(uint32_t)0x2000000 - (uint32_t)i;
    uint32_t yyyyyyyy = data ^ complement ^ m ^ k;
    r = xfer32(yyyyyyyy) >> 16;

    // Client replies with lower bits of destination address
    if (r != (i & 0xffff))
      return false;
  }

  // Final checksum update
  c ^= f;
  for (unsigned bit = 0; bit < 32; ++bit) {
    bool carry = c & 1;
    c >>= 1;
    if (carry)
      c ^= x;
  }

  // Client just acknowledged the last bits
  r = xfer16(0x0065);
  if (r != size & 0xffff)
    return false;

  // Wait until all slaves are ready for CRC transfer
  while (true) {
    r = xfer16(0x0065);
    if (r == 0x0075)
      break;
    if (r != 0x0074)
      return false;
  }

  // Signal that CRC will follow
  r = xfer16(0x0066);
  if (r != 0x0075)
    return false;

  // Exchange CRC
  r = xfer16((uint16_t)(c & 0xffff));
  if (r != c)
    return false;

  // Success!
  return true;
}

uint16_t xfer16(uint16_t output) {
  uint32_t input = xfer32((uint32_t)output & 0xffff);
  return (uint16_t)(input >> 16);
}

uint32_t xfer32(uint32_t output) {
  uint32_t input = 0;

  // Send bits, from most- to least-significant
  for (int i = 31; i >= 0; --i) {

    // Clock falling-edge
    digitalWrite(SC_PIN, LOW);

    // Write i-th bit
    digitalWrite(SI_PIN, (output & (1 << i)) ? HIGH : LOW);

    // Wait a bit
    delayMicroseconds(4);

    // Clock rising-edge, slave will latch the ouput
    digitalWrite(SC_PIN, HIGH);

    // Read i-th bit as well
    input |= (digitalRead(SO_PIN) == HIGH ? 1 : 0) << i;

    // Wait a bit
    delayMicroseconds(4);
  }

  // Leave SI high to signal readiness
  digitalWrite(SI_PIN, HIGH);

  return input;
}
