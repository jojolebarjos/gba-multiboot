
#include <stdint.h>

#define REG_DISPLAYCONTROL   *((volatile uint32_t *)0x04000000)
#define VIDEOMODE_3          0x0003
#define BGMODE_2             0x0400

#define REG_VCOUNT           *((volatile uint16_t *)0x04000006)
#define MEM_VRAM             ((volatile uint16_t *)0x06000000)
#define SCREEN_WIDTH         240
#define SCREEN_HEIGHT        160

#define REG_KEYINPUT         *((volatile uint16_t *)0x04000130)

#define KEY_RIGHT            (1 << 4)
#define KEY_LEFT             (1 << 5)
#define KEY_UP               (1 << 6)
#define KEY_DOWN             (1 << 7)


// Using BGR15
uint16_t make_color(unsigned r, unsigned g, unsigned b) {
    return r | (g << 5) | (b << 10);
}


// Busy wait for next frame
void vsync(void) {

    // Wait until VDraw
    while (REG_VCOUNT >= 160);

    // Wait until VBlank
    while (REG_VCOUNT < 160);
}


// Entry point
int main(void) {
    int i;
    uint16_t keyinput;
    int x, y;

    // Start at center
    x = SCREEN_WIDTH / 2;
    y = SCREEN_HEIGHT / 2;

    // Activate video mode 3, to allow direct access to 16-bits pixel buffer
    REG_DISPLAYCONTROL = VIDEOMODE_3 | BGMODE_2;

    // Paint screen black
    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        MEM_VRAM[i] = make_color(0, 0, 0);

    // Loop forever
    for (;;) {

        // Wait until next frame
        vsync();

        // Get buttons state
        keyinput = REG_KEYINPUT;

        // Paint old pixel black
        MEM_VRAM[x + y * SCREEN_WIDTH] = make_color(0, 0, 0);

        // Handle buttons
        if ((keyinput & KEY_RIGHT) == 0)
            x += 1;
        if ((keyinput & KEY_LEFT) == 0)
            x -= 1;
        if ((keyinput & KEY_DOWN) == 0)
            y += 1;
        if ((keyinput & KEY_UP) == 0)
            y -= 1;

        // Warp around, if needed
        if (x < 0)
            x += SCREEN_WIDTH;
        if (x >= SCREEN_WIDTH)
            x -= SCREEN_WIDTH;
        if (y < 0)
            y += SCREEN_HEIGHT;
        if (y >= SCREEN_HEIGHT)
            y -= SCREEN_HEIGHT;

        // Paint pixel white
        MEM_VRAM[x + y * SCREEN_WIDTH] = make_color(31, 31, 31);
    }

    return 0;
}
