#Site specific networking utilities & definitions
# -- revised: 11/10/19 brent@mbari.org
#

ESPshore=134.89.2.91  #ESP shore server

gatewayUpdated() {
#invoked after gateway interface may have been updated
  gate=`topIf`
  oldGate=`cat /var/run/topIf 2>/dev/null`
  [ "$oldGate" != "$gate" ] && {
    echo "$gate" >/var/run/topIf
    tunFn=/var/run/tunnel2shore.pid
    #cause inittab to restart tunnel2shore
    tunPID=`cat $tunFn 2>/dev/null` &&
      kill -0 "$tunPID" 2>/dev/null && kill $tunPID
  }
  return 0
}
