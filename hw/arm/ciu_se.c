/*
 * CIU Emulation
 *
 * Copyright 2022 Yuxiang Zhang
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "hw/sysbus.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"
#include "qemu/log.h"

#include "hw/arm/ciu_se.h"

#define CIU_FLASH_SIZE 0x49000
#define CIU_SRAM_SIZE 16 * 1024
#define HCLK_FRQ 5000000

#if 1
#include "migration/vmstate.h"
#include "chardev/char-fe.h"
#include "qemu/module.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"

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
#endif

#if 1
static uint64_t clock_read(void *opaque, hwaddr addr, unsigned int size) {
  CIUState *s = CIU_SE(opaque);
  switch (addr)
  {
  case 0x80:
    return s->nvm_nvm_eint << 1;
  case 0x208:
    return (s->sysclk_div<<4) | (s->sysclk_sel<<3) | 1<<2; // CLKF48RDY
    break;
  
  default:
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n", __func__, addr,
                  size);
    break;
  }
  return 0;
}

static void nvm_operate(CIUState *s, uint8_t opcode)
{
  if (s->nvm_key_state != 7) {
    qemu_log_mask(LOG_GUEST_ERROR, "%s: NVM is locked.\n", __func__);
    return;
  }
  uint32_t addr_high = s->nvm_op_addr & ~511u;
  if (addr_high >= s->flash_size) {
    qemu_log_mask(LOG_GUEST_ERROR, "%s: address 0x%x out of range.\n",
      __func__, s->nvm_op_addr);
    return;
  }
  qemu_log_mask(LOG_UNIMP, "%s: op [%u] on page at 0x%x\n",
                __func__, opcode, addr_high);
  switch (opcode)
  {
  case 2:
    memset(s->nvm_pagebuf, 0xFF, 512);
    s->nvm_nvm_eint = 1;
    break;

  case 10:
    memset(s->nvm_storage + addr_high, 0xFF, 512);
    s->nvm_nvm_eint = 1;
    break;

  case 4:
  case 5:
  case 12:
  case 14:
    memcpy(s->nvm_storage + addr_high, s->nvm_pagebuf, 512);
    s->nvm_nvm_eint = 1;
    break;
  
  default:
    break;
  }

}

static void clock_write(void *opaque, hwaddr addr, uint64_t data,
                        unsigned int size) {
  CIUState *s = CIU_SE(opaque);
  uint32_t clk_freq;
  switch (addr)
  {
  case 0x80:
    if (!(data & 2))
      s->nvm_nvm_eint = 0;
    break;
  case 0x84:
    if ((data & 0xFFFFFFF0) != 0x57AF6C00)
      break;
    nvm_operate(s, data & 0xf);
    break;
  case 0xA0:
    s->nvm_key_state &= ~1u;
    s->nvm_key_state |= (data == 0xAA55AA55) << 0;
    break;
  case 0xA4:
    s->nvm_key_state &= ~2u;
    s->nvm_key_state |= (data == 0x55AA55AA) << 1;
    break;
  case 0xA8:
    s->nvm_key_state &= ~4u;
    s->nvm_key_state |= (data == 0xA5A55A5A) << 2;
    break;
  case 0x208:
    s->sysclk_div = (data >> 4) & 0xf; 
    s->sysclk_sel = (data >> 3) & 1;
    clk_freq = s->sysclk_sel ? 48000000 : 40000000;
    clk_freq >>= (s->sysclk_sel > 5 ? 5 : s->sysclk_sel);
    qemu_log_mask(LOG_UNIMP, "%s: Set SYSCLK to %u Hz\n",
                  __func__, clk_freq);
    clock_update_hz(s->sysclk, clk_freq);
    break;
  
  default:
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n",
                  __func__, addr, data, size);
    break;
  }
}

static const MemoryRegionOps clock_ops = {.read = clock_read,
                                          .write = clock_write,
                                          .endianness = DEVICE_BIG_ENDIAN,
                                          };

static uint64_t gpio_read(void *opaque, hwaddr addr, unsigned int size) {
  static int log_cnt = 0;
  if(++log_cnt < 100)
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n", __func__, addr,
                  size);
  return 0xFFFFFFFF;
}

static void gpio_write(void *opaque, hwaddr addr, uint64_t data,
                        unsigned int size) {
  static int log_cnt = 0;
  if(++log_cnt < 100)
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n",
                  __func__, addr, data, size);
}

static const MemoryRegionOps gpio_ops = {.read = gpio_read,
                                          .write = gpio_write,
                                          .endianness = DEVICE_BIG_ENDIAN,
                                          };

static void flash_write(void *opaque, hwaddr addr, uint64_t data,
  unsigned int size) {
  CIUState *s = CIU_SE(opaque);

  // qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n",
  //               __func__, addr, data, size);

  if (s->nvm_key_state != 7) {
    qemu_log_mask(LOG_GUEST_ERROR, "%s: NVM is locked.\n", __func__);
    return;
  }

  assert(addr + size <= s->flash_size);
  s->nvm_op_addr = (uint32_t)addr;
  addr &= 511;
  s->nvm_pagebuf[addr+3] = data & 0xff;
  s->nvm_pagebuf[addr+2] = (data >> 8) & 0xff;
  s->nvm_pagebuf[addr+1] = (data >> 16) & 0xff;
  s->nvm_pagebuf[addr+0] = (data >> 24) & 0xff;
}

static const MemoryRegionOps flash_ops = {
    .write = flash_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void ciu_realize(DeviceState *dev_soc, Error **errp) {
  CIUState *s = CIU_SE(dev_soc);
  MemoryRegion *mr;
  Error *err = NULL;
  uint8_t i = 0;
  hwaddr base_addr = 0;

  if (!s->board_memory) {
    error_setg(errp, "memory property was not set");
    return;
  }

  /*
   * HCLK on this SoC is fixed, so we set up sysclk ourselves and
   * the board shouldn't connect it.
   */
  if (clock_has_source(s->sysclk)) {
    error_setg(errp, "sysclk clock must not be wired up by the board code");
    return;
  }
  /* This clock doesn't need migration because it is fixed-frequency */
  clock_set_hz(s->sysclk, HCLK_FRQ);
  qdev_connect_clock_in(DEVICE(&s->cpu), "cpuclk", s->sysclk);

  object_property_set_link(OBJECT(&s->cpu), "memory", OBJECT(&s->container),
                           &error_abort);
  if (!sysbus_realize(SYS_BUS_DEVICE(&s->cpu), errp)) {
    return;
  }

  memory_region_add_subregion_overlap(&s->container, 0, s->board_memory, -1);

  memory_region_init_rom_device(&s->flash, OBJECT(s), &flash_ops, s,
    "ciu.flash", s->flash_size, &err);
  s->nvm_storage = memory_region_get_ram_ptr(&s->flash);
  memset(s->nvm_storage, 0xFF, s->flash_size);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  memory_region_add_subregion(&s->container, 0x00000000, &s->flash);

  memory_region_init_ram(&s->factory_code, OBJECT(s), "ciu.factory_code", 0x40, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  memory_region_add_subregion(&s->container, 0x1FFFFE00, &s->factory_code);

  memory_region_init_ram(&s->sram, OBJECT(s), "ciu.sram", s->sram_size, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  memory_region_add_subregion(&s->container, 0x20000000, &s->sram);

  /* UART */
  if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart), errp)) {
    return;
  }
  mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->uart), 0);
  memory_region_add_subregion_overlap(&s->container, 0x40005000, mr, 0);
  // sysbus_connect_irq( // TODO: UART IRQ
  //     SYS_BUS_DEVICE(&s->uart), 0,
  //     qdev_get_gpio_in(DEVICE(&s->cpu), 27));

  /* RNG */
