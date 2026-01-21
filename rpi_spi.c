/* 
 * SPI Accessor functions for MEPA
 *
 * Copyright (C) 2023 Microchip Technology Inc.
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

#include <stdarg.h> // For va_list
#include <unistd.h>     // For TCP read/write
#include <sys/stat.h>// For /sys/class/mdio file status
#include <sys/types.h>// For /sys/class/mdio file status
#include <errno.h>// For System errors
#include <unistd.h>// For /sys/class/mdio
#include <stdio.h> // for FILE types
#include <stdlib.h> // for EXIT_SUCCESS

// MJ Addition
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/gpio.h>
#include "rpi_spi.h"

#define DEV_SPI "/dev/spidev0.1"	/* SPI Device Node */
#define SPI_MSG_SIZE 1				/* Message Size for SPI */

static int8_t spi_fd = 0;			/* File Descriptor for SPI */

/* Initialize Variables */
static uint8_t mode = SPI_MODE_0;
static uint8_t bits = 8;
static uint64_t speed = 1000000;				/* 1 MHz */

char rpi_spi_buffer[16];
//ENDOF MJ Addition

////////////////////////////////////////////////////////////////////////////////
// SPI Functions for Raspberry Pi 4B
////////////////////////////////////////////////////////////////////////////////
int spi_set_cs_high(void)
{
	/* Initialize Variables */
	uint8_t ret = 0;

	/* Close SPI File Descriptor.
	 * Note: Closing SPI sets CS pin to HIGH (Active Low)
	 */
	ret = close(spi_fd);

	return (ret);
}

int spi_set_cs_low(void)
{
	/* Initialize Variables */
	uint8_t ret = 0;

	/* Opening SPI sets CS pin to LOW (Active Low) */
	spi_fd = open(DEV_SPI, O_RDWR);

	if (spi_fd < 0)
	{
		printf("SPI ERROR: CS LOW %s ret=%d\n", DEV_SPI, spi_fd);
		exit(1);
	}

	return (ret);
}

void spi_write(uint8_t *data, unsigned int len)
{
	/* Create Buffers */
	struct spi_ioc_transfer xfer[SPI_MSG_SIZE] = {0};

	/* Fill buffer */
	xfer[0].tx_buf = data;
	xfer[0].len = len;
	xfer[0].speed_hz = speed;
	xfer[0].bits_per_word = bits;
	xfer[0].delay_usecs = 0;

	/* Write data */
	if (ioctl(spi_fd, SPI_IOC_MESSAGE(SPI_MSG_SIZE), xfer) < 0)
	{
		perror("SPI ERROR: SPI_IOC_MESSAGE");
	}
}

void spi_read(uint8_t *data, unsigned int len)
{
	/* Create Buffers */
	struct spi_ioc_transfer xfer[SPI_MSG_SIZE] = {0};

	/* Fill buffer */
	xfer[0].rx_buf = data;
	xfer[0].len = len;
	xfer[0].speed_hz = speed;
	xfer[0].bits_per_word = bits;
	xfer[0].delay_usecs = 0;

	/* Read data */
	if (ioctl(spi_fd, SPI_IOC_MESSAGE(SPI_MSG_SIZE), xfer) < 0)
	{
		perror("SPI ERROR: SPI_IOC_MESSAGE");
	}
}

int spi_initialize(void)
{
	/* Initialize Variables */
	uint8_t ret = 0;

	/*  open device node */
	spi_fd = open(DEV_SPI, O_RDWR);

	/* Error Checking */
	if (spi_fd < 0)
	{
		printf("SPI ERROR: open %s ret = %d\n", DEV_SPI, spi_fd);
		exit(1);
	}

	/*  Set spi mode */
	if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0)
	{
		printf("SPI ERROR: ioctl() set mode: %#x\n", errno);
		exit(1);
	}

	if (ioctl(spi_fd, SPI_IOC_RD_MODE, &mode) < 0)
	{
		printf("SPI ERROR: ioctl() get mode: %#x\n", errno);
		exit(1);
	}

	// Bits per word
	if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
	{
		printf("SPI ERROR: ioctl() set bits: %#x\n", errno);
		exit(1);
	}

	if (ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0)
	{
		printf("SPI ERROR: ioctl() get bits: %#x\n", errno);
		exit(1);
	}

	/* Set SPI speed*/
	if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
	{
		printf("SPI ERROR: ioctl() set speed: %#x\n", errno);
		exit(1);
	}

	/* Close SPI Device Node */
	ret = close(spi_fd);

	return (ret);
}

