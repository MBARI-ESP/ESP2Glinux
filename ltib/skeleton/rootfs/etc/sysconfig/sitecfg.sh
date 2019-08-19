#Site specific networking utilities & definitions
# -- revised: 8/18/19 brent@mbari.org
#

ESPshore=134.89.2.91  #ESP shore server

gatewayUpdated() {
#invoked after gateway interface updated
#first argument is previous gateway interface's name
  gate=`topIf` && [ "$gate" -a "$1" != "$gate" ] && {
    tunFn=/var/run/tunnel2shore
    tunPID=`cat $tunFn`
    rm $tunFn
    kill $tunPID  #causes inittab to restart tunnel2shore
  }
  return 0
}
