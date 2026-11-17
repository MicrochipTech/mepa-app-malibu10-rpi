// Header file for rpi_gpio.c

#ifndef _RPI_GPIO_H_
#define _RPI_GPIO_H_

// Headers
#include <gpiod.h>

int gpio_set(unsigned int gpio_no, int val);


#endif /*_RPI_GPIO_H*/ 