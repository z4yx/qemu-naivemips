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

#define CIU_SRAM_SIZE 16 * 1024
#define HCLK_FRQ 5000000

static uint64_t clock_read(void *opaque, hwaddr addr, unsigned int size) {
  CIUState *s = CIU_SE(opaque);
  switch (addr)
  {
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


static void clock_write(void *opaque, hwaddr addr, uint64_t data,
                        unsigned int size) {
  CIUState *s = CIU_SE(opaque);
  uint32_t clk_freq;
  switch (addr)
  {
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
  return 0;
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


static void ciu_realize(DeviceState *dev_soc, Error **errp) {
  CIUState *s = CIU_SE(dev_soc);
  MemoryRegion *mr;
  Error *err = NULL;

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

  /* Flash */
  if (!sysbus_realize(SYS_BUS_DEVICE(&s->flash), errp)) {
    return;
  }
  mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->flash), 0);
  memory_region_add_subregion_overlap(&s->container, 0x50007080, mr, 3);
  mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->flash), 1);
  memory_region_add_subregion(&s->container, 0x00000000, mr);
  mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(&s->flash), 2);
  memory_region_add_subregion(&s->container, 0x1FFF8000, mr);
  // sysbus_connect_irq( // TODO: flash IRQ
  //     SYS_BUS_DEVICE(&s->flash), 0,
  //     qdev_get_gpio_in(DEVICE(&s->cpu), 27));

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

  object_initialize_child(obj, "nvm", &s->flash, TYPE_CIU_NVM);

  // object_initialize_child(obj, "gpio", &s->gpio, TYPE_NRF51_GPIO);

  s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
}

static Property ciu_properties[] = {
    DEFINE_PROP_LINK("memory", CIUState, board_memory, TYPE_MEMORY_REGION,
                     MemoryRegion *),
    DEFINE_PROP_UINT32("sram-size", CIUState, sram_size, CIU_SRAM_SIZE),
    DEFINE_PROP_END_OF_LIST(),
};

static void ciu_reset(DeviceState *d)
{
    CIUState *s = CIU_SE(d);

    puts(__func__);
    s->sysclk_sel = 0;
    s->sysclk_div = 3;
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
