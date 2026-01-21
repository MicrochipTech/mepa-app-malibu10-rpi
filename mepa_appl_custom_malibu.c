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
void appl_set_trace(void);
mepa_rc appl_mepa_phy_init(mepa_port_no_t port_no);
mepa_rc appl_mepa_reset_phy(mepa_port_no_t port_no);
mepa_rc appl_mepa_poll(mepa_port_no_t port_no);

void appl_mepa_tracer(const mepa_trace_data_t *data, va_list args);

// Functions below were taken from the example pseudocode in mepa-doc.html
void *appl_mem_alloc(struct mepa_callout_ctx *ctx, size_t size);
void appl_mem_free(struct mepa_callout_ctx *ctx, void *ptr);

// SPI IO Test - ref: vtss_appl_10g_phy_malibu.c
void appl_malibu_spi_io_test(mepa_callout_t *callout, mepa_callout_ctx_t *callout_ctx);

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

    printf("Raspberry Pi 5 Malibu Code.\r\n");

    appl_set_trace();

    // Test the trace functions. You should see an output in the terminal.
    T_E("Error at %d\r\n", __LINE__);
    T_D("Debug at %d\r\n", __LINE__);
    T_W("Warning at %d\r\n", __LINE__);
    T_I("Info at %d\r\n", __LINE__);

    printf ("//Default Setup Assumptions: \n" );
    printf ("//Board has Power Applied prior to Demo start \n" );
    printf ("//Pwr Supply/Voltages Stable, Ref Clk Stable, Ref & PLL Stable \n\n" );

    /* ****************************************************** */
    /*                       BOARD SETUP                      */
    /* ****************************************************** */
    // Initialize the "board" (which refers to the RPi and VSC8258EV setup)
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
    appl_malibu_spi_io_test(&appl_rpi_spi, &appl_callout_ctx);

    // Give time to confirm the SPI IO test worked... - around 2s
    usleep(2000000);

    /* ****************************************************** */
    /*                       PHY Bring-up                     */
    /* ****************************************************** */    
    // Reset PHY ports
    // See MEPA.Doc.html from 2025.06 > mepa_reset() is called before mepa_conf_set()
    for(i = 0; i < APPL_PORT_COUNT; i++)
    {
        printf("Resetting port %d\n", i);
        rc = appl_mepa_reset_phy(i);
        printf("appl_mepa_reset_phy: rc: %d\r\n\r\n\r\n\r\n", rc);
    }

    // Set the PHY Configuration for each port.
    // Reference: vtss_appl_10g_phy_malibu.c > 1st for loop of main()
    // Also, refer to meba/src/sparx5/meba.c > malibu_init()
    for(i = 0; i < APPL_PORT_COUNT; i++)
    {
        printf("Configuring port %d\n", i);
        rc = appl_mepa_phy_init(i);
        printf("appl_mepa_phy_init: rc: %d\r\n\r\n\r\n\r\n", rc);
    }

    // Wait for PHY to stabilize - around 1s
    usleep(1000000);
    
    // Check PHY capability of Port 2
    mepa_phy_info_t appl_phy_info;
    memset(&appl_phy_info, 0, sizeof(appl_phy_info));
    rc = mepa_phy_info_get(appl_malibu_device[2], &appl_phy_info);
    printf("mepa_phy_info_get: rc: %d, PHY Cap: 0x%X, Part: 0x%X\r\n", rc, appl_phy_info.cap, appl_phy_info.part_number);

    // mepa_media_get not implemented.
    // // Get Media of port 2 (with Cu SFP plugged in)
    // mepa_media_interface_t appl_phy_media;
    // rc = mepa_media_get(appl_malibu_device[2], &appl_phy_media);h
    // T_I("mepa_media_get: rc %d, port 2 %d", rc, appl_phy_media);

    // // Get Media of port 3 (with 10G SFP+ plugged in)
    // rc = mepa_media_get(appl_malibu_device[3], &appl_phy_media);
    // T_I("mepa_media_get: rc %d, port 3 %d", rc, appl_phy_media);

    // Set media
    // mepa_media_interface_t phy_media_if = MESA_PHY_MEDIA_IF_SFP_PASSTHRU;
    // rc = mepa_media_set(appl_malibu_device[2], phy_media_if);
    // printf("mepa_media_set: rc %d, port 2", rc);


    // Poll PHY ports.
    for(i = 0; i < 10; i++)
    {
        rc = appl_mepa_poll(0);
        rc = appl_mepa_poll(1);
        rc = appl_mepa_poll(2);
        rc = appl_mepa_poll(3);
        printf("\n");
        // printf("appl_mepa_poll: rc: %d\r\n\r\n", rc);
        usleep(500000); // 500ms
    }
    
    // Get Debug info. See https://microchip.my.site.com/s/article/Dumping-VSC-PHY-Registers
    // mepa_debug_info_t mepa_dbg;
    // memset (&mepa_dbg, 0, sizeof(mesa_debug_info_t));
    // mepa_dbg.full = 1;
    // mepa_dbg.clear = 1;
    // mepa_dbg.vml_format = 0;

    // mepa_dbg.layer = MEPA_DEBUG_LAYER_ALL;   // All Layers or CIL or AIL
    //                                 // MEPA_DEBUG_LAYER_CIL or MEPA_DEBUG_LAYER_AIL

    // mepa_dbg.group = MEPA_DEBUG_GROUP_PHY; // Gen PHY Register Dump

    // rc = mepa_debug_info_dump(appl_malibu_device[2], (mesa_debug_printf_t) printf, &mepa_dbg);

    return 0;
}

 // *****************************************************************************
