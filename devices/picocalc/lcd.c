//
//  Pico Logo
//  Copyright 2025 Blair Leduc. See LICENSE for details.
//
//  PicoCalc LCD display driver
//
//  This driver interfaces with the ST7789P LCD controller on the PicoCalc.
//
//  It is optimised for a character-based display with a fixed-width, 8-pixel wide font
//  and 65K colours in the RGB565 format. This driver requires little memory as it
//  uses the frame memory on the controller directly.
//
//  NOTE: Some code below is written to respect timing constraints of the ST7789P controller.
//        For instance, you can usually get away with a short chip select high pulse widths, but
//        writing to the display RAM requires the minimum chip select high pulse width of 40ns.
//

#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"

#include "lcd.h"
#include "devices/palette.h"

static bool lcd_initialised = false; // flag to indicate if the LCD is initialised

static uint16_t lcd_scroll_top = 0;                      // top fixed area for vertical scrolling
static uint16_t lcd_memory_scroll_height = FRAME_HEIGHT; // scroll area height
static uint16_t lcd_scroll_bottom = 0;                   // bottom fixed area for vertical scrolling
static uint16_t lcd_y_offset = 0;                        // offset for vertical scrolling

static uint8_t foreground = 254; // foreground palette slot
static uint8_t background = 255; // background palette slot

// Text drawing
const font_t *font = &logo_font; // default font
static uint8_t char_buffer[GLYPH_WIDTH * GLYPH_HEIGHT] __attribute__((aligned(4)));
static uint8_t line_buffer[WIDTH * GLYPH_HEIGHT] __attribute__((aligned(4)));

// Palette
static uint16_t palette[256] = {0};

// Background processing
static uint32_t irq_state;
static repeating_timer_t cursor_timer;

static void lcd_disable_interrupts()
{
    irq_state = save_and_disable_interrupts();
    // gpio_put(3, true);
}

static void lcd_enable_interrupts()
{
    // gpio_put(3, false);
    restore_interrupts(irq_state);
}

//
// Palette functions
//

void lcd_set_palette_value(uint8_t slot, uint16_t colour)
{
    palette[slot] = colour;
}

uint16_t lcd_get_palette_value(uint8_t slot)
{
    return palette[slot];
}

void lcd_set_palette_rgb(uint8_t slot, uint8_t r, uint8_t g, uint8_t b)
{
    // Convert 8-bit RGB to RGB565
    uint16_t r5 = (r >> 3) & 0x1F;
    uint16_t g6 = (g >> 2) & 0x3F;
    uint16_t b5 = (b >> 3) & 0x1F;
    palette[slot] = (r5 << 11) | (g6 << 5) | b5;
}

void lcd_get_palette_rgb(uint8_t slot, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint16_t colour = palette[slot];
    // Convert RGB565 to 8-bit RGB
    // Expand by replicating high bits into low bits for better accuracy
    uint8_t r5 = (colour >> 11) & 0x1F;
    uint8_t g6 = (colour >> 5) & 0x3F;
    uint8_t b5 = colour & 0x1F;
    *r = (r5 << 3) | (r5 >> 2);
    *g = (g6 << 2) | (g6 >> 4);
    *b = (b5 << 3) | (b5 >> 2);
}

void lcd_restore_palette(void)
{
    // Restore first 128 slots from the default palette
    memcpy(palette, default_palette, sizeof(default_palette));
}


//
// Character attributes
//

// Set foreground colour
void lcd_set_foreground(uint8_t slot)
{
    foreground = slot;
}

// Set background colour
void lcd_set_background(uint8_t slot)
{
    background = slot;
}

//
// Low-level SPI functions
//

// Send a command
void lcd_write_cmd(uint8_t cmd)
{
    gpio_put(LCD_DCX, 0); // Command
    gpio_put(LCD_CSX, 0);
    spi_write_blocking(LCD_SPI, &cmd, 1);
    gpio_put(LCD_CSX, 1);
}

