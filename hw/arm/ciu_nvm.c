/*
 * CIU NVM Emulation
 *
 * Copyright 2022 Yuxiang Zhang
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */
#include "hw/misc/unimp.h"
#include "hw/qdev-clock.h"
#include "hw/qdev-properties-system.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/osdep.h"

#include "hw/arm/ciu_nvm.h"

static void nvm_operate(CIUNVMState *s, uint8_t opcode) {
  uint32_t addr_high;
  uint32_t region_size;
  const char *region_name;
  void *storage;

  if (s->nvm_key_state != 7) {
    qemu_log_mask(LOG_GUEST_ERROR, "%s: NVM is locked.\n", __func__);
    return;
  }
  if (s->nvm_op_region == 1) {
    storage = s->user_code_wrap.storage;
    region_size = s->user_code_size;
    region_name = "User Code";
  } else if (s->nvm_op_region == 2) {
    storage = s->user_param_wrap.storage;
    region_size = s->user_param_size;
    region_name = "User Param";
  } else {
    qemu_log_mask(LOG_GUEST_ERROR, "%s: Unknown region to operate.\n",
                  __func__);
    return;
  }
  addr_high = s->nvm_op_addr & ~511u;
  if (addr_high >= region_size) {
    qemu_log_mask(LOG_GUEST_ERROR, "%s: address 0x%x out of range.\n", __func__,
                  s->nvm_op_addr);
    return;
  }
  qemu_log_mask(LOG_UNIMP, "%s: op [%u] on %s page at 0x%x\n", __func__, opcode,
                region_name, addr_high);
  switch (opcode) {
  case 2:
    memset(s->nvm_pagebuf, 0xFF, 512);
    s->nvm_nvm_eint = 1;
    break;

  case 10:
    memset(storage + addr_high, 0xFF, 512);
    s->nvm_nvm_eint = 1;
    break;

  case 4:
  case 5:
  case 12:
  case 14:
    memcpy(storage + addr_high, s->nvm_pagebuf, 512);
    s->nvm_nvm_eint = 1;
    break;

  default:
    break;
  }
}

static uint64_t flash_regs_read(void *opaque, hwaddr addr, unsigned int size) {
  CIUNVMState *s = CIU_NVM(opaque);
  switch (addr) {
  case 0x00:
    return s->nvm_nvm_eint << 1;

  default:
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n", __func__, addr,
                  size);
    break;
  }
  return 0;
}

static void flash_regs_write(void *opaque, hwaddr addr, uint64_t data,
                             unsigned int size) {
  CIUNVMState *s = CIU_NVM(opaque);
  switch (addr) {
  case 0x00:
    if (!(data & 2))
      s->nvm_nvm_eint = 0;
    break;
  case 0x04:
    if ((data & 0xFFFFFFF0) != 0x57AF6C00)
      break;
    nvm_operate(s, data & 0xf);
    break;
  case 0x20:
    s->nvm_key_state &= ~1u;
    s->nvm_key_state |= (data == 0xAA55AA55) << 0;
    break;
  case 0x24:
    s->nvm_key_state &= ~2u;
    s->nvm_key_state |= (data == 0x55AA55AA) << 1;
    break;
  case 0x28:
    s->nvm_key_state &= ~4u;
    s->nvm_key_state |= (data == 0xA5A55A5A) << 2;
    break;

  default:
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n",
                  __func__, addr, data, size);
    break;
  }
}

static const MemoryRegionOps flash_regs_ops = {
    .read = flash_regs_read,
    .write = flash_regs_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void flash_write(void *opaque, hwaddr addr, uint64_t data,
                        unsigned int size) {
  CIUNVMState *s = ((struct NVMOpWrapper *)opaque)->s;

  if (s->nvm_key_state != 7) {
    qemu_log_mask(LOG_GUEST_ERROR, "%s: NVM is locked.\n", __func__);
    return;
  }

  s->nvm_op_region = ((struct NVMOpWrapper *)opaque)->region;
  s->nvm_op_addr = (uint32_t)addr;

//   qemu_log_mask(LOG_UNIMP, "%s: %u/0x%" HWADDR_PRIx " <- 0x%" PRIx64 " [%u]\n",
//                 __func__, s->nvm_op_region, addr, data, size);
  addr &= 511;
  s->nvm_pagebuf[addr + 3] = data & 0xff;
  s->nvm_pagebuf[addr + 2] = (data >> 8) & 0xff;
  s->nvm_pagebuf[addr + 1] = (data >> 16) & 0xff;
  s->nvm_pagebuf[addr + 0] = (data >> 24) & 0xff;
}

static const MemoryRegionOps flash_mmio_ops = {
    .write = flash_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void ciu_nvm_reset(DeviceState *d) {
  CIUNVMState *s = CIU_NVM(d);

  s->nvm_key_state = 0;
  s->nvm_nvm_eint = 0;
}

static void ciu_nvm_realize(DeviceState *dev, Error **errp) {
  CIUNVMState *s = CIU_NVM(dev);
  Error *err = NULL;

  memory_region_init_rom_device(&s->user_code, OBJECT(s), &flash_mmio_ops,
                                &s->user_code_wrap, "ciu.user_code",
                                s->user_code_size, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  s->user_code_wrap.storage = memory_region_get_ram_ptr(&s->user_code);
  memset(s->user_code_wrap.storage, 0xFF, s->user_code_size);
  s->user_code_wrap.s = s;
  s->user_code_wrap.region = 1;
  sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->user_code);

  memory_region_init_rom_device(&s->user_param, OBJECT(s), &flash_mmio_ops,
                                &s->user_param_wrap, "ciu.user_param",
                                s->user_param_size, &err);
  if (err) {
    error_propagate(errp, err);
    return;
  }
  s->user_param_wrap.storage = memory_region_get_ram_ptr(&s->user_param);
  memset(s->user_param_wrap.storage, 0xFF, s->user_param_size);
  s->user_param_wrap.s = s;
  s->user_param_wrap.region = 2;
  sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->user_param);
}

static void ciu_nvm_init(Object *obj) {
  CIUNVMState *s = CIU_NVM(obj);

  memory_region_init_io(&s->iomem_regs, OBJECT(s), &flash_regs_ops, s,
                        TYPE_CIU_NVM, 0x40);
  sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem_regs);
}

static const VMStateDescription vmstate_ciu_nvm = {
    .name = "ciu-nvm",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){VMSTATE_UINT8(nvm_key_state, CIUNVMState),
                               VMSTATE_UINT8(nvm_nvm_eint, CIUNVMState),
                               VMSTATE_END_OF_LIST()}};

static Property ciu_nvm_properties[] = {
    DEFINE_PROP_UINT32("user_code_size", CIUNVMState, user_code_size, 0x49000),
    DEFINE_PROP_UINT32("user_param_size", CIUNVMState, user_param_size, 512),
    DEFINE_PROP_END_OF_LIST(),
};

static void ciu_nvm_class_init(ObjectClass *klass, void *data) {
  DeviceClass *dc = DEVICE_CLASS(klass);

  dc->realize = ciu_nvm_realize;
  dc->reset = ciu_nvm_reset;
  dc->vmsd = &vmstate_ciu_nvm;
  device_class_set_props(dc, ciu_nvm_properties);
}

static const TypeInfo ciu_nvm_info = {
    .name = TYPE_CIU_NVM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CIUNVMState),
    .instance_init = ciu_nvm_init,
    .class_init = ciu_nvm_class_init,
};

static void ciu_nvm_register_types(void) {
  type_register_static(&ciu_nvm_info);
}

type_init(ciu_nvm_register_types)
