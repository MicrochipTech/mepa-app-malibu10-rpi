// Header file for rpi_spi.c

#ifndef _RPI_SPI_H_
#define _RPI_SPI_H_

// Headers
#include "microchip/ethernet/phy/api.h"
#include "microchip/ethernet/board/api.h"
#include <stdbool.h>

#define BOOL bool

#define T_D(format, ...) MEPA_trace(MEPA_TRACE_GRP_GEN, MEPA_TRACE_LVL_DEBUG, __FUNCTION__, __LINE__, __FILE__, format, ##__VA_ARGS__);
#define T_I(format, ...) MEPA_trace(MEPA_TRACE_GRP_GEN, MEPA_TRACE_LVL_INFO, __FUNCTION__, __LINE__, __FILE__, format, ##__VA_ARGS__);
#define T_W(format, ...) MEPA_trace(MEPA_TRACE_GRP_GEN, MEPA_TRACE_LVL_WARNING, __FUNCTION__, __LINE__, __FILE__, format, ##__VA_ARGS__);
#define T_E(format, ...) MEPA_trace(MEPA_TRACE_GRP_GEN, MEPA_TRACE_LVL_ERROR, __FUNCTION__, __LINE__, __FILE__, format, ##__VA_ARGS__);
#define T_N(format, ...) MEPA_trace(MEPA_TRACE_GRP_GEN, MEPA_TRACE_LVL_NOISE, __FUNCTION__, __LINE__, __FILE__, format, ##__VA_ARGS__);

// Function prototypes
int spi_set_cs_high(void);
int spi_set_cs_low(void);
void spi_write(uint8_t *data, unsigned int len);
void spi_read(uint8_t *data, unsigned int len);
int spi_initialize(void);

mepa_rc rpi_spi_32bit_write(
            struct mepa_callout_ctx *ctx,
            mepa_port_no_t port_no,
            uint8_t             mmd,
            uint16_t            addr,
            uint32_t            *value);

mepa_rc rpi_spi_32bit_read(
                struct mepa_callout_ctx *ctx,
                mepa_port_no_t port_no,
                uint8_t             mmd,
                uint16_t            addr,
                uint32_t            *value);

mepa_rc rpi_spi_16bit_write(
            struct mepa_callout_ctx *ctx,
            //mepa_port_no_t port_no,
            uint8_t             mmd,
            uint16_t            addr,
            uint16_t            value);

mepa_rc rpi_spi_16bit_read(
                struct mepa_callout_ctx *ctx,
                //mepa_port_no_t port_no,
                uint8_t             mmd,
                uint16_t            addr,
                uint16_t            *value);

mepa_rc rpi_spi_32bit_read_rbt_test(
                mepa_port_no_t port_no,
                uint8_t             mmd,
                uint16_t            addr,
                uint32_t            *value);

BOOL vtss_appl_malibu_spi_io_test(const vtss_inst_t inst, const mepa_port_no_t port_no);


#endif /*_RPI_SPI_H*/