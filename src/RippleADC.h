#ifndef RIPPLE_ADC_H
#define RIPPLE_ADC_H

#include <Arduino.h>

/**
 * @class RippleADC
 * @brief Software API abstracting platform-specific high-speed DMA ADC sampling on the shunt resistor pin (A2).
 *
 * Target sampling requirements (documented based on motor pole count and max RPM):
 * - Motor types: Märklin 3-pole and 5-pole DC-refitted motors.
 * - Maximum mechanical speed: 15,000 RPM (250 Hz).
 * - Ripple frequency:
 *   - 3-pole motor: 6 commutator ripples/rev -> 1500 Hz max.
 *   - 5-pole motor: 10 commutator ripples/rev -> 2500 Hz max.
 * - Nyquist limit requires sampling above 5 kHz. However, to isolate ripples from the 20 kHz PWM
 *   carrier and DC noise, digital band-pass filtering (100 Hz to 2 kHz) and peak-detection require
 *   substantial sampling points per wave.
 * - Chosen sampling rate: 50 kHz to 100 kHz (e.g., 50 kHz provides 20 samples per ripple cycle at 2.5 kHz).
 *
 * The RippleADC class manages circular buffer acquisition via DMA without blocking the CPU or interfering
 * with the 20 kHz PWM.
 */
class RippleADC {
public:
    static const size_t BUFFER_SIZE = 512; // Sample buffer size

    RippleADC();
    ~RippleADC();

    /**
     * @brief Initializes the high-speed ADC and DMA channel.
     * @param pin The analog pin to sample (e.g. PIN_SHUNT / A2).
     * @param sample_rate_hz Desired sampling rate in Hz (e.g., 50000 to 100000).
     * @return True if initialization is successful, false otherwise.
     */
    bool begin(uint8_t pin, uint32_t sample_rate_hz = 50000);

    /**
     * @brief Starts continuous DMA-based sampling.
     */
    void start();

    /**
     * @brief Stops continuous DMA-based sampling.
     */
    void stop();

    /**
     * @brief Checks if a new block of samples is available.
     * @return True if a buffer half or full boundary is crossed.
     */
    bool available() const;

    /**
     * @brief Retrieves the latest captured raw ADC samples.
     * @param dest Destination buffer.
     * @param length Number of samples to read.
     * @return Number of samples successfully copied.
     */
    size_t readSamples(uint16_t* dest, size_t length);

    /**
     * @brief Debug helper to print the status of the DMA transfer.
     */
    void printStatus() const;

private:
    uint8_t pin_;
    uint32_t sample_rate_;
    volatile bool running_;
    uint16_t buffer_[BUFFER_SIZE];
    volatile size_t head_index_; // Points to latest DMA write position
};

#endif // RIPPLE_ADC_H
