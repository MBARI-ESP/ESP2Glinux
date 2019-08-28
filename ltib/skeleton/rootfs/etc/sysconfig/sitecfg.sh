#Site specific networking utilities & definitions
# -- revised: 8/27/19 brent@mbari.org
#

ESPshore=134.89.2.91  #ESP shore server

gatewayUpdated() {
#invoked after gateway interface updated
#first argument is previous gateway interface's name
  gate=`topIf` && [ "$gate" ] && {
    [ "$1" != "$gate" ] && {
      tunFn=/var/run/tunnel2shore
      #cause inittab to restart tunnel2shore
      tunPID=`cat $tunFn 2>/dev/null` && {
        rm $tunFn
        kill -0 "$tunPID" 2>/dev/null && kill $tunPID
      }
    }
    echo 127.0.0.1 mail >>/etc/hosts
  }
  return 0
}
