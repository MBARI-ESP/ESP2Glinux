#Site specific networking utilities & definitions
# -- revised: 10/31/21 brent@mbari.org

ESPshore=134.89.2.90  #ESPshore2 server

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

gateChange() {
#invoked just before gateway interface may be updated
#$1 is the new gateway interface or "" if topIf is going down
  local tunGate
  read -r tunGate 2>/dev/null </var/run/tunGate
  [ "$tunGate" ] && {
    read -r newGate 2>/dev/null </var/run/resolv/$1
#    [ "$tunGate" != "$newGate" ] && closeTunnels
  }
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
  }
  return 0
}

