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
#include "microchip/ethernet/board/api.h"
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

mepa_device_t       *appl_malibu_device[APPL_PORT_COUNT];
mepa_callout_ctx_t  appl_callout_ctx[APPL_PORT_COUNT];
mepa_conf_t         appl_malibu_conf;

mepa_board_conf_t   appl_board_conf;        // Only used to specify port_no through numeric_handle

// Define appl_rpi_spi. See rpi_spi.c for implementation details.
mepa_callout_t appl_rpi_spi =
{
    .spi_read = rpi_spi_32bit_read,
    .spi_write = rpi_spi_32bit_write,
    
    .mem_alloc = appl_mem_alloc,
    .mem_free = appl_mem_free,
};

// *****************************************************************************
// *****************************************************************************
// Local Variables/Defines
// *****************************************************************************
// *****************************************************************************

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

char *mediatype2txt[] = {
    "MEPA_MEDIA_TYPE_SR",
    "MEPA_MEDIA_TYPE_SR2",
    "MEPA_MEDIA_TYPE_DAC",
    "MEPA_MEDIA_TYPE_ZR",
    "MEPA_MEDIA_TYPE_KR",
    "MEPA_MEDIA_TYPE_SR_SC",
    "MEPA_MEDIA_TYPE_SR2_SC",
    "MEPA_MEDIA_TYPE_DAC_SC",
    "MEPA_MEDIA_TYPE_ZR_SC",
    "MEPA_MEDIA_TYPE_ZR2_SC",
    "MEPA_MEDIA_TYPE_KR_SC",
    "MEPA_MEDIA_TYPE_LR",
    "MEPA_MEDIA_TYPE_ER",
    "MEPA_MEDIA_TYPE_SFP28_25G_SR",
    "MEPA_MEDIA_TYPE_SFP28_25G_LR",
    "MEPA_MEDIA_TYPE_SFP28_25G_ER",
    "MEPA_MEDIA_TYPE_SFP28_25G_DAC1M",
    "MEPA_MEDIA_TYPE_SFP28_25G_DAC2M",
    "MEPA_MEDIA_TYPE_1000BASE_T",
    "MEPA_MEDIA_TYPE_NONE",
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

void appl_spi_read_write(mepa_callout_t *callout, 
                         mepa_callout_ctx_t *callout_ctx, 
                         uint8_t mmd,
                         uint16_t addr,
                         uint32_t *value,
                         bool iswrite)
{
    mepa_port_no_t port_no = callout_ctx->port_no;
    T_D("\nPort no is %d\n", port_no);

    if(iswrite)
    {
        callout->spi_write(callout_ctx, port_no, mmd, addr, value);
    }
    else
    {
        callout->spi_read(callout_ctx, port_no, mmd, addr, value);
    }
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

mepa_rc appl_mepa_status_get(mepa_port_no_t port_no)
{
    mepa_rc rc = MEPA_RC_OK;

#if 0
    mepa_status_t appl_status = {};
    rc = mepa_poll(appl_malibu_device[port_no], &appl_status);
    printf("Port: %d, rc: %d, Speed: %s, fdx: %s, Cu: %s, Fi: %s, Link: %s\n", port_no, rc, portspeed2txt[appl_status.speed], \
            appl_status.fdx? "Yes":"No", appl_status.copper? "Yes":"No", appl_status.fiber? "Yes":"No", appl_status.link? "Up" : "Down");
#endif

    // Note: mepa_poll() calls vtss.c > phy_10g_poll(), which calls vtss_phy_10g_status_get().
    // However, mepa_poll() does not yet expose 10G PHY-specific statuses (as of SW-MEPA 2025.12), 
    // such as PMA/PCS links, block lock, etc.
    // So, we call vtss_phy_10g_status_get() directly below to get these statuses.
    vtss_phy_10g_status_t status_10g = {};
    vtss_phy_10g_cnt_t cnt = {};
    phy10g_oper_mode_t oper_mode = appl_malibu_conf.conf_10g.oper_mode;

    // Note: vtss_phy_10g_status_get() expects a vtss_inst_t struct to be passed to it.
    // phy_10g_poll() is able to get this by typecasting the private 'data' member of mepa_device_t
    // to another type (phy_data_t), which is only defined in vtss_private.h -- a header file that
    // is not meant to be included by an application.
    // Passing NULL still works because the internal VTSS APIs treat this as the default instance (see vtss_phy_inst_check())
    if (vtss_phy_10g_status_get(NULL, port_no, &status_10g) != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_status_get() failed, port %d", port_no);
        return MEPA_RC_ERROR;
    }

    // Print status
    // Ref: vtss_appl_malibu_status_get() in vtss_appl_10g_phy_malibu.c
    printf("Port %d %-6s %-12s %-16s %-12s %-12s\n", port_no, "", "Link", "Link-down-event", "Rx-Fault-Sticky", "Tx-Fault-Sticky");
    printf ("Port %u:\n", port_no);
    printf ("--------\n");
    printf ("%-12s %-12s %-16s %-12s %-12s\n", "Line PMA:", status_10g.pma.rx_link?"Yes":"No",
              status_10g.pma.link_down ? "Yes" : "No", status_10g.pma.rx_fault ? "Yes" : "No", status_10g.pma.tx_fault ? "Yes" : "No");
    printf ("%-12s %-12s %-16s %-12s %-12s\n", "Host PMA:", status_10g.hpma.rx_link?"Yes":"No",
            status_10g.hpma.link_down ? "Yes" : "No", status_10g.hpma.rx_fault ? "Yes" : "No", status_10g.hpma.tx_fault ? "Yes" : "No");
    printf ("%-12s %-12s %-16s %-12s %-12s\n", "WIS:", oper_mode == VTSS_PHY_WAN_MODE ? status_10g.wis.rx_link ? "Yes" : "No" : "-",
            oper_mode == VTSS_PHY_WAN_MODE ? status_10g.wis.link_down ? "Yes" : "No" : "-",
            oper_mode == VTSS_PHY_WAN_MODE ? status_10g.wis.rx_fault ? "Yes" : "No" : "-",
            oper_mode == VTSS_PHY_WAN_MODE ? status_10g.wis.tx_fault ? "Yes" : "No" : "-");
    printf ("%-12s %-12s %-16s %-12s %-12s\n", "Line PCS:", status_10g.pcs.rx_link ? "Yes" : "No",
            status_10g.pcs.link_down ? "Yes" : "No", status_10g.pcs.rx_fault ? "Yes" : "No", status_10g.pcs.tx_fault ? "Yes" : "No");
    printf ("%-12s %-12s %-16s %-12s %-12s\n", "Host PCS:", status_10g.hpcs.rx_link ? "Yes" : "No",
            status_10g.hpcs.link_down ? "Yes" : "No", status_10g.hpcs.rx_fault ? "Yes" : "No", status_10g.hpcs.tx_fault ? "Yes" : "No");
    printf ("%-12s %-12s %-16s %-12s %-12s\n", "XAUI Status:", status_10g.xs.rx_link ? "Yes" : "No",
            status_10g.xs.link_down ? "Yes" : "No", status_10g.xs.rx_fault ? "Yes" : "No", status_10g.xs.tx_fault ? "Yes" : "No");
    printf ("%-12s %-12s \n", "Line 1G PCS:", status_10g.lpcs_1g ? "Yes" : "No");
    printf ("%-12s %-12s \n", "Host 1G PCS:", status_10g.hpcs_1g ? "Yes" : "No");
    printf ("%-12s %-12s \n", "Link UP:", status_10g.status ? "Yes" : "No");
    printf ("%-12s %-12s \n", "Block lock:", status_10g.block_lock ? "Yes" : "No");
    printf ("%-12s %-12s \n", "LOPC status:", status_10g.lopc_stat ? "Yes" : "No");

    if (vtss_phy_10g_cnt_get(NULL, port_no, &cnt) != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_cnt_get failed, port %d", port_no);
        return MEPA_RC_ERROR;
    }
    printf ("\n");
    printf ("PCS counters:\n");
    printf ("%-20s %-12s\n", "  Block_latched:", cnt.pcs.block_lock_latched ? "Yes" : "No");
    printf ("%-20s %-12s\n", "  High_ber_latched:", cnt.pcs.high_ber_latched ? "Yes" : "No");
    printf ("%-20s %-12d\n", "  Ber_cnt:", cnt.pcs.ber_cnt);
    printf ("%-20s %-12d\n", "  Err_blk_cnt:", cnt.pcs.err_blk_cnt);
    printf ("\n");
    
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
    switch(phy_mode)
    {
        case PHY_MODE_10G_LAN:   /* 0=MODE_10GLAN */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_LAN_MODE;
            appl_malibu_conf.speed = MESA_SPEED_10G;

            // Assuming SR/LR limiting module on the line side and long traces (> 10 in)
            // or backplane on the host side
            // oper_mode.h_media = MEPA_MEDIA_TYPE_KR_SC;
            // oper_mode.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_KR_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            break;

        case PHY_MODE_1G_LAN: /* 1=MODE_1GLAN */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_1G_MODE;
            appl_malibu_conf.speed = MESA_SPEED_1G;

            // Always use these media settings for 1G LAN data rates
            // oper_mode.h_media = MEPA_MEDIA_TYPE_SR2_SC;
            // oper_mode.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;

            break;

        case PHY_MODE_10G_WAN: /* 2=MODE_10GWAN */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_WAN_MODE;
            appl_malibu_conf.speed = MESA_SPEED_10G;

            // Assuming SR/LR limiting module on the line side and long traces (> 10 in)
            // or backplane on the host side
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_KR_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;

            break;

        case PHY_MODE_10G_RPTR:   /* 0=MODE_10GRPTR */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_REPEATER_MODE;
            // oper_mode.rate = VTSS_RPTR_RATE_10_3125;     // No MEPA type for repeater mode rate as of 2025.12
            appl_malibu_conf.speed = MESA_SPEED_10G;

            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_DAC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            break;

        case PHY_MODE_1G_RPTR:   /* 0=MODE_1GRPTR */
            // MJ Addition 2024-08-09. uncomment the two lines below when testing with a 1G Cu SFP
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_REPEATER_MODE;
            // oper_mode.rate = VTSS_RPTR_RATE_1_25;       // No MEPA type for repeater mode rate as of 2025.12
            appl_malibu_conf.speed = MESA_SPEED_1G;

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
    // See VSC8258EV Schematics in the product page.
    // https://www.microchip.com/en-us/product/vsc8258 > Design Resources
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
    if (*port_no >= APPL_BASE_PORT && *port_no < APPL_PORT_COUNT)
    {
        return true;
    }

    printf ("Error - Invalid Port Number: %d  Valid Range: %d - %d \n",
            *port_no, APPL_BASE_PORT, (APPL_PORT_COUNT-1));

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
    unsigned int i = 0, j = 0;
    int phy_mode = 0;
    char value_str[255] = {0};
    char command[255] = {0};
    char port_no_str[255] = {0};
    uint32_t val32 = 0;
    uint16_t value = 0;
    mepa_port_no_t port_no=0;

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
    /*                       MEPA INIT                        */
    /* ****************************************************** */
    // Code below is based on sw-mepa/board-configs/src/meba_generic.c > meba_phy_driver_init()

    // Another reference is the KB Article below.
    // https://support.microchip.com/s/article/Creating-MEPA-API-PHY-Instances-for-VSC-PHY

    // Loop through all ports (PHYs) in the system.
    for (i = 0; i < APPL_PORT_COUNT; ++i)
    {
        // Configure the board configuration (note temporary life time).
        memset(&appl_board_conf, 0, sizeof(appl_board_conf));
        appl_board_conf.numeric_handle = i;

        // Fill application specific data in the context area. This is likely to
        // include bus instance, MDIO address etc.
        memset(&appl_callout_ctx[i], 0, sizeof(appl_callout_ctx[i]));
        appl_callout_ctx[i].port_no = i;
        
        // Create the MEPA devices
        // Real applications needs to check for error as well.
        appl_malibu_device[i] = mepa_create(&appl_rpi_spi,
                                            &appl_callout_ctx[i],
                                            &appl_board_conf);
    }

    // Link to base port since we're dealing with a quad-port PHY.
    // For VSC8258, we can just link directly to Port 0.
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

    /* ****************************************************** */
    /*                       SPI IO Test                      */
    /* ****************************************************** */
    // Run SPI IO Test after mepa_create()
    printf("appl_malibu_spi_io_test()\r\n");
    appl_malibu_spi_io_test(&appl_rpi_spi, appl_callout_ctx, APPL_PORT_COUNT);
    printf("\n");
    
    /* ****************************************************** */
    /*                       PHY Bring-up                     */
    /* ****************************************************** */    
    printf("Configuring Operating MODE for ALL Ports; 0=MODE_10GLAN; 1=MODE_1GLAN; 2=MODE_10GWAN; 3=MODE_10GRPTR; 4=MODE_1GRPTR\n");
    printf("Enter Oper_MODE (0/1/2/3/4): ");
    memset(&value_str[0], 0, sizeof(value_str));
    scanf("%s", &value_str[0]);
    phy_mode = atoi(value_str);
    printf("\n");

    switch (phy_mode)
    {
        case PHY_MODE_10G_LAN:
            printf ("Operating MODE for ALL Ports: 0=MODE_10GLAN\n");
            break;
        case PHY_MODE_1G_LAN:
            printf ("Operating MODE for ALL Ports: 1=MODE_1GLAN\n");
            break;
        case PHY_MODE_10G_WAN:
            printf ("Operating MODE for ALL Ports: 2=MODE_10GWAN\n");
            break;
        case PHY_MODE_10G_RPTR:
            printf ("Operating MODE for ALL Ports: 3=MODE_10G_RPTR\n");
            break;
        case PHY_MODE_1G_RPTR:
            printf ("Operating MODE for ALL Ports: 4=MODE_1G_RPTR\n");
            break;
        default:
            printf ("Operating MODE ALL Ports INVALID, Setting 10G_LAN Mode \n");
            phy_mode = 0;
    }

    // Reset and configure PHY ports
    // See sw-mepa/mepa/docs/linkup_config.adoc#reset-configuration
    // mepa_reset() is called before mepa_conf_set()
    for(i = 0; i < APPL_PORT_COUNT; i++)
    {
        printf("Resetting port %d\n", i);
        rc = appl_mepa_reset_phy(i);
        printf("appl_mepa_reset_phy: rc: %d\n\n", rc);
    }

    // Set the PHY Configuration for each port.
    // Reference: vtss_appl_10g_phy_malibu.c > 1st for loop of main()
    // Also, refer to board-configs/src/sparx5/meba.c > malibu_init()
    for(i = 0; i < APPL_PORT_COUNT; i++)
    {
        printf("Configuring port %d\n", i);
        rc = appl_mepa_phy_init(i, phy_mode);
        printf("appl_mepa_phy_init: rc: %d\n\n", rc);
    }

    // Wait for PHY to stabilize - around 1s
    usleep(1000000);

    /* ****************************************************** */
    /*                 Additional Features                    */
    /* ****************************************************** */ 
    
    // Check PHY capability of Port 0
    mepa_phy_info_t appl_phy_info;
    memset(&appl_phy_info, 0, sizeof(appl_phy_info));
    rc = mepa_phy_info_get(appl_malibu_device[0], &appl_phy_info);
    printf("mepa_phy_info_get: rc: %d, PHY Cap: 0x%X, Part: 0x%X\r\n", rc, appl_phy_info.cap, appl_phy_info.part_number);

    fflush(stdout);

    /* ****************************************************** */
    /*                       Main Demo                        */
    /* ****************************************************** */ 
    // Reference: vtss_appl_10g_phy_malibu.c
    while(1)
    {
        {
            printf (" *************************************\n");
            printf (" The following commands are supported:\n");
            printf (" dump <port_no> - Dump PHY, PHY_TS, MACsec, or ALL register groups for Port \n");
            printf (" ----------------------------------------  |  ------------------------------------------------------- \n");
            printf (" spird    <port_no> <dev> <addr>           |  spiwr       <port_no> <dev> <addr> <value> - Value MUST be in hex\n");
            printf (" get_lmedia <port_no> - Get Line Media I/F |  set_lmedia  <port_no> - Set Line Media I/F for port \n");
            printf (" get_hmedia <port_no> - Get Host Media I/F |  set_hmedia  <port_no> - Set Host Media I/F for port \n");
            printf (" ----------------------------------------  |  ------------------------------------------------------- \n");
            // printf (" 10g_kr   <port_no> - 10G Base KR          |  synce       <port_no> - Sync-E Config   \n");
            // printf (" prbs     <port_no> - PRBS                 |  obuf        <port_no> - Output Buf Control  \n");
            printf (" status   <port_no> - Rtn PHY Link status  |  vscope      <port_no> - Config for VSCOPE FAST/FULL SCAN \n");
            // printf (" lpback   <port_no> - Set PHY Loopback     |  sfpdump     <port_no> <i2c_addr> - Dump SFP register connected to port_no\n");
            // printf (" int10g   <port_no> - Set 10g Interuppts   |  poll10g     <port_no>                                    \n");
            // printf (" ex_int   <port_no> - Set Ext Interuppts   |  ex_poll     <port_no>                                    \n");
            // printf (" ts_int   <port_no> - Set TS Interuppts    |  ts_poll     <port_no>                                    \n");
            // printf (" macsec   <port_no> - MACSEC Block config  |  1588        <port_no> - 1588 Block Config   \n");
            // printf (" dbgdump  <port_no> - Reg Dump             |                                                \n");

            printf ("\n exit - Exit Program \n");
            printf (" \n");
            printf ("> ");
        }

        rc = scanf("%s", &command[0]);

        if (strcmp(command, "status")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            appl_mepa_status_get(port_no);

            continue;
        }
        else if (strcmp(command, "exit")  == 0)
        {
            break;
        }
        else if (strcmp(command, "dump")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            // Get Debug info. See https://microchip.my.site.com/s/article/Dumping-VSC-PHY-Registers
            mepa_debug_info_t mepa_dbg;
            memset (&mepa_dbg, 0, sizeof(mepa_debug_info_t));
            mepa_dbg.full = 1;
            mepa_dbg.clear = 1;
            mepa_dbg.vml_format = 0;

            mepa_dbg.layer = MEPA_DEBUG_LAYER_ALL;   // All Layers or CIL or AIL
                                                     // MEPA_DEBUG_LAYER_CIL or MEPA_DEBUG_LAYER_AIL

            // Ask user what register group to dump.
            printf ("Enter Register Group: all/phy/ts/macsec \n");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);

            if(!strcmp(value_str, "all"))
            {
                mepa_dbg.group = MEPA_DEBUG_GROUP_ALL;
                printf("Debug group set to MEPA_DEBUG_GROUP_ALL!\n");
            }
            else if(!strcmp(value_str, "phy"))
            {
                mepa_dbg.group = MEPA_DEBUG_GROUP_PHY;
                printf("Debug group set to MEPA_DEBUG_GROUP_PHY!\n");
            }
            else if(!strcmp(value_str, "ts"))
            {
                mepa_dbg.group = MEPA_DEBUG_GROUP_PHY_TS;
                printf("Debug group set to MEPA_DEBUG_GROUP_PHY_TS!\n");
            }
            else if(!strcmp(value_str, "macsec"))
            {
                mepa_dbg.group = MEPA_DEBUG_GROUP_MACSEC;
                printf("Debug group set to MEPA_DEBUG_GROUP_MACSEC!\n");
            }
            else
            {
                printf("Invalid debug group!\n");
                // Skip printing debug info
                continue;
            }

            printf("\n==========================================================\n");
            printf("Full Reg Dump for Port %d\n", port_no);
            // Wait for 1s before the reg dump is printed
            usleep(1000000);

            rc = mepa_debug_info_dump(appl_malibu_device[port_no], (mesa_debug_printf_t) printf, &mepa_dbg);
            continue;
        }
        else if (strcmp(command, "spird")  == 0)
        {
            uint32_t val32 = 0;
            uint16_t dev = 0;
            uint16_t addr = 0;

            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            printf ("Enter Dev/MMD (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            dev = strtol(value_str, NULL, 16);

            printf ("Enter Addr (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            addr = strtol(value_str, NULL, 16);

            // Note: Since there are no MEPA functions that will allow PHY register access specifically through
            // SPI, we call the SPI callout directly through the wrapper function below.
            appl_spi_read_write(&appl_rpi_spi, &appl_callout_ctx[port_no], dev, addr, &val32, false);
            printf("\nRegister 0x%X:0x%04X: 0x%X\n\n", dev, addr, val32);

            // Note: calling mepa_clause45_read() with only SPI Callouts provided will resolve to SPI calls, but the API
            // function itself will truncate the read value to 16-bits as it only accepts a pointer to a uint16_t type.
            continue;
        }
        else if (strcmp(command, "spiwr")  == 0)
        {
            uint32_t val32 = 0;
            uint16_t dev = 0;
            uint16_t addr = 0;

            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            printf ("Enter Dev (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            dev = strtol(value_str, NULL, 16);

            printf ("Enter Dev (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            addr = strtol(value_str, NULL, 16);

            printf ("Enter Value (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            val32 = strtol(value_str, NULL, 16);

            // board->inst->init_conf.spi_32bit_read_write(board->inst, port_no, SPI_WR, dev, addr, &val32);
            // Note: Since there are no MEPA functions that will allow PHY register access specifically through
            // SPI, we call the SPI callout directly through the wrapper function below.
            appl_spi_read_write(&appl_rpi_spi, &appl_callout_ctx[port_no], dev, addr, &val32, true);
            printf("\nWrote 0x%X to Register 0x%X:0x%04X\n\n", val32, dev, addr);

            continue;
        }
        else if (strcmp(command, "get_lmedia")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            rc = mepa_conf_get(appl_malibu_device[port_no], &appl_malibu_conf);
            if (rc != MEPA_RC_OK)
            {
                T_E("%s: mepa_conf_get() error %d\n", __func__, rc);
                return rc;
            }

            printf("Port %d Line Media type: %s\n", port_no, mediatype2txt[appl_malibu_conf.conf_10g.l_media]);
            continue;
        }
        else if (strcmp(command, "get_hmedia")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            rc = mepa_conf_get(appl_malibu_device[port_no], &appl_malibu_conf);
            if (rc != MEPA_RC_OK)
            {
                T_E("%s: mepa_conf_get() error %d\n", __func__, rc);
                return rc;
            }

            printf("Port %d Host Media type: %s\n", port_no, mediatype2txt[appl_malibu_conf.conf_10g.h_media]);
            continue;;
        }
            continue;
        }
    }

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