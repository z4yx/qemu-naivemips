/*
 * CIU Emulation
 *
 * Copyright 2022 Yuxiang Zhang
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef CIU_SE_H
#define CIU_SE_H

#include "hw/sysbus.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/ciu_uart.h"
#include "hw/arm/ciu_nvm.h"
#include "hw/clock.h"
#include "qom/object.h"

#define TYPE_CIU_SE "ciu-se"
OBJECT_DECLARE_SIMPLE_TYPE(CIUState, CIU_SE)

struct CIUState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    ARMv7MState cpu;

    CIUUartState uart;
    CIUNVMState flash;
    // CIURNGState rng;
    // CIUGPIOState gpio;
    // CIUTimerState timer[CIU_NUM_TIMERS];

    MemoryRegion sram;
    MemoryRegion factory_code;
    MemoryRegion sysreg;
    MemoryRegion gpio;

    uint32_t sram_size;

    uint8_t  sysclk_sel;
    uint8_t  sysclk_div;

    MemoryRegion *board_memory;
    MemoryRegion container;

    Clock *sysclk;
};

#endif
