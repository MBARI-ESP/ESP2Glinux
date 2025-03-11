#shared utilites for bringing network links up and down
# -- brent@mbari.org  3/11/25

. /usr/share/netutils.sh

USBportOn() {
  return 1
}
USBportOff() {
  :
}
USBportState() {
  return 99
}

type ykushcmd >/dev/null 2>&1 && {
  USBportOn() {  #$1=port#
    ykush -u $1
  }
  USBportOff() {
    ykush -d $1
  }
  USBportState() {
  #output contains ON, OFF, or neither if state unknown
    ykush -g $1
  }
  ykush() {
    tryReset=${USBresetDelay:+reset}
    for try in 1 2 $tryReset $tryReset; do
      txt=`ykushcmd "$@"` &&
      case "$txt" in
        *Unable*)  #try to hide ykushcmd transient errors
          if [ "$try" = reset ]; then
            resetUSBunlessBusy $USBresetDelay
            sleep $USBresetDelay
          else
            sleep $try
          fi
       ;;
        *)
          break
      esac
    done
    echo "$txt"
  }
}

bringDown() {
#bring down specified $1=interface, $2=USB port #
  ifdown $1
  [ "$2" ] && USBportOff $2
}

bringUp() {
#bring up specified $1=interface, $2=USB port #, $3=delay
  [ "$2" ] && {
    USBportOn $2 && {
      [ "$3" ] || return 0  #if no delay, assume udev rule will bring iface up
    }
    sleep $3
  }
  ifup $1 || {
    logger -t $prog -p daemon.warning "Failed to bring up $1"
    return 1
  }
}

bringUpCell() {
#$1=optional delay before re-starting sat
  if [ "$cellIface" ]; then
    bringUp "$cellIface" "$cellUSBport" "$cellDelay"
  else
    [ "$1" ] && sleep $1
    bringUp "$satIface" "$satUSBport" "$satDelay"
  fi
}
bringUpSat() {
#$1=optional delay before re-starting cell
  if [ "$satIface" ]; then
    bringUp "$satIface" "$satUSBport" "$satDelay"
  else
    [ "$1" ] && sleep $1
    bringup  "$cellIface" "$cellUSBport" "$cellDelay"
  fi
}

bringDownCell() {
  bringDown "$cellIface" "$cellUSBport"
}
bringDownSat() {
  bringDown "$satIface" "$satUSBport"
}

startCell() {
  [ "$satUSBport" -a "$cellIface" ] && USBportOff "$satUSBport"
  bringUpCell
}

cellState() {
  USBportState "$cellUSBport"
}
satState() {
  USBportState "$satUSBport"
}
