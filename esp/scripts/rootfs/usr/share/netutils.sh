#Common networking utilities
# -- revised: 9/1/09 brent@mbari.org
#

ipUp() {
# set up basic internet protocol items per shell environment variables:
#  IFNAME = network device
#  IPADDR = internet address
#  BROADCAST = broadcast IP address
#  NETMASK = subnetwork mask
#  NETWORK = IP subnet (to add explicit route)
#  GATEWAY = default gateway's IP address
#  MTU = Maximum Transmit Unit
  local mask= cast=+  #let ifconfig derive these by default
  [ "$NETMASK" ] && mask=" netmask $NETMASK"
  [ "$BROADCAST" ] && cast="$BROADCAST"
  [ "$MTU" ] && mtu=" mtu $MTU"
  ifconfig $IFNAME $IPADDR$mask broadcast $cast$mtu || {
    echo "FAILED:  ifconfig $IFNAME $IPADDR$mask broadcast $cast$mtu"
    return 2
  }
  [ "$NETWORK" ] && route add -net $NETWORK $mask dev $IFNAME
  gateUp $IFNAME $GATEWAY
  hostsUp $IFNAME
}

ipDown() {
# tear down IP interface per shell environment variables:
#  IFNAME = network device
  local netIface=${1:-$IFNAME}
  hostDown $netIface
  gateDown $netIface
  gateUp
  hostsUp
}


gateUp() {
  # add any specified device with gateways
  # then activate the appropriate gateway with its associated resolv.conf
  mkdir -p /var/run/resolv && cd /var/run/resolv || {
    echo "Cannot access /var/run/resolv directory" 2>&1
    return 1
  }
  local interface RESOLV_IF resolvDev gateways ifs ifs2 topIface newIface=$1
  [ "$1" ] && {
    echo "#$*"  #store device and gateway in leading comment of its resolv.conf
    type resolv_conf >/dev/null 2>&1 && resolv_conf
  } > $newIface
  local priorityFn=/etc/sysconfig/gateway.priority
  unset topIface
  { #read up to two lines of net interface types from $priorityFn
    if read ifs; then
      for interface in $ifs; do  #first try only ifaces with gateways
        [ -r "$interface" ] && {
          RESOLV_IF=/var/run/resolv/$interface
          read resolvDev gateways < $RESOLV_IF
          if [ "$resolvDev" = "#$interface" ]; then
            [ "$gateways" ] && {
              topIface=$interface; break
            }
          else
            echo "$RESOLV_IF -- should begin with #$interface" 2>&1
          fi
        }
      done
      [ "$topIface" ] || {  #could not find any active net iface with a gateway
        read ifs2  #optional 2nd line specifies priority for ifaces w/o gateways
        : ${ifs2:=$ifs}  #reuse 1st line if 2nd is blank
        for interface in $ifs2; do
          [ -r "$interface" ] && {
            RESOLV_IF=/var/run/resolv/$interface
            read resolvDev gateways < $RESOLV_IF
            [ "$resolvDev" = "#$interface" ] && {
              topIface=$interface; break
            }
            echo "$RESOLV_IF should begin with #$interface" 2>&1
          }
        done
      }
      : ${topIface:=$newIface}  #use newIf if no prioritized net iface found
      if [ "$topIface" ]; then
        setGateways $topIface $gateways
      else
        echo "No prioritized net interfaces up -- gateway unchanged" 2>&1
      fi
    else
      echo "Blank or missing $priorityFn" 2>&1
      return 3
    fi
  } <$priorityFn
}
  
gateDown() {
  rm -f /var/run/resolv/$1
}



hostsUp() {
  #update iface specific hosts file and merge with those of other ifaces
  #first adds interface specific hosts file if current iface specified
  [ "$1" ] && type hosts 2>&1 >/dev/null && hosts > /var/run/$1.hosts
  {
    echo "$(netIfIP $(topIf)) $(hostname)"
    cat /var/run/*.hosts
  } >/etc/hosts 2>/dev/null
  :
}

hostDown() {
  rm -f /var/run/$1.hosts
}


setGateways() {
  #update resolv.conf from device $1 and set up default gateways per following args
  local gateway netIface=$1
  cat /var/run/resolv/$netIface > /etc/resolv.conf &&
  if [ "$2" ]; then  #replace all default routes
    shift
    while route del default gw 0.0.0.0 2>/dev/null; do
      :
    done
    for gateway; do
       route add default gw $gateway dev $netIface
    done
  else  #delete only this interface's default routes
    while route del default gw 0.0.0.0 dev $netIface 2>/dev/null; do
      :
    done
  fi
}


defaultRoutes() {
  #output ip addresses of default routes for specified net interface
  #for all if $1 omitted
  local zero defRoute junk
  route -n | grep "^0\.0\.0\.0 .* $1" | {
    while read zero defRoute junk; do
      echo $defRoute
    done
  }
}

searchDomains() {
  #output list of search domains prefixed by any specified
  local searchDomFn=/etc/DOMAINS
  [ -r $searchDomFn ] && {
    [ "$1" ] && {
      {
        echo -n " " && cat $searchDomFn && echo -n " "
      } | grep -q " $* " || echo -n "$* "
    }
    cat $searchDomFn
  }
}

netIfIP() {
  #output the IP address of the specified network interface
  #if there's a valid IP, any addition args are also output
  ifconfig $1 2>/dev/null | tr : " " | {
    local ignore inet addr ip
    read ignore
    read inet addr ip ignore
    shift
    [ "$addr" = "addr" ] && echo $ip $*
  }
}

topIf() {
  #output the name of the top priority network interface
  local iface
  read iface gateways < /etc/resolv.conf
  echo ${iface###}
}

    
