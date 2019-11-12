#Common networking utilities
# -- revised: 11/12/19 brent@mbari.org

syscfg=/etc/sysconfig

ipUp() {
# set up basic internet protocol items per shell environment variables:
#  IFNAME = network device (required)
#  IPADDR = internet address
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
  ipopts="${IPADDR-0}$mask$cast$mtu"
  ifconfig $IFNAME $ipopts || {
    echo "FAILED:  ifconfig $IFNAME $ipopts"
    return 2
  }
  [ "$NETWORK" ] && {
    route add -net $NETWORK$mask dev $IFNAME || return 3
  }
  gateUp $IFNAME $GATEWAY && hostsUp $IFNAME || return
  #also bring up associated VPN interface if this one provides gateway
  [ "$VPN" -a "$GATEWAY" ] && vpnUp $VPN $IFNAME
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
#if $1 is a "IP/vpn" iface, bring that iface up, unless it is already up and
#there is already a host route to the vpn server via an interface other than $2.
#Always reconnect vpn if its current $carrier will be replaced by $2
server=`dirname $1` && [ "$server" != . ] &&
  if vpn=`basename $1`; then #check for being carried by another iface
    vpnServer=`netIfIP $vpn` && [ "$vpnServer" = "$server" ] &&
      carrier=`hostIface $server` && {
        gatePriority "$2" "$carrier" || return
      }
    ifDown $vpn
    ifUp $vpn
  fi
}

gatePriority() {
#return 0 if active interface $1 has equal or higher gateway priority than $2
#return 1 if active interface $1 should not effect routes on $2
#return >1 if niether interface is has an active gateway
  [ "$1" = "$2" ] && return
  read -r ifs <$syscfg/gateway.priority || return 6
  local oldPWD=$PWD
  cd /var/run/resolv
  for interface in $ifs; do  #consider only UP interfaces with gateways
    case "$interface" in
      "$1") cd $oldPWD; return 0
    ;;
      "$2") cd $oldPWD; return 1
    esac
  done
  cd $oldPWD
  return 2
}

notUnplugged() {
#return 0 if either interface named $1 defines no carrier or it is up
  [ "`cat /sys/class/net/$1/carrier 2>/dev/null`" != 0 ]
}

isUp() {
#return 0 if interface or alias is configured (UP)
  ifconfig | grep -q ^$1
}

gateUp() {
# add any specified interface ($1) with its gateway IP ($2)
# then activate the appropriate gateway with its associated resolv.conf
# if called without args, restore the highest priority gateway
  local oldPWD=$PWD
  resolv=/var/run/resolv
  cd $resolv 2>/dev/null || {
    mkdir $resolv && cd $resolv || return
  }
  local interface resolvIF resolvDev gateway ifs ifs2 topIface
  [ "$1" ] && {
    rm -f "$1"
    [ "$2" ] && {
      echo "#$*" #store device and gateway in leading comment of its resolv.conf
      type resolv_conf >/dev/null 2>&1 && resolv_conf
    } >"$1"
  }
  local priorityFn=$syscfg/gateway.priority
  unset topIface
  { #read up to two lines of net interface types from $priorityFn
    if read -r ifs; then
      for interface in $ifs; do  #first try only active ifaces with gateways
        [ -r "$interface" ] && notUnplugged "$interface" && {
          read -r resolvDev gateway <$interface
          if [ "$resolvDev" = "#$interface" ]; then
            [ "$gateway" ] && {
              topIface=$interface; break
            }
          else
            echo "$resolv/$interface -- should begin with #$interface" >&2
          fi
        }
      done
      [ "$topIface" ] || {  #could not find any active net iface with a gateway
        read -r ifs2  #optional 2nd line gives priority for inactive ifaces
        : ${ifs2:=$ifs}  #reuse 1st line if 2nd is blank
        for interface in $ifs2; do
          [ -r "$interface" ] && {
            read -r resolvDev gateway <$interface
            if [ "$resolvDev" = "#$interface" ]; then
              [ "$gateway" ] && {
                topIface=$interface; break
              }
            else
              echo "$resolv/$interface -- should begin with #$interface" >&2
            fi
          }
        done
      }
      cd $oldPWD
      : ${topIface:=$1}  #use given interface no prioritized net iface found
      if [ "$topIface" ]; then
        setGateway $topIface $gateway
      else
#       echo "No gateway interfaces up" >&2
        >/etc/resolv.conf
      fi
    else
      echo "Blank or missing $priorityFn" >&2
      cd $oldPWD
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
  [ "$1" ] && type hosts >/dev/null 2>&1 && hosts >/var/run/$1.hosts
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
#update resolv.conf from device $1
#if $2 specified, set device $1's gw route to that IP address
  local topIface=$1 gateway=$2
  gateChange $topIface
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
  read -r iface gateway 2>/dev/null </etc/resolv.conf && echo ${iface###}
}

gateIP() {
#output the IP address of the gateway for the given interface
  resolvIF="/var/run/resolv/$1"
  read -r resolvDev gateway 2>/dev/null <$resolvIF &&
  [ "$resolvDev" = "#$1" ] && {
    [ "$gateway" ] || return 1
    echo $gateway
    return 0
  }
  echo "$resolvIF -- should begin with #$interface" >&2
  return 2
}

gateUpdated() {
#invoked after gateway interface updated
#this dummy fn may be replaced by sitecfg below
:
}
gateChange() {
#invoked just before gateway interface may be updated
#$1 is the new gateway interface
:
}
. $syscfg/sitecfg.sh

eachAlias() {
#do first arg for each of specified interface's alias configurations
#additional args are passed through
  cfg=$syscfg/ifcfg-
  aliases=`echo $cfg$2:*`
  arg1=$1
  [ "$aliases" = "$cfg$2:*" ] || {
    shift 2
    for alias in $aliases; do
       $arg1 ${alias#$cfg} ${@}
    done
  }
}