////////////////////////////////////////////////////////////////////////////////
// Wrapper Functions for MEPA
////////////////////////////////////////////////////////////////////////////////

mepa_rc rpi_spi_32bit_write(
            struct mepa_callout_ctx *ctx,
            mepa_port_no_t port_no,
            uint8_t             mmd,
            uint16_t            addr,
            uint32_t            *value)
{
    uint32_t v;
    int i;

    errno = 0;

    // NOTE: May need to check if MEBA is needed. However,
    // keep in mind MEPA does not seem to be meant to dereference this...
    // mepa_port_no_t port_no = ctx->port_no;

    // Initialize the spi buffer
    memset(rpi_spi_buffer, 0, sizeof(rpi_spi_buffer));
    // See Table 64 of VSC8258 Datasheet. Each command contains up to 7 bytes from my understanding.

    //sprintf(rpi_spi_buffer, "spiwr %x %x %x %x\n", port_no, mmd, addr, *value);
    //printf("\r\nIn %s: spiwr %x %x %x %x\r\n", __func__, port_no, mmd, addr, *value);

    //MJ Addition: Create the SPI frame. Assemption: This is only a single SPI Read as decribed in Figure 101 and Table 64 of the VSC8258 Datasheet
    // Assumptions: mmd is "Device Number", addr is "Register Number", port_no is "Port/Channel Number"
    // NOTE: spidev sends data in array index 0 first.
    rpi_spi_buffer[0] = (uint8_t)(0x80 + (port_no << 5) + mmd);
    rpi_spi_buffer[1] = (uint8_t)(addr >> 8);
    rpi_spi_buffer[2] = (uint8_t)(addr & 0xFF);
    rpi_spi_buffer[3] = (uint8_t)(*value >> 24);
    rpi_spi_buffer[4] = (uint8_t)(*value >> 16);
    rpi_spi_buffer[5] = (uint8_t)(*value >> 8);
    rpi_spi_buffer[6] = (uint8_t)(*value & 0xFF);

    //printf("%s: Check: %x %x %x %x %x %x %x\r\n", __func__, 
    //                                rpi_spi_buffer[0], rpi_spi_buffer[1], rpi_spi_buffer[2], 
    //                                rpi_spi_buffer[3], rpi_spi_buffer[4], rpi_spi_buffer[5], rpi_spi_buffer[6]);

    if ((mmd == 0x1e) && (addr == 0x9002 || addr == 0x9202))
    {
        //printf ("%s: WR rpi_spi_buffer = %s\r\n", __func__, rpi_spi_buffer);
    }

    //T_N("spi_write_rbt: %s", rpi_spi_buffer);

    spi_set_cs_low();
    spi_write(rpi_spi_buffer, 7);
    spi_read(rpi_spi_buffer, 7);
    spi_set_cs_high();

    //T_N("spi32_wr port_no = 0x%X, mmd = 0x%X, addr = 0x%X, value = 0x%X", port_no, mmd, addr, *value);
    if ((mmd == 0x1e) && (addr == 0x9002 || addr == 0x9202)) 
    {
        //printf("%s: spi32_wr port_no = 0x%X, mmd = 0x%X, addr = 0x%X, value = 0x%X\n", __func__, port_no, mmd, addr, *value);
    }
    return MEPA_RC_OK;
}