// Send 8-bit data (byte)
void lcd_write_data(uint8_t len, ...)
{
    va_list args;
    va_start(args, len);
    gpio_put(LCD_DCX, 1); // Data
    gpio_put(LCD_CSX, 0);
    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t data = va_arg(args, int); // get the next byte of data
        spi_write_blocking(LCD_SPI, &data, 1);
    }
    gpio_put(LCD_CSX, 1);
    va_end(args);
}

// Send 16-bit data (half-word)
void lcd_write16_data(uint8_t len, ...)
{
    va_list args;

    // DO NOT MOVE THE spi_set_format() OR THE gpio_put(LCD_DCX) CALLS!
    // They are placed before the gpio_put(LCD_CSX) to ensure that a minimum
    // chip select high pulse width is achieved (at least 40ns)
    spi_set_format(LCD_SPI, 16, 0, 0, SPI_MSB_FIRST);

    va_start(args, len);
    gpio_put(LCD_DCX, 1); // Data
    gpio_put(LCD_CSX, 0);
    for (uint8_t i = 0; i < len; i++)
    {
        uint16_t data = va_arg(args, int); // get the next half-word of data
        spi_write16_blocking(LCD_SPI, &data, 1);
    }
    gpio_put(LCD_CSX, 1);
    va_end(args);

    spi_set_format(LCD_SPI, 8, 0, 0, SPI_MSB_FIRST);
}

void lcd_write16_buf(const uint16_t *buffer, size_t len)
{
    // DO NOT MOVE THE spi_set_format() OR THE gpio_put(LCD_DCX) CALLS!
    // They are placed before the gpio_put(LCD_CSX) to ensure that a minimum
    // chip select high pulse width is achieved (at least 40ns)
    spi_set_format(LCD_SPI, 16, 0, 0, SPI_MSB_FIRST);

    gpio_put(LCD_DCX, 1); // Data
    gpio_put(LCD_CSX, 0);
    spi_write16_blocking(LCD_SPI, buffer, len);
    gpio_put(LCD_CSX, 1);

    spi_set_format(LCD_SPI, 8, 0, 0, SPI_MSB_FIRST);
}

// Write len bytes directly from src to the SPI, and discard any data received back
// Each byte in src is treated as an 8-bit palette index into palette_16bit[]
static int __not_in_flash_func(spi_write16_pixels_blocking)(spi_inst_t *spi, const uint8_t *src, size_t len) {
    invalid_params_if(HARDWARE_SPI, 0 > (int)len);
    // Deliberately overflow FIFO, then clean up afterward, to minimise amount
    // of APB polling required per halfword
    for (size_t i = 0; i < len; ++i) {
        while (!spi_is_writable(spi))
            tight_loop_contents();
        spi_get_hw(spi)->dr = (uint32_t)palette[src[i]];
    }

    while (spi_is_readable(spi))
        (void)spi_get_hw(spi)->dr;
    while (spi_get_hw(spi)->sr & SPI_SSPSR_BSY_BITS)
        tight_loop_contents();
    while (spi_is_readable(spi))
        (void)spi_get_hw(spi)->dr;

    // Don't leave overrun flag set
    spi_get_hw(spi)->icr = SPI_SSPICR_RORIC_BITS;

    return (int)len;
}

static void lcd_write_pixels_buf(const uint8_t *buffer, size_t len)
{
    // DO NOT MOVE THE spi_set_format() OR THE gpio_put(LCD_DCX) CALLS!
    // They are placed before the gpio_put(LCD_CSX) to ensure that a minimum
    // chip select high pulse width is achieved (at least 40ns)
    spi_set_format(LCD_SPI, 16, 0, 0, SPI_MSB_FIRST);

    gpio_put(LCD_DCX, 1); // Data
    gpio_put(LCD_CSX, 0);
    spi_write16_pixels_blocking(LCD_SPI, buffer, len);
    gpio_put(LCD_CSX, 1);

    spi_set_format(LCD_SPI, 8, 0, 0, SPI_MSB_FIRST);
}

