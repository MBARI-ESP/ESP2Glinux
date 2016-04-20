#Common networking utilities
# -- revised: 4/19/16 brent@mbari.org
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
#  VPN = associated VPN server / interface
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
  [ "$NETWORK" ] && route add -net $NETWORK$mask dev $IFNAME && return 3
  gateUp $IFNAME $GATEWAY && hostsUp $IFNAME || return $?
  #also bring up associated VPN interface if this one provides gateway
  [ "$VPN" -a "$GATEWAY" ] && (sleep 2; vpnUp $VPN $IFNAME)&
  return 0
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

vpnUp() {
#if $1 specs "IP/vpn" iface, bring that iface up, unless there is already
#a host route to the vpn server via an interface other than $2.
#Always bring down vpn if its current $carrier is of lower priority than $2
server=`dirname $1` && [ "$server" != . ] &&
  if vpn=`basename $1`; then #check for being carried by another iface
    carrier=`hostIface $server` && lowerGatePriority "$2" "$carrier" && return 0
    ifdown $vpn
    ifup $vpn
  fi
}

lowerGatePriority() {
#return 0 iff interface $1 has a lower gateway priority than $2
  [ "$1" = "$2" -o -z "$2" ] && return 1;
  read -r ifs </etc/sysconfig/gateway.priority || return $?
  for interface in $ifs; do  #consider only interfaces with gateways
    case "$2" in
      "$interface") return 0
    esac
    case "$1" in
      "$interface") return 1
    esac
  done
  echo "lowerGatePriority():  unknown interface '$2'" >&2
  return 9
}

notUnplugged() {
#return 0 if either interface named $1 defines no carrier or it is up
  [ "`cat /sys/class/net/$1/carrier 2>/dev/null`" != 0 ]
}

gateUp() {
  # add any specified device with its gateway IP
  # then activate the appropriate gateway with its associated resolv.conf
  mkdir -p /var/run/resolv && cd /var/run/resolv || {
    echo "Cannot access /var/run/resolv directory" >&2
    return 1
  }
  local interface RESOLV_IF resolvDev gateway ifs ifs2 topIface newIface=$1
  rm -f $newIface
  [ "$2" ] && {
    echo "#$*"  #store device and gateway in leading comment of its resolv.conf
    type resolv_conf >/dev/null 2>&1 && resolv_conf
  } >$newIface
  local priorityFn=/etc/sysconfig/gateway.priority
  unset topIface
  { #read up to two lines of net interface types from $priorityFn
    if read -r ifs; then
      for interface in $ifs; do  #first try only active ifaces with gateways
        [ -r "$interface" ] && notUnplugged "$interface" && {
          RESOLV_IF="/var/run/resolv/$interface"
          read -r resolvDev gateway <$RESOLV_IF
          if [ "$resolvDev" = "#$interface" ]; then
            [ "$gateway" ] && {
              topIface=$interface; break
            }
          else
            echo "$RESOLV_IF -- should begin with #$interface" >&2
          fi
        }
      done
      [ "$topIface" ] || {  #could not find any active net iface with a gateway
        read -r ifs2  #optional 2nd line gives priority for inactive ifaces
        : ${ifs2:=$ifs}  #reuse 1st line if 2nd is blank
        for interface in $ifs2; do
          [ -r "$interface" ] && {
            RESOLV_IF="/var/run/resolv/$interface"
            read -r resolvDev gateway <$RESOLV_IF
            if [ "$resolvDev" = "#$interface" ]; then
              [ "$gateway" ] && {
                topIface=$interface; break
              }
            else
              echo "$RESOLV_IF -- should begin with #$interface" >&2
            fi
          }
        done
      }
      : ${topIface:=$newIface}  #use newIf if no prioritized net iface found
      if [ "$topIface" ]; then
        setGateway $topIface $gateway
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
    hosts >/var/run/$1.hosts || return $?
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
      while read -r dest gateIP mask flags metric ref use iface ignored; do
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
    while read -r zero defRoute junk; do
      echo $defRoute
    done
  }
}

hostIface() {
  #output name of interface associated with host route to specified IP address
  #returns false if no such host route found
  route -n | while read -r dest gate genmask flags metric ref use iface more; do
    [ "$dest" = "$1" -a "$genmask" = 255.255.255.255 ] && {
      echo $iface
      break
    }
  done
}

gateIface() {
  #output name of interface associated w/specified gateway IP route
  #returns false if no such host route found
  route -n | while read -r dest gate genmask flags metric ref use iface more; do
    [ "$gate" = "$1" ] && {
      echo $iface
      break
    }
  done
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
    read -r ignore
    read -r inet addr ip ignore
    shift
    [ "$addr" = "addr" ] && echo $ip $*
  }
}

netIfPtp() {
  #output the IP address of the PtP peer for specified PPP interface
  #if there's a valid peer, any additional args are also output
  ifconfig $1 2>/dev/null | tr : " " | {
    local ignore inet addr ip ptp peer ignore
    read -r ignore
    read -r inet addr ip ptp peer ignore
    shift
    [ "$ptp" = "P-t-P" ] && echo $peer $*
  }
}

topIf() {
  #output the name of the top priority network interface
  local iface
  read -r iface gateway 2>/dev/null </etc/resolv.conf &&
  echo ${iface###}
}

gateIP() {
#output the IP address of the gateway for the given interface
  RESOLV_IF="/var/run/resolv/$1"
  read -r resolvDev gateway 2>/dev/null <$RESOLV_IF &&
  [ "$resolvDev" = "#$1" ] && {
    [ "$gateway" ] || return 1
    echo $gateway
    return 0
  }
  echo "$RESOLV_IF -- should begin with #$interface" >&2
  return 2
}
