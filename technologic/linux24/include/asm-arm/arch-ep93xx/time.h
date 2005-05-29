/*
 *  linux/include/asm-arm/arch-ep93xx/time.h
 *
 * (c) Copyright 2001 LynuxWorks, Inc., San Jose, CA.  All rights reserved.
 *
 * Copyright (C) 2002-2003 Cirrus Logic, Inc.
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
#include <asm/system.h>
#include <asm/leds.h>
#include <asm/arch/hardware.h>

/* First timer channel used for timer interrupt */

/*
 * IRQ handler for the timer
 */
static void ep93xx_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{

    // ...clear the interrupt
    outl( 1, TIMER1CLEAR );

    do_timer(regs);
    do_profile(regs);
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
extern __inline__ void setup_timer(void)
{
    timer_irq.handler = ep93xx_timer_interrupt;

    /* 
     * Make irqs happen for the system timer
     */
    setup_arm_irq(IRQ_TIMER1, &timer_irq);

    /*
     * Start timer 1, leave others alone
     */
    outl( 0, TIMER1CONTROL );

    /*
     * Since the clock is giving u 2 uSeconds per tick,
     * the timer load value is the timer interval
     * divided by 2. 
     */
    outl( 5085, TIMER1LOAD );
    outl( 5085, TIMER1VALUE);  /* countdown */

    /*
     * Set up Timer 1 for 508 kHz clock and periodic mode.
     */ 
    outl( 0xC8, TIMER1CONTROL );
}