mepa_rc rpi_spi_32bit_read(
                struct mepa_callout_ctx *ctx,
                mepa_port_no_t port_no,
                uint8_t             mmd,
                uint16_t            addr,
                uint32_t            *value)
{
    uint32_t v;
    int i;

    errno = 0;

    // NOTE: May need to check if MEBA is needed. However,
    // keep in mind MEPA does not seem to be meant to dereference this...
    // mepa_port_no_t port_no = ctx->port_no;

    // Initialize the spi buffer
    memset(rpi_spi_buffer, 0, sizeof(rpi_spi_buffer));
    // See Table 64 of VSC8258 Datasheet. Each command contains up to 7 bytes from my understanding.

    T_N("spird %x %x %x\r\n", port_no, mmd, addr);

    if ((mmd == 0x1e) && (addr == 0x9002 || addr == 0x9202))
    {
        //printf ("%s: RD rpi_spi_buffer = %s", __func__, rpi_spi_buffer);
    }

    // MJ Addition: Create the SPI frame. Assemption: This is only a single SPI Read as decribed in Figure 101 and Table 64 of the VSC8258 Datasheet
    // Assumptions: mmd is "Device Number", addr is "Register Number", port_no is "Port/Channel Number"
    // Data bytes are don't cares
    // NOTE: spidev sends data in array index 0 first.
    rpi_spi_buffer[0] = (uint8_t)(0x00 + (port_no << 5) + mmd);
    rpi_spi_buffer[1] = (uint8_t)(addr >> 8);
    rpi_spi_buffer[2] = (uint8_t)(addr & 0xFF);
    
    T_N("Check1: %x %x %x\r\n", rpi_spi_buffer[0], rpi_spi_buffer[1], rpi_spi_buffer[2]);

    ////T_N("%s: %s", __func__, rpi_spi_buffer);

    spi_set_cs_low();
    spi_write(rpi_spi_buffer, 7);
    memset(rpi_spi_buffer, 0, sizeof(rpi_spi_buffer));
    spi_read(rpi_spi_buffer, 7);
    spi_set_cs_high();

    T_N("Check2: %x %x %x %x %x %x %x\r\n",
                                   rpi_spi_buffer[0], rpi_spi_buffer[1], rpi_spi_buffer[2], 
                                   rpi_spi_buffer[3], rpi_spi_buffer[4], rpi_spi_buffer[5], rpi_spi_buffer[6]);
    
    //    fprintf(stderr, "%s\n", buffer);
    //rpi_spi_buffer[8] = '\0'; //NULL to strip off the CR/LF and prompt;
    // v = strtoul(rpi_spi_buffer,NULL,16);
    v = (uint32_t)((rpi_spi_buffer[3] << 24) + (rpi_spi_buffer[4] << 16) + (rpi_spi_buffer[5] << 8) + rpi_spi_buffer[6]);

    if(rpi_spi_buffer[0] == 'V' && rpi_spi_buffer[1] == 'a' && rpi_spi_buffer[2] == 'l'){
        //TODO: put in better error checking to ensure valid response
    //    if(NULL == rpi_spi_buffer){
    //    fprintf(stderr, "%s: not a decimal number\n", rpi_spi_buffer);
    //    } else if ((ULONG_MIN == v || ULONG_MAX == v) && errno == ERANGE) {
    //      fprintf(stderr, "%s: out of range\n", rpi_spi_buffer);
    //    } else if (v > INT_MAX) {
    //      fprintf(stderr, "%X: > INT_MAX\n", v);
    //    } else if (v < INT_MIN) {
    //      fprintf(stderr, "%X: < INT_MIN\n", v);
        //T_E("missing value: rpi_spi_buffer=%s, port 0x%X, mmd 0x%X, addr = 0x%X", rpi_spi_buffer, port_no, mmd, addr);
        //printf("%s: ERROR: missing value: rpi_spi_buffer=%s, port 0x%X, mmd 0x%X, addr = 0x%X\n", __func__, rpi_spi_buffer, port_no, mmd, addr);
        //fflush(stdout);
        return MEPA_RC_ERROR;
    } 
    else
    {
        *value = (uint32_t)v;
    }
    ////T_N("spi32_rd port_no = 0x%X, mmd = 0x%X, addr = 0x%X, value = 0x%X", port_no, mmd, addr, *value);
    if ((mmd == 0x1e) && (addr == 0x9002 || addr == 0x9202))
    {
        //printf("%s: spi32_rd port_no = 0x%X, mmd = 0x%X, addr = 0x%X, value = 0x%X\n", __func__, port_no, mmd, addr, *value);
    }

    T_N("Check3: 0x%x\r\n", *value);
    
    return MEPA_RC_OK;
}

mepa_rc rpi_spi_16bit_write(
            struct mepa_callout_ctx *ctx,
            //mepa_port_no_t port_no,
            uint8_t             mmd,
            uint16_t            addr,
            uint16_t            value)
{
    return rpi_spi_32bit_write(ctx, ctx->port_no, mmd, addr, (uint32_t)value);
}

mepa_rc rpi_spi_16bit_read(
                struct mepa_callout_ctx *ctx,
                //mepa_port_no_t port_no,
                uint8_t             mmd,
                uint16_t            addr,
                uint16_t            *value)
{
    uint32_t *temp_value = value;
    mepa_rc rc = rpi_spi_32bit_read(ctx, ctx->port_no, mmd, addr, temp_value);
    *temp_value = (*temp_value & 0xFFFF);
    *value = (uint16_t) (*temp_value);
    return rc;
}

