#!/bin/sh

# global variables
logfile=/home/osmc/X/remoted-log.txt
pidfile=/home/osmc/X/remoted.pid

# command to run the loop
main() {
	# prevent stopping process when terminal closes
	trap "" HUP
	# save current pid to lockfile
	echo $$ > $pidfile
	echo "Entering remote control daemon main loop PID: $$"
	
	cd /home/osmc/X/
	exec nice -n -19 ./remote /dev/ttyUSB0 /dev/input/event0
}

# check if daemon is running
check() {
	# check if PID file exists
	if [ -f $pidfile ]; then
		pid=$(cat $pidfile)

		# check if saved PID is running
		(kill -s 0 $pid) 2> /dev/null
		
		if [ $? = 0 ]; then
			echo "Daemon is running on PID $pid"
			status=1
			return 1
		fi
	fi
	
	# does not exist
	echo "Daemon not running"
	status=0
	return 0
}

# start daemon process
start(){
	# before starting a new one, check if not running already
	check
	if [ $status -eq 1 ]; then
		echo "Running - Cannot start another"
		exit 1
	else
		rm $pidfile
	fi

	# start daemon in background & redirect stdout
	su -c "($0 main &) &> $logfile" &

	# wait for PID to show
	while [ ! -f $pidfile ]; do
		sleep 0.1
	done

	# inform what the PID is for the daemon process
	echo "Daemon started on PID " $(cat $pidfile)
	exit 0
}

# command to stop daemon if running
stop() {
	# check if a PID file exists
	if [ -f $pidfile ]; then
		pid=$(cat $pidfile)
		# try kill PID and present possible errors
		(kill $pid) 2> /dev/null
		case $? in
			0) echo "Daemon PID $pid stopped"; exit 0 ;;
			1) echo "Daemon not running - PID file outdated"; exit 1 ;;
			*) echo "Error $? in 'kill' "; exit 1 ;;
		esac
	else
		echo "Daemon not running - PID file does not exist"
		exit 1
	fi
}

# Guarantee process running as root
currentuser=$(whoami)
rootpsw="lpi"
echo "Current user is $currentuser"

# If not root, then restart the process
if [ $currentuser != root ]; then
	echo "Restarting process as root"
	exec echo $rootpsw | sudo -S su -c "$0 $1" root
	exit 0
fi

# Execute instruction as requested
echo "Command is: $1" #
case $1 in
	start) echo "Start remote control daemon"; start;;
	stop) echo "Stop remote control daemon"; stop ;;
	main) echo "Start main routine"; main ;;
	log) echo "Show log file:"; cat $logfile ;;
	check) check ;;
	*) echo "Unknown command"; exit 1 ;;
esac
