#Site specific networking utilities & definitions
# -- revised: 11/12/19 brent@mbari.org

ESPshore=134.89.2.91  #ESP shore server

gateUpdated() {
#invoked just after gateway interface may have been updated
  local gate tunGate tunFn tunPID
  read -r gate 2>/dev/null </etc/resolv.conf
  read -r tunGate 2>/dev/null </var/run/tunGate
  [ "$tunGate" != "$gate" ] && { #tunnel gateway changed
    echo "$gate" >/var/run/tunGate
    tunFn=/var/run/tunnel2shore.pid
    #cause inittab to restart tunnel2shore
    tunPID=`cat $tunFn 2>/dev/null` &&
      kill -0 "$tunPID" 2>/dev/null && kill $tunPID
  }
  return 0
}

gateChange() {
#invoked just before gateway interface may be updated
#$1 is the new gateway interface
  local tunGate
  read -r tunGate 2>/dev/null </var/run/tunGate
  [ "$tunGate" ] && {
    read -r newGate 2>/dev/null </var/run/resolv/$1
    [ "$tunGate" != "$newGate" ] && closeTunnels
  }
}

closeTunnels() {
  #signal tunnel deamons that interface will close soon
  >/var/run/tunGate  #next gateUpdated must reopen tunnels
  local tunFn tun
  for tunFn in /var/run/tunnel*.pid; do
    [ -s "$tunFn" ] && {
      tun=`cat $tunFn` && {
        kill -USR1 $tun && {
          echo "Closing `basename ${tunFn%\.pid}`"
          for t in 9 8 7 6 5 4 3 2 1 0; do  #wait for tunnel daemon to die
            sleep 1
            kill -0 $tun 2>/dev/null || break
          done
        }
      }
    }
  done
}