mepa_rc rpi_spi_32bit_read_rbt_test(
                mepa_port_no_t port_no,
                uint8_t             mmd,
                uint16_t            addr,
                uint32_t            *value)
{
    uint32_t v;
    int i;

    errno = 0;

    // Initialize the spi buffer
    memset(rpi_spi_buffer, 0, sizeof(rpi_spi_buffer));
    // See Table 64 of VSC8258 Datasheet. Each command contains up to 7 bytes from my understanding.

    //sprintf(rpi_spi_buffer, "spird %x %x %x\n", port_no, mmd, addr);
    //printf("\r\nIn %s: spird %x %x %x\r\n", __func__, port_no, mmd, addr);

    if ((mmd == 0x1e) && (addr == 0x9002 || addr == 0x9202))
    {
        //printf ("%s: RD rpi_spi_buffer = %s", __func__, rpi_spi_buffer);
    }

    //MJ Addition: Create the SPI frame. Assemption: This is only a single SPI Read as decribed in Figure 101 and Table 64 of the VSC8258 Datasheet
    // Assumptions: mmd is "Device Number", addr is "Register Number", port_no is "Port/Channel Number"
    // Data bytes are don't cares
    // NOTE: spidev sends data in array index 0 first.
    rpi_spi_buffer[0] = (uint8_t)(0x00 + (port_no << 5) + mmd);
    rpi_spi_buffer[1] = (uint8_t)(addr >> 8);
    rpi_spi_buffer[2] = (uint8_t)(addr & 0xFF);
    
    //printf("%s: Check: %x %x %x\r\n", __func__, rpi_spi_buffer[0], rpi_spi_buffer[1], rpi_spi_buffer[2]);

    //T_N("%s: %s", __func__, rpi_spi_buffer);

    spi_set_cs_low();
    spi_write(rpi_spi_buffer, 7);
    memset(rpi_spi_buffer, 0, sizeof(rpi_spi_buffer));
    spi_read(rpi_spi_buffer, 7);
    spi_set_cs_high();

    //printf("%s: Check2: %x %x %x %x %x %x %x\r\n", __func__, 
    //                                rpi_spi_buffer[0], rpi_spi_buffer[1], rpi_spi_buffer[2], 
    //                                rpi_spi_buffer[3], rpi_spi_buffer[4], rpi_spi_buffer[5], rpi_spi_buffer[6]);
    
    //    fprintf(stderr, "%s\n", buffer);
    //rpi_spi_buffer[8] = '\0'; //NULL to strip off the CR/LF and prompt;
    // v = strtoul(rpi_spi_buffer,NULL,16);
    v = (uint32_t)((rpi_spi_buffer[3] << 24) + (rpi_spi_buffer[4] << 16) + (rpi_spi_buffer[5] << 8) + rpi_spi_buffer[6]);

    if(rpi_spi_buffer[0] == 'V' && rpi_spi_buffer[1] == 'a' && rpi_spi_buffer[2] == 'l'){
        //TODO: put in better error checking to ensure valid response
    //    if(NULL == rpi_spi_buffer){
    //    fprintf(stderr, "%s: not a decimal number\n", rpi_spi_buffer);
    //    } else if ((ULONG_MIN == v || ULONG_MAX == v) && errno == ERANGE) {
    //      fprintf(stderr, "%s: out of range\n", rpi_spi_buffer);
    //    } else if (v > INT_MAX) {
    //      fprintf(stderr, "%X: > INT_MAX\n", v);
    //    } else if (v < INT_MIN) {
    //      fprintf(stderr, "%X: < INT_MIN\n", v);
        //T_E("missing value: rpi_spi_buffer=%s, port 0x%X, mmd 0x%X, addr = 0x%X", rpi_spi_buffer, port_no, mmd, addr);
        //printf("%s: ERROR: missing value: rpi_spi_buffer=%s, port 0x%X, mmd 0x%X, addr = 0x%X\n", __func__, rpi_spi_buffer, port_no, mmd, addr);
        //fflush(stdout);
        return MEPA_RC_ERROR;
    } 
    else
    {
        *value = (uint32_t)v;
    }
    //T_N("spi32_rd port_no = 0x%X, mmd = 0x%X, addr = 0x%X, value = 0x%X", port_no, mmd, addr, *value);
    if ((mmd == 0x1e) && (addr == 0x9002 || addr == 0x9202))
    {
        //printf("%s: spi32_rd port_no = 0x%X, mmd = 0x%X, addr = 0x%X, value = 0x%X\n", __func__, port_no, mmd, addr, *value);
    }

    //printf("%s: Check3: 0x%x\r\n", __func__, *value);
    
    return MEPA_RC_OK;
}


// *******************************
// END OF FILE