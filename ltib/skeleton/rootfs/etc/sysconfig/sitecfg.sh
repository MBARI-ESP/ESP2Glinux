#Site specific networking utilities & definitions
# -- revised: 8/18/19 brent@mbari.org
#

ESPshore=134.89.2.91  #ESP shore server

gatewayUpdated() {
#invoked after gateway interface updated
#first argument is previous gateway interface's name
  gate=`topIf` && [ "$1" != "$gate" ] && ( /usr/sbin/tunnel2shore& )
  return 0
}
