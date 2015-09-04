#Common functions for bringing up network interfaces
# -- revised: 9/2/09 brent@mbari.org
#

. /usr/share/netutils.sh  #networking utilities

usage ()
{
    echo "
usage:  ifup [netInterface|full path to definition file] {mode}
        configure and enable the specified network interface
        only bring up IFNAME if its AUTOSTART mode matches regex {mode}
        or Yes or No if the mode parameter is omitted
"
    return 1
}

ifup_function ()
{
  [ "$IFNAME" ] || {
    echo "Network IFNAME to start was not specified"
    usage
    return 200
  }
set -f
  startMode=${1-n|N|no|No|NO|yes|y|Y|Yes|YES}  #yes, no, or missing
  while :; do
    eval "
      case "$AUTOSTART" in
        $startMode)
          break
      esac
    "
#      echo -e "Skipping $IFNAME because its AUTOSTART mode does not match
#         $startMode" >&2
set +f
    return 0
  done
set +f
  fn=/var/run/*$IFNAME.pid
  pidfns=`echo $fn`
  [ "$pidfns" = "$fn" ] || {  #check for active locks...
    unset owners
    for pidfn in $pidfns; do  #while removing stale ones
      owner=`head -n1 $pidfn` 2>/dev/null && {
        if kill -0 $owner 2>/dev/null; then
          owners="$owners $owner"
        else
          rm $pidfn  #remove stale pidfile
        fi
      }
    done
    [ "$owners" ] && {
      echo "$IFNAME is already in use by process: $owners" >&2
      return 7
    }
  }
  echo "Bringing up interface $IFNAME ..."
  ! type ifPrep >/dev/null 2>&1 || ifPrep && {
    case "$BOOTPROTO" in 
      "")  #unspecified BOOTPROTO defers ipUp
      ;;
      dhcp*)
        [ -z "$IPADDR" ] || ipUp && {
          daemon=/sbin/udhcpc  #only use this dhcp client
          if test -x $daemon  ; then
            pidfn=/var/run/udhcpc-$IFNAME.pid
            if [ -r $pidfn ]; then
              kill `head -n1 $pidfn` 2>/dev/null
            else
              mkdir -p `dirname $pidfn`
            fi
            echo -n "Determining IP configuration for $IFNAME...."
            insmod af_packet >/dev/null 2>&1
            mode=${BOOTPROTO#dhcp-}
            [ "$mode" = "$BOOTPROTO" ] && mode=n
            [ "$DHCPNAME" ] && DHCPNAME="-H $DHCPNAME"
            $daemon -p $pidfn $DHCPNAME -$mode -i $IFNAME ||
              echo "DHCP failed:  $interface IP=$IPADDR"
          else
              echo "No $daemon client daemon installed!"
              false
          fi
        }
      ;;
      static)
        ipUp && echo "$IFNAME IP=$IPADDR"
      ;;
      *)
        echo "Unrecognized BOOTPROTO=\"$BOOTPROTO\"" >&2
        false
      ;;
    esac || return $?
    type ifPost >/dev/null 2>&1 || return 0
    ifPost
  }
}

