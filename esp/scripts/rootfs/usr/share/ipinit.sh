# ipinit.sh -- set up basic internet protocol items per shell environment variables:
#  DEVICE = network device
#  IPADDR = internet address
#  BROADCAST = LAN broadcast IP address
#  NETWORK = LAN IP subnet
#  GATEWAY = default gateway's IP address
#  MTU = Maximum Transmit Unit

unset mask cast
[ "$NETMASK" ] && mask="netmask $NETMASK"
[ "$BROADCAST" ] && cast="broadcast $BROADCAST"
[ "$MTU" ] && mtu="mtu $MTU"
ifconfig $DEVICE $IPADDR $mask $cast $mtu || return 2
[ "$NETWORK" ] && route add -net $NETWORK $mask dev $DEVICE
[ "$GATEWAY" ] && route add default gateway $GATEWAY dev $DEVICE
eval $NSpostCmd    #do config's special routing preparations  
:
