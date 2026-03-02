/* 
 * Custom program for VSC8258 using MEPA.
 * Code is loosely-based on phy_demo_appl/vtss_appl_10g_phy_malibu.c
 *
 * Copyright (C) 2025 Microchip Technology Inc.
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

// *****************************************************************************
// *****************************************************************************
// Section: Header Includes
// *****************************************************************************
// *****************************************************************************

// Standard C includes
#include <stdio.h>
#include <unistd.h>     // For usleep()
#include <stdint.h>

// MEPA includes
#include "microchip/ethernet/phy/api.h"
#include "vtss_phy_10g_api.h"       // For vtss_phy_10g_mode_t

// Other includes
#include "rpi_spi.h"        // For the SPI accessor functions
#include "io_test.h"

// *****************************************************************************
// *****************************************************************************
// Section: Macros and Constant defines
// *****************************************************************************
// *****************************************************************************

#define APPL_PORT_COUNT         4       // VSC8258 is a 4-port PHY
#define APPL_BASE_PORT          0       // Port 0 is used as the base port.

// Macros for debug levels:
// APPL_TRACE_LVL_NOISE
// APPL_TRACE_LVL_DEBUG
// APPL_TRACE_LVL_INFO
// APPL_TRACE_LVL_WARNING

// *****************************************************************************
// *****************************************************************************
// Section: Function prototypes
// *****************************************************************************
// *****************************************************************************

int appl_mepa_init(void);
void appl_spi_init(void);

// Functions below were taken from the example pseudocode in mepa-doc.html
void *appl_mem_alloc(struct mepa_callout_ctx *ctx, size_t size);
void appl_mem_free(struct mepa_callout_ctx *ctx, void *ptr);

// *****************************************************************************
// *****************************************************************************
// Section: MEPA Structs
// *****************************************************************************
// *****************************************************************************

typedef struct mepa_callout_ctx mepa_callout_ctx_t;

mepa_device_t       *appl_malibu_device[APPL_PORT_COUNT];
mepa_conf_t         appl_malibu_conf;

// Declare this as an array of structs, not an array of pointers to structs.
// See how "APPL_mepa_callout_cxt" was declared in the code snippet in mepa-doc.html
mepa_callout_ctx_t  appl_callout_ctx[APPL_PORT_COUNT];

mepa_board_conf_t   appl_board_conf;        // Not used in the application since MEBA is not needed here.
                                            // I also don't know how to use this.

// Define appl_rpi_spi. See rpi_spi.c for implementation details.
mepa_callout_t appl_rpi_spi =
{
    .spi_read = rpi_spi_32bit_read,
    .spi_write = rpi_spi_32bit_write,
    
    .mmd_read = rpi_spi_16bit_read,
    .mmd_write = rpi_spi_16bit_write,

    .mem_alloc = appl_mem_alloc,
    .mem_free = appl_mem_free,
};

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

 int main(void)
 {
    // Defining local variables
    mepa_rc rc = 0;
    unsigned int i = 0;

    printf("Malibu SPI IO Test\r\n");

    /* ****************************************************** */
    /*                       BOARD SETUP                      */
    /* ****************************************************** */
    // Initialize the "board" (which refers to the RPi and VSC8258EV setup)
    // See reply from JohnH
    // Test the board... or at least read the Device ID
    printf("%s: Connecting to the Malibu board via SPI... \r\n", __func__);
    printf("%s: Initializing RPI SPI... \r\n", __func__);
    appl_spi_init();

    // Test SPI
    uint32_t val = 0;
    rpi_spi_32bit_read_rbt_test(0x0, 0x1e, 0x0, &val);
    printf("In Line %d: Dev ID = 0x%x\r\n\r\n", __LINE__, val);

    // Real applications MUST check the return value
    rc = appl_mepa_init();
    printf("mepa_create: rc: %d\r\n\r\n", rc);

    // Run SPI IO Test after board initialization by appl_mepa_init()
    printf("appl_malibu_spi_io_test\r\n\r\n");
    appl_malibu_spi_io_test(&appl_rpi_spi, &appl_callout_ctx, APPL_PORT_COUNT);

    return 0;
}

 // *****************************************************************************
// *****************************************************************************
// Section: Function Defines
// *****************************************************************************
// *****************************************************************************

int appl_mepa_init(void)
{
    unsigned int i = 0;

   // Loop through all ports (PHYs) in the system.
    for (i = 0; i < APPL_PORT_COUNT; ++i)
    {
        memset(&appl_board_conf, 0, sizeof(appl_board_conf));
        appl_board_conf.numeric_handle = i;

        memset(&appl_callout_ctx[i], 0, sizeof(appl_callout_ctx[i]));
        appl_callout_ctx[i].port_no = i;
        
        appl_malibu_device[i] = mepa_create(&appl_rpi_spi,
                                            &appl_callout_ctx[i],
                                            &appl_board_conf);
    }

    for (i = 0; i < APPL_PORT_COUNT; ++i)
    {
        // The application needs to keep track on which PHYs is located in common
        // packets.
        if(i != APPL_BASE_PORT)
        {
            mepa_link_base_port(appl_malibu_device[i], 
                                appl_malibu_device[APPL_BASE_PORT],
                                i);
        }
    }

    return MEPA_RC_OK;
}

void appl_spi_init(void)
{
    spi_initialize();
}

void *appl_mem_alloc(struct mepa_callout_ctx *ctx, size_t size)
{
    return malloc(size);
}

void appl_mem_free(struct mepa_callout_ctx *ctx, void *ptr)
{
    free(ptr);
}

// *****************************************************************************
// End of file
