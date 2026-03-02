/* 
 * SPI IO Test for VSC8258 using MEPA.
 * Code is based on phy_demo_appl/vtss_appl_10g_phy_malibu.c > vtss_appl_malibu_spi_io_test()
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

#include "io_test.h"


void appl_malibu_spi_io_test(mepa_callout_t *callout, mepa_callout_ctx_t *callout_ctx, int port_count)
{
    uint32_t val32 = 0;
    uint32_t val32B = 0;
    uint16_t addr;
    uint8_t dev;
    int i = 0;

    printf("Test Read Global Device_ID register (0x1E:0x0000)...\n");
    dev = 0x1E; addr = 0x0;
    for(i = 0; i < port_count; i++)
    {
        callout->spi_read(callout_ctx, i, dev, addr, &val32);
        printf("[Port %d] 0x%X:0x%04X = 0x%X\n", i, dev, addr, val32);
        val32 = 0;  // Reset variable value
    }
    printf("\n");


    printf("Test Write [Port 0] LINE_PMA_32BIT Register SD10G65_OB_CFG2 (0x01:0xF112)...\n");
    dev = 0x1; addr = 0xF112;
    callout->spi_read(callout_ctx, 0, dev, addr, &val32);
    printf("[Port 0] Read value: 0x%X:0x%04X = 0x%X | ", dev, addr, val32);
    val32 = 0x003DF828;
    printf("Writing 0x%X...\n", val32);
    callout->spi_write(callout_ctx, 0, dev, addr, &val32);
    val32 = 0;      // Reset variable value
    callout->spi_read(callout_ctx, 0, dev, addr, &val32);
    printf("[Port 0] New value:  0x%X:0x%04X = 0x%X\n\n", dev, addr, val32);


    printf("Test Write [Port 1] LINE_PMA_32BIT Register SD10G65_OB_CFG2 (0x01:0xF112)...\n");
    dev = 0x1; addr = 0xF112;
    callout->spi_read(callout_ctx, 1, dev, addr, &val32);
    printf("[Port 1] Read value: 0x%X:0x%04X = 0x%X | ", dev, addr, val32);
    val32 = 0x004DF828;
    printf("Writing 0x%X...\n", val32);
    callout->spi_write(callout_ctx, 1, dev, addr, &val32);
    callout->spi_read(callout_ctx, 1, dev, addr, &val32);
    printf("[Port 1] New value:  0x%X:0x%04X = 0x%X\n\n", dev, addr, val32);


    printf("Test Write [Port 2] LINE_PMA_32BIT Register SD10G65_OB_CFG2 (0x01:0xF112)...\n");
    dev = 0x1; addr = 0xF112;
    callout->spi_read(callout_ctx, 2, dev, addr, &val32);
    printf("[Port 2] Read value: 0x%X:0x%04X = 0x%X | ", dev, addr, val32);
    val32 = 0x005DF828;
    printf("Writing 0x%X...\n", val32);
    callout->spi_write(callout_ctx, 2, dev, addr, &val32);
    callout->spi_read(callout_ctx, 2, dev, addr, &val32);
    printf("[Port 2] New value:  0x%X:0x%04X = 0x%X | ", dev, addr, val32);
    val32 = 0x007DF820;
    printf("Writing 0x%X...\n", val32);
    callout->spi_write(callout_ctx, 2, dev, addr, &val32);
    callout->spi_read(callout_ctx, 2, dev, addr, &val32);
    printf("[Port 2] New value:  0x%X:0x%04X = 0x%X\n\n", dev, addr, val32);


    printf("Test Write [Port 3] LINE_PMA_32BIT Register SD10G65_OB_CFG2 (0x01:0xF112)...\n");
    dev = 0x1; addr = 0xF112;
    callout->spi_read(callout_ctx, 3, dev, addr, &val32);
    printf("[Port 3] Read value: 0x%X:0x%04X = 0x%X | ", dev, addr, val32);
    val32 = 0x006DF828;
    printf("Writing 0x%X...\n", val32);
    callout->spi_write(callout_ctx, 3, dev, addr, &val32);
    callout->spi_read(callout_ctx, 3, dev, addr, &val32);
    printf("[Port 3] New value:  0x%X:0x%04X = 0x%X | ", dev, addr, val32);
    val32 = 0x007DF820;
    printf("Writing 0x%X...\n", val32);
    callout->spi_write(callout_ctx, 3, dev, addr, &val32);
    callout->spi_read(callout_ctx, 3, dev, addr, &val32);
    printf("[Port 3] New value:  0x%X:0x%04X = 0x%X\n\n", dev, addr, val32);


    printf("Test Read LINE_PMA_32BIT Register SD10G65_IB_CFG0 (0x01:0xF120)...\n");
    dev = 0x1; addr = 0xF120;
    for(i = 0; i < port_count; i++)
    {
        callout->spi_read(callout_ctx, i, dev, addr, &val32);
        printf("[Port %d] 0x%X:0x%04X = 0x%X\n", i, dev, addr, val32);
        val32 = 0;  // Reset variable value
    }
    printf("\n");


    printf("Test Write [Port 0] LINE_PMA_32BIT Register SD10G65_IB_CFG1 (0x01:0xF121)...\n");
    dev = 0x1; addr = 0xF121;
    callout->spi_read(callout_ctx, 0, dev, addr, &val32);
    val32B = 0x48888924;
    printf("[Port 0] Read value: 0x%X:0x%04X = 0x%X | Writing 0x%X...\n", dev, addr, val32, val32B);
    callout->spi_write(callout_ctx, 0, dev, addr, &val32B);
    callout->spi_read(callout_ctx, 0, dev, addr, &val32B);
    printf("[Port 0] New value:  0x%X:0x%04X = 0x%X | Writing 0x%X...\n", dev, addr, val32B, val32);
    callout->spi_write(callout_ctx, 0, dev, addr, &val32);
    callout->spi_read(callout_ctx, 0,  dev, addr, &val32);
    printf("[Port 0] New value:  0x%X:0x%04X = 0x%X\n", dev, addr, val32);

    return;
}

// *****************************************************************************
// End of file