//
//  ST7365P LCD controller functions
//

// Select the target of the pixel data in the display RAM that will follow
static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Set column address (X)
    lcd_write_cmd(LCD_CMD_CASET);
    lcd_write_data(4,
                   UPPER8(x0), LOWER8(x0),
                   UPPER8(x1), LOWER8(x1));

    // Set row address (Y)
    lcd_write_cmd(LCD_CMD_RASET);
    lcd_write_data(4,
                   UPPER8(y0), LOWER8(y0),
                   UPPER8(y1), LOWER8(y1));

    // Prepare to write to RAM
    lcd_write_cmd(LCD_CMD_RAMWR);
}

//
//  Send pixel data to the display
//
//  All display RAM updates come through this function. This function is responsible for
//  setting the correct window in the display RAM and writing the pixel data to it. It also
//  handles the vertical scrolling by adjusting the y-coordinate based on the current scroll
//  offset (lcd_y_offset).

void lcd_blit(const uint8_t *pixels, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    lcd_disable_interrupts();
    if (y >= lcd_scroll_top && y < HEIGHT - lcd_scroll_bottom)
    {
        // Adjust y for vertical scroll offset and wrap within memory height
        uint16_t y_virtual = (lcd_y_offset + y) % lcd_memory_scroll_height;
        uint16_t y_end = lcd_scroll_top + y_virtual + height - 1;
        if (y_end >= lcd_scroll_top + lcd_memory_scroll_height)
        {
            y_end = lcd_scroll_top + lcd_memory_scroll_height - 1;
        }
        lcd_set_window(x, lcd_scroll_top + y_virtual, x + width - 1, y_end);
    }
    else
    {
        // No vertical scrolling, use the actual y-coordinate
        lcd_set_window(x, y, x + width - 1, y + height - 1);
    }

    lcd_write_pixels_buf(pixels, width * height);
    lcd_enable_interrupts();
}

// Draw a solid rectangle on the display
void lcd_solid_rectangle(uint8_t colour, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    static uint8_t pixels[WIDTH];

    for (uint16_t row = 0; row < height; row++)
    {
        for (uint16_t i = 0; i < width; i++)
        {
            pixels[i] = colour;
        }
        lcd_blit(pixels, x, y + row, width, 1);
    }
}

//
//  Scrolling area of the display
//
//  This forum post provides a good explanation of how scrolling on the ST7789P display works:
//      https://forum.arduino.cc/t/st7735s-scrolling/564506
//
//  These functions (lcd_define_scrolling, lcd_scroll_up, and lcd_scroll_down) configure and
//  set the vertical scrolling area of the display, but it is the responsibility of lcd_blit()
//  to ensure that the pixel data is written to the correct location in the display RAM.
//

void lcd_define_scrolling(uint16_t top_fixed_area, uint16_t bottom_fixed_area)
{
    uint16_t scroll_area = HEIGHT - (top_fixed_area + bottom_fixed_area);
    if (scroll_area == 0 || scroll_area > FRAME_HEIGHT)
    {
        // Invalid scrolling area, reset to full screen
        top_fixed_area = 0;
        bottom_fixed_area = 0;
        scroll_area = FRAME_HEIGHT;
    }

    lcd_scroll_top = top_fixed_area;
    lcd_memory_scroll_height = FRAME_HEIGHT - (top_fixed_area + bottom_fixed_area);
    lcd_scroll_bottom = bottom_fixed_area;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCRDEF);
    lcd_write_data(6,
                   UPPER8(lcd_scroll_top),
                   LOWER8(lcd_scroll_top),
                   UPPER8(scroll_area),
                   LOWER8(scroll_area),
                   UPPER8(lcd_scroll_bottom),
                   LOWER8(lcd_scroll_bottom));
    lcd_enable_interrupts();

    lcd_scroll_reset(); // Reset the scroll area to the top
}

