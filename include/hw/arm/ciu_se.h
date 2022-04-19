/*
 * CIU Emulation
 *
 * Copyright 2022 Yuxiang Zhang
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */


#ifndef CIU_UART_H
#define CIU_UART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "hw/registerfields.h"
#include "qom/object.h"

#define TYPE_CIU_UART "ciu.uart"
OBJECT_DECLARE_SIMPLE_TYPE(CIUUartState, CIU_UART)

struct CIUUartState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    CharBackend chr;
    qemu_irq irq;
    guint watch_tag;

    uint32_t reg_rx;
    uint32_t reg_sr;
};
#endif

#ifndef CIU_SE_H
#define CIU_SE_H

#include "hw/sysbus.h"
#include "hw/arm/armv7m.h"
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
    // CIURNGState rng;
    // CIUNVMState nvm;
    // CIUGPIOState gpio;
    // CIUTimerState timer[CIU_NUM_TIMERS];

    // MemoryRegion iomem;
    MemoryRegion sram;
    MemoryRegion flash;
    MemoryRegion factory_code;
    MemoryRegion sysreg;
    MemoryRegion gpio;

    uint32_t sram_size;
    uint32_t flash_size;

    uint8_t sysclk_sel;
    uint8_t sysclk_div;
    uint8_t nvm_key_state;
    uint8_t nvm_nvm_eint;
    uint8_t nvm_pagebuf[512];

    MemoryRegion *board_memory;
    MemoryRegion container;

    Clock *sysclk;
};

#endif
