# revised:  11/19/14 brent@mbari.org

source target.gdb
cd ~/ltib/rpm/BUILD/u-boot-2009.11

define bootload
  #this load offset puts it at 0x1102900 -- see jump below
  monitor reset halt
  file
  file u-boot
  load u-boot 0xdda29000
end

define startboot
  bootload
  thbreak lpc313x_init
  set $pc = 0x11029000
  continue
end

define restart
  #we load the code where it needs to run
  #but, as a result, we must carefully avoid copying it
  monitor reset halt
  file u-boot
  load
  thbreak start.S:135
  continue
  set $pc = stack_setup
#  thbreak lpc313x_init
#  continue
  thbreak drv_usbtty_init
  continue
  finish
  set $pc = start_armboot
  step
  echo Stopped at top of start_armboot()\n
end
define remake
  make "CROSS_COMPILE=arm-none-linux-gnueabi-"
  restart
end
