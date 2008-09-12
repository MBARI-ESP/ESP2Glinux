#ipdown.sh -- tear down IP interface per shell environment variables:
#  DEVICE = network device

resolv=/etc/resolv.conf
oldHosts=/var/run/oldHosts.$DEVICE

if [ "`head -1 $resolv`" = "#$DEVICE" ]; then
  > $resolv
  [ -r $oldHosts ] && cp $oldHosts /etc/hosts
fi
