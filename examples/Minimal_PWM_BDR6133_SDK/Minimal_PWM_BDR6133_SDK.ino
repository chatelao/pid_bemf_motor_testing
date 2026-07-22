#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// XIAO RP2040 Pin Mapping -> Physische GPIOs
#define PIN_NEO_PWR 11
#define PIN_NEO_DAT 12
#define PIN_D7      1
#define PIN_D8      2

// --- 1. PIO WS2812 Bare-Metal Implementierung ---
// Kompiliertes PIO Programm für 800kHz WS2812 (GRB)
static const uint16_t ws2812_program_instructions[] = {
    0x6221, //  0: out    x, 1            side 0 [2]
    0x1123, //  1: jmp    !x, 3           side 1 [1]
    0x1400, //  2: jmp    0               side 1 [4]
    0xa442, //  3: nop                    side 0 [4]
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 24); // 24-Bit für GRB
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    
    // Clock Teiler berechnen (10 Zyklen pro Bit)
    float div = clock_get_hz(clk_sys) / (freq * 10);
    sm_config_set_clkdiv(&c, div);
    
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static inline void put_pixel(uint32_t pixel_grb) {
    // Schiebt die 24-Bit Farbdaten blockierend in den PIO FIFO
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Emuliert setBrightness(50) destruktiv und formatiert in GRB
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    r = (r * 50) / 255;
    g = (g * 50) / 255;
    b = (b * 50) / 255;
    return ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
}

// --- 2. Hardware PWM Variablen ---
uint slice_d7, slice_d8;
uint chan_d7, chan_d8;
const uint32_t PWM_WRAP = 2499; // Bei 1MHz Timer-Clock -> 2500 Ticks = 400Hz

void setup() {
    // Neopixel Power aktivieren
    gpio_init(PIN_NEO_PWR);
    gpio_set_dir(PIN_NEO_PWR, GPIO_OUT);
    gpio_put(PIN_NEO_PWR, 1);

    // PIO initialisieren (Nutzt PIO 0, State Machine 0)
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, PIN_NEO_DAT, 800000); // 800 kHz

    // PWM für D7 und D8 initialisieren
    gpio_set_function(PIN_D7, GPIO_FUNC_PWM);
    gpio_set_function(PIN_D8, GPIO_FUNC_PWM);

    slice_d7 = pwm_gpio_to_slice_num(PIN_D7);
    chan_d7  = pwm_gpio_to_channel(PIN_D7);
    slice_d8 = pwm_gpio_to_slice_num(PIN_D8);
    chan_d8  = pwm_gpio_to_channel(PIN_D8);

    pwm_config config = pwm_get_default_config();
    // System Clock (typ. 125MHz) durch 125 teilen -> 1MHz Zählertakt
    pwm_config_set_clkdiv(&config, 125.0f);
    pwm_config_set_wrap(&config, PWM_WRAP);

    pwm_init(slice_d7, &config, true);
    pwm_init(slice_d8, &config, true);

    // D7 und D8 initial auf 0 setzen
    pwm_set_chan_level(slice_d7, chan_d7, 0);
    pwm_set_chan_level(slice_d8, chan_d8, 0);
}

// Hilfsfunktion, um 0-255 auf den internen 0-2499 Wrap zu skalieren
void set_pwm_8bit(uint slice, uint chan, uint8_t val) {
    uint32_t level = (val * PWM_WRAP) / 255;
    pwm_set_chan_level(slice, chan, level);
}

void loop() {
    set_pwm_8bit(slice_d8, chan_d8, 255);
    delay(10); // Beibehalten aus dem Original, auch wenn logisch fragwürdig

    for(int i = 0; i < 256; i++) {
        set_pwm_8bit(slice_d8, chan_d8, i);
        put_pixel(urgb_u32(i, 0, 0));
        delay(10);
    }

    put_pixel(urgb_u32(0, 0, 255));
    delay(500);

    for(int i = 255; i >= 0; i--) {
        set_pwm_8bit(slice_d8, chan_d8, i);
        put_pixel(urgb_u32(0, i, 0));
        delay(10);
    }

    delay(500);
}