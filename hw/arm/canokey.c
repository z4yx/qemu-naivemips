/*
 * Canokey
 *
 * Copyright 2022 Yuxiang Zhang
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"

#include "hw/arm/ciu_se.h"
#include "hw/qdev-properties.h"
#include "qom/object.h"

struct CanokeyMachineState {
    MachineState parent;

    CIUState ciu;
};

#define TYPE_CANOKEY_MACHINE MACHINE_TYPE_NAME("canokey")

OBJECT_DECLARE_SIMPLE_TYPE(CanokeyMachineState, CANOKEY_MACHINE)

static void canokey_init(MachineState *machine)
{
    CanokeyMachineState *s = CANOKEY_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();

    object_initialize_child(OBJECT(machine), "ciu", &s->ciu,
                            TYPE_CIU_SE);
    qdev_prop_set_chr(DEVICE(&s->ciu), "serial0", serial_hd(0));
    object_property_set_link(OBJECT(&s->ciu), "memory",
                             OBJECT(system_memory), &error_fatal);
    sysbus_realize(SYS_BUS_DEVICE(&s->ciu), &error_fatal);

    armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename,
                       s->ciu.flash_size);
}

static void canokey_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "CanoKey";
    mc->init = canokey_init;
    mc->max_cpus = 1;
}

static const TypeInfo canokey_info = {
    .name = TYPE_CANOKEY_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(CanokeyMachineState),
    .class_init = canokey_machine_class_init,
};

static void canokey_machine_init(void)
{
    type_register_static(&canokey_info);
}

type_init(canokey_machine_init);
