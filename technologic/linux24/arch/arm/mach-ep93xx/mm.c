/*
 *  linux/arch/arm/mach-ep93xx/mm.c
 *
 *  Extra MM routines for the Cirrus EP93xx
 *
 *  Copyright (C) 1999,2000 Arm Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *  Copyright (C) 2002-2003 Cirrus Logic, Inc.
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
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/arch/bits.h> 
#include <asm/sizes.h> 
#include <asm/arch/platform.h> 
 
#include <asm/mach/map.h>

/*
 *  IO Map for EP93xx:
 *
 *  8000 0000 - 807f ffff = AHB peripherals  (8 Meg)
 *  8080 0000 - 809f ffff = APB peripherals  (2 Meg)
 *  8900 0000 - 82ff ffff = TS-7XXX 8bit isa io/mem
 *  8b00 0000 - 86ff ffff = TS-7XXX 16bit isa io/mem
 */ 
static struct map_desc ep93xx_io_desc[] __initdata = 
{
    //
    // Virtual Address                Physical Addresss    Size in Bytes  Domain     R  W  C  B Last
    { IO_ADDRESS(EP93XX_AHB_BASE),    EP93XX_AHB_BASE,         SZ_8M    , DOMAIN_IO, 0, 1, 0, 0 },
    { IO_ADDRESS(EP93XX_APB_BASE),    EP93XX_APB_BASE,         SZ_2M    , DOMAIN_IO, 0, 1, 0, 0 },
    { IO_ADDRESS(TS7XXX_IO8_BASE),    TS7XXX_IO8_BASE_PHYS,     SZ_64M , DOMAIN_IO, 0, 1, 0, 0 },
    { IO_ADDRESS(TS7XXX_IO16_BASE),   TS7XXX_IO16_BASE_PHYS,    SZ_64M , DOMAIN_IO, 0, 1, 0, 0 },
    { IO_ADDRESS(TS7XXX_FLASH_BASE),  TS7XXX_FLASH_BASE_PHYS,   SZ_16M, DOMAIN_IO, 0, 1, 0, 0 },
    { IO_ADDRESS(TS7XXX_FLASH2_BASE),  TS7XXX_FLASH2_BASE_PHYS,   SZ_16M, DOMAIN_IO, 0, 1, 0, 0 },
    { IO_ADDRESS(TS7XXX_FLASH3_BASE),  TS7XXX_FLASH3_BASE_PHYS,   SZ_16M, DOMAIN_IO, 0, 1, 0, 0 },
    LAST_DESC
};

void __init ep93xx_map_io(void)
{
	extern int console_loglevel;
	unsigned char stat;

	iotable_init(ep93xx_io_desc);
	stat = inb(TS7XXX_STATUS);
	if ((stat & 0x1) != 0x1) console_loglevel = 4;
	outw(0x4, TS7XXX_ENIO);
}
