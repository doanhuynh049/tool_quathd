#!/bin/sh
SCRIPTNAME=lighttpd.sh
COMMAND=$1
HOMEDIR=$(cd $(dirname $0)/..;pwd)
INCFILE=/tmp/lighttpd.inc
HTTP_PORT=80

start_localapp()
{
    echo "Starting Lighttpd ...";

    rm -f $INCFILE
    touch $INCFILE
    if [ $HTTP_PORT != 80 ] ; then
        echo "\$SERVER[\"socket\"] == \":$HTTP_PORT\" {"        >> $INCFILE
        echo "    server.document-root = \"/opt/rec3/www\""     >> $INCFILE
        echo "    server.dir-listing = \"disable\""             >> $INCFILE
        echo "}"                                                >> $INCFILE
        echo "\$SERVER[\"socket\"] == \":80\" {"                >> $INCFILE
        echo "    \$HTTP[\"remoteip\"] != \"169.254.1.0/24\" {" >> $INCFILE
        echo "            url.access-deny = ( \"\" )"           >> $INCFILE
        echo "    }"                                            >> $INCFILE
        echo "}"                                                >> $INCFILE
    fi

    export LD_LIBRARY_PATH=$HOMEDIR/lib:$LD_LIBRARY_PATH
    lighttpd -f /tmp/lighttpd.conf
    sleep 1
    echo 1 > /sys/class/gpio/MOUT5/value
}

stop_localapp()
{
    echo "Stopping Lighttpd ...";
    killall -9 lighttpd

    rm -f $INCFILE
}

case "$COMMAND" in
start)
    if [ "x$2" != "x" ] ; then 
        HTTP_PORT=$2
    fi
    start_localapp
    ;;
stop)
    stop_localapp 2> /dev/null
	
	if [ "x$2" != "x" ] ; then 
	HTTP_PORT=$2
    fi
    start_localapp
	
    ;;
*)
    echo "Usage: $SCRIPTNAME {start|stop|status|restart}" >&2
    exit 1
    ;;
esac

exit 0

