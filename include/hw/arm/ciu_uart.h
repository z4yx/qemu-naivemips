/*
 * CIU UART Emulation
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