// *****************************************************************************
// Section: Function Defines
// *****************************************************************************
// *****************************************************************************

void appl_set_trace(void)
{
    // Register the tracer function
    MEPA_TRACE_FUNCTION = appl_mepa_tracer;
}

int appl_mepa_init(void)
{
    // Code is taken from the mepa_init() snippet 
    // in mepa-doc.html#mepa/docs/mepa_instantiation
    // If board has a GPIO which needs to be toggled to release PHYs from reset,
    // then this can be done here.

    // 2024-07-18: Another useful reference for this is in
    // meba/src/meba_generic.c > meba_phy_driver_init()

    unsigned int i = 0;

   // Loop through all ports (PHYs) in the system.
    for (i = 0; i < APPL_PORT_COUNT; ++i)
    {
        // Configure the board configuration (note temporary life time).
        memset(&appl_board_conf, 0, sizeof(appl_board_conf));
        appl_board_conf.numeric_handle = i;

        // Fill application specific data in the context area. This is likely to
        // include bus instance, MDIO address etc.
        //APPL_fill_port_data(i, &APPL_mepa_callout_cxt[i]);
        memset(&appl_callout_ctx[i], 0, sizeof(appl_callout_ctx[i]));
        appl_callout_ctx[i].port_no = i;
        
        // Create the MEPA devices (real applications needs to check for error as
        // well).
        appl_malibu_device[i] = mepa_create(&appl_rpi_spi,
                                            &appl_callout_ctx[i],
                                            &appl_board_conf);
    }

   // Link to base port since we're dealing with a quad-port PHY.
   // For VSC8258, we can just link directly to Port 0,
   // but for multi-PHY systems, it's better to loop through each PHY
   // port as shown in the KB below.
   // https://microchip.my.site.com/s/article/Creating-MEPA-API-PHY-Instances-for-VSC-PHY
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

mepa_rc appl_mepa_phy_init(mepa_port_no_t port_no)
{
    mepa_rc rc = MEPA_RC_OK;

    rc = mepa_conf_get(appl_malibu_device[port_no], &appl_malibu_conf);
    if (rc != MEPA_RC_OK)
    {
        T_E("%s: mepa_conf_get() error %d\n", __func__, rc);
        return rc;
    }
    
    
    // Configure the 10g PHY operating mode
    // from meba/src/sparx5/meba.c > malibu_init
    appl_malibu_conf.speed = MESA_SPEED_10G;
    appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_LAN_MODE;
    appl_malibu_conf.conf_10g.interface_mode = MEPA_PHY_SFI_XFI;
    appl_malibu_conf.conf_10g.channel_id = MEPA_CHANNELID_NONE;
    appl_malibu_conf.conf_10g.xfi_pol_invert = 1;
    appl_malibu_conf.conf_10g.polarity.host_rx = false;
    appl_malibu_conf.conf_10g.polarity.line_rx = false;
    appl_malibu_conf.conf_10g.polarity.host_tx = false;
    appl_malibu_conf.conf_10g.polarity.line_tx = (port_no < 2)? false : true;
    appl_malibu_conf.conf_10g.is_host_wan = false;
    appl_malibu_conf.conf_10g.lref_for_host = false;
    appl_malibu_conf.conf_10g.h_clk_src_is_high_amp = true;
    appl_malibu_conf.conf_10g.l_clk_src_is_high_amp = true;
    appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_DAC;
    appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_DAC;
    appl_malibu_conf.conf_10g.channel_high_to_low = false;
    // appl_malibu_conf.admin.enable = 1;

    rc = mepa_conf_set(appl_malibu_device[port_no], &appl_malibu_conf);
    if (rc != MEPA_RC_OK)
    {
        T_E("%s: mepa_conf_set() error %d", __func__, rc);
        return rc;
    }

    return rc;
}

mepa_rc appl_mepa_reset_phy(mepa_port_no_t port_no)
{
    mepa_reset_param_t rst_conf = {};

    // See malibu_10g_reset in vtss.c
    rst_conf.reset_point = MEPA_RESET_POINT_PRE;

    return mepa_reset(appl_malibu_device[port_no], &rst_conf);
}

mepa_rc appl_mepa_poll(mepa_port_no_t port_no)
{
    mepa_rc rc = MEPA_RC_OK;

    // See microchip/ethernet/common.h > mesa_port_speed_t
    char *portspeed2txt[] = {
        "Undefined",
        "10 Mbps",
        "100 Mbps",
        "1000 Mbps",
        "2500 Mbps",
        "5 Gbps",
        "10 Gbps",
        "12 Gbps",
        "25 Gbps",
        "Auto",
    };

    mepa_status_t appl_status = {};
    rc = mepa_poll(appl_malibu_device[port_no], &appl_status);
    printf("Port: %d, rc: %d, Speed: %s, fdx: %s, Cu: %s, Fi: %s, Link: %s\n", port_no, rc, portspeed2txt[appl_status.speed], \
            appl_status.fdx? "Yes":"No", appl_status.copper? "Yes":"No", appl_status.fiber? "Yes":"No", appl_status.link? "Up" : "Down");
    
    return rc;
}

void appl_mepa_tracer(const mepa_trace_data_t *data, va_list args)
{
    // Taken from the example code snippet in mepa-doc.html#mepa/docs/mepa_instantiation
#if !defined(APPL_TRACE_LVL_NONE)
#if !defined(APPL_TRACE_LVL_ERROR)
#if !defined(APPL_TRACE_LVL_WARNING)
#if !defined(APPL_TRACE_LVL_INFO)
#if !defined(APPL_TRACE_LVL_DEBUG)
#if !defined(APPL_TRACE_LVL_NOISE)
    #define APPL_TRACE_LVL_NONE
#endif
#endif
#endif
#endif
#endif
#endif

#ifdef APPL_TRACE_LVL_NONE
    return;
#endif

#ifdef APPL_TRACE_LVL_ERROR
    if(data->level < MEPA_TRACE_LVL_ERROR)
    {
        return;
    }
#endif


#ifdef APPL_TRACE_LVL_WARNING
    if(data->level < MEPA_TRACE_LVL_WARNING)
    {
        return;
    }
#endif

#ifdef APPL_TRACE_LVL_INFO
    if(data->level < MEPA_TRACE_LVL_INFO)
    {
        return;
    }
#endif


#ifdef APPL_TRACE_LVL_DEBUG
    if(data->level < MEPA_TRACE_LVL_DEBUG)
    {
        return;
    }
#endif

#ifdef APPL_TRACE_LVL_NOISE
    if(data->level < MEPA_TRACE_LVL_NOISE)
    {
        return;
    }
#endif

    // Do filtering, and optionally add more details.
    printf("%s ", data->location);
    //vprintf(data->format, args);

    /* Note: for real applications, it may be useful to 
     * do filtering here such that only certain print levels
     * show up on the output. For example:
     *       if(data->level ==  MEPA_TRACE_LVL_DEBUG)
     * #ifdef (TRACE_LEVEL_DEBUG)
     *           vprintf(...);
     * #endif
     *
     *  Then, compile the application with the correct flags
     *  gcc .. -DTRACE_LEVEL_DEBUG -DTRACE_LEVEL_INFO ...
     *
     * It's the user's responsibility to use the trace levels
     * properly in the application.
    */

    char *trace_str[] = {
        "null",
        "RACKET",
        "NOISE",
        "null",
        "DEBUG",
        "null",
        "INFO",
        "null",
        "WARNING",
        "ERROR",
        "NONE"
    };
    
    char tempstr[512];
    memset(tempstr, 0, sizeof(tempstr));

    printf("(%s): ", trace_str[data->level]);
    vsprintf(tempstr, data->format, args);
    printf("%s", tempstr);

    // Check for newline:
    if(tempstr[strlen(tempstr)-1] != '\n')
    {
        printf("\r\n");
    }

    return;
}

void *appl_mem_alloc(struct mepa_callout_ctx *ctx, size_t size)
{
    return malloc(size);
}

void appl_mem_free(struct mepa_callout_ctx *ctx, void *ptr)
{
    free(ptr);
}

void appl_malibu_spi_io_test(mepa_callout_t *callout, mepa_callout_ctx_t *callout_ctx)
{
    uint32_t val32 = 0;
    uint32_t val32B = 0;
    uint16_t addr;
    uint8_t dev;
    mepa_port_no_t port_no = callout_ctx->port_no;

    printf("Test reading global device ID register...\n");
    dev = 0x1E; addr = 0x0;
    callout->spi_read(&callout_ctx[0], 0, dev, addr, &val32);
    printf("0x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);

    dev = 0x1E; addr = 0x0;
    callout->spi_read(&callout_ctx[1], 1, dev, addr, &val32);
    printf("1x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);

    dev = 0x1E; addr = 0x0;
    callout->spi_read(&callout_ctx[2], 2, dev, addr, &val32);
    printf("2x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);

    // // dev = 0x1E; addr = 0x9002;
    // // inst->init_conf.spi_32bit_read_write(NULL, 0, SPI_RD, dev, addr, &val32);
    // // printf("0x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    // // *val32 = 0x4;
    // // inst->init_conf.spi_32bit_read_write(NULL, 0, SPI_WR, dev, addr, &val32);
    // // inst->init_conf.spi_32bit_read_write(NULL, 0, SPI_RD, dev, addr, &val32);
    // // printf("0x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    
    // // dev = 0x1E; addr = 0x9002;
    // // inst->init_conf.spi_32bit_read_write(NULL, 2, SPI_RD, dev, addr, &val32);
    // // printf("2x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    // // *val32 = 0x4;
    // // inst->init_conf.spi_32bit_read_write(NULL, 2, SPI_WR, dev, addr, &val32);
    // // inst->init_conf.spi_32bit_read_write(NULL, 2, SPI_RD, dev, addr, &val32);
    // // printf("2x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    
    // // dev = 0x1E; addr = 0x9202;
    // // inst->init_conf.spi_32bit_read_write(NULL, 0, SPI_RD, dev, addr, &val32);
    // // printf("0x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    // // *val32 = 0x4;
    // // inst->init_conf.spi_32bit_read_write(NULL, 0, SPI_WR, dev, addr, &val32);
    // // inst->init_conf.spi_32bit_read_write(NULL, 0, SPI_RD, dev, addr, &val32);
    // // printf("0x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    
    // // dev = 0x1E; addr = 0x9202;
    // // inst->init_conf.spi_32bit_read_write(NULL, 2, SPI_RD, dev, addr, &val32);
    // // printf("2x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    // // *val32 = 0x4;
    // // inst->init_conf.spi_32bit_read_write(NULL, 2, SPI_WR, dev, addr, &val32);
    // // inst->init_conf.spi_32bit_read_write(NULL, 2, SPI_RD, dev, addr, &val32);
    // // printf("2x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);
    
    dev = 0x1; addr = 0xF112;
    printf("Test writing to 0x%Xx%04X...\n", dev, addr);
    callout->spi_read(&callout_ctx[0], 0, dev, addr, &val32);
    printf("0x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    val32 = 0x003DF828;
    callout->spi_write(&callout_ctx[0], 0, dev, addr, &val32);
    callout->spi_read(&callout_ctx[0], 0, dev, addr, &val32);
    printf("0x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);

    dev = 0x1; addr = 0xF112;
    printf("Test writing to 1x%Xx%04X...\n", dev, addr);
    callout->spi_read(&callout_ctx[1], 1, dev, addr, &val32);
    printf("1x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    val32 = 0x004DF828;
    callout->spi_write(&callout_ctx[1], 1, dev, addr, &val32);
    callout->spi_read(&callout_ctx[1], 1, dev, addr, &val32);
    printf("1x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);

    dev = 0x1; addr = 0xF112;
    printf("Test writing to 2x%Xx%04X...\n", dev, addr);
    callout->spi_read(&callout_ctx[2], 2, dev, addr, &val32);
    printf("2x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    val32 = 0x005DF828;
    callout->spi_write(&callout_ctx[2], 2, dev, addr, &val32);
    callout->spi_read(&callout_ctx[2], 2, dev, addr, &val32);
    printf("2x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    val32 = 0x007DF820;
    callout->spi_write(&callout_ctx[2], 2, dev, addr, &val32);
    callout->spi_read(&callout_ctx[2], 2, dev, addr, &val32);
    printf("2x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);

    dev = 0x1; addr = 0xF112;
    printf("Test writing to 3x%Xx%04X...\n", dev, addr);
    callout->spi_read(&callout_ctx[3], 3, dev, addr, &val32);
    printf("3x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    val32 = 0x006DF828;
    callout->spi_write(&callout_ctx[3], 3, dev, addr, &val32);
    callout->spi_read(&callout_ctx[3], 3, dev, addr, &val32);
    printf("3x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    val32 = 0x007DF820;
    callout->spi_write(&callout_ctx[3], 3, dev, addr, &val32);
    callout->spi_read(&callout_ctx[3], 3, dev, addr, &val32);
    printf("3x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);

    dev = 0x1; addr = 0xF120;
    printf("Test reading from 0x%Xx%04X...\n", dev, addr);
    callout->spi_read(&callout_ctx[0], 0, dev, addr, &val32);
    printf("0x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    callout->spi_read(&callout_ctx[1], 1, dev, addr, &val32);
    printf("1x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    callout->spi_read(&callout_ctx[2], 2, dev, addr, &val32);
    printf("2x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    callout->spi_read(&callout_ctx[3], 3, dev, addr, &val32);
    printf("3x%Xx%04X = 0x%X\n\n", dev, addr, val32); fflush(stdout);

    dev = 0x1; addr = 0xF121;
    printf("Test writing to 0x%Xx%04X...\n", dev, addr);
    callout->spi_read(&callout_ctx[0], 0, dev, addr, &val32);
    printf("0x%Xx%04X = 0x%X\n", dev, addr, val32); fflush(stdout);
    val32B = 0x48888924;
    callout->spi_write(&callout_ctx[0], 0, dev, addr, &val32B);
    callout->spi_read(&callout_ctx[0], 0, dev, addr, &val32B);
    printf("0x%Xx%04X = 0x%X\n", dev, addr, val32B); fflush(stdout);
    // *val32 = 0x88888924;
    callout->spi_write(&callout_ctx[0], 0, dev, addr, &val32);
    callout->spi_read(&callout_ctx[0],0,  dev, addr, &val32);
    printf("0x%Xx%04X = 0x%X\n\n\n", dev, addr, val32); fflush(stdout);

    return;
}

// *****************************************************************************
// End of file