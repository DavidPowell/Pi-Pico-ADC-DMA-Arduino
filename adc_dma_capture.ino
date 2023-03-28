/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// For ADC input:
#include "hardware/adc.h"
#include "hardware/dma.h"

// This example uses the DMA to capture many samples from the ADC.
//
// - We are putting the ADC in free-running capture mode at 0.5 Msps
//
// - A DMA channel will be attached to the ADC sample FIFO
//
// - Configure the ADC to right-shift samples to 8 bits of significance, so we
//   can DMA into a byte buffer
//
// This could be extended to use the ADC's round robin feature to sample two
// channels concurrently at 0.25 Msps each.
//
// Connect an external signal generator to GPIO 26 (noting the 0-3.3V safe input range)

// Channel 0 is GPIO26
#define CAPTURE_CHANNEL 0
#define CAPTURE_DEPTH 1000

uint8_t capture_buf[CAPTURE_DEPTH];

uint dma_chan;
dma_channel_config cfg;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
  adc_gpio_init(26 + CAPTURE_CHANNEL);

  adc_init();
  adc_select_input(CAPTURE_CHANNEL);
  adc_fifo_setup(
    true,    // Write each completed conversion to the sample FIFO
    true,    // Enable DMA data request (DREQ)
    1,       // DREQ (and IRQ) asserted when at least 1 sample present
    false,   // We won't see the ERR bit because of 8 bit reads; disable.
    true     // Shift each sample to 8 bits when pushing to FIFO
  );

  // Divisor of 0 -> full speed. Free-running capture with the divider is
  // equivalent to pressing the ADC_CS_START_ONCE button once per `div + 1`
  // cycles (div not necessarily an integer). Each conversion takes 96
  // cycles, so in general you want a divider of 0 (hold down the button
  // continuously) or > 95 (take samples less frequently than 96 cycle
  // intervals). This is all timed by the 48 MHz ADC clock.
  adc_set_clkdiv(0);

  sleep_ms(1000);
  // Set up the DMA to start transferring data as soon as it appears in FIFO
  dma_chan = dma_claim_unused_channel(true);
  cfg = dma_channel_get_default_config(dma_chan);

  // Reading from constant address, writing to incrementing byte addresses
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);

  // Pace transfers based on availability of ADC samples
  channel_config_set_dreq(&cfg, DREQ_ADC);

}

void loop(void)
{

  Serial.println("Arming DMA");

  dma_channel_configure(dma_chan, &cfg,
    capture_buf,    // dst
    &adc_hw->fifo,  // src
    CAPTURE_DEPTH,  // transfer count
    true            // start immediately
  );

  Serial.println("Starting capture");
  adc_run(true);

  // Once DMA finishes, stop any new conversions from starting, and clean up
  // the FIFO in case the ADC was still mid-conversion.
  dma_channel_wait_for_finish_blocking(dma_chan);
  Serial.println("Capture finished");
  adc_run(false);
  adc_fifo_drain();

  // Print samples to stdout so you can display them in Arduino Serial monitor
  for (int i = 0; i < CAPTURE_DEPTH; ++i) {
    Serial.println(capture_buf[i]);
  }

  sleep_ms(1000);

}