void lcd_scroll_reset()
{
    // Clear the scrolling area by filling it with the background colour
    lcd_y_offset = 0; // Reset the scroll offset
    uint16_t scroll_area_start = lcd_scroll_top + lcd_y_offset;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCSAD); // Sets where in display RAM the scroll area starts
    lcd_write_data(2, UPPER8(scroll_area_start), LOWER8(scroll_area_start));
    lcd_enable_interrupts();
}

void lcd_scroll_clear()
{
    lcd_scroll_reset(); // Reset the scroll area to the top

    // Clear the scrolling area
    lcd_solid_rectangle(background, 0, lcd_scroll_top, WIDTH, lcd_memory_scroll_height);
}

// Scroll the screen up one line (make space at the bottom)
void lcd_scroll_up()
{
    // Ensure the scroll height is non-zero to avoid division by zero
    if (lcd_memory_scroll_height == 0)
    {
        return; // Exit early if the scroll height is invalid
    }
    // This will rotate the content in the scroll area up by one line
    lcd_y_offset = (lcd_y_offset + GLYPH_HEIGHT) % lcd_memory_scroll_height;
    uint16_t scroll_area_start = lcd_scroll_top + lcd_y_offset;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCSAD); // Sets where in display RAM the scroll area starts
    lcd_write_data(2, UPPER8(scroll_area_start), LOWER8(scroll_area_start));
    lcd_enable_interrupts();

    // Clear the new line at the bottom
    lcd_solid_rectangle(background, 0, HEIGHT - GLYPH_HEIGHT, WIDTH, GLYPH_HEIGHT);
}

// Scroll the screen down one line (making space at the top)
void lcd_scroll_down()
{
    // Ensure lcd_memory_scroll_height is non-zero to avoid division by zero
    if (lcd_memory_scroll_height == 0)
    {
        return; // Safely exit if the scroll height is zero
    }
    // This will rotate the content in the scroll area down by one line
    lcd_y_offset = (lcd_y_offset - GLYPH_HEIGHT + lcd_memory_scroll_height) % lcd_memory_scroll_height;
    uint16_t scroll_area_start = lcd_scroll_top + lcd_y_offset;

    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_VSCSAD); // Sets where in display RAM the scroll area starts
    lcd_write_data(2, UPPER8(scroll_area_start), LOWER8(scroll_area_start));
    lcd_enable_interrupts();

    // Clear the new line at the top
    lcd_solid_rectangle(background, 0, lcd_scroll_top, WIDTH, GLYPH_HEIGHT);
}

//
// Text drawing functions
//

// Clear the entire screen
void lcd_clear_screen()
{
    lcd_scroll_reset(); // Reset the scrolling area to the top
    lcd_solid_rectangle(background, 0, 0, WIDTH, FRAME_HEIGHT);
}

void lcd_erase_line(uint8_t row, uint8_t col_start, uint8_t col_end)
{
    lcd_solid_rectangle(background, col_start * GLYPH_WIDTH, row * GLYPH_HEIGHT, (col_end - col_start + 1) * GLYPH_WIDTH, GLYPH_HEIGHT);
}

// Helper function to decode a character and determine colours for rendering
// Handles reverse video: if bit 7 is set, foreground and background are swapped
static inline void decode_char(uint8_t c, uint8_t *char_code, uint8_t *fg, uint8_t *bg)
{
    bool reverse = (c & 0x80) != 0;
    *char_code = c & 0x7F;
    *fg = reverse ? background : foreground;
    *bg = reverse ? foreground : background;
}

