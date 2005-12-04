#Common functions for bringing up network interfaces
# -- revised: 11/30/05 brent@mbari.org
#
HOSTS=/etc/hosts

usage ()
{
    echo "usage:  ifup [netInterface|full path to definition file]"
    echo "  configure and enable the specified network interface"
    return 1
}

ifup_function ()
{
    [ "$DEVICE" ] || {
      echo "Network DEVICE to start was not specified"
      usage
      return $?
    }
    ifconfig | grep -q ^$DEVICE && {
      echo "$DEVICE is already UP"
      return 0
    }
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
    [ "$IPADDR" ] && {
      unset mask cast
      [ "$NETMASK" ] && mask="netmask $NETMASK"
      [ "$BROADCAST" ] && cast="broadcast $BROADCAST"
      ifconfig $DEVICE $IPADDR $mask $cast || return 2
      [ "$NETWORK" ] && route add -net $NETWORK $mask dev $DEVICE
      [ "$GATEWAY" ] && route add default gateway $GATEWAY dev $DEVICE
    }
    eval $NSprepCmd    #do config's name service preparations  
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
    return 0
}
