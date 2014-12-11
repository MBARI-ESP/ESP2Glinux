# revised:  11/19/14 brent@mbari.org

source target.gdb
cd ~/ltib/rpm/BUILD/u-boot-2009.11

define bootload
  #load bootloader into SRAM
  #uboot must have been built with -Os optimization, or it will not fit
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

define reload
  #we load the code where it needs to run, but, as a result...
  #we must skip the code that normally copies the loader into DRAM
  file u-boot
  load
  thbreak start.S:135
end

define reset
  #we load the code where it needs to run, but, as a result...
  #we must skip the code that normally copies the loader into DRAM
  monitor reset halt
  reload
end

define reinit
  reset
  continue
  set $pc = stack_setup
  thbreak lpc313x_init
  continue
  echo At top of lpc313x_init().  You must SKIP ...copy_boot_image()\n
end

define restart
  reset
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
  if $argc == 0
    make
  end
  if $argc == 1
    make $arg0
  end
  if $argc == 2
    make $arg0 $arg1
  end
  if $argc == 3
    make $arg0 $arg1 $arg2
  end
  if $argc == 4
    make $arg0 $arg1 $arg2 $arg3
  end
  if $argc == 5
    make $arg0 $arg1 $arg2 $arg3 $arg4
  end
end