//   if (!sysbus_realize(SYS_BUS_DEVICE(&s->rng), errp)) {
//     return;
//   }

//   mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->rng), 0);
//   memory_region_add_subregion_overlap(&s->container, 0x40002800, mr, 0);
//   sysbus_connect_irq(
//       SYS_BUS_DEVICE(&s->rng), 0,
//       qdev_get_gpio_in(DEVICE(&s->cpu), BASE_TO_IRQ(NRF51_RNG_BASE)));

  /* UICR, FICR, NVMC, FLASH */
//   if (!object_property_set_uint(OBJECT(&s->nvm), "flash-size", s->flash_size,
//                                 errp)) {
//     return;
//   }

//   if (!sysbus_realize(SYS_BUS_DEVICE(&s->nvm), errp)) {
//     return;
//   }

//   mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->nvm), 0);
//   memory_region_add_subregion_overlap(&s->container, NRF51_NVMC_BASE, mr, 0);
//   mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->nvm), 1);
//   memory_region_add_subregion_overlap(&s->container, NRF51_FICR_BASE, mr, 0);
//   mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->nvm), 2);
//   memory_region_add_subregion_overlap(&s->container, NRF51_UICR_BASE, mr, 0);
//   mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->nvm), 3);
//   memory_region_add_subregion_overlap(&s->container, NRF51_FLASH_BASE, mr, 0);

  /* GPIO */
  memory_region_init_io(&s->gpio, OBJECT(dev_soc), &gpio_ops, NULL,
                        "ciu.gpio", 0x1000);
  memory_region_add_subregion_overlap(&s->container, 0x40003000, &s->gpio,
                                      -1);
