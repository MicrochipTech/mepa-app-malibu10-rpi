/* 
 * Custom program for VSC8258 using MEPA.
 * Code is loosely-based on phy_demo_appl/vtss_appl_10g_phy_malibu.c
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
#include "io_test.h"        // For the IO Test

// *****************************************************************************
// *****************************************************************************
// Section: Macros and Constant defines
// *****************************************************************************
// *****************************************************************************

#define APPL_PORT_COUNT         4       // VSC8258 is a 4-port PHY
#define APPL_BASE_PORT          0       // Port 0 is used as the base port.

#define PHY_MODE_10G_LAN        0
#define PHY_MODE_1G_LAN         1
#define PHY_MODE_10G_WAN        2
#define PHY_MODE_10G_RPTR       3
#define PHY_MODE_1G_RPTR        4

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
void appl_mepa_tracer(const mepa_trace_data_t *data, va_list args);

// Functions below were taken from the example pseudocode in mepa/docs/linkup_config.adoc
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

mepa_callout_ctx_t  appl_callout_ctx;

mepa_board_conf_t   appl_board_conf;        // Not used in the application since MEBA is not needed here.

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
// Local Functions
// *****************************************************************************
// *****************************************************************************

void appl_spi_init(void)
{
    spi_initialize();
}

void appl_set_trace(void)
{
    // Register the tracer function
    MEPA_TRACE_FUNCTION = appl_mepa_tracer;
}

mepa_rc appl_mepa_reset_phy(mepa_port_no_t port_no)
{
    mepa_reset_param_t rst_conf = {};

    // See malibu_10g_reset in vtss.c... only MEPA_RESET_POINT_PRE is used.
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

mepa_rc appl_mepa_phy_init(mepa_port_no_t port_no, int phy_mode)
{
    mepa_rc rc = MEPA_RC_OK;

    rc = mepa_conf_get(appl_malibu_device[port_no], &appl_malibu_conf);
    if (rc != MEPA_RC_OK)
    {
        T_E("%s: mepa_conf_get() error %d\n", __func__, rc);
        return rc;
    }
    
    // Configure the 10G PHY operating mode
    // from board-configs/src/sparx5/meba.c > malibu_init
    appl_malibu_conf.speed = MESA_SPEED_10G;
    

    // h_media/l_media:
    // --> MEPA_MEDIA_TYPE_SR2_SC (limiting SR/LR/ER/ZR limiting modules)
    // --> MEPA_MEDIA_TYPE_DAC_SC (Direct Attach Cu Cable)
    // --> MEPA_MEDIA_TYPE_KR_SC  (10GBASE-KR backplane)
    // --> MEPA_MEDIA_TYPE_ZR_SC  (linear ZR modules)
    switch(phy_mode)
    {
        case PHY_MODE_10G_LAN:   /* 0=MODE_10GLAN */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_LAN_MODE;

            // Assuming SR/LR limiting module on the line side and long traces (> 10 in)
            // or backplane on the host side
            // oper_mode.h_media = MEPA_MEDIA_TYPE_KR_SC;
            // oper_mode.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_KR_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            break;

        case PHY_MODE_1G_LAN: /* 1=MODE_1GLAN */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_1G_MODE;

            // Always use these media settings for 1G LAN data rates
            // oper_mode.h_media = MEPA_MEDIA_TYPE_SR2_SC;
            // oper_mode.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;

            break;

        case PHY_MODE_10G_WAN: /* 2=MODE_10GWAN */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_WAN_MODE;

            // Assuming SR/LR limiting module on the line side and long traces (> 10 in)
            // or backplane on the host side
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_KR_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;

            break;

        case PHY_MODE_10G_RPTR:   /* 0=MODE_10GRPTR */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_REPEATER_MODE;
            // oper_mode.rate = VTSS_RPTR_RATE_10_3125;     // No MEPA type for repeater mode rate as of 2025.12

            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_DAC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            break;

        case PHY_MODE_1G_RPTR:   /* 0=MODE_1GRPTR */
            // MJ Addition 2024-08-09. uncomment the two lines below when testing with a 1G Cu SFP
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_REPEATER_MODE;
            // oper_mode.rate = VTSS_RPTR_RATE_1_25;       // No MEPA type for repeater mode rate as of 2025.12

            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_DAC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            break;

        default:
            // Error
            T_E("mepa_conf_set config, port %d\n", port_no);
            printf("mepa_conf_set config failed, port %d -- Improper Mode Selection!\n", port_no);
    }
    
    appl_malibu_conf.conf_10g.interface_mode = MEPA_PHY_SFI_XFI;
    appl_malibu_conf.conf_10g.channel_id = MEPA_CHANNELID_NONE;

    // Invert polarity of Line/Host Tx/Rx
    // See VSC8258EV Schematics
    appl_malibu_conf.conf_10g.polarity.host_rx = false;
    appl_malibu_conf.conf_10g.polarity.line_rx = false;
    appl_malibu_conf.conf_10g.polarity.host_tx = false;
    appl_malibu_conf.conf_10g.polarity.line_tx = (port_no < 2)? false : true;

    // H/LREFCLK is_high_amp :
    // --> TRUE (1100mV to 2400mV diff swing)
    // --> FALSE (200mV to 1200mV diff swing)
    appl_malibu_conf.conf_10g.h_clk_src_is_high_amp = true;
    appl_malibu_conf.conf_10g.l_clk_src_is_high_amp = true;

    appl_malibu_conf.conf_10g.xfi_pol_invert = 1;
    appl_malibu_conf.conf_10g.is_host_wan = false;
    appl_malibu_conf.conf_10g.lref_for_host = false;
    appl_malibu_conf.conf_10g.channel_high_to_low = false;

    rc = mepa_conf_set(appl_malibu_device[port_no], &appl_malibu_conf);
    if (rc != MEPA_RC_OK)
    {
        T_E("%s: mepa_conf_set() error %d", __func__, rc);
        return rc;
    }

    return rc;
}

bool get_valid_port_no(mepa_port_no_t* port_no, char port_no_str[])
{
    scanf("%s", &port_no_str[0]);
    *port_no = atoi(port_no_str);

    // Validate the Port Number, ensure it is in range
    if (*port_no >= VTSS_PORT_NO_START && *port_no < VTSS_PORT_NO_END)
    {
        return true;
    }

    printf ("Error - Invalid Port Number: %d  Valid Range: %d - %d \n",
            *port_no, VTSS_PORT_NO_START, (VTSS_PORT_NO_END-1));

   return false;
}

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

    /* ****************************************************** */
    /*                       SPI IO Test                      */
    /* ****************************************************** */
    // Run SPI IO Test after mepa_create()
    printf("appl_malibu_spi_io_test()\r\n");
    appl_malibu_spi_io_test(&appl_rpi_spi, &appl_callout_ctx, APPL_PORT_COUNT);
    printf("\n");

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

// *****************************************************************************
// End of file