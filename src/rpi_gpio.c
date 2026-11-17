/* 
 * GPIO Shim functions for Raspberry Pi
 * This acts as a compatibility layer between Pi OS 12 and Pi OS 13
 * which use different libgpiod versions.
 *
 * Copyright (C) 2026 Microchip Technology Inc.
 *
 * Author: MJ Neri https://support.microchip.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "rpi_gpio.h"

#ifdef LIBGPIOD_V2
// For libgpiod v2 documentation, see: https://libgpiod.readthedocs.io/
// Git repo at https://github.com/brgl/libgpiod
int gpio_set(unsigned int gpio_no, int val)
{
    // Use 'gpioinfo' command to find where GPIO25 is mapped to.
    struct gpiod_chip *c = gpiod_chip_open("/dev/gpiochip0");   /* "/dev/gpiochip0" */
    if (c == NULL)
    {
        return -1;
    }

    // Real applications must check the return value.
    struct gpiod_line_settings *s = gpiod_line_settings_new();
    if(s == NULL)
    {
        return -1;
    }
    struct gpiod_line_config *lc = gpiod_line_config_new();
    if(lc == NULL)
    {
        return -1;
    }

    // Ref: https://libgpiod.readthedocs.io/en/master/core_line_defs.html#_CPPv420gpiod_line_direction
    gpiod_line_settings_set_direction(s, GPIOD_LINE_DIRECTION_OUTPUT);

    // libgpiod v2 seems to assume the GPIO is active high.
    gpiod_line_settings_set_output_value(s, val ? GPIOD_LINE_VALUE_ACTIVE
                                                : GPIOD_LINE_VALUE_INACTIVE);

    gpiod_line_config_add_line_settings(lc, &gpio_no, 1, s);

    struct gpiod_line_request *r = gpiod_chip_request_lines(c, NULL, lc);

    gpiod_line_config_free(lc);
    gpiod_line_settings_free(s);
    gpiod_chip_close(c);
    if (!r)
    {
        return -1;
    }

    gpiod_line_request_release(r);
    return 0;
}
#else
int gpio_set(unsigned int gpio_no, int val)
{
    gpiod_ctxless_set_value("0", gpio_no, val, 0, "malibu_custom", NULL, NULL);
    return 0;
}
#endif