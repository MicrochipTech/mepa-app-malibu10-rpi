
# Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
# SPDX-License-Identifier: MIT

INCLUDES="-I include_common/ -I src/"
SOURCES="src/malibu_io_test.c src/rpi_spi.c src/io_test.c"
DEBUGBUILD="-g"
ERRORCHECK="-W -Werror -Wall"
STATICLIBS="mepa/libmepa.a mepa/libmepa_common.a mepa/vtss/libmepa_drv_vtss_custom.a"

COMPILECOMMAND="gcc $INCLUDES $SOURCES $DEBUGBUILD $STATICLIBS -o malibu_io_test"
echo $COMPILECOMMAND
echo ""
$COMPILECOMMAND
