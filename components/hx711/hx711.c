#include "hx711.h"
#include "esp_log.h"
#include <rom/ets_sys.h>

#define HIGH 1
#define LOW 0
#define CLOCK_DELAY_US 20

#define DEBUGTAG "hx711"

static gpio_int_type_t GPIO_PIN_INTR_DISABLE = GPIO_INTR_DISABLE;
static gpio_num_t GPIO_PD_SCK = GPIO_NUM_20;	// Power Down and Serial Clock Input Pin
static gpio_num_t GPIO_DOUT = GPIO_NUM_19;		// Serial Data Output Pin
static hx711_GAIN GAIN = eGAIN_128;		// amplification factor
static unsigned long OFFSET = 0;	// used for tare weight
static float SCALE = 1;	// used to return weight in grams, kg, ounces, whatever

void hx711_init(gpio_num_t dout, gpio_num_t pd_sck, hx711_GAIN gain )
{
	GPIO_PD_SCK = pd_sck;
	GPIO_DOUT = dout;

	gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<GPIO_PD_SCK);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL<<GPIO_DOUT);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;
	gpio_config(&io_conf);

	hx711_set_gain(gain);
}

bool hx711_is_ready()
{
	return gpio_get_level(GPIO_DOUT);
}

void hx711_set_gain(hx711_GAIN gain)
{
	GAIN = gain;
	gpio_set_level(GPIO_PD_SCK, LOW);
	hx711_read();
}

uint8_t hx711_shiftIn()
{
	uint8_t value = 0;

    for(int i = 0; i < 8; ++i) 
    {
        gpio_set_level(GPIO_PD_SCK, HIGH);
        ets_delay_us(CLOCK_DELAY_US);
        value |= gpio_get_level(GPIO_DOUT) << (7 - i); //get level returns 
        gpio_set_level(GPIO_PD_SCK, LOW);
        ets_delay_us(CLOCK_DELAY_US);
    }

    return value;
}

unsigned long hx711_read()
{
	gpio_set_level(GPIO_PD_SCK, LOW);
	// wait for the chip to become ready
	while (hx711_is_ready()) 
	{
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}

	unsigned long value = 0;

	//--- Enter critical section ----
	portDISABLE_INTERRUPTS();

	for(int i=0; i < 24 ; i++)
	{   
		gpio_set_level(GPIO_PD_SCK, HIGH);
        ets_delay_us(CLOCK_DELAY_US);
        value = value << 1;
        gpio_set_level(GPIO_PD_SCK, LOW);
        ets_delay_us(CLOCK_DELAY_US);

        if(gpio_get_level(GPIO_DOUT))
        	value++;
	}

	// set the channel and the gain factor for the next reading using the clock pin
	for (unsigned int i = 0; i < GAIN; i++) 
	{	
		gpio_set_level(GPIO_PD_SCK, HIGH);
		ets_delay_us(CLOCK_DELAY_US);
		gpio_set_level(GPIO_PD_SCK, LOW);
		ets_delay_us(CLOCK_DELAY_US);
	}	
	portENABLE_INTERRUPTS();
	//--- Exit critical section ----

	value =value^0x800000;

	return (value);
}



unsigned long  hx711_read_average(char times) 
{
	unsigned long sum = 0;
	for (char i = 0; i < times; i++) 
	{
		sum += hx711_read();
	}
	return sum / times;
}

unsigned long hx711_get_value(char times) 
{
	unsigned long avg = hx711_read_average(times);
	if(avg > OFFSET)
		return avg - OFFSET;
	else
		return 0;
}

float hx711_get_units(char times) 
{
	return hx711_get_value(times) / SCALE;
}

void hx711_tare( ) 
{
	unsigned long sum = 0; 
	sum = hx711_read_average(20);
	hx711_set_offset(sum);
}

void hx711_set_scale(float scale ) 
{
	SCALE = scale;
}

float hx711_get_scale()
 {
	return SCALE;
}

void hx711_set_offset(unsigned long offset)
 {
	OFFSET = offset;
}

unsigned long hx711_get_offset(unsigned long offset) 
{
	return OFFSET;
}

void hx711_power_down() 
{
	gpio_set_level(GPIO_PD_SCK, LOW);
	ets_delay_us(CLOCK_DELAY_US);
	gpio_set_level(GPIO_PD_SCK, HIGH);
	ets_delay_us(CLOCK_DELAY_US);
}

void hx711_power_up() 
{
	gpio_set_level(GPIO_PD_SCK, LOW);
}