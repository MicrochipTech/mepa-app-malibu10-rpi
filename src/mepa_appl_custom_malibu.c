/* 
 * Custom program for VSC8258 using MEPA.
 * Code is loosely-based on mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c
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
#include <ctype.h>      // For isprint(), used in "sfpdump" command

// MEPA includes
#include "microchip/ethernet/phy/api.h"
#include "microchip/ethernet/board/api.h"
#include "vtss_phy_10g_api.h"       // Included for the VTSS 10G PHY APIs.

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
#define PHY_MODE_1G_LAN_CL37    1
#define PHY_MODE_1G_LAN         2
#define PHY_MODE_10G_WAN        3
#define PHY_MODE_10G_RPTR       4
#define PHY_MODE_1G_RPTR        5

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

// Reference: mesa/meba/src/sparx5/meba.c
typedef struct {
    vtss_gpio_10g_no_t gpio_tx_dis;      /* Tx Disable GPIO number */
    vtss_gpio_10g_no_t gpio_aggr_int;    /* Aggregated Interrupt-0 GPIO number */
    vtss_gpio_10g_no_t gpio_i2c_clk;     /* GPIO Pin selection as I2C_CLK for I2C
                                            communication with SFP  */
    vtss_gpio_10g_no_t gpio_i2c_dat;     /* GPIO Pin selection as I2C_DATA for I2C
                                            communication with SFP */
    vtss_gpio_10g_no_t gpio_virtual;     /* Per port Virtual GPIO number,for internal GPIO usage */
    vtss_gpio_10g_no_t gpio_sfp_mod_det; /* GPIO Pin selection as SFP module detect */
    vtss_gpio_10g_no_t gpio_tx_fault;    /* GPIO Pin for TX_FAULT */
    vtss_gpio_10g_no_t gpio_rx_los;      /* GPIO Pin for RX_LOS */
    uint32_t           aggr_intrpt;      /* Channel interrupt bitmask */
} appl_malibu_gpio_port_map_t;

