#common ppp modem configuraton

isp=lte #default to generic LTE peer
modemTTY=/dev/modem  #should be overridden for each specific modem

#VPN=$ESPshore/shore   #server IP / VPN interface

ifPrep() {  #specify ip address of vpn server/vpn iface name
  ATTACH=${IFALIAS-$IFNAME} \
    pppd call $isp unit ${IFNAME#ppp} $modemTTY${VPN:+" ipparam $VPN"}
}

hosts() {    #add lines before EOS
  cat <<EOS
$(netIfIP $IFNAME $(hostname)-$IFNAME $(hostname)-${IFALIAS-$IFNAME})
$ESPshore ESPshore
EOS
}

pppDown() {
  ckpid=`cat /var/run/inadyn.pid 2>/dev/null` &&
    kill -CONT $ckpid
}
