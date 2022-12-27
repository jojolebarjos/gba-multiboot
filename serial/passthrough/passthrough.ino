
// Note: we should use Arduino/mbed SPI capabilities, but there are issues with Serial on Arduino Nano 33 BLE

// Make sure that your Arduino is 3v3!
#define SO_PIN 2
#define SI_PIN 3
#define SC_PIN 4

void setup() {

  pinMode(SO_PIN, INPUT);
  pinMode(SI_PIN, OUTPUT);
  pinMode(SC_PIN, OUTPUT);

  // Initiate serial connection through built-in USB
  Serial.begin(9600);
  while (!Serial);
}


uint32_t exchange(uint32_t output) {
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

void loop() {
  char bytes[4];
  uint32_t data;

  // Exchange bytes, if available
  if (Serial.available() >= 4) {

    // Get 32-bits data chunk
    Serial.readBytes(bytes, 4);
    data = bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24;

    // Transfer over SPI
    data = exchange(data);

    // Send back to host
    bytes[0] = data & 0xFF;
    bytes[1] = (data >> 8) & 0xFF;
    bytes[2] = (data >> 16) & 0xFF;
    bytes[3] = (data >> 24) & 0xFF;
    Serial.write(bytes, 4);
    Serial.flush();
  }
}
