#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include <Adafruit_NeoPixel.h>

// XIAO RP2040 Pin Mapping -> Physical GPIOs
#define PIN_NEO_PWR 11
#define PIN_NEO_DAT 12

#define PIN_D7       1
#define PIN_D8       2

// --- 1. Adafruit NeoPixel Setup ---
// 1 Pixel, connected to PIN_NEO_DAT, 800kHz GRB bitstream
Adafruit_NeoPixel pixels(1, PIN_NEO_DAT, NEO_GRB + NEO_KHZ800);

// --- 2. Hardware PWM Variables ---
uint slice_d7, chan_d7;
uint slice_d8, chan_d8;

// const uint32_t PWM_WRAP = 2499; // At 1MHz Timer-Clock -> 2500 Ticks = 400Hz
const uint32_t PWM_WRAP = 50; // At 1MHz Timer-Clock -> 50 Ticks = 20kHz

void setup() {
    // Activate Neopixel Power (using standard Arduino API for simplicity)
    pinMode(PIN_NEO_PWR, OUTPUT);
    digitalWrite(PIN_NEO_PWR, HIGH);

    // Initialize NeoPixel
    pixels.begin();
    pixels.setBrightness(50); // Matches the previous custom logic scale
    pixels.show(); // Initialize all pixels to 'off'

    // Initialize PWM for D7 and D8 via RP2040 SDK
    gpio_set_function(PIN_D7, GPIO_FUNC_PWM);
    gpio_set_function(PIN_D8, GPIO_FUNC_PWM);

    slice_d7 = pwm_gpio_to_slice_num(PIN_D7);
    chan_d7  = pwm_gpio_to_channel(PIN_D7);
    slice_d8 = pwm_gpio_to_slice_num(PIN_D8);
    chan_d8  = pwm_gpio_to_channel(PIN_D8);

    pwm_config config = pwm_get_default_config();
    // Divide System Clock (typ. 125MHz) by 125 -> 1MHz counter clock
    pwm_config_set_clkdiv(&config, 125.0f);
    pwm_config_set_wrap(&config, PWM_WRAP);

    pwm_init(slice_d7, &config, true);
    pwm_init(slice_d8, &config, true);

    // Set D7 and D8 initially to 0
    pwm_set_chan_level(slice_d7, chan_d7, 0);
    pwm_set_chan_level(slice_d8, chan_d8, 0);
}

// Helper function to scale 0-255 to the internal 0-2499 Wrap
void set_pwm_8bit(uint slice, uint chan, uint8_t val) {
    uint32_t level = (val * PWM_WRAP) / 255;
    pwm_set_chan_level(slice, chan, level);
}

void loop() {
    set_pwm_8bit(slice_d8, chan_d8, 255);
    delay(10); // Kept from original

    for(int i = 0; i < 256; i++) {
        set_pwm_8bit(slice_d8, chan_d8, i);
        pixels.setPixelColor(0, pixels.Color(i, 0, 0)); // Red
        pixels.show();
        delay(10);
    }

    pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
    pixels.show();
    delay(500);

    for(int i = 255; i >= 0; i--) {
        set_pwm_8bit(slice_d8, chan_d8, i);
        pixels.setPixelColor(0, pixels.Color(0, i, 0)); // Green
        pixels.show();
        delay(10);
    }

    delay(500);
}