// Draw a character at the specified position
// If the high bit (0x80) is set, the character is rendered in reverse video
// (background and foreground colours are swapped)
void lcd_putc(uint8_t column, uint8_t row, uint8_t c)
{
    uint8_t char_code, fg, bg;
    decode_char(c, &char_code, &fg, &bg);
    
    const uint8_t *glyph = &font->glyphs[char_code * GLYPH_HEIGHT];
    uint8_t *buffer = char_buffer;

    for (uint8_t i = 0; i < GLYPH_HEIGHT; i++, glyph++)
    {
        // Fill the row with the glyph data
        *(buffer++) = (*glyph & 0x80) ? fg : bg;
        *(buffer++) = (*glyph & 0x40) ? fg : bg;
        *(buffer++) = (*glyph & 0x20) ? fg : bg;
        *(buffer++) = (*glyph & 0x10) ? fg : bg;
        *(buffer++) = (*glyph & 0x08) ? fg : bg;
        *(buffer++) = (*glyph & 0x04) ? fg : bg;
        *(buffer++) = (*glyph & 0x02) ? fg : bg;
        *(buffer++) = (*glyph & 0x01) ? fg : bg;
    }

    lcd_blit(char_buffer, column * GLYPH_WIDTH, row * GLYPH_HEIGHT, GLYPH_WIDTH, GLYPH_HEIGHT);
}

// Draw a string at the specified position
// Characters with the high bit (0x80) set are rendered in reverse video
void lcd_putstr(uint8_t column, uint8_t row, const char *str)
{
    int len = strlen(str);
    int pos = 0;
    while (*str)
    {
        uint8_t char_code, fg, bg;
        decode_char(*str++, &char_code, &fg, &bg);
        
        uint8_t *buffer = line_buffer + (pos++ * GLYPH_WIDTH);
        const uint8_t *glyph = &font->glyphs[char_code * GLYPH_HEIGHT];

        for (uint8_t i = 0; i < GLYPH_HEIGHT; i++, glyph++)
        {
            // Fill the row with the glyph data
            *(buffer++) = (*glyph & 0x80) ? fg : bg;
            *(buffer++) = (*glyph & 0x40) ? fg : bg;
            *(buffer++) = (*glyph & 0x20) ? fg : bg;
            *(buffer++) = (*glyph & 0x10) ? fg : bg;
            *(buffer++) = (*glyph & 0x08) ? fg : bg;
            *(buffer++) = (*glyph & 0x04) ? fg : bg;
            *(buffer++) = (*glyph & 0x02) ? fg : bg;
            *(buffer++) = (*glyph & 0x01) ? fg : bg;
            buffer += (len - 1) * GLYPH_WIDTH;
        }
    }

    if (len)
    {
        lcd_blit(line_buffer, column * GLYPH_WIDTH, row * GLYPH_HEIGHT, GLYPH_WIDTH * len, GLYPH_HEIGHT);
    }
}

//
// The cursor
//
// A performance cheat: The cursor is drawn as a solid line at the bottom of the
// character cell. The cursor is positioned here since the printable glyphs
// do not extend to that row (on purpose). Drawing and erasing the cursor does
// not corrupt the glyphs.
//
// Except for the box drawing glyphs who do extend into that row. Disable the
// cursor when printing these if you want to see the box drawing glyphs
// uncorrupted.

static uint8_t cursor_column = 0;          // cursor x position for drawing
static uint8_t cursor_row = 0;             // cursor y position for drawing
static bool cursor_enabled = true;         // cursor visibility state
static LcdCursorStyle cursor_style = LCD_CURSOR_UNDERLINE;  // cursor style
static uint8_t cursor_char = ' ';          // character under cursor (for block style)

// Enable or disable the cursor
void lcd_enable_cursor(bool cursor_on)
{
    cursor_enabled = cursor_on;
}

// Check if the cursor is enabled
bool lcd_cursor_enabled()
{
    return cursor_enabled;
}

// Set the cursor style
void lcd_set_cursor_style(LcdCursorStyle style)
{
    cursor_style = style;
}

