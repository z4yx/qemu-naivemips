/*
 * CIU NVM Emulation
 *
 * Copyright 2022 Yuxiang Zhang
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */


#ifndef CIU_NVM_H
#define CIU_NVM_H

#include "hw/sysbus.h"
#include "hw/registerfields.h"
#include "qom/object.h"

#define TYPE_CIU_NVM "ciu.nvm"
OBJECT_DECLARE_SIMPLE_TYPE(CIUNVMState, CIU_NVM)
struct NVMOpWrapper {
    CIUNVMState *s;
    void        *storage;
    uint32_t    region;
};
struct CIUNVMState {
    SysBusDevice parent_obj;

    MemoryRegion iomem_regs;
    MemoryRegion user_code;
    MemoryRegion user_param;
    qemu_irq irq;

    uint32_t user_code_size;
    uint32_t user_param_size;

    uint8_t  nvm_key_state;
    uint8_t  nvm_nvm_eint;
    uint8_t  nvm_pagebuf[512];
    uint32_t nvm_op_addr;
    uint32_t nvm_op_region;

    struct NVMOpWrapper user_code_wrap;
    struct NVMOpWrapper user_param_wrap;
};
#endif
