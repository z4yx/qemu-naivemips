/*
 * CIU UART Emulation
 *
 * Copyright 2022 Yuxiang Zhang
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */


#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"
#include "hw/qdev-properties-system.h"
#include "hw/qdev-properties.h"
#include "chardev/char-fe.h"
#include "migration/vmstate.h"

#include "hw/arm/ciu_uart.h"

enum {
    UARTSR_RXINT = 2,
    UARTSR_TXINT = 4,
};

static uint64_t ciu_uart_read(void *opaque, hwaddr addr,
                                unsigned size)
{
    CIUUartState *s = opaque;
    uint64_t ret = 0;

    switch (addr) {
    case 0x14:
        s->reg_sr &= ~(UARTSR_RXINT);
        ret = s->reg_rx;
        break;

    case 0:
        ret = s->reg_sr;
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "ciu-uart: read access to unknown register 0x"
                      TARGET_FMT_plx "\n", addr);
    }

    return ret;
}

static void ciu_uart_write(void *opaque, hwaddr addr, uint64_t value,
                             unsigned size)
{
    CIUUartState *s = opaque;
    unsigned char ch = value;

    switch (addr) {
    case 0x18:
        /* XXX this blocks entire thread. Rewrite to use
         * qemu_chr_fe_write and background I/O callbacks */
        s->reg_sr &= ~UARTSR_TXINT;
        qemu_chr_fe_write_all(&s->chr, &ch, 1);
        s->reg_sr |= UARTSR_TXINT;
        break;

    case 0:
        if (!(ch & UARTSR_TXINT))
          s->reg_sr &= ~(UARTSR_TXINT);
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "ciu-uart: write access to unknown register 0x"
                      TARGET_FMT_plx "\n", addr);
    }
}

static const MemoryRegionOps uart_mmio_ops = {
    .read = ciu_uart_read,
    .write = ciu_uart_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_BIG_ENDIAN,
};

static int uart_can_rx(void *opaque)
{
    CIUUartState *s = opaque;

    return !(s->reg_sr & UARTSR_RXINT);
}

static void uart_rx(void *opaque, const uint8_t *buf, int size)
{
    CIUUartState *s = opaque;

    assert(uart_can_rx(opaque));

    s->reg_sr |= UARTSR_RXINT;
    s->reg_rx = *buf;
}

static void uart_event(void *opaque, QEMUChrEvent event)
{
}

static void ciu_uart_reset(DeviceState *d)
{
    CIUUartState *s = CIU_UART(d);

    s->reg_rx = 0;
    s->reg_sr = 0;
}

static void ciu_uart_realize(DeviceState *dev, Error **errp)
{
    CIUUartState *s = CIU_UART(dev);

    qemu_chr_fe_set_handlers(&s->chr, uart_can_rx, uart_rx,
                             uart_event, NULL, s, NULL, true);
}

static void ciu_uart_init(Object *obj)
{
    CIUUartState *s = CIU_UART(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &uart_mmio_ops, s,
                          TYPE_CIU_UART, 0x1C);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
}

static const VMStateDescription vmstate_ciu_uart = {
    .name = "ciu-uart",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(reg_rx, CIUUartState),
        VMSTATE_UINT32(reg_sr, CIUUartState),
        VMSTATE_END_OF_LIST()
    }
};

static Property ciu_uart_properties[] = {
    DEFINE_PROP_CHR("chardev", CIUUartState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void ciu_uart_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ciu_uart_realize;
    dc->reset = ciu_uart_reset;
    dc->vmsd = &vmstate_ciu_uart;
    device_class_set_props(dc, ciu_uart_properties);
}

static const TypeInfo ciu_uart_info = {
    .name = TYPE_CIU_UART,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CIUUartState),
    .instance_init = ciu_uart_init,
    .class_init = ciu_uart_class_init,
};

static void ciu_uart_register_types(void)
{
    type_register_static(&ciu_uart_info);
}

type_init(ciu_uart_register_types)