//   if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio), errp)) {
//     return;
//   }

//   mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->gpio), 0);
//   memory_region_add_subregion_overlap(&s->container, 0x40003000, mr, 0);

//   /* Pass all GPIOs to the SOC layer so they are available to the board */
//   qdev_pass_gpios(DEVICE(&s->gpio), dev_soc, NULL);


  /* STUB Peripherals */
  memory_region_init_io(&s->sysreg, OBJECT(dev_soc), &clock_ops, s,
                        "ciu.sysreg", 0x1000);
  memory_region_add_subregion_overlap(&s->container, 0x50007000, &s->sysreg,
                                      -1);

  create_unimplemented_device("ciu.usb", 0x50002000, 0x100);
  create_unimplemented_device("ciu.crc", 0x50005000, 0x100);
  create_unimplemented_device("ciu.wdt", 0x40000000, 0x100);
  create_unimplemented_device("ciu.tim", 0x40000800, 0x100);
  create_unimplemented_device("ciu.spi", 0x40001800, 0x100);
}

static void ciu_init(Object *obj) {
  uint8_t i = 0;

  CIUState *s = CIU_SE(obj);

  memory_region_init(&s->container, obj, "ciu-container", UINT64_MAX);

  object_initialize_child(OBJECT(s), "armv6m", &s->cpu, TYPE_ARMV7M);
  qdev_prop_set_string(DEVICE(&s->cpu), "cpu-type",
                       ARM_CPU_TYPE_NAME("sc000"));
  qdev_prop_set_uint32(DEVICE(&s->cpu), "num-irq", 32);
  qdev_prop_set_bit(DEVICE(&s->cpu), "bigend", true);

  object_initialize_child(obj, "uart", &s->uart, TYPE_CIU_UART);
  object_property_add_alias(obj, "serial0", OBJECT(&s->uart), "chardev");

  // object_initialize_child(obj, "rng", &s->rng, TYPE_NRF51_RNG);

  // object_initialize_child(obj, "nvm", &s->nvm, TYPE_NRF51_NVM);

  // object_initialize_child(obj, "gpio", &s->gpio, TYPE_NRF51_GPIO);

  s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
}

static Property ciu_properties[] = {
    DEFINE_PROP_LINK("memory", CIUState, board_memory, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_UINT32("sram-size", CIUState, sram_size, CIU_SRAM_SIZE),
    DEFINE_PROP_UINT32("flash-size", CIUState, flash_size, CIU_FLASH_SIZE),
    DEFINE_PROP_END_OF_LIST(),
};

static void ciu_reset(DeviceState *d)
{
    CIUState *s = CIU_SE(d);

    puts(__func__);
    s->sysclk_sel = 0;
    s->sysclk_div = 3;
    s->nvm_key_state = 0;
    s->nvm_nvm_eint = 0;
}

static void ciu_class_init(ObjectClass *klass, void *data) {
  DeviceClass *dc = DEVICE_CLASS(klass);

  dc->realize = ciu_realize;
  dc->reset = ciu_reset;
  device_class_set_props(dc, ciu_properties);
}

static const TypeInfo ciu_info = {
    .name = TYPE_CIU_SE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CIUState),
    .instance_init = ciu_init,
    .class_init = ciu_class_init,
};

static void ciu_types(void) { type_register_static(&ciu_info); }
type_init(ciu_types)
#endif
