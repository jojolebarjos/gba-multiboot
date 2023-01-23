
#include <stdint.h>


#define REG_VCOUNT           *((volatile uint16_t *)0x04000006)

#define REG_KEYINPUT         *((volatile uint16_t *)0x04000130)

#define REG_RCNT             *((volatile uint16_t *)0x04000134)
#define REG_SIOCNT           *((volatile uint16_t *)0x04000128)
#define REG_SIODATA8         *((volatile uint16_t *)0x0400012A)


// Busy wait for next frame
void vsync(void) {

    // Wait until VDraw
    while (REG_VCOUNT >= 160);

    // Wait until VBlank
    while (REG_VCOUNT < 160);
}


// Entry point
int main(void) {
    uint16_t keyinput, old_keyinput;

    // Configure UART
    REG_RCNT = 0;
    REG_SIOCNT = 0;
    REG_SIOCNT =

        // 9600 bps
        (0 << 0) |
        (0 << 1) |

        // Send blindly
        (0 << 2) |

        // 8 bits
        (1 << 7) |

        // Enable FIFO
        (1 << 8) |

        // Disable parity bit
        (0 << 9) |

        // Enable send/receive signals
        (1 << 10) |
        (1 << 11) |

        // UART mode
        (1 << 12) |
        (1 << 13) |

        // Disable IRQ
        (0 << 14);

    // Loop forever
    old_keyinput = 0;
    for (;;) {

        // Wait until next frame
        // TODO use vsync interrupt
        vsync();

        // Get buttons state
        keyinput = REG_KEYINPUT;

        // If there was a change, push on FIFO
        if (old_keyinput != keyinput) {

            // Make sure that FIFO is empty (but it should be long gone)
            // Note: reading SIOCNT also clears any flag
            while (REG_SIOCNT & 0x0010);

            // Push both halves, with a MSB marking which half it is
            REG_SIODATA8 = (keyinput & 0x1f) | 0x00;
            REG_SIODATA8 = ((keyinput >> 5) & 0x1f) | 0x80;

            // Keep old state, to check for change
            old_keyinput = keyinput;
        }
    }

    return 0;
}
