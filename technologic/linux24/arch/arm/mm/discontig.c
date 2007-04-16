/*
 * linux/arch/arm/mm/discontig.c
 *
 * Discontiguous memory support.
 *
 * Initial code: Copyright (C) 1999-2000 Nicolas Pitre
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#if NR_NODES > 24
#error Fix Me Please
#endif

/*
 * Our node_data structure for discontiguous memory.
 */

static bootmem_data_t node_bootmem_data[NR_NODES];

pg_data_t discontig_node_data[NR_NODES] = {
  { bdata: &node_bootmem_data[0] },
  { bdata: &node_bootmem_data[1] },
#if NR_NODES > 2
  { bdata: &node_bootmem_data[2] },
#endif
#if NR_NODES > 3
  { bdata: &node_bootmem_data[3] },
#endif
#if NR_NODES > 4
  { bdata: &node_bootmem_data[4] },
#endif
#if NR_NODES > 5
  { bdata: &node_bootmem_data[5] },
#endif
#if NR_NODES > 6
  { bdata: &node_bootmem_data[6] },
#endif
#if NR_NODES > 7
  { bdata: &node_bootmem_data[7] },
#endif
#if NR_NODES > 8
  { bdata: &node_bootmem_data[8] },
#endif
#if NR_NODES > 9
  { bdata: &node_bootmem_data[9] },
#endif
#if NR_NODES > 10 
  { bdata: &node_bootmem_data[10] },
#endif
#if NR_NODES > 11 
  { bdata: &node_bootmem_data[11] },
#endif
#if NR_NODES > 12 
  { bdata: &node_bootmem_data[12] },
#endif
#if NR_NODES > 13 
  { bdata: &node_bootmem_data[13] },
#endif
#if NR_NODES > 14 
  { bdata: &node_bootmem_data[14] },
#endif
#if NR_NODES > 15
  { bdata: &node_bootmem_data[15] },
#endif
#if NR_NODES > 16 
  { bdata: &node_bootmem_data[16] },
#endif
#if NR_NODES > 17 
  { bdata: &node_bootmem_data[17] },
#endif
#if NR_NODES > 18 
  { bdata: &node_bootmem_data[18] },
#endif
#if NR_NODES > 19
  { bdata: &node_bootmem_data[19] },
#endif
#if NR_NODES > 20 
  { bdata: &node_bootmem_data[20] },
#endif
#if NR_NODES > 21 
  { bdata: &node_bootmem_data[21] },
#endif
#if NR_NODES > 22 
  { bdata: &node_bootmem_data[22] },
#endif
#if NR_NODES > 23
  { bdata: &node_bootmem_data[23] },
#endif
};

EXPORT_SYMBOL(discontig_node_data);

