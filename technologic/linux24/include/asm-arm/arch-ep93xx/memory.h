/*
 *  linux/include/asm-arm/arch-ep93xx/memory.h
 *
 *  ******************************************************
 *	*    CONFUSED?  Read Documentation/IO-mapping.txt	 *
 *  ******************************************************
 *
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2002-2003 Cirrus Logic Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARCH_MMU_H
#define __ASM_ARCH_MMU_H

/*
 * For EP93xx, SDRAM can be discontiguous, in a set number of blocks
 * of equal size and (usually) equal spacing.  The 9301 spacing isn't equal.
 *
 * SDRAM_START is the physical address of the start of SDRAM.
 * SDRAM_NUMBER_OF_BLOCKS = # of blocks of SDRAM.
 * Each block is of size SDRAM_BLOCK_SIZE and starts at a boundary
 * of SDRAM_BLOCK_START_BOUNDARY.
 *
 * So memory blocks are at:
 *  SDRAM_START
 *  SDRAM_START + SDRAM_BLOCK_START_BOUNDARY
 *  SDRAM_START + (SDRAM_BLOCK_START_BOUNDARY * 2)
 *  SDRAM_START + (SDRAM_BLOCK_START_BOUNDARY * 3)
 *  so on
 */

#ifndef CONFIG_DISCONTIGMEM

/*
 * Single 32Meg block of physical memory physically located at 0 .
 */
#define SDRAM_START                         0x00000000
#define SDRAM_NUMBER_OF_BLOCKS              1
#define SDRAM_BLOCK_SIZE                    0x02000000
#define SDRAM_BLOCK_START_BOUNDARY          0x00000000

#else /* CONFIG_DISCONTIGMEM */

#ifdef CONFIG_ARCH_EP9301

/*
 * The 9301 memory map doesn't have regular gaps because two
 * address pins aren't connected - see asm-arm/mach-ep93xx/arch.c to
 * see how it is.
 */
#define SDRAM_START                         0x00000000
#define SDRAM_NUMBER_OF_BLOCKS              4
#define SDRAM_BLOCK_SIZE                    0x00800000
#define SDRAM_BLOCK_START_BOUNDARY          0x01000000

#else /* CONFIG_ARCH_EP9312 or CONFIG_ARCH_EP9315 */

/* 
 * 2 32Meg blocks that are located physically at 0 and 64Meg. 
 */
#define SDRAM_START                         0x00000000
#define SDRAM_NUMBER_OF_BLOCKS              2
#define SDRAM_BLOCK_SIZE                    0x02000000
#define SDRAM_BLOCK_START_BOUNDARY          0x04000000

#endif

/*
 * Here we are assuming EP93xx is configured to have two 32MB SDRAM 
 * areas with 32MB of empty space between them.  So use 24 for the node 
 * max shift to get 64MB node sizes.
 */
#define NODE_MAX_MEM_SHIFT	26
#define NODE_MAX_MEM_SIZE	(1<<NODE_MAX_MEM_SHIFT)

#endif /* CONFIG_DISCONTIGMEM */


/*
 * MEM_SIZE and PHYS_OFFSET are used to set size of SDRAM for
 * initial page table in arch/arm/kernel/setup.c
 * For ep93xx, PHYS_OFFSET is set to be SDRAM_START.
 */
#define MEM_SIZE                            (SDRAM_BLOCK_SIZE)

/*
 * If memory is not discontiguous, this is #defined	in 
 * arch/arm/mm/init.c to be 1.
 */
#ifdef CONFIG_DISCONTIGMEM
#define NR_NODES 							24
#endif

/*
 * Where to load the ramdisk (virtual address, not physical) and how 
 * big to make it. (used in arch/arm/kernel/setup.c
 * In both cases, when redboot loads the ramdisk image to 0x01000000,
 * the processor will find it because the linux map is funny.
 */
#ifdef CONFIG_ARCH_EP9301
#define RAMDISK_START_VIRT      (0xC4000000)
#else
#define RAMDISK_START_VIRT      (0xC1000000)
#endif

/*
 * The ramdisk size comes from a make menuconfig option.
 */
#define RAMDISK_SIZE            ((CONFIG_BLK_DEV_RAM_SIZE)<<10)
 
/*
 * Task size: 2GB (from 0 to base of IO in virtual space)
 */
#define TASK_SIZE		(0x80000000UL)
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

/*
 * Page offset: 3GB (start of kernel memory in virtual space)
 * Phys offset: 0   (start of kernel memory in physical space)
 */
#define PAGE_OFFSET		(0xC0000000UL)
#define PHYS_OFFSET		(SDRAM_START)

