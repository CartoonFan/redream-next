#include "guest/sh4/sh4.h"
#include "jit/jit.h"

#if 0
#define LOG_CCN LOG_INFO
#else
#define LOG_CCN(...)
#endif

/* with OIX, bit 25, rather than bit 13, determines which 4kb bank to use */
#define CACHE_OFFSET(addr, OIX) \
  ((OIX ? ((addr & 0x2000000) >> 13) : ((addr & 0x2000) >> 1)) | (addr & 0xfff))

static void sh4_ccn_reset(struct sh4 *sh4) {
  /* FIXME this isn't right. when the IC is reset a pending flag is set and the
     cache is actually reset at the end of the current block. however, the docs
     for the SH4 IC state "After CCR is updated, an instruction that performs
     data access to the P0, P1, P3, or U0 area should be located at least four
     instructions after the CCR update instruction. Also, a branch instruction
     to the P0, P1, P3, or U0 area should be located at least eight instructions
     after the CCR update instruction."

     i'm not sure if this will ever actually cause problems, but there may need
     to be some const prop that tries to detect writes to CCR and prematurely
     end the block */
  LOG_INFO("sh4_ccn_reset");

  jit_invalidate_blocks(sh4->jit);
}

void sh4_ccn_sq_prefetch(void *data, uint32_t addr) {
  PROF_ENTER("cpu", "sh4_ccn_sq_prefetch");

  /* make sure this is a sq related prefetch */
  DCHECK(addr >= 0xe0000000 && addr <= 0xe3ffffff);

  struct sh4 *sh4 = data;

  uint32_t dst = 0x0;
  uint32_t sqi = (addr & 0x20) >> 5;

  if (sh4->MMUCR->AT) {
    /* get upper 12 bits from UTLB */
    uint32_t vpn = addr >> 20;
    dst = sh4->utlb_sq_map[vpn & 0x3f];

    /* get lower 20 bits from original address */
    dst |= addr & 0xfffe0;
  } else {
    /* get upper 6 bits from QACR* registers */
    if (sqi) {
      dst = (*sh4->QACR1 & 0x1c) << 24;
    } else {
      dst = (*sh4->QACR0 & 0x1c) << 24;
    }

    /* get lower 26 bits from original address */
    dst |= addr & 0x3ffffe0;
  }

  as_memcpy_to_guest(sh4->memory_if->space, dst, sh4->ctx.sq[sqi], 32);

  PROF_LEAVE();
}

uint32_t sh4_ccn_cache_read(struct sh4 *sh4, uint32_t addr,
                            uint32_t data_mask) {
  CHECK_EQ(sh4->CCR->ORA, 1u);
  addr = CACHE_OFFSET(addr, sh4->CCR->OIX);
  return READ_DATA(&sh4->ctx.cache[addr]);
}

void sh4_ccn_cache_write(struct sh4 *sh4, uint32_t addr, uint32_t data,
                         uint32_t data_mask) {
  CHECK_EQ(sh4->CCR->ORA, 1u);
  addr = CACHE_OFFSET(addr, sh4->CCR->OIX);
  WRITE_DATA(&sh4->ctx.cache[addr]);
}

uint32_t sh4_ccn_sq_read(struct sh4 *sh4, uint32_t addr, uint32_t data_mask) {
  uint32_t sqi = (addr & 0x20) >> 5;
  unsigned idx = (addr & 0x1c) >> 2;
  return sh4->ctx.sq[sqi][idx];
}

void sh4_ccn_sq_write(struct sh4 *sh4, uint32_t addr, uint32_t data,
                      uint32_t data_mask) {
  uint32_t sqi = (addr & 0x20) >> 5;
  uint32_t idx = (addr & 0x1c) >> 2;
  sh4->ctx.sq[sqi][idx] = data;
}

uint32_t sh4_ccn_icache_read(struct sh4 *sh4, uint32_t addr,
                             uint32_t data_mask) {
  LOG_CCN("sh4_ccn_icache_read 0x%08x", addr);

  /* return an invalid entry */
  return 0;
}

void sh4_ccn_icache_write(struct sh4 *sh4, uint32_t addr, uint32_t data,
                          uint32_t data_mask) {
  LOG_CCN("sh4_ccn_icache_write 0x%08x", addr);

  /* ignore */
}

uint32_t sh4_ccn_ocache_read(struct sh4 *sh4, uint32_t addr,
                             uint32_t data_mask) {
  LOG_CCN("sh4_ccn_ocache_read 0x%08x", addr);

  /* return an invalid entry */
  return 0;
}

void sh4_ccn_ocache_write(struct sh4 *sh4, uint32_t addr, uint32_t data,
                          uint32_t data_mask) {
  LOG_CCN("sh4_ccn_ocache_write 0x%08x", addr);

  /* ignore */
}

REG_W32(sh4_cb, MMUCR) {
  struct sh4 *sh4 = dc->sh4;

  sh4->MMUCR->full = value;

  if (sh4->MMUCR->AT) {
    LOG_WARNING("MMU not fully supported");
  }
}

REG_W32(sh4_cb, CCR) {
  struct sh4 *sh4 = dc->sh4;

  /* TODO check for cache toggle
  union ccr CCR_OLD = *sh4->CCR;*/
  sh4->CCR->full = value;

  if (sh4->CCR->ICI) {
    sh4_ccn_reset(sh4);
  }

  /* ICI / OCI is read-only */
  sh4->CCR->ICI = 0;
  sh4->CCR->OCI = 0;
}
