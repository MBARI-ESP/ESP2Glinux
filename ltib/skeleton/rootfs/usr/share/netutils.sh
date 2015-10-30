#Common networking utilities
# -- revised: 10/27/15 brent@mbari.org
#

ipUp() {
# set up basic internet protocol items per shell environment variables:
#  IFNAME = network device (required)
#  IPADDR = internet address (required)
#  BROADCAST = broadcast IP address
#  NETMASK = subnetwork mask
#  NETWORK = IP subnet (to add explicit route)
#  GATEWAY = default gateway's IP address
#  MTU = Maximum Transmit Unit
  local mask= cast=
  [ "$NETMASK" ] && {
    mask=" netmask $NETMASK"
    cast=" broadcast +"
  }
  [ "$BROADCAST" ] && cast=" broadcast $BROADCAST"
  [ "$MTU" ] && mtu=" mtu $MTU"
  ipopts="$IPADDR$mask$cast$mtu"
  [ "$ipopts" ] && {
    ifconfig $IFNAME $ipopts || {
      echo "FAILED:  ifconfig $IFNAME $IPADDR$mask$cast$mtu"
      return 2
    }
  }
  [ "$NETWORK" ] && route add -net $NETWORK$mask dev $IFNAME
  gateUp $IFNAME $GATEWAY && hostsUp $IFNAME
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
  # add any specified device with its gateway IP
  # then activate the appropriate gateway with its associated resolv.conf
  mkdir -p /var/run/resolv && cd /var/run/resolv || {
    echo "Cannot access /var/run/resolv directory" >&2
    return 1
  }
  local interface RESOLV_IF resolvDev gateways ifs ifs2 topIface newIface=$1
  rm -f $newIface
  [ "$2" ] && {
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
            echo "$RESOLV_IF -- should begin with #$interface" >&2
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
            echo "$RESOLV_IF should begin with #$interface" >&2
          }
        done
      }
      : ${topIface:=$newIface}  #use newIf if no prioritized net iface found
      if [ "$topIface" ]; then
        setGateway $topIface $gateways
      else
:       echo "No prioritized net interfaces up -- gateway unchanged" >&2
      fi
    else
      echo "Blank or missing $priorityFn" >&2
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
  [ "$1" ] && type hosts >/dev/null 2>&1 && {
    hosts > /var/run/$1.hosts || return $?
  }
  {
    echo "$(netIfIP $(topIf)) $(hostname)"
    cat /var/run/*.hosts
  } >/etc/hosts 2>/dev/null
  :
}

hostDown() {
  rm -f /var/run/$1.hosts
}


setGateway() {
  #update resolv.conf from device $1, then
  #set up default gateway to following gateway IP addresses
  local topIface=$1 gateway=$2
  cat /var/run/resolv/$topIface >/etc/resolv.conf 2>/dev/null
  if [ "$gateway" ]; then  #replace default routes if changed
    route -n | grep "^0\.0\.0\.0 " | {
      del=
      add=$gateway
      while read dest gateIP mask flags metric ref use iface ignored; do
        if [ "$iface" = "$topIface" -a "$gateIP" = "$gateway" ]; then
          unset add
        else
          del="$del $gateIP/$iface"
        fi
      done
      [ "$add" ] && route add default gw $add dev $topIface
      for oldGate in $del; do
        IFS=/; set -- $oldGate; unset IFS
        route del default gw $1 dev $2
      done
    }
  else  #delete only this interface's default routes
    while route del default dev $topIface 2>/dev/null; do
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
  #if there's a valid IP, any additional args are also output
  ifconfig $1 2>/dev/null | tr : " " | {
    local ignore inet addr ip
    read ignore
    read inet addr ip ignore
    shift
    [ "$addr" = "addr" ] && echo $ip $*
  }
}

netIfPtp() {
  #output the IP address of the PtP peer for specified PPP interface
  #if there's a valid peer, any additional args are also output
  ifconfig $1 2>/dev/null | tr : " " | {
    local ignore inet addr ip ptp peer ignore
    read ignore
    read inet addr ip ptp peer ignore
    shift
    [ "$ptp" = "P-t-P" ] && echo $peer $*
  }
}

topIf() {
  #output the name of the top priority network interface
  local iface
  read iface gateways < /etc/resolv.conf
  echo ${iface###}
}