#ifndef __ASSEMBLY__
static inline unsigned long __virt_to_phys(unsigned long vpage) {
	unsigned long block = vpage >> 24;
	unsigned long offset = vpage & 0x00ffffffUL;

	switch(block) {
	case 0xc8:
		return (0xc0000000UL + offset);
	case 0xc9:
		return (0xc1000000UL + offset);
	case 0xca:
		return (0xc4000000UL + offset);
	case 0xcb:
		return (0xc5000000UL + offset);
	case 0xcc:
		return (0xd0000000UL + offset);
	case 0xcd:
		return (0xd1000000UL + offset);
	case 0xce:
		return (0xd4000000UL + offset);
	case 0xcf:
		return (0xd5000000UL + offset);
	case 0xd0:
		return (0xe0000000UL + offset);
	case 0xd1:
		return (0xe1000000UL + offset);
	case 0xd2:
		return (0xe4000000UL + offset);
	case 0xd3:
		return (0xe5000000UL + offset);
	case 0xd4:
		return (0xe2000000UL + offset);
	case 0xd5:
		return (0xe3000000UL + offset);
	case 0xd6:
		return (0xe6000000UL + offset);
	case 0xd7:
		return (0xe7000000UL + offset);
	case 0xc0:
	case 0xc1:
	case 0xc2:
	case 0xc3:
	case 0xc4:
	case 0xc5:
	case 0xc6:
	case 0xc7:
	default:
		return (vpage - 0xc0000000UL);
	}
}

static inline unsigned long __phys_to_virt(unsigned long ppage) {
	unsigned long block = ppage >> 24;
	unsigned long offset = ppage & 0x00ffffffUL;

	switch(block) {
	case 0xc0:
		return (0xc8000000UL + offset);
	case 0xc1:
		return (0xc9000000UL + offset);
	case 0xc4:
		return (0xca000000UL + offset);
	case 0xc5:
		return (0xcb000000UL + offset);
	case 0xd0:
		return (0xcc000000UL + offset);
	case 0xd1:
		return (0xcd000000UL + offset);
	case 0xd4:
		return (0xce000000UL + offset);
	case 0xd5:
		return (0xcf000000UL + offset);
	case 0xe0:
		return (0xd0000000UL + offset);
	case 0xe1:
		return (0xd1000000UL + offset);
	case 0xe4:
		return (0xd2000000UL + offset);
	case 0xe5:
		return (0xd3000000UL + offset);
	case 0xe2:
		return (0xd4000000UL + offset);
	case 0xe3:
		return (0xd5000000UL + offset);
	case 0xe6:
		return (0xd6000000UL + offset);
	case 0xe7:
		return (0xd7000000UL + offset);
	case 0x0:
	case 0x1:
	case 0x2:
	case 0x3:
	case 0x4:
	case 0x5:
	case 0x6:
	case 0x7:
	default:
		return (ppage + 0xc0000000UL);
	}
}

/*
 * Given a page frame number, convert it to a node id.
 */
static inline unsigned long PFN_TO_NID(unsigned long pfn) {
	unsigned long block = (pfn >> 12); 

	switch(block) {
	case 0x0:
		return 0;
	case 0x1:
		return 1;
	case 0x2:
		return 2;
	case 0x3:
		return 3;
	case 0x4:
		return 4;
	case 0x5:
		return 5;
	case 0x6:
		return 6;
	case 0x7:
		return 7;
	case 0xc0:
		return 8;
	case 0xc1:
		return 9;
	case 0xc4:
		return 10;
	case 0xc5:
		return 11;
	case 0xd0:
		return 12;
	case 0xd1:
		return 13;
	case 0xd4:
		return 14;
	case 0xd5:
		return 15;
	case 0xe0:
		return 16;
	case 0xe1:
		return 17;
	case 0xe4:
		return 18;
	case 0xe5:
		return 19;
	case 0xe2:
		return 20;
	case 0xe3:
		return 21;
	case 0xe6:
		return 22;
	case 0xe7:
		return 23;
	default:
		return 0xff;
	}
}
#endif

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *              address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *              to an address that the kernel can use.
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(x)	 __virt_to_phys(x)

#define __bus_to_virt__is_a_macro
#define __bus_to_virt(x)	 __phys_to_virt(x)


/*
 * Note that this file is included by include/asm-arm/memory.h so 
 * the macros in this file have to play nice with those.
 */
#ifdef CONFIG_DISCONTIGMEM

/*
 * Given a kernel address, find the home node of the underlying memory.
 */
#define KVADDR_TO_NID(addr) \
		((unsigned long)(PFN_TO_NID(__virt_to_phys((unsigned long)addr) >> PAGE_SHIFT)))

/*
 * Given a page frame number, convert it to a node id.
 */
#if 0
#define PFN_TO_NID(pfn) \
	(((pfn) < (0x10000000 >> PAGE_SHIFT)) ? \
	 (((pfn) - PHYS_PFN_OFFSET) >> (NODE_MAX_MEM_SHIFT - PAGE_SHIFT)) \
	 : \
	 ((((pfn) - (0xc0000000 >> PAGE_SHIFT)) >> (NODE_MAX_MEM_SHIFT - PAGE_SHIFT)) + 4))
#endif

/*
 * Given a kaddr, ADDR_TO_MAPBASE finds the owning node of the memory
 * and returns the mem_map of that node.
 */
#define ADDR_TO_MAPBASE(kaddr) NODE_MEM_MAP(KVADDR_TO_NID((unsigned long)(kaddr)))

#define PFN_TO_MAPBASE(pfn)	NODE_MEM_MAP(PFN_TO_NID(pfn))

/*
 * Given a kaddr, LOCAL_MAR_NR finds the owning node of the memory
 * and returns the index corresponding to the appropriate page in the
 * node's mem_map.
 */
#define LOCAL_MAP_NR(kaddr) \
	(((unsigned long)(kaddr) & (0xffffffUL)) >> PAGE_SHIFT)

#endif /* CONFIG_DISCONTIGMEM */

#endif

