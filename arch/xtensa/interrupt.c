/*******************************************************************
 * Copyright 2021-2080 evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/

#include "../interrupt.h"

#include "context.h"
#include "cpu.h"

extern boot_info_t* boot_info;

interrupt_handler_t* interrutp_handlers[IDT_NUMBER];
u32 idt[IDT_NUMBER * 2] __attribute__((aligned(32)));

void interrupt_init() {
  kprintf("interrupt init\n");
  boot_info->idt_base = idt;
  boot_info->idt_number = IDT_NUMBER;
  for (int i = 0; i < boot_info->idt_number; i++) {
    interrutp_set(i);
  }
  u32 val = idt;
}

u32 read_cntv_tval(void) {
  u32 val;
  return val;
}

void write_cntv_tval(u32 val) {
  return;
}

u32 read_cntfrq(void) {
  u32 val;
  return val;
}

void enable_cntv(u32 cntv_ctl) {
}

void disable_cntv(u32 cntv_ctl) {
}

uint64_t read_cntvct(void) {
  uint64_t val;
  return (val);
}

void interrutp_regist(u32 vec, interrupt_handler_t handler) {
  interrutp_handlers[vec] = handler;
  interrutp_set(vec);
}

void interrutp_set(int i) {
  idt[i] = 0xe59ff000 +
           (IDT_NUMBER-2) * 4;  // ldr	pc, [pc, #24] 0x24=36=4*8=32+4
  u32 base = (u32)interrutp_handlers[i];
  idt[i + IDT_NUMBER] = base;
}