// Get the cursor style
LcdCursorStyle lcd_get_cursor_style(void)
{
    return cursor_style;
}

// Set the character under the cursor (used for block cursor style)
void lcd_set_cursor_char(uint8_t c)
{
    cursor_char = c;
}

// Move the cursor to the specified position
// This function updates the cursor position and ensures it is within the bounds of the display.
void lcd_move_cursor(uint8_t column, uint8_t row)
{
    // Move the cursor to the specified position
    cursor_column = column;
    cursor_row = row;

    // Ensure the cursor position is within bounds
    if (cursor_column > MAX_COLUMN)
        cursor_column = MAX_COLUMN;
    if (cursor_row > MAX_ROW)
        cursor_row = MAX_ROW;
}

// Draw the cursor at the current position
void lcd_draw_cursor()
{
    if (!cursor_enabled)
        return;

    if (cursor_style == LCD_CURSOR_BLOCK)
    {
        // Block cursor: draw character in reverse video
        lcd_putc(cursor_column, cursor_row, cursor_char | 0x80);
    }
    else
    {
        // Underline cursor: draw a line at the bottom of the character cell
        lcd_solid_rectangle(foreground, cursor_column * GLYPH_WIDTH, ((cursor_row + 1) * GLYPH_HEIGHT) - 1, GLYPH_WIDTH, 1);
    }
}

// Erase the cursor at the current position
void lcd_erase_cursor()
{
    if (!cursor_enabled)
        return;

    if (cursor_style == LCD_CURSOR_BLOCK)
    {
        // Block cursor: redraw character in normal video
        lcd_putc(cursor_column, cursor_row, cursor_char);
    }
    else
    {
        // Underline cursor: erase the line at the bottom of the character cell
        lcd_solid_rectangle(background, cursor_column * GLYPH_WIDTH, ((cursor_row + 1) * GLYPH_HEIGHT) - 1, GLYPH_WIDTH, 1);
    }
}

//
//  Display control functions
//

// Reset the LCD display
void lcd_reset()
{
    // Blip the reset pin to reset the LCD controller
    gpio_put(LCD_RST, 0);
    busy_wait_us(20); // 20µs reset pulse (10µs minimum)

    gpio_put(LCD_RST, 1);
    busy_wait_us(120000); // 5ms required after reset, but 120ms needed before sleep out command
}

// Turn on the LCD display
void lcd_display_on()
{
    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_DISPON);
    lcd_enable_interrupts();
}

// Turn off the LCD display
void lcd_display_off()
{
    lcd_disable_interrupts();
    lcd_write_cmd(LCD_CMD_DISPOFF);
    lcd_enable_interrupts();
}

//
//  Background processing
//
//  Handle background tasks such as blinking the cursor
//

// Blink the cursor at regular intervals
bool on_cursor_timer(repeating_timer_t *rt)
{
    static bool cursor_visible = false;

    if (!lcd_cursor_enabled())
    {
        return true; // if the SPI bus is not available or cursor is disabled, do not toggle cursor
    }

    if (cursor_visible)
    {
        lcd_erase_cursor();
    }
    else
    {
        lcd_draw_cursor();
    }

    cursor_visible = !cursor_visible; // Toggle cursor visibility
    return true;                      // Keep the timer running
}

