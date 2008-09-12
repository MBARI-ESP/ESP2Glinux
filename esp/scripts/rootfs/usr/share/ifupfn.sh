#Common functions for bringing up network interfaces
# -- revised: 9/11/08 brent@mbari.org
#
HOSTS=/etc/hosts

usage ()
{
    echo "
usage:  ifup [netInterface|full path to definition file] {mode}
        configure and enable the specified network interface
        only bring up DEVICE if its AUTOSTART mode matches regex {mode}
        or Yes or No if the mode parameter is omitted
"
    return 1
}

ifup_function ()
{
    [ "$DEVICE" ] || {
      echo "Network DEVICE to start was not specified"
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
#      echo -e "Skipping $DEVICE because its AUTOSTART mode does not match
#         $startMode" >&2
set +f
      return 201
    done
set +f
    fn=/var/run/*$DEVICE.pid
    pidfns=`echo $fn`
    [ "$pidfns" = "$fn" ] || {  #check for active locks...
      unset owners
      for pidfn in $pidfns; do  #while removing stale ones
        owner=`cat $pidfn` && {
          if kill -0 $owner 2>/dev/null; then
            owners="$owners $owner"
          else
            rm $pidfn  #remove stale pidfile
          fi
        }
      done
      [ "$owners" ] && {
        echo "$DEVICE is already in use by process: $owners"
        return 0
      }
    }
    echo "Bringing up interface $DEVICE ..."
    export DEVICE IPADDR MTU BROADCAST GATEWAY NETWORK NETMASK NSpostCmd
    eval $NSprepCmd    #do config's name service preparations  
    [ "$IPADDR" ] && . /usr/share/ipinit.sh
    case "$BOOTPROTO" in
      dhcp*)
        daemon=/sbin/udhcpc  #only use this dhcp client
        if test -x $daemon  ; then
          pidfn=/var/run/udhcpc-$DEVICE.pid
          if [ -r $pidfn ]; then
            kill `cat $pidfn` 2>/dev/null
          else
            mkdir -p `dirname $pidfn`
          fi
          echo -n "Determining IP configuration for $DEVICE...."
	  insmod af_packet >/dev/null 2>&1
          mode=${BOOTPROTO#dhcp-}
          [ "$mode" = "$BOOTPROTO" ] && mode=n
          [ "$DHCPNAME" ] && DHCPNAME="-H $DHCPNAME"
          $daemon -p $pidfn $DHCPNAME -$mode -i $DEVICE ||
            echo "DHCP failed:  $interface IP=$IPADDR $mask $cast"
        else
            echo "No $daemon client daemon installed!"
        fi
      ;;
      *)
        [ "$IPADDR" ] && echo "$DEVICE IP=$IPADDR $mask $cast"
      ;;
    esac
}
