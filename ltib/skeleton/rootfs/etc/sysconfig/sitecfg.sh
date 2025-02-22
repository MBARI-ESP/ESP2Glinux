#Site specific networking utilities & definitions
# -- revised: 2/22/25 brent@mbari.org

ESPshore=134.89.2.91  #ESPshore.mbari.org
wg2shore=wg2shore     #name of wireguard interface to shore

#configuration for rerouteESPshore script
cellIface=cdc1	#cell modem
cellUSBport=1	#port # on yepkit hub
cellDelay=	#cell modem connects automatically when powered

satIface=ppp7	#certus modem
satUSBport=3	#port # on ypkit hub
satDelay=45	#delay between applying power and ifup

USBresetDelay=2 #number of seconds to power off USB when yepkit hub missing

closeTunnels() {
  #signal tunnel deamons that interface will close soon
  local tunFn tunPID smtpPID
  for tunFn in /var/run/tunnel*.pid; do
    [ -s "$tunFn" ] && {
      tunPID=`cat $tunFn` && {
        smtpPID=`fuser 25/tcp 2>/dev/null` && kill -USR1 $tunPID && {
          local secs=20 tunnel=`basename ${tunFn%\.pid}`
          echo "Closing $tunnel"
          #wait for process listening on SMTP port to die
          while let secs--; do
            sleep 1
            kill -0 "$smtpPID" 2>/dev/null || break
          done
        }
      }
    }
  done
  >/var/run/tunGate  #next gateUpdated must reopen tunnels
}

optimizeWg2shore() {
#optimize the keepalive interval for the current network
  ifaceExists $wg2shore || return
  shoreIf=`routeTo $ESPshore` || return
  aliveInterval=25
  case "$shoreIf" in
    ppp7)  #certus
    	aliveInterval=145
  esac
  peerPubKey=`wg show $wg2shore peers`
  wg set $wg2shore peer $peerPubKey persistent-keepalive $aliveInterval
}

gateUpdated() {
#invoked just after gateway interface may have been updated
  local gate tunGate tunFn tunPID
  read -r gate 2>/dev/null </etc/resolv.conf
  read -r tunGate 2>/dev/null </var/run/tunGate
  [ "$tunGate" != "$gate" ] && { #tunnel gateway changed
    echo "$gate" >/var/run/tunGate
    tunFn=/var/run/tunnel2shore.pid
    #cause inittab to restart tunnel2shore
    tunPID=`cat $tunFn 2>/dev/null` && [ "$tunPID" ] && kill -HUP $tunPID

    iptables -t nat -F &&
    outIf=`topIf` && extIP=`netIfIP $outIf` &&
    iptables -t nat -A POSTROUTING -o $outIf -j SNAT --to $extIP

    (sleep 1; /etc/init.d/nfsmount start) &
  }
  optimizeWg2shore
  return 0
}

if false; then  #do *not* close tunnels when gateway iface changes
gateChange() {
#invoked just before gateway interface may be updated
#$1 is the new gateway interface or "" if topIf is going down
  local tunGate
  read -r tunGate 2>/dev/null </var/run/tunGate
  [ "$tunGate" ] && {
    read -r newGate 2>/dev/null </var/run/resolv/$1
    [ "$tunGate" != "$newGate" ] && closeTunnels
  }
}
fi
