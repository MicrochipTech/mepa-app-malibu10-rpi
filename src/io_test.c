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

mepa_rc appl_malibu_spi_io_test(mepa_callout_t *callout, mepa_callout_ctx_t *callout_ctx, int port_count)
{
    uint32_t val32 = 0;
    uint32_t wr32 = 0;
    uint16_t addr;
    uint8_t dev;
    int i = 0;

    printf("Read Global Device_ID register (0x1E:0x0000)...\n");
    dev = 0x1E; addr = 0x0;
    for(i = 0; i < port_count; i++)
    {
        val32 = 0;  // Reset variable value
        callout->spi_read(&callout_ctx[i], i, dev, addr, &val32);
        printf("[Port %d] 0x%X:0x%04X = 0x%X\n", i, dev, addr, val32);

        // Malibu PHYs: VSC8254, VSC8256, VSC8257, VSC8258
        if((val32 != 0x8254) && (val32 != 0x8256) && (val32 != 0x8257) && (val32 != 0x8258))
        {
            printf("Error: wrong/unexpected Device_ID value (0x%X) received!\n", val32);
            return MEPA_RC_ERROR;
        }
    }
    printf("\n\n");




    // Test Writes to SD10G65_OB_CFG2 (0x01:0xF112)
    // NOTE: Default value of Register after PHY Reset is 0x7DF820
    // https://support.microchip.com/s/article/VSC825n-Registers
    printf("Test Writes to LINE_PMA_32BIT Register SD10G65_OB_CFG2 (0x01:0xF112)...\n");
    dev = 0x1; addr = 0xF112;
    for (i = 0; i < port_count; i++)
    {
        printf("=== Port %d ===\n", i);
        val32 = 0;  // Reset variable value
        wr32 = 0x3DF828 + (i << 20);

        callout->spi_read(&callout_ctx[i], i, dev, addr, &val32);    
        printf("[Port %d] Read value: 0x%X:0x%04X = 0x%X ; Writing 0x%X...\n", i, dev, addr, val32, wr32);
        if(val32 == 0xDEFEC8ED)
        {
            printf("Error: Register value is not expected. Double-check SPI connections!\n");
            return MEPA_RC_ERROR;
        }

        callout->spi_write(&callout_ctx[i], i, dev, addr, &wr32);
        val32 = 0;      // Reset variable value
        wr32 = 0x7DF820;
        callout->spi_read(&callout_ctx[i], i, dev, addr, &val32);
        printf("[Port %d] New value:  0x%X:0x%04X = 0x%X ; Write back 0x%X...\n", i, dev, addr, val32, wr32);
        if(val32 == 0xDEFEC8ED)
        {
            printf("Error: Register value is not expected. Double-check SPI connections!\n");
            return MEPA_RC_ERROR;
        }

        callout->spi_write(&callout_ctx[i], i, dev, addr, &wr32);
        val32 = 0;      // Reset variable value
        callout->spi_read(&callout_ctx[i], i, dev, addr, &val32);
        printf("[Port %d] New value:  0x%X:0x%04X = 0x%X\n\n", i, dev, addr, val32);
        if(val32 == 0xDEFEC8ED)
        {
            printf("Error: Register value is not expected. Double-check SPI connections!\n");
            return MEPA_RC_ERROR;
        }
    }
    printf("\n");





    // Test reads from SD10G65_IB_CFG0 (0x01:0xF120)
    // NOTE: Default value of Register after PHY Reset is 0x04000C00
    // https://support.microchip.com/s/article/VSC825n-Registers
    printf("Test Reads from LINE_PMA_32BIT Register SD10G65_IB_CFG0 (0x01:0xF120)...\n");
    dev = 0x1; addr = 0xF120;
    for(i = 0; i < port_count; i++)
    {
        val32 = 0;
        callout->spi_read(&callout_ctx[i], i, dev, addr, &val32);
        printf("[Port %d] 0x%X:0x%04X = 0x%X\n", i, dev, addr, val32);
        if(val32 == 0xDEFEC8ED)
        {
            printf("Error: Register value is not expected. Double-check SPI connections!\n");
            return MEPA_RC_ERROR;
        }
    }
    printf("\n\n");





    // Test reads from SD10G65_IB_CFG1 (0x01:0xF121)
    // NOTE: Default value of Register after PHY Reset is 0x88888924
    // https://support.microchip.com/s/article/VSC825n-Registers
    printf("Test writes to LINE_PMA_32BIT Register SD10G65_IB_CFG1 (0x01:0xF121)...\n");
    dev = 0x1; addr = 0xF121;
    for(i = 0; i < port_count; i++)
    {
        printf("=== Port %d ===\n", i);
        val32 = 0;  // Reset variable value
        wr32 = 0x48888924;

        callout->spi_read(&callout_ctx[i], i, dev, addr, &val32);
        printf("[Port %d] Read value: 0x%X:0x%04X = 0x%X | Writing 0x%X...\n", i, dev, addr, val32, wr32);
        if(val32 == 0xDEFEC8ED)
        {
            printf("Error: Register value is not expected. Double-check SPI connections!\n");
            return MEPA_RC_ERROR;
        }

        callout->spi_write(&callout_ctx[i], i, dev, addr, &wr32);
        val32 = 0;
        wr32 = 0x88888924;
        callout->spi_read(&callout_ctx[i], i, dev, addr, &val32);      
        printf("[Port %d] New value:  0x%X:0x%04X = 0x%X | Write back 0x%X...\n", i, dev, addr, val32, wr32);
        if(val32 == 0xDEFEC8ED)
        {
            printf("Error: Register value is not expected. Double-check SPI connections!\n");
            return MEPA_RC_ERROR;
        }


        callout->spi_write(&callout_ctx[0], 0, dev, addr, &wr32);
        val32 = 0;
        callout->spi_read(&callout_ctx[0],0,  dev, addr, &val32);
        printf("[Port %d] New value:  0x%X:0x%04X = 0x%X\n\n", i, dev, addr, val32);
        if(val32 == 0xDEFEC8ED)
        {
            printf("Error: Register value is not expected. Double-check SPI connections!\n");
            return MEPA_RC_ERROR;
        }
    }
    printf("\n");
    




    return MEPA_RC_OK;
}

// *****************************************************************************
// End of file