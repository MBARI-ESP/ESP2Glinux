/*
 *  linux/include/asm-arm/arch-integrator/serial.h
 *
 *  Copyright (C) 1999 ARM Limited
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
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/arch/platform.h>
#include <asm/irq.h>

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD (1843200 / 16)

#define IRQ_UARTINT0  	IRQ_EXT1
#define IRQ_UARTINT1	 IRQ_EXT3

#define _SER_IRQ0	IRQ_UARTINT0
#define _SER_IRQ1	IRQ_UARTINT1

#define RS_TABLE_SIZE

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

     /* UART CLK        PORT  IRQ     FLAGS        */
#define STD_SERIAL_PORT_DEFNS \
	{ 0, BASE_BAUD, TS7XXX_TTYS0, _SER_IRQ1, STD_COM_FLAGS },	/* ttyS0 */	\
	{ 0, BASE_BAUD, TS7XXX_TTYS1, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS1 */ \
	{ 0, BASE_BAUD, TS7XXX_TTYS2, _SER_IRQ1, STD_COM_FLAGS },	/* ttyS2 */	\
	{ 0, BASE_BAUD, TS7XXX_TTYS3, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS3 */ \
	{ 0, BASE_BAUD, TS7XXX_TTYS4, _SER_IRQ1, STD_COM_FLAGS },	/* ttyS4 */ \
	{ 0, BASE_BAUD, TS7XXX_TTYS5, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS5 */ \
        { 0 },                                                          /* ttyS6 */ \
        { 0 },                                                          /* ttyS7 */ \


//additional serial ports for MBARI's Octal USART -- 3/28/05 Brent@mbari.org
#define XR_BAUD (14745600 / 16)                //The XR788's baud rate generator
#define XR_BASE (TS7XXX_IO8_BASE + 0x01C00400) //base chip @0x400 in 8-bit I/O space

#define EXTRA_SERIAL_PORT_DEFNS \
	{ 0, XR_BAUD, XR_BASE+0x00, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS8 */    \
	{ 0, XR_BAUD, XR_BASE+0x10, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS9 */    \
	{ 0, XR_BAUD, XR_BASE+0x20, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS10 */   \
	{ 0, XR_BAUD, XR_BASE+0x30, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS11 */   \
	{ 0, XR_BAUD, XR_BASE+0x40, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS12 */   \
	{ 0, XR_BAUD, XR_BASE+0x50, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS13 */   \
	{ 0, XR_BAUD, XR_BASE+0x60, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS14 */   \
	{ 0, XR_BAUD, XR_BASE+0x70, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS15 */   \
        
#endif
