daemon=darkhttpd
exePath=/usr/local/sbin
pidFile=/var/run/$daemon.pid
OPTIONS="/var/log --daemon --pidfile $pidFile --uid www --gid mbari"
OPTIONS="$OPTIONS --mimetypes /usr/local/share/mime.types"  

case "$1" in
    start)
       echo "Starting $daemon..."
       $exePath/$daemon $OPTIONS
       ;;
                         
    restart)
      $0 stop 
      $0 start
       ;;
       
    stop)
       echo "Stopping $daemon"
       if [ -s $pidFile ]; then
         kill `cat $pidFile`
         rm -f $pidFile
       else
         killall $daemon
       fi
       ;;
       
    *)
       echo "usage: start|restart|stop"
       ;;
esac
