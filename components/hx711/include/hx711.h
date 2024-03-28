
#ifndef hx711_h
#define hx711_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"


typedef enum hx711_GAIN
{
	eGAIN_128 = 1,
	eGAIN_64 = 3,
	eGAIN_32 = 2
}hx711_GAIN;


// define clock and data pin, channel, and gain factor
// channel selection is made by passing the appropriate gain: 128 or 64 for channel A, 32 for channel B
// gain: 128 or 64 for channel A; channel B works with 32 gain factor only
void hx711_init(gpio_num_t dout, gpio_num_t pd_sck, hx711_GAIN gain);


// check if hx711 is ready
// from the datasheet: When output data is not ready for retrieval, digital output pin DOUT is high. Serial clock
// input PD_SCK should be low. When DOUT goes to low, it indicates data is ready for retrieval.
bool hx711_is_ready();

// set the gain factor; takes effect only after a call to read()
// channel A can be set for a 128 or 64 gain; channel B has a fixed 32 gain
// depending on the parameter, the channel is also set to either A or B
void hx711_set_gain(hx711_GAIN gain);

// waits for the chip to be ready and returns a reading
unsigned long hx711_read();

// returns an average reading; times = how many times to read
unsigned long hx711_read_average(char times );

// returns (read_average() - OFFSET), that is the current value without the tare weight; times = how many readings to do
unsigned long hx711_get_value(char times);

// returns get_value() divided by SCALE, that is the raw value divided by a value obtained via calibration
// times = how many readings to do
float hx711_get_units(char times);

// set the OFFSET value for tare weight; times = how many times to read the tare value
void hx711_tare();

// set the SCALE value; this value is used to convert the raw data to "human readable" data (measure units)
void hx711_set_scale(float scale );

// get the current SCALE
float hx711_get_scale();

// set OFFSET, the value that's subtracted from the actual reading (tare weight)
void hx711_set_offset(unsigned long offset);

// get the current OFFSET
unsigned long hx711_get_offset();

// puts the chip into power down mode
void hx711_power_down();

// wakes up the chip after power down mode
void hx711_power_up();

#endif