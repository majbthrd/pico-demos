/*****************************************************************************
 * Copyright (c) 2021 Peter Lawrence                                         *
 *                                                                           *
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

#include <libmem.h>
#include <libmem_loader.h>
#include <string.h>
#include <rp2040.h>

extern unsigned char __SRAM_segment_used_end__[];
extern unsigned char __SRAM_segment_end__[];

#define FLASH_ORIGIN 0x10000000
#define FLASH_SIZE   (1024*1024)
#define SECTOR_SIZE  4096
#define PAGE_SIZE    256

static libmem_geometry_t qspi_flash_geometry[2] =
{
  [0] = { .size = SECTOR_SIZE, .count = FLASH_SIZE / SECTOR_SIZE, },
};

#define rom_table_code(c1,c2) (((uint32_t)c2 << 8) | c1)
#define rom_hword_as_ptr(rom_address) (void *)(uint32_t)(*(uint16_t *)rom_address)

struct
{
  void (*flash_enter_cmd_xip)(void);
  void (*connect_internal_flash)(void);
  void (*flash_exit_xip)(void);
  void (*flash_flush_cache)(void);
  void (*flash_range_erase)(uint32_t addr, size_t count, uint32_t block_size, uint8_t block_cmd);
  void (*flash_range_program)(uint32_t addr, const uint8_t *data, size_t count);
} handle;

static uint8_t scratchpad[PAGE_SIZE];
static libmem_driver_handle_t prom;

static int libmem_erase_impl(libmem_driver_handle_t *h, uint8_t *start, size_t size, uint8_t **erased_start, size_t *erased_size)
{
  uint32_t erase_addr  = ((uint32_t)start & ~(SECTOR_SIZE - 1)); /* align downwards to a page boundary */
  uint32_t ending_addr = ((uint32_t)start + size + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1);
  uint32_t padded_size = ending_addr - erase_addr;
  uint32_t outcome = LIBMEM_STATUS_SUCCESS;

  handle.connect_internal_flash();
  handle.flash_exit_xip();
  handle.flash_range_erase(erase_addr - FLASH_ORIGIN, padded_size, 1UL << 16, 0xD8);
  handle.flash_flush_cache();
  handle.flash_enter_cmd_xip();

  if (erased_start) *erased_start = (uint8_t *)erase_addr;
  if (erased_size)  *erased_size  = padded_size;
bail:
  return outcome;
}

static int libmem_write_impl(libmem_driver_handle_t *h, uint8_t *dest, const uint8_t *src, size_t size)
{
  uint32_t outcome = LIBMEM_STATUS_SUCCESS;
  uint32_t aligned_addr, alignment_offset, chunk_size;

  handle.connect_internal_flash();
  handle.flash_exit_xip();

  aligned_addr = (uint32_t)dest & ~(PAGE_SIZE - 1);
  alignment_offset = (uint32_t)dest - aligned_addr;
  if (alignment_offset)
  {
    memset(scratchpad, 0xFF, PAGE_SIZE);
    chunk_size = PAGE_SIZE - alignment_offset;
    if (size < chunk_size) chunk_size = size;
    memcpy(scratchpad + alignment_offset, src, chunk_size);
    handle.flash_range_program(aligned_addr - FLASH_ORIGIN, scratchpad, PAGE_SIZE);
    src += chunk_size; size -= chunk_size; dest += chunk_size;
  }

  if (size >= PAGE_SIZE)
  {
    chunk_size = size & ~(PAGE_SIZE - 1);
    handle.flash_range_program((uint32_t)dest - FLASH_ORIGIN, src, chunk_size);
    src += chunk_size; size -= chunk_size; dest += chunk_size;
  }

  if (size)
  {
    memset(scratchpad, 0xFF, PAGE_SIZE);
    memcpy(scratchpad, src, size);
    handle.flash_range_program((uint32_t)dest - FLASH_ORIGIN, scratchpad, PAGE_SIZE);
  }

  handle.flash_flush_cache();
  handle.flash_enter_cmd_xip();

  return outcome;
}

static const libmem_driver_functions_t driver_functions =
{       
  .write  = libmem_write_impl,
  .fill   = NULL,
  .erase  = libmem_erase_impl,
  .lock   = NULL,
  .unlock = NULL,
  .flush  = NULL,
};

int main(unsigned long param0)
{
  const uint32_t magic = *(uint32_t *)0x10;
  if (0x01754D != (magic & 0xFFFFFF)) return -1;

  void *(*rom_table_lookup)(uint16_t *table, uint32_t code) = rom_hword_as_ptr(0x18);
  uint16_t *func_table = (uint16_t *)rom_hword_as_ptr(0x14);
  handle.flash_enter_cmd_xip    = rom_table_lookup(func_table, rom_table_code('C', 'X'));
  handle.connect_internal_flash = rom_table_lookup(func_table, rom_table_code('I', 'F'));
  handle.flash_exit_xip         = rom_table_lookup(func_table, rom_table_code('E', 'X'));
  handle.flash_flush_cache      = rom_table_lookup(func_table, rom_table_code('F', 'C'));
  handle.flash_range_erase      = rom_table_lookup(func_table, rom_table_code('R', 'E'));
  handle.flash_range_program    = rom_table_lookup(func_table, rom_table_code('R', 'P'));

  /* glitchlessly transition to the ref clock */
  hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
  while (!(clocks_hw->clk[clk_sys].selected & 0x1u));

  /* magic sequence for memory-mapped flash to function */
  handle.connect_internal_flash();
  handle.flash_exit_xip();
  handle.flash_enter_cmd_xip();

#if 0
  volatile int wait = 1; while (wait);
  static uint8_t pattern[256];
  libmem_erase_impl(NULL, (uint8_t *)FLASH_ORIGIN, sizeof(pattern), NULL, NULL);
  for (res = 0; res < sizeof(pattern); res++)
    pattern[res] = res;
  libmem_write_impl(NULL, (uint8_t *)FLASH_ORIGIN + 3, pattern, sizeof(pattern));
#endif

  libmem_register_driver(&prom, (uint8_t *)FLASH_ORIGIN, FLASH_SIZE, qspi_flash_geometry, 0, &driver_functions, 0);

  int res = libmem_rpc_loader_start(__SRAM_segment_used_end__, __SRAM_segment_end__ - 1); 
  libmem_rpc_loader_exit(res, 0);

  return 0;
}
