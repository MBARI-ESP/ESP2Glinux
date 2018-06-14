This document describes how to load OS software on the MBARI 
Embedded Artists LPC3141 based boards

First,
Ensure that the 3 boot jumpers are configured to boot from SDCARD. Normally,
 the only change required is move the RED boot jumper in J11 towards the DIMM
J10 should be away from the DIMM and J12 should be towards it.

Plug a serial console into the board being updated via the RS-232 or USB ports.
Set your serial terminal emulator to operate at 115200 baud 
with 8 data bits, one stop bit and no parity.

When the template SDcard is installed on a new CPU DIMM module,
it should boot directly into Linux.
If it does not, try aborting into U-boot with a quick Control-C at

  Hit <ctrl-C> to stop autoboot:

and type the U-Boot command:

  run sdBoot   #to force it to boot Linux from the SD card


Once Linux has booted from the template SDcard, log in as root user...
As the root Linux user, type the command:

#  clone

You must type 'yes' to proceed to overwrite the internal SPI FLASH memory
with a copy of the system software.  Follow the instructions output by
the clone script to complete the required configuration of
the U-Boot bootloader.

#  resetting the environment is necessary only if the CPU DIMM is being reused
#  (but it can't hurt in any case)
# abort boot with Control-C to get into the U-Boot monitor
#  In U-Boot:

run resetEnvironment  #this clears the monitor configuration and reboots

# abort boot with Control-C again to stay in the U-Boot monitor
#  In U-Boot:
run flashSDuboot   #copies U-boot from SD to SPI flash
edit bootcmd            #should be "run sdBoot"
edit:  run flashBoot    #change default boot from SD to internal flash memory

# Note that the Ethernet MAC address is stored in U-boot's parameter memory
# If you don't set the MAC, U-Boot will generate a random one, but
# it may not be globally unique!!!
# Therefore, it's best if you change the MAC to the address printed on the 
# CPU DIMM module, as this is globally registered.
edit ethaddr
ethaddr=00:08:0e:d3:27:69   #this is a typical "random" MAC
ethaddr=00:1a:f1:xx:xx:xx   #global MACs for these boards are prefixed 00:1a:f1


run flashSDkernel  #copies kernel from SD to SPI flash and saves boot parameters

# move the RED Boot jumper back into the "flash" position

reset   #test initial boot from flash

#if system does not boot:   reset, abort the boot, and force it to boot from SD
#with the U-Boot command:

run sdBoot  #see Notes below to restart the configuration process


#Once the system does boot from flash, log in as root to immediately reboot it
# Let linux shutdown.
# Stop the reboot at the U-Boot prompt with Control-C  (or cut power)
# Remove the template SD card (store it somewhere safe :-)

#You should not remove an SD card once it has booted into Linux.
#(no hardware damage results, but the card's filesytems may be corrupted)
#Before proceeding, BE ABSOLUTELY CERTAIN that the template SD card is REMOVED!!

# In U-boot:

reset

# After the system has booted and you've logged in as root
#   Install the (blank) card you will ship with the system
# (the utilities used below work better if run from a network login
#  rather than using the serial console)

cfdisk  -z  /dev/mmcblk0   #the -z says to ignore whatever was on the SD card

# find tutorial about the linux cfdisk command for disk partitioning
#You want to create three primary partitions of the indicated types 
# and sizes below:

   Name      Flags    Part TypeFS Type        [Label]     Size (MB)
 -----------------------------------------------------------------
   mmcblk0p1           Primary W95 FAT32 (LBA)                14.00
   mmcblk0p3           Primary BootIt                          0.37
   mmcblk0p2           Primary Linux                  (rest of disk)
 
# The sizes are not critical.  The first partition stores the linux
#kernel.  It can be as small as 4MB.
# The p3 partition of type 'BootIt' holds the U-Boot boot loader.
# It must be physically before the big Linux parition. 
# You will create the p1 parition first, then the Linux p2 partition.
# Specify a size for the Linux partition that is about 0.5 MB less than
# the maximum remaining available space for it.  When prompted whether
# to allocate this big partition and the begin or end of the space,
# indicate you want it at the end.
# This will leave you a ~0.5MB space remaining just after the FAT32
# partition.  The BootIt partition's minimum size is 0.3MB.

#  The hexidecimal type of the FAT32(LBA) parition is 0C  (12 decimal)
#  The hexidecimal type of the Linux parition is 83
#  The hexidecimal type of the BootIt parition is DF

#  Once you have the table looking correct in cfdisk, write it to disk
#  with the W command, then quit cfdisk.


# Initialize these newly created partitions:
# I like to incorporate something unique into volume labels
# Typically, I'll use the last four digits of the host's 
# hexidecimal ethernet mac address.  To see that use:
#   ifconfig eth0 | grep HW
 
mkdosfs -n ESPBOOT-id  /dev/mmcblk0p1   #where id is a short, unique id
mkfs.ext4  -L  ESPROOT-id  /dev/mmcblk0p2  #use the same short, unique id
tune2fs -c0 -i0  /dev/mmcblk0p2

#Install the u-boot.bin and uImage files to make this SD card bootable
mount /boot
cd
cp boot/* /boot
cp boot/u-boot.bin  /dev/mmcblk0p3   #copies the U-boot loader from flash
rm -rf boot   #optional (saves 3MB space on the 250MB internal flash)

# mount source and destination for populating the linux partition
mount --bind / /flash
mount /card

cp -a  /flash/.  /card        #this takes a couple minutes
mkcardroot /card  ESPSWAP-id  #this takes about 15 seconds

# create a directory for the esp user to write logs and images
mkdir /card/var/log/esp
chown esp:users  /card/var/log/esp

sync
reboot
#complaints about being unable to write files are normal during this reboot

# the system should now be able to boot from either the internal flash
#  or this SD-card (which will also store deployment data)

# At the U-Boot prompt:
run flashBoot # to boot from flash
# or
run sdBoot    # to boot from SD card

# The RED boot jumper ONLY affects from where U-Boot itself is loaded
# The bootcmd variable in U-Boot is determines how the Linux kernel is loaded
#  edit bootcmd to change it to either "run flashBoot" or "run sdBoot"

===============================================================
# Notes:
# If you ever need view the current parameters in U-boot

print

# use the edit command, as in the example above, to edit parameters.
# Then use:

save

# to save them to internal flash memory

run resetEnvironment   #to clear all your changes
#including the global MAC address! 

# the mbariA (and later) versions of U-Boot default to booting Linux
# from the SD card when there are no valid U-Boot parameters in stored in flash.
# Older versions default to trying to load from the flash in this case,
#  which is likely to fail.

