
# Copyright (c) 2004-2020 Microchip Technology Inc. and its subsidiaries.
# SPDX-License-Identifier: MIT

# Trace Levels:
#   -DAPPL_TRACE_LVL_RACKET 
#   -DAPPL_TRACE_LVL_NOISE  
#   -DAPPL_TRACE_LVL_DEBUG  
#   -DAPPL_TRACE_LVL_INFO   
#   -DAPPL_TRACE_LVL_WARNING
#   -DAPPL_TRACE_LVL_ERROR  
#   -DAPPL_TRACE_LVL_NONE

INCLUDES="-I include_common/ -I src/"
SOURCES="src/mepa_appl_custom_malibu.c src/rpi_spi.c src/io_test.c "
DEBUGBUILD="-g"
ERRORCHECK="-W -Werror -Wall"
WARNINGCHECK="-Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types"
STATICLIBS="mepa/libmepa.a mepa/libmepa_common.a mepa/vtss/libmepa_drv_vtss_custom.a"
MACROS="-DAPPL_TRACE_LVL_ERROR"


COMPILECOMMAND="gcc $INCLUDES $SOURCES $DEBUGBUILD $WARNINGCHECK $STATICLIBS $MACROS -o malibu_custom"
echo $COMPILECOMMAND
echo ""
$COMPILECOMMAND
