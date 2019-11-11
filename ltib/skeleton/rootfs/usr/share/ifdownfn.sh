#Common functions for bringing down network interfaces
# -- revised: 11/10/19 brent@mbari.org

. /usr/share/netutils.sh

closeTunnel() {
  #signal tunnel deamons that interface will close soon
  for tunFn in /var/run/tunnel*.pid; do
    [ -s "$tunFn" ] && {
      tun=`cat $tunFn` && {
        kill -USR1 $tun && {
          for t in 9 8 7 6 5 4 3 2 1 0; do  #wait for tunnel daemon to die
            sleep 1
            kill -0 $tun 2>/dev/null || break
          done
        }
      }
    }
  done
}

ifAliasDown() {
  fn=/var/run/*$1.pid
  pidfns=`echo $fn`
  [ "$pidfns" = "$fn" ] && {
    isUp $1 || return 0
  }
  echo "Shutting down interface $1 ..."
  [ "$(topIf)" == "$1" ] && closeTunnel
  for pidfn in $pidfns; do
    daemon=`head -n1 $pidfn 2>/dev/null`
    [ "$daemon" ] && {
      case $pidfn in
        *dhcp*-*) signal=HUP ;;
        *) signal=TERM ;;
      esac
      for try in 1 2 last; do
        kill -$signal $daemon 2>/dev/null  #relinquish any lease
        for t in 1 2 3 4 5; do  #wait for daemon to die
          sleep 1
          kill -0 $daemon 2>/dev/null || break 2
        done
        [ "$try" = last ] && {
          echo "Forcing $1 (PID $daemon) to terminate" >&2
          rm $pidfn
          kill -9 $daemon
        }
      done
      #ppp will delete its own .pid files
      [ "$signal" != TERM ] && rm -f $pidfn
    }
  done
  ifconfig $* 2>/dev/null
  hostDown $1
  gateDown $1
}

ifDown() {
  case "$1" in
    *:*) #an interface alias
      ifAliasDown $1 down
    ;;

    *) #anything else must be the name of an interface
      [ "$2" = "force" ] && eachAlias ifAliasDown $1 down
      op=0
      isUp ^$1: || op=down  #force only if no aliases remain
      ifAliasDown $1 $op
  esac
}
