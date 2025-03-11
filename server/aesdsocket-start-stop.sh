
case "$1" in
    --help)
        echo "Usage: aesdsocket-start-stop.sh [start|stop]"
        ;;
    start)
        # Start the aesdsocket utility
        /usr/bin/aesdsocket -d
        ;;
    stop)
        # Stop the aesdsocket utility
        pkill aesdsocket
        ;;
    *)
        echo "Usage: aesdsocket-start-stop.sh [start|stop]"
        exit 1
        ;;
esac