#Common functions for bringing up network interfaces
# -- revised: 11/10/05 brent@mbari.org
#
HOSTS=/etc/hosts

usage ()
{
    echo "usage:  ifup [netInterface|full path to definition file]"
    echo "  configure and enable the specified network interface"
    exit 1
}

ifup_function ()
{
    [ "${DEVICE}" ] || {
      echo "Network DEVICE to start was not specified"
      usage
    }
    echo "Bringing up interface $DEVICE ..."
    [ "$IPADDR" ] && {
      unset mask cast
      [ "$NETMASK" ] && mask="netmask $NETMASK"
      [ "$BROADCAST" ] && cast="broadcast $BROADCAST"
      ifconfig $DEVICE $IPADDR $mask $cast || exit 2
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
}