// Initialize the LCD display
void lcd_init()
{
    if (lcd_initialised)
    {
        return; // already initialized
    }

    // Load the palette
    memcpy(palette, default_palette, sizeof(default_palette));

    // initialise GPIO
    gpio_init(LCD_SCL);
    gpio_init(LCD_SDI);
    gpio_init(LCD_SDO);
    gpio_init(LCD_CSX);
    gpio_init(LCD_DCX);
    gpio_init(LCD_RST);

    gpio_set_dir(LCD_SCL, GPIO_OUT);
    gpio_set_dir(LCD_SDI, GPIO_OUT);
    gpio_set_dir(LCD_CSX, GPIO_OUT);
    gpio_set_dir(LCD_DCX, GPIO_OUT);
    gpio_set_dir(LCD_RST, GPIO_OUT);

    // initialise 4-wire SPI
    spi_init(LCD_SPI, LCD_BAUDRATE);
    gpio_set_function(LCD_SCL, GPIO_FUNC_SPI);
    gpio_set_function(LCD_SDI, GPIO_FUNC_SPI);
    gpio_set_function(LCD_SDO, GPIO_FUNC_SPI);

    gpio_put(LCD_CSX, 1);
    gpio_put(LCD_RST, 1);

    lcd_disable_interrupts();

    lcd_reset(); // reset the LCD controller

    lcd_write_cmd(LCD_CMD_SWRESET); // reset the commands and parameters to their S/W Reset default values
    busy_wait_us(10000);            // required to wait at least 5ms

    lcd_write_cmd(LCD_CMD_PGC); // Positive Gamma Control
    lcd_write_data(15, 
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F);

    lcd_write_cmd(LCD_CMD_NGC); // Negative Gamma Control
    lcd_write_data(15,
        0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F);

    lcd_write_cmd(LCD_CMD_PWR1); // Power Control 1
    lcd_write_data(2, 0x17, 0x15);

    lcd_write_cmd(LCD_CMD_PWR2); // Power Control 2
    lcd_write_data(1, 0x41);

    lcd_write_cmd(LCD_CMD_VCMPCTL); // VCOM Control
    lcd_write_data(3, 0x00, 0x12, 0x80);

    lcd_write_cmd(LCD_CMD_MADCTL); // memory access control
    lcd_write_data(1, 0x48);       // BGR colour filter panel, top to bottom, left to right

    lcd_write_cmd(LCD_CMD_COLMOD); // pixel format set
    lcd_write_data(1, 0x55);       // 16 bit/pixel (RGB565)

    lcd_write_cmd(LCD_CMD_IFMODE); // Interface Mode Control
    lcd_write_data(1, 0x00);

    lcd_write_cmd(LCD_CMD_FRMCTR1); // Frame Rate Control
    lcd_write_data(1, 0xA0);

    lcd_write_cmd(LCD_CMD_INVON); // display inversion on

    lcd_write_cmd(LCD_CMD_DIC); // Display Inversion Control
    lcd_write_data(1, 0x02);

    lcd_write_cmd(LCD_CMD_DFC); // Display Function Control
    lcd_write_data(3, 0x02, 0x02, 0x3B);

    lcd_write_cmd(LCD_CMD_EMS); // entry mode set
    lcd_write_data(1, 0x06);    // normal display, 16-bit (RGB) to 18-bit (rgb) colour
                                //   conversion: r(0) = b(0) = 0

    lcd_write_cmd(LCD_CMD_E9); // Manufacturer command
    lcd_write_data(1, 0x00);

    lcd_write_cmd(LCD_CMD_F7); // Adjust Control 3
    lcd_write_data(4, 0xA9, 0x51, 0x2C, 0x82);

    lcd_write_cmd(LCD_CMD_VSCRDEF); // vertical scroll definition
    lcd_write_data(6,
                   0x00, 0x00, // top fixed area of 0 pixels
                   0x01, 0x40, // scroll area height of 320 pixels
                   0x00, 0x00  // bottom fixed area of 0 pixels
    );

    lcd_write_cmd(LCD_CMD_SLPOUT); // sleep out
    lcd_enable_interrupts();

    busy_wait_us(10000); // required to wait at least 5ms

    // Clear the screen
    lcd_clear_screen();

    // Now that the display is initialized, display RAM garbage is cleared,
    // turn on the display
    lcd_display_on();

    // Blink the cursor every second (500 ms on, 500 ms off)
    add_repeating_timer_ms(-500, on_cursor_timer, NULL, &cursor_timer);

    lcd_initialised = true; // Set the initialised flag
}