// Reference: mesa/meba/src/sparx5/meba.c
static const appl_malibu_gpio_port_map_t malibu_gpio_map[] = {
    // PHY CH0
    {
        .gpio_tx_dis        = 4,
        .gpio_aggr_int      = 34,
        .gpio_i2c_clk       = 2,
        .gpio_i2c_dat       = 3,
        .gpio_virtual       = 0,
        .gpio_sfp_mod_det   = 1,
        .gpio_tx_fault      = 5,
        .gpio_rx_los        = 6,
        .aggr_intrpt        = ((1 << VTSS_10G_GPIO_AGGR_INTRPT_CH0_INTR0_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_0_INTR0_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_1_INTR0_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_GPIO_INTR_EN)),
    },
    // PHY CH1
    {
        .gpio_tx_dis        = 12, 
        .gpio_aggr_int      = 34,
        .gpio_i2c_clk       = 10,
        .gpio_i2c_dat       = 11,
        .gpio_virtual       = 0,
        .gpio_sfp_mod_det   = 9,
        .gpio_tx_fault      = 13,
        .gpio_rx_los        = 14,
        .aggr_intrpt        = ((1 << VTSS_10G_GPIO_AGGR_INTRPT_CH1_INTR0_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_0_INTR1_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_1_INTR1_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_GPIO_INTR_EN)),
     },    
     // PHY CH2
    {
        .gpio_tx_dis        = 20,
        .gpio_aggr_int      = 34,
        .gpio_i2c_clk       = 18,
        .gpio_i2c_dat       = 19,
        .gpio_virtual       = 0,
        .gpio_sfp_mod_det   = 17,
        .gpio_tx_fault      = 21,
        .gpio_rx_los        = 22,
        .aggr_intrpt        = ((1 << VTSS_10G_GPIO_AGGR_INTRPT_CH2_INTR0_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_0_INTR2_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_1_INTR2_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_GPIO_INTR_EN)),
     },
     // PHY CH3
    {
        .gpio_tx_dis        = 28,
        .gpio_aggr_int      = 34,
        .gpio_i2c_clk       = 26,
        .gpio_i2c_dat       = 27,
        .gpio_virtual       = 0,
        .gpio_sfp_mod_det   = 25,
        .gpio_tx_fault      = 29,
        .gpio_rx_los        = 30,
        .aggr_intrpt        = ((1 << VTSS_10G_GPIO_AGGR_INTRPT_CH3_INTR0_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_0_INTR3_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_IP1588_1_INTR3_EN) |
                               (1 << VTSS_10G_GPIO_AGGR_INTRPT_GPIO_INTR_EN)),
     },
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

int appl_mepa_set_oper_mode(void)
{
    int phy_mode = 0;
    char value_str[255] = {0};

    printf("Configuring Operating MODE for PHY Ports; 0=MODE_10GLAN; 1=MODE_1GLAN_CL37; 2=MODE_1G_LAN; 3=MODE_10GWAN; 4=MODE_10GRPTR; 5=MODE_1GRPTR\n");
    printf("Enter Oper_MODE (0/1/2/3/4/5): ");
    memset(&value_str[0], 0, sizeof(value_str));
    scanf("%s", &value_str[0]);
    phy_mode = atoi(value_str);
    printf("\n");

    switch (phy_mode)
    {
        case PHY_MODE_10G_LAN:
            printf ("Operating MODE for ALL Ports: 0=MODE_10GLAN\n");
            break;
        case PHY_MODE_1G_LAN_CL37:
            printf ("Operating MODE for ALL Ports: 1=MODE_1GLAN_CL37\n");
            break;
        case PHY_MODE_1G_LAN:
            printf ("Operating MODE for ALL Ports: 2=MODE_1GLAN\n");
            break;
        case PHY_MODE_10G_WAN:
            printf ("Operating MODE for ALL Ports: 3=MODE_10GWAN\n");
            break;
        case PHY_MODE_10G_RPTR:
            printf ("Operating MODE for ALL Ports: 4=MODE_10G_RPTR\n");
            break;
        case PHY_MODE_1G_RPTR:
            printf ("Operating MODE for ALL Ports: 5=MODE_1G_RPTR\n");
            break;
        default:
            printf ("Operating MODE ALL Ports INVALID, Setting 10G_LAN Mode \n");
            phy_mode = 0;
            break;
    }

    return phy_mode;
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

    // Print current operating mode!
    printf ("%-12s %-12s \n", "PHY OpMode:", (oper_mode == MEPA_PHY_LAN_MODE) ? "MEPA_PHY_LAN_MODE" : 
                                               (oper_mode == MEPA_PHY_1G_MODE)  ?  "MEPA_PHY_1G_MODE" :
                                               (oper_mode == MEPA_PHY_WAN_MODE)  ?  "MEPA_PHY_WAN_MODE" :
                                               (oper_mode == MEPA_PHY_REPEATER_MODE)  ?  "MEPA_PHY_REPEATER_MODE" :
                                               "Undefined!");

    // Show CL37 status
    if(oper_mode == MEPA_PHY_1G_MODE)
    {
        printf ("%-12s %-12s \n", "CL37 ANEG:", ((appl_malibu_conf.speed == MESA_SPEED_AUTO)) ? "Enabled" : "Disabled");
    }
    // Show repeater speed
    if(oper_mode == MEPA_PHY_REPEATER_MODE)
    {
        printf ("%-12s %-12s \n", "PHY Speed:", ((appl_malibu_conf.speed == MESA_SPEED_10G)) ? "10G" : "1G");
    }

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

        case PHY_MODE_1G_LAN: /* 1=MODE_1GLAN_CL37; 2=MODE_1G_LAN */
        case PHY_MODE_1G_LAN_CL37:
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_1G_MODE;
            
            // Use MESA_SPEED_1G to disable CL37 ANEG!
            // Use MESA_SPEED_AUTO to enable CL37 ANEG!
            appl_malibu_conf.speed = (phy_mode == PHY_MODE_1G_LAN)? MESA_SPEED_1G : MESA_SPEED_AUTO;

            // Always use these media settings for 1G LAN data rates
            // oper_mode.h_media = MEPA_MEDIA_TYPE_SR2_SC;
            // oper_mode.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_SR2_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;

            break;

        case PHY_MODE_10G_WAN: /* 3=MODE_10GWAN */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_WAN_MODE;
            appl_malibu_conf.speed = MESA_SPEED_10G;

            // Assuming SR/LR limiting module on the line side and long traces (> 10 in)
            // or backplane on the host side
            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_KR_SC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;

            break;

        case PHY_MODE_10G_RPTR:   /* 4=MODE_10G_RPTR */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_REPEATER_MODE;
            // oper_mode.rate = VTSS_RPTR_RATE_10_3125;     // No MEPA type for repeater mode rate as of 2025.12
            appl_malibu_conf.speed = MESA_SPEED_10G;

            appl_malibu_conf.conf_10g.h_media = MEPA_MEDIA_TYPE_DAC;
            appl_malibu_conf.conf_10g.l_media = MEPA_MEDIA_TYPE_SR2_SC;
            break;

        case PHY_MODE_1G_RPTR:   /* 5=MODE_1G_RPTR */
            appl_malibu_conf.conf_10g.oper_mode = MEPA_PHY_REPEATER_MODE;
            // oper_mode.rate = VTSS_RPTR_RATE_1_25;       // No MEPA type for repeater mode rate as of 2025.12
            #if 1
                // Note: due to lack of a vtss_rptr_rate_t struct member in the MEPA type phy10g_conf_t,
                // we can't set the Repeater mode rate through MEPA.
                // For now, notify the user!
                printf("\nNOTE: It is currently not possible to set the Repeater Mode rate through MEPA.\n");
                printf("Setting speed to MESA_SPEED_10G for now.\n");
                appl_malibu_conf.speed = MESA_SPEED_10G;
            #endif

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
    appl_malibu_conf.conf_10g.is_host_wan = (phy_mode == PHY_MODE_10G_WAN)? true : false;
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

bool appl_malibu_vscope_cntrl(mepa_port_no_t port_no)
{
    vtss_rc rc = VTSS_RC_OK;
    int i=0;
    int j=0;
    char value_str[255] = {0};
    vtss_phy_10g_vscope_conf_t vscope_conf;             // FAST SCAN
    vtss_phy_10g_vscope_scan_conf_t vscope_scan_conf;   // FULL SCAN parameters -- does not apply to FAST SCAN
    vtss_phy_10g_vscope_scan_status_t scan_status;

    // NOTE! VSCOPE is only available in 10G Modes!
    // Refer to the Datasheet, Section 3.2.2
    // https://ww1.microchip.com/downloads/en/DeviceDoc/VMDS-10487.pdf
    mepa_conf_get(appl_malibu_device[port_no], &appl_malibu_conf);
    mepa_port_speed_t speed = appl_malibu_conf.speed;
    if(speed != MESA_SPEED_10G)
    {
        printf("\nNOTE: The VScope input signal monitoring integrated circuit feature is only available in the 10G operation mode.\n");
        return false;
    }
    

    // Initialize all structs to 0
    memset(&vscope_conf, 0, sizeof(vtss_phy_10g_vscope_conf_t));
    memset(&vscope_scan_conf, 0, sizeof(vtss_phy_10g_vscope_scan_conf_t));
    memset(&scan_status, 0, sizeof(vtss_phy_10g_vscope_scan_status_t));

    // There are 2 options:
    // -- Fast Scan --> VTSS_PHY_10G_FAST_SCAN
    // -- Full Vscope --> VTSS_PHY_10G_FULL_SCAN
    printf ("Configure VSCOPE:  0=FULL SCAN   1=FAST SCAN  \n");
    memset (&value_str[0], 0, sizeof(value_str));
    scanf("%s", &value_str[0]);

    if (value_str [0] == '1')
    {
        vscope_conf.scan_type = VTSS_PHY_10G_FAST_SCAN;
    }
    else
    {
        vscope_conf.scan_type = VTSS_PHY_10G_FULL_SCAN;
    }

    vscope_conf.line = TRUE;
    vscope_conf.enable = TRUE;
    vscope_conf.error_thres = 3;

    if (vscope_conf.scan_type == VTSS_PHY_10G_FAST_SCAN)
    {
        printf("VSCOPE Config:  VTSS_PHY_10G_FAST_SCAN !\n");
    }
    else if (vscope_conf.scan_type == VTSS_PHY_10G_FULL_SCAN)
    {
        printf("VSCOPE Config:  VTSS_PHY_10G_FULL_SCAN !\n");
    }
    else
    {
        printf("VSCOPE Config:  ERROR VSCOPE MISCONFIGURED !\n");
    }

    /* x_start can be anything from 0 (LHS of the sampling window) to 127, but do not recommend than greater than 20% of U.I.  Generally no
    reason to use x_start other than 0.
    */
    /* y_axis has a center point at count 31-32 (it's 6 bit counter).  Generally +/-12 is sufficient for a typical eye visulation (efficient run time)
    */
    /* x_incr and y_incr = 0 is to collect all points.  Increment by n to skip every nth point.
    */
    /* ber is TBD for definition - unclear whether e.g. value 5 means 10^-5 i.e. an undefined confidence for the BER value input.
    note the GDC sample count is unknown how implemented in the digital HW.
    */
    /* note overall:  the vscope_scan_conf arguments are all directly written to SD10G registers.  I.e. digitial PMA performs this processing
    */
    /* notes about results:  the error_free_y returned from FAST_SCAN is "inner bounds".  the amp_range returned from FAST_SCAN is "outer bound" i.e. the
    maximum  amplitude swing observed at receiver.  However the statistical confidence of the reported "inner_bound" depends on the GDC sampling/count
    for the ber parameter passed in [a relationship which is unknown at this time w/o GDC help].
    */

    vscope_scan_conf.x_start = 0;
    vscope_scan_conf.y_start = 20;
    vscope_scan_conf.x_count = 127;
    vscope_scan_conf.y_count = 25;
    vscope_scan_conf.x_incr = 0;
    vscope_scan_conf.y_incr = 0;
    vscope_scan_conf.ber = 5;

    scan_status.scan_conf = vscope_scan_conf;

    if (vtss_phy_10g_vscope_conf_set(NULL, port_no, &vscope_conf) != VTSS_RC_OK)
    {
        printf("Malibu Error setting 10G Serdes VScope configuration on Port %d \n", port_no);
    }
    else
    {
        // Get the results
        printf("\nMalibu VSCOPE successfully configured\n");
        if((rc = vtss_phy_10g_vscope_scan_status_get(NULL, port_no, &scan_status)) != VTSS_RC_OK)
        {
            printf("Malibu Error Fetching VSCOPE data\n");
        }
        else
        {
            if (vscope_conf.scan_type == VTSS_PHY_10G_FAST_SCAN)
            {
                printf("FAST SCAN error free x points: %d\n", scan_status.error_free_x);
                printf("FAST SCAN error free y points: %d\n", scan_status.error_free_y);
                printf("FAST SCAN amplitude range: %d\n", scan_status.amp_range);
            }
            else if (vscope_conf.scan_type == VTSS_PHY_10G_FULL_SCAN)
            {
                // Create a data eye on a screen --> "X" for no errors, "." for errors
                printf("VScope FULL SCAN for Port %d are as follows:\n", port_no);
                for(j=vscope_scan_conf.y_start; j < (vscope_scan_conf.y_start + vscope_scan_conf.y_count); j = j + vscope_scan_conf.y_incr + 1)
                {
                    if (scan_status.errors[0][j] == 0)
                        j++;

                    printf("\n");

                    for(i=vscope_scan_conf.x_start; i < (vscope_scan_conf.x_start + vscope_scan_conf.x_count) ;i = i + vscope_scan_conf.x_incr + 1)
                    {
                        if(scan_status.errors[i][j] == 0)
                            printf("X");
                        else
                            printf(".");
                    }
                }

                printf("\n");
            }
            else
            {
                printf("Scan Type not supported\n");
            }
        }
    }
    
    return true;
}

mepa_rc appl_malibu_gpio_conf(mepa_port_no_t port_no)
{
    vtss_rc rc = VTSS_RC_OK;
    vtss_gpio_10g_gpio_mode_t gpio_conf = {};
    const appl_malibu_gpio_port_map_t *gmap = &malibu_gpio_map[port_no];

    // Initialize Malibu PHY GPIOs for VSC8258EV
    // See the VSC8258EV schematics in the Product page: https://www.microchip.com/en-us/product/vsc8258
    //      Scroll down to find "VSC8256/VSC8257/VSC8258 Evaluation Board Design Files"
    // NOTE: main reference for this function is meba/src/sparx5/meba.c > malibu_gpio_conf()
    // -------------
    // Also note that as of SW-MEPA 2025.12, no implementation of mepa_gpio_mode_set() exists for Malibu PHYs
    // yet, so we call vtss APIs directly.

    /* ********************************************************** */
    // GPIO Input functionality
    /* ********************************************************** */
    // SFP_MOD_DET
    rc = vtss_phy_10g_gpio_mode_get(NULL, port_no, gmap->gpio_sfp_mod_det, &gpio_conf);
    if(rc == VTSS_RC_OK)
    {
        gpio_conf.mode = VTSS_10G_PHY_GPIO_IN;
        gpio_conf.input = VTSS_10G_GPIO_INPUT_NONE;
        rc = vtss_phy_10g_gpio_mode_set(NULL, port_no, gmap->gpio_sfp_mod_det, &gpio_conf);
    }

    if(rc != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_gpio_mode_set, port %d, gpio %d, mode: INPUT (SFP_MOD_DET)\n", port_no, gmap->gpio_sfp_mod_det);
        return rc;
    }
    else
    {
        printf("Malibu GPIO Input: SFP_MOD_DET configuration for port %d, gpio %d \n", port_no, gmap->gpio_sfp_mod_det);
    }

    // RX_LOS
    rc = vtss_phy_10g_gpio_mode_get(NULL, port_no, gmap->gpio_rx_los, &gpio_conf);
    if(rc == VTSS_RC_OK)
    {
        gpio_conf.mode = VTSS_10G_PHY_GPIO_IN;
        gpio_conf.input = VTSS_10G_GPIO_INPUT_LINE_LOPC;
        rc = vtss_phy_10g_gpio_mode_set(NULL, port_no, gmap->gpio_rx_los, &gpio_conf);
    }

    if(rc != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_gpio_mode_set, port %d, gpio %d, mode: INPUT (RX_LOS)\n", port_no, gmap->gpio_rx_los);
        return rc;
    }
    else
    {
        printf("Malibu GPIO Input: RX_LOS configuration for port %d, gpio %d \n", port_no, gmap->gpio_rx_los);
    }

    // TX_FAULT
    rc = vtss_phy_10g_gpio_mode_get(NULL, port_no, gmap->gpio_tx_fault, &gpio_conf);
    if(rc == VTSS_RC_OK)
    {
        gpio_conf.mode = VTSS_10G_PHY_GPIO_IN;
        gpio_conf.input = VTSS_10G_GPIO_INPUT_NONE;
        rc = vtss_phy_10g_gpio_mode_set(NULL, port_no, gmap->gpio_tx_fault, &gpio_conf);
    }

    if(rc != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_gpio_mode_set, port %d, gpio %d, mode: INPUT (TX_FAULT)\n", port_no, gmap->gpio_tx_fault);
        return rc;
    }
    else
    {
        printf("Malibu GPIO Input: TX_FAULT configuration for port %d, gpio %d \n", port_no, gmap->gpio_tx_fault);
    }

    /* ********************************************************** */
    // GPIO Output functionality
    /* ********************************************************** */
    // TX_DIS
    rc = vtss_phy_10g_gpio_mode_get(NULL, port_no, gmap->gpio_tx_dis, &gpio_conf);
    if(rc == VTSS_RC_OK)
    {
        gpio_conf.mode = VTSS_10G_PHY_GPIO_DRIVE_LOW;
        gpio_conf.in_sig = VTSS_10G_GPIO_INTR_SGNL_NONE;
        rc = vtss_phy_10g_gpio_mode_set(NULL, port_no, gmap->gpio_tx_dis, &gpio_conf);
    }

    if(rc != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_gpio_mode_set, port %d, gpio %d, mode: DRIVE_LOW (TX_DISABLE)\n", port_no, gmap->gpio_tx_dis);
        return rc;
    }
    else
    {
        printf("Malibu GPIO Output: Driving LOW configuration for port %d, gpio %d (TX_DISABLE)\n", port_no, gmap->gpio_tx_dis);
    }

    /* ********************************************************** */
    // GPIO I2C Pins
    /* ********************************************************** */
    // I2C_DAT
    rc = vtss_phy_10g_gpio_mode_get(NULL, port_no, gmap->gpio_i2c_dat, &gpio_conf);
    if(rc == VTSS_RC_OK)
    {
        printf("Malibu port %d I2C DAT pin %d\n", port_no, gmap->gpio_i2c_dat);
        gpio_conf.mode = VTSS_10G_PHY_GPIO_OUT;
        gpio_conf.in_sig = VTSS_10G_GPIO_INTR_SGNL_I2C_MSTR_DATA_OUT;
        gpio_conf.p_gpio = 0;   // Route the internal signal "SDA Output" to GPIO0_OUT
        rc = vtss_phy_10g_gpio_mode_set(NULL, port_no, gmap->gpio_i2c_dat, &gpio_conf);
    }

    if(rc != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_gpio_mode_set, port %d, gpio %d, mode: OUTPUT (I2C SDA)\n", port_no, gmap->gpio_i2c_dat);
        return rc;
    }
    else
    {
        printf("Malibu GPIO Output: I2C Master DATA configuration for port %d, gpio %d\n", port_no, gmap->gpio_i2c_dat);
    }

    // I2C_CLK
    rc = vtss_phy_10g_gpio_mode_get(NULL, port_no, gmap->gpio_i2c_clk, &gpio_conf);
    if(rc == VTSS_RC_OK)
    {
        printf("Malibu port %d I2C CLK pin %d\n", port_no, gmap->gpio_i2c_clk);
        gpio_conf.mode = VTSS_10G_PHY_GPIO_OUT;
        gpio_conf.in_sig = VTSS_10G_GPIO_INTR_SGNL_I2C_MSTR_CLK_OUT;
        gpio_conf.p_gpio = 1;   // Route the internal signal "SCL Output" to GPIO1_OUT
        rc = vtss_phy_10g_gpio_mode_set(NULL, port_no, gmap->gpio_i2c_clk, &gpio_conf);
    }

    if(rc != VTSS_RC_OK)
    {
        T_E("vtss_phy_10g_gpio_mode_set, port %d, gpio %d, mode: OUTPUT (I2C SCL)\n", port_no, gmap->gpio_i2c_clk);
        return rc;
    }
    else
    {
        printf("Malibu GPIO Output: I2C Master CLK configuration for port %d, gpio %d\n", port_no, gmap->gpio_i2c_clk);
    }

    /* ********************************************************** */
    // Test I2C Access to SFP+ ports!
    /* ********************************************************** */

    // Now call the read or write function with the relevant address and data
    // MEPA has I2C functions for Malibu PHYs, so we will use them instead of vtss_phy_10g_i2c_read/write().
    // Also, keep in mind the following comment from vtss.c:
    // ------------------------
    /* In 10G Malibu PHY's the arguments i2c_dev_addr and word_access of API i2c_read/write have no functionality,
    * so the value for these arguments can be given as zero
    */
    // ------------------------
    // The 'cnt' argument is also not used, so we can just pass '0'
    u16 address = 0x0;
    u8 data;

    // Skip I2C access for ports 0 and 1 (which don't connect to SFP+ ports on VSC8258EV)
    if(port_no < 2)
    {
        return rc;
    }

    for (address = 0; address < 16; address++)
    {
        if(mepa_i2c_read(appl_malibu_device[port_no], 0, address, 0, 0, 0, &data) != MEPA_RC_OK)
        {
	        T_E("mepa_i2c_read, port %d, gpio %d, address = 0x%X\n", port_no, gmap->gpio_i2c_clk, address);
	        printf("Malibu Error reading I2C register on SFP+ module for port %d, gpio %d \n", port_no, gmap->gpio_i2c_clk);
        } 
        else
        {
	        printf("Malibu reading I2C register @ addr = %d: value = 0x%X \n", address, data);
        }
    }

    address = 0x3E;
    if (mepa_i2c_read(appl_malibu_device[port_no], 0, address, 0, 0, 0, &data) != MEPA_RC_OK)
    {
        T_E("mepa_i2c_read, port %d, gpio %d, address = 0x%X\n", port_no, gmap->gpio_i2c_clk, address);
        printf("Malibu Error reading I2C register on SFP+ module for port %d, gpio %d \n", port_no, gmap->gpio_i2c_clk);
    } 
    else
    {
        printf("Malibu reading I2C register @ addr = 0x%X: value = 0x%X \n", address, data);
    }

    data = 0xAB;
    if (mepa_i2c_write(appl_malibu_device[port_no], 0, address, 0, 0, 0, &data) != MEPA_RC_OK)
    {
        T_E("mepa_i2c_write, port %d, gpio %d, address = 0x%X, data = 0x%X\n", port_no, gmap->gpio_i2c_clk, address, data);
        printf("Malibu Error writing I2C register on SFP+ module for port %d, gpio %d \n", port_no, gmap->gpio_i2c_clk);
    }

    if (mepa_i2c_read(appl_malibu_device[port_no], 0, address, 0, 0, 0, &data) != MEPA_RC_OK)
    {
        T_E("mepa_i2c_read, port %d, gpio %d, address = 0x%X\n", port_no, gmap->gpio_i2c_clk, address);
        printf("Malibu Error reading I2C register on SFP+ module for port %d, gpio %d \n", port_no, gmap->gpio_i2c_clk);
    }
    else
    {
        printf("Malibu reading I2C register @ addr = 0x%X: value = 0x%X \n", address, data);
    }

    /* ********************************************************** */

    return rc;
}

void appl_malibu_sfp_rom_get(mepa_port_no_t port_no, uint8_t *data, unsigned int len)
{
    int i = 0, j = 0;

    for (i = 0; i < len; i++)
    {
        if (mepa_i2c_read(appl_malibu_device[port_no], 0, i, 0, 0, 0, &data[i]) != MEPA_RC_OK)
        {
            T_E("mepa_i2c_read, port %d\n", port_no);
            printf("Malibu Error reading I2C register on SFP+ module for port %d\n", port_no);
        }
    }

    return;
}

mepa_rc appl_malibu_loopback_conf(mepa_port_no_t port_no)
{
    /*
     Note: with MEPA, the usual way to setup PHYs for loopback would be
     to use mepa_loopback_get/set() functions. However, as of
     SW-MEPA v2025.12, there is no implementation yet of these APIs for VSC825x (Malibu10)
     PHYs. Thus, we call VTSS APIs directly below.
    */
   // Note that the code below was taken from the lpback implementation
   // of mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c

    mepa_rc rc = MEPA_RC_OK;
    vtss_phy_10g_loopback_t    lpback;
    vtss_lb_type_t             lpback_type = VTSS_LB_NONE;
    char                       lpback_descr[6];
    char                       value_str[255] = {0};
    
    memset (&lpback_descr[0], 0, sizeof(lpback_descr));

    // Populate the lpback struct
    vtss_phy_10g_loopback_get(NULL, port_no, &lpback);

    // Print current loopback setting in the PHY.
    switch (lpback.lb_type)
    {
        case VTSS_LB_NONE:  snprintf(lpback_descr, 5, "NONE");   break;
        case VTSS_LB_H2:    snprintf(lpback_descr, 3, "H2");     break;
        case VTSS_LB_H3:    snprintf(lpback_descr, 3, "H3");     break;
        case VTSS_LB_H4:    snprintf(lpback_descr, 3, "H4");     break;
        case VTSS_LB_L1:    snprintf(lpback_descr, 3, "L1");     break;
        case VTSS_LB_L2:    snprintf(lpback_descr, 3, "L2");     break;
        case VTSS_LB_L3:    snprintf(lpback_descr, 3, "L3");     break;
        case VTSS_LB_L2C:   snprintf(lpback_descr, 4, "L2C");    break;
        default:
            printf ("Current Loopback Description INVALID,  Port_no: %d \n",  port_no);
            memset (&lpback_descr[0], 0, sizeof(lpback_descr));
    }

    printf ("Current Loopback is: %s  Type: %s \n\n",  (lpback.enable ? "Enabled" : "Disabled"), lpback_descr);

    // Ask user to input the desired Loopback mode!
    // Refer to the Device Datasheet, Section 3.10 for what these modes mean!
    // https://ww1.microchip.com/downloads/en/DeviceDoc/VMDS-10487.pdf
    printf ("Loopback Options for Port: %d \n", port_no);
    printf ("  H2: Host Loopback 2, Host PMA Interface (1G and 10G)\n");
    printf ("  H3: Host Loopback 3, Line PCS after the gearbox (10G) \n");
    printf ("  H4: Host Loopback 4, WIS-line PMA interface (10G) \n");
    printf ("  L1: Line Loopback 1, Host PCS after the gearbox (10G) \n");
    printf ("  L2: Line Loopback 2, XGMII interface (1G and 10G)\n");
    printf ("  L3: Line Loopback 3, Line PMA interface (1G and 10G) \n");
    printf (" L2C: Line Loopback 2C, Line loopback 2 after cross connect    \n\n");
    printf ("Enter Loopback Type: H2/H3/H4/L1/L2/L3/L2C \n");
    memset (&value_str[0], 0, sizeof(value_str));
    scanf("%s", &value_str[0]);

    if (value_str [0] == 'h' || value_str [0] == 'H' )
    {
        switch (value_str [1])
        {
            case '2':
                lpback_type = VTSS_LB_H2;
                break;
            case '3':
                lpback_type = VTSS_LB_H3;
                break;
            case '4':
                lpback_type = VTSS_LB_H4;
                break;
            default:
                break;
        }
    }
    else if (value_str [0] == 'l' || value_str [0] == 'L' )
    {
        switch (value_str [1])
        {
            case '1':
                lpback_type = VTSS_LB_L1;
                break;
            case '2':
                if (value_str [2] == 'c' || value_str [2] == 'C')
                {
                    lpback_type = VTSS_LB_L2C;
                } 
                else
                {
                    lpback_type = VTSS_LB_L2;
                }
                break;
            case '3':
                lpback_type = VTSS_LB_L3;
                break;
            default:
                break;
        }

    }
    else
    {
        lpback_type = VTSS_LB_NONE;
    }

    lpback.lb_type = lpback_type;
    switch (lpback.lb_type)
    {
        case VTSS_LB_NONE:  snprintf(lpback_descr, 5, "NONE");   break;
        case VTSS_LB_H2:    snprintf(lpback_descr, 3, "H2");     break;
        case VTSS_LB_H3:    snprintf(lpback_descr, 3, "H3");     break;
        case VTSS_LB_H4:    snprintf(lpback_descr, 3, "H4");     break;
        case VTSS_LB_L1:    snprintf(lpback_descr, 3, "L1");     break;
        case VTSS_LB_L2:    snprintf(lpback_descr, 3, "L2");     break;
        case VTSS_LB_L3:    snprintf(lpback_descr, 3, "L3");     break;
        case VTSS_LB_L2C:   snprintf(lpback_descr, 4, "L2C");    break;
        default:
            printf ("Current Loopback Description INVALID,  Port_no: %d \n",  port_no);
            memset (&lpback_descr[0], 0, sizeof(lpback_descr));
    }
    
    printf ("Selected Loopback Type: %s \n",  lpback_descr);
    printf ("E=Enable or D=Disable Loopback? \n");
    memset (&value_str[0], 0, sizeof(value_str));
    scanf("%s", &value_str[0]);

    if (value_str [0] == 'e' || value_str [0] == 'E')
    {
        lpback.enable = 1;
    }
    else
    {
        lpback.enable = 0;
    }

    printf ("Port %d, Setting Loopback Type: %s  to  %s  \n",  port_no, lpback_descr, (lpback.enable ? "Enabled" : "Disabled"));
    vtss_phy_10g_loopback_set(NULL, port_no, &lpback);

    return rc;
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

    // Note: VTSS APIs normally expect a vtss_inst_t struct to be passed to them.
    // Normally, MEPA APIs can get the vtss_inst_t struct by typecasting a member of the mepa_device_t instance
    // to another data type (which is only exposed to the PHY Drivers).
    // Thus, any VTSS APIs called directly by this application will pass 'NULL' as the vtss_inst_t value,
    // as the internal APIs treat this as the default instance (see vtss_phy_inst_check())

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
    phy_mode = appl_mepa_set_oper_mode();

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
    // Reference: mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c > 1st for loop of main()
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

    // GPIO Initialization
    // References:
    //      mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c
    //      mesa/meba/src/sparx5/meba.c > malibu_gpio_map
    for (i = 0; i < APPL_PORT_COUNT; i++)
    {
        printf("Configuring GPIOs for port %d\n", i);
        rc = appl_malibu_gpio_conf(i);
        printf("appl_malibu_gpio_conf: rc: %d\n\n", rc);
    }
    
    // Check PHY capability of Port 0
    mepa_phy_info_t appl_phy_info;
    memset(&appl_phy_info, 0, sizeof(appl_phy_info));
    rc = mepa_phy_info_get(appl_malibu_device[0], &appl_phy_info);
    printf("mepa_phy_info_get: rc: %d, PHY Cap: 0x%X, Part: 0x%X\r\n", rc, appl_phy_info.cap, appl_phy_info.part_number);

    fflush(stdout);

    /* ****************************************************** */
    /*                       Main Demo                        */
    /* ****************************************************** */ 
    // Reference: mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c
    while(1)
    {
        {
            printf (" *************************************\n");
            printf (" The following commands are supported:\n");
            printf (" dump <port_no> - Dump detailed PHY, PHY_TS, MACsec, or ALL register groups for Port \n");
            printf (" ----------------------------------------  |  ------------------------------------------------------- \n");
            printf (" spird    <port_no> <dev> <addr>           |  spiwr       <port_no> <dev> <addr> <value> - Value MUST be in hex\n");
            printf (" ----------------------------------------  |  ------------------------------------------------------- \n");
            // printf (" 10g_kr   <port_no> - 10G Base KR          |  synce       <port_no> - Sync-E Config   \n");
            // printf (" prbs     <port_no> - PRBS                 |  obuf        <port_no> - Output Buf Control  \n");
            printf (" status   <port_no> - Rtn PHY Link status  |  vscope      <port_no> - Config for VSCOPE FAST/FULL SCAN \n");
            printf (" lpback   <port_no> - Set PHY Loopback     |  oper_mode   <port_no> - Re-configure Port for the desired configuration.\n");
            // printf (" int10g   <port_no> - Set 10g Interuppts   |  poll10g     <port_no>                                    \n");
            // printf (" ex_int   <port_no> - Set Ext Interuppts   |  ex_poll     <port_no>                                    \n");
            // printf (" ts_int   <port_no> - Set TS Interuppts    |  ts_poll     <port_no>                                    \n");
            // printf (" macsec   <port_no> - MACSEC Block config  |  1588        <port_no> - 1588 Block Config   \n");
            printf (" dbgdump  <port_no> - Simple PHY Reg Dump  |                                                \n");
            printf (" ----------------------------------------  |  ------------------------------------------------------- \n");
            printf (" sfpdump  <port_no> <i2c_addr> - SFP Dump  |  sfp_status <port_no> - Basic SFP Status Info\n");
            printf (" sfp_txdis <port_no> - Disable TX on SFP   |  sfp_txen <port_no>   - Enable TX on SFP\n");

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

            printf ("Enter Dev/MMD (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            dev = strtol(value_str, NULL, 16);

            printf ("Enter Addr (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            addr = strtol(value_str, NULL, 16);

            printf ("Enter Value (in HEX, ie. 0x ): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            val32 = strtol(value_str, NULL, 16);

            // Note: Since there are no MEPA functions that will allow PHY register access specifically through
            // SPI, we call the SPI callout directly through the wrapper function below.
            appl_spi_read_write(&appl_rpi_spi, &appl_callout_ctx[port_no], dev, addr, &val32, true);
            printf("\nWrote 0x%X to Register 0x%X:0x%04X\n\n", val32, dev, addr);

            continue;
        }
        else if (strcmp(command, "vscope")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            // Note: function below was taken directly from mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c
            // since there are no VSCOPE APIs in MEPA yet.
            // Ref: https://support.microchip.com/s/article/VSC8258EV---Run-the-phy-demo-appl-Example-on-a-Raspberry-Pi
            appl_malibu_vscope_cntrl(port_no);

            continue;
        }
        else if (strcmp(command, "dbgdump")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            // Note Code below was taken from mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c
            // since there are no MEPA equivalents for vtss_phy_10g_debug_register_dump() yet (as of SW-MEPA 2025.12)
            /* ********************************************************** */
            /* ******    DEBUG REG PRINT OUT          ******************* */
            /* ********************************************************** */

            vtss_debug_printf_t pr = (vtss_debug_printf_t) printf;
            printf("\n==========================================================\n");
            printf("Debug Reg Dump for Port %d\n", port_no);
            if (vtss_phy_10g_debug_register_dump(NULL, pr, FALSE, port_no) != VTSS_RC_OK)
            {
                T_E("vtss_phy_10g_debug_register_dump, port %d\n", port_no);
            }
            printf("==========================================================\n");
            
            continue;
        }
        else if (strcmp(command, "sfpdump")  == 0)
        {
            // sfpdump     <port_no> <i2c_addr> 
            uint8_t sfpdata[256];
            uint32_t i2c_addr = 0;
            uint32_t val32 = 0;
            memset(sfpdata, 0, sizeof(sfpdata));

            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            if(port_no < 2)
            {
                // Only Ports 2 & 3 have SFPs on VSC8258EV, so skip the other ports.
                printf("Only Ports 2 & 3 have SFP ports on VSC8258EV!\n\n");
                continue;
            }

            printf("Enter I2C Slave Address (in HEX, ie. 0x): ");
            memset (&value_str[0], 0, sizeof(value_str));
            scanf("%s", &value_str[0]);
            i2c_addr = strtol(value_str, NULL, 16);

            // Read 0x1C:0x0000 (SLAVE_ID Register) first to get the current I2C Slave address value
            appl_spi_read_write(&appl_rpi_spi, &appl_callout_ctx[port_no], 0x01, 0xC000, &val32, false);
            
            // Then write the i2c_addr...
            appl_spi_read_write(&appl_rpi_spi, &appl_callout_ctx[port_no], 0x01, 0xC000, &i2c_addr, true);
            
            // Read back value to be sure...
            appl_spi_read_write(&appl_rpi_spi, &appl_callout_ctx[port_no], 0x01, 0xC000, &val32, false);
            printf("\r\n0x01:0xC000 new value: 0x%X\r\n\r\n", val32);

            // Now read through the SFP registers...
            appl_malibu_sfp_rom_get(port_no, &sfpdata[0], sizeof(sfpdata));

            // Print the SFP Dump!
            printf("Dumping SFP Address 0x%X\n", i2c_addr);
            for(i = 0; i < 16; i++)
            {
                printf("%02X: ", i);
                for(j = 0; j < 16; j++)
                {
                    printf("%02X ", sfpdata[j+(16*i)]);
                }
                printf(" ");
                for(j = 0; j < 16; j++)
                {
                    printf("%c", isprint(sfpdata[j+(16*i)])? sfpdata[j+(16*i)] : '.');
                }
                printf("\r\n");
            }

            // Write back the default i2c_addr for 0x01:0xC000
            i2c_addr = 0x50;
            appl_spi_read_write(&appl_rpi_spi, &appl_callout_ctx[port_no], 0x01, 0xC000, &i2c_addr, true);

            continue;
        }
        else if (strcmp(command, "sfp_status")  == 0)
        {
            mepa_bool_t rxlos, txfault, txdis, moddet;
            uint8_t sfpdata[256];
            memset(sfpdata, 0, sizeof(sfpdata));

            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            if(port_no < 2)
            {
                // Only Ports 2 & 3 have SFPs on VSC8258EV, so skip the other ports.
                printf("Only Ports 2 & 3 have SFP ports on VSC8258EV!\n\n");
                continue;
            }

            // RXLOS, TX_FAULT, TXDIS, MODDET
            mepa_gpio_in_get(appl_malibu_device[port_no], malibu_gpio_map[port_no].gpio_rx_los, &rxlos);
            mepa_gpio_in_get(appl_malibu_device[port_no], malibu_gpio_map[port_no].gpio_tx_fault, &txfault);
            mepa_gpio_in_get(appl_malibu_device[port_no], malibu_gpio_map[port_no].gpio_sfp_mod_det, &moddet);
            mepa_gpio_in_get(appl_malibu_device[port_no], malibu_gpio_map[port_no].gpio_tx_dis, &txdis);

            printf("Port %d SFP Status: \n", port_no);
            printf("  SFP Inserted: %s\n", moddet? "No" : "Yes");

            if(!moddet)
            {
                printf("  Signal Loss: %s\n", rxlos? "Yes" : "No");
                printf("  TX Fault: %s\n", txfault? "Yes" : "No");
                printf("  TX Disabled: %s\n", txdis? "Yes" : "No");

                // Get Transceiver Info.
                // Refer to SFF-8472 for info on how to decode the SFP EEPROM info!
                // https://members.snia.org/document/dl/25916
                appl_malibu_sfp_rom_get(port_no, &sfpdata[0], sizeof(sfpdata));

                printf("\nTransceiver information:\n");
                
                snprintf(value_str, 17, "%s", &sfpdata[20]);
                printf("  Vendor:        %s\n", value_str);
                snprintf(value_str, 17, "%s", &sfpdata[40]);
                printf("  Part Number:   %s\n", value_str);
                snprintf(value_str, 17, "%s", &sfpdata[68]);
                printf("  Serial Number: %s\n", value_str);
                snprintf(value_str, 9, "%s", &sfpdata[84]);
                printf("  Date Code:     %s\n", value_str);
            }

            printf("\n\n");
                        
            continue;
        }
        else if (strcmp(command, "sfp_txdis")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }
            
            if(port_no < 2)
            {
                // Only Ports 2 & 3 have SFPs on VSC8258EV, so skip the other ports.
                printf("Only Ports 2 & 3 have SFP ports on VSC8258EV!\n\n");
                continue;
            }

            // Deassert TXDIS through GPIO:
            if(mepa_gpio_out_set(appl_malibu_device[port_no], malibu_gpio_map[port_no].gpio_tx_dis, 1) != MEPA_RC_OK)
            {
                T_E("mepa_gpio_out_set() Port %d, error %d\n", port_no, rc);
            }
            else
            {
                printf("TX Disabled (GPIO%d) on SFP for Port %d!\n", malibu_gpio_map[port_no].gpio_tx_dis, port_no);
            }
                   
            continue;
        }
        else if (strcmp(command, "sfp_txen")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == false)
            {
                continue;
            }

            if(port_no < 2)
            {
                // Only Ports 2 & 3 have SFPs on VSC8258EV, so skip the other ports.
                printf("Only Ports 2 & 3 have SFP ports on VSC8258EV!\n\n");
                continue;
            }

            // Assert TXDIS through GPIO:
            if(mepa_gpio_out_set(appl_malibu_device[port_no], malibu_gpio_map[port_no].gpio_tx_dis, 0) != MEPA_RC_OK)
            {
                T_E("mepa_gpio_out_set() Port %d, error %d\n", port_no, rc);
            }
            else
            {
                printf("TX Enabled (GPIO%d) on SFP for Port %d!\n", malibu_gpio_map[port_no].gpio_tx_dis, port_no);
            }
            
            continue;
        }
        else if (strcmp(command, "lpback")  == 0)
        {
            
            if (get_valid_port_no(&port_no, port_no_str) == FALSE) {
                continue;
            }
            
            appl_malibu_loopback_conf(port_no);

            continue;
        }
        else if (strcmp(command, "oper_mode")  == 0)
        {
            if (get_valid_port_no(&port_no, port_no_str) == FALSE) {
                continue;
            }

            phy_mode = appl_mepa_set_oper_mode();

            // Reset and configure PHY ports
            // See sw-mepa/mepa/docs/linkup_config.adoc#reset-configuration
            // mepa_reset() is called before mepa_conf_set()
            printf("Resetting port %d\n", port_no);
            rc = appl_mepa_reset_phy(port_no);
            printf("appl_mepa_reset_phy: rc: %d\n\n", rc);
                    
            // Wait for PHY to stabilize - around 1s
            usleep(1000000);

            // Set the PHY Configuration for each port.
            // Reference: mesa/phy_demo_appl/appl/vtss_appl_10g_phy_malibu.c > 1st for loop of main()
            // Also, refer to board-configs/src/sparx5/meba.c > malibu_init()
            printf("Configuring port %d\n", port_no);
            rc = appl_mepa_phy_init(port_no, phy_mode);
            printf("appl_mepa_phy_init: rc: %d\n\n", rc);
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