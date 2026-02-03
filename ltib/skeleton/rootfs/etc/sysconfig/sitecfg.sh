#Site specific networking utilities & definitions
# -- revised: 1/31/26 brent@mbari.org

ESPshore=134.89.2.91  #ESPshore.mbari.org
wg2shore=wg2shore     #name of wireguard interface to shore

#configuration for rerouteESPshore script
cellIface=wwan0	#cell modem (may be cdc1)
cellUSBport=1	#port # on yepkit hub
cellDelay=	#cell modem connects automatically when powered

satIface=ppp7	#certus modem
satUSBport=3	#port # on ypkit hub
satDelay=45	#delay between applying power and ifup

USBresetDelay=4 #number of seconds to power off USB when yepkit hub missing

optimizeWg2shore() {
#optimize the keepalive interval for the current network
  ifaceExists $wg2shore || return
  shoreIf=`routeTo $ESPshore` || return
  aliveInterval=25
  case "$shoreIf" in
    ppp7)  #certus
    	aliveInterval=135
;;
    usb*|rnd*|wwan*|sim*)
    	aliveInterval=55
  esac
  peerPubKey=`wg show $wg2shore peers`
  wg set $wg2shore peer $peerPubKey persistent-keepalive $aliveInterval
}

gateUpdated() {
#invoked just after gateway interface may have been updated
#1st arg is 'up' if new interface just come up, 'down' if IF just went down
  local gate tunGate
  read -r tunGate 2>/dev/null <$run/tunGate
  if gate=`topIf`; then
    [ "$tunGate" = "$gate" ] || { #tunnel gateway changed
      [ "$tunGate" ] && masquerade -D $tunGate
      echo "$gate" >$run/tunGate
      masquerade -A $gate
      #cause inittab to restart tunnel2shore
      local tunPID=`cat $run/tunnel2shore.pid 2>/dev/null` &&
        [ "$tunPID" ] && kill -HUP $tunPID
      optimizeWg2shore

      local lock=$run/nfsmount.auto
      if [ "$1" = up ]; then
        [ -f $lock ] && (  #auto nfsmount in 7 seconds
          exec 8<$lock
          flock --nonblock 8 || exit  #another instance is running
          prev=`cat <&8` && [ "$prev" ] && kill $prev
          echo $$ >$lock
          flock --unlock 8
          sleep 7
          flock --nonblock 8 || exit
          >$lock
          /etc/init.d/nfsmount quietly
        )&
      else  #cellular carriers drop lease if no traffic, so renew it whenever a
        signalDHCPclient renew $gate  #lower priority gateway promoted to topIf
      fi
    }
  else
    [ "$tunGate" ] && masquerade -D $tunGate
    rm -f $run/tunGate
  fi
}
