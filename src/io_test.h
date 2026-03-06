// Header file for io_test.c

#ifndef _IO_TEST_H_
#define _IO_TEST_H_

// Headers
#include <stdio.h>
#include "microchip/ethernet/phy/api.h"
#include "microchip/ethernet/board/api.h"

// SPI IO Test - ref: vtss_appl_10g_phy_malibu.c
mepa_rc appl_malibu_spi_io_test(mepa_callout_t *callout, mepa_callout_ctx_t *callout_ctx, int port_count);


#endif /*_IO_TEST_H*/