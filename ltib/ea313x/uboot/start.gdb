source target.gdb
cd ~/ltib/rpm/BUILD/u-boot-2009.11

define reload
  #this load offset puts it at 0x1102900 -- see jump below
  monitor reset init
  file
  file u-boot
  load u-boot 0xdda29000
end
define restart
  reload
  hbreak lpc313x_init
  set $pc = 0x11029000
  continue
  clear lpc313x_init
  hbreak main_loop
  info break
end
define remake
  make "CROSS_COMPILE=arm-none-linux-gnueabi-"
  restart
end
