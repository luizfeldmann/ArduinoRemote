#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <errno.h>
#include <termios.h>

#define dev "/dev/uinput"
#define read_buffer_size 16

typedef struct {
	int KEY_CODE;
	const char *REMOTE_HEX;
	const char *SYSTEM_CALL;
} KEY_MAP_ENTRY;

const KEY_MAP_ENTRY KEYSMAP[] = 
{
	{KEY_ENTER, "75A956A7", ""},
	{KEY_UP, "F24119FE", ""},
	{KEY_DOWN, "B489062B", ""},
	{KEY_LEFT, "C53794B4", ""},
	{KEY_RIGHT, "BC9DF06", ""},
	{KEY_ESC, "BB9BDEE7", ""},
	{KEY_BACKSPACE, "76CF1379", ""},
	{KEY_HOME, "F640360", ""},
	{KEY_STOP, "C332FABB", "xbmc-send --action=\"Stop\""}, // BTN RED
	{KEY_0, "F32F72D7", ""},
	{KEY_1, "C9767F76", ""},
	{KEY_2, "C8155AB1", ""},
	{KEY_3, "B6996DAE", ""},
	{KEY_4, "969AE844", ""},
	{KEY_5, "4AAFAC67", ""},
	{KEY_6, "9C2A936C", ""},
	{KEY_7, "833ED333", ""},
	{KEY_8, "55F2B93", ""},
	{KEY_9, "DE78B0D0", ""}
};

int openinput()
{
	printf("\nStarted creation of virtual input device on %s", dev);
	struct uinput_setup usetup;

	int fd = open(dev, O_WRONLY | O_NONBLOCK);

	if ( fd < 0 )
	{
		fprintf(stderr, "\nError opening device %s", strerror(errno));
		return -1;
	}
	
	printf("\nOpened device %s", dev);

	// configure as a keyboard (EV_KEY)
	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0)
	{
		fprintf(stderr, "\nError configuring device %s EV_KEY", strerror(errno));
		return -1;
	}

	// configure all keys present on the keymap
	for (int i = 0; i < sizeof(KEYSMAP)/sizeof(KEYSMAP[0]); i++)
	{
		printf("\nIR: %s> KEY [%d] CALL: %s; ", KEYSMAP[i].REMOTE_HEX, KEYSMAP[i].KEY_CODE, KEYSMAP[i].SYSTEM_CALL);
		if (ioctl(fd, UI_SET_KEYBIT, KEYSMAP[i].KEY_CODE) != 0)
		{
			fprintf(stderr, "\nError configuring device %s", strerror(errno));
			return -1;
		}
	}

	printf("\nConfigured keys successfully");

	memset(&usetup, 0, sizeof(usetup));
   	usetup.id.bustype = BUS_USB;
   	usetup.id.vendor = 0x1234;
   	usetup.id.product = 0x5678;
   	strcpy(usetup.name, "Remote control input");

   	if (ioctl(fd, UI_DEV_SETUP, &usetup) + ioctl(fd, UI_DEV_CREATE) != 0)
	{
		fprintf(stderr, "\nError creating device %s", strerror(errno));
		return -1;
	}
	
	printf("\nCreated virtual device! %d", fd);

   	return fd;
}

int openserial(char *port)
{
	int fd;
	fd = open(port, O_RDWR | O_NOCTTY ); // read write | no terminal
	if (fd < 0)
	{
		fprintf(stderr, "\nError in opening %s serial device: %s", port, strerror(errno));
		return -1;
	}

        printf("\nOpened device %s successfully\n", port);


	struct termios SerialPortSettings;

	tcgetattr(fd, &SerialPortSettings); // get current attributes


	cfsetispeed(&SerialPortSettings,B9600); // set input baud rate
	cfsetospeed(&SerialPortSettings,B9600); // set output baud rate

	SerialPortSettings.c_cflag &= ~PARENB; // No parity bit
	SerialPortSettings.c_cflag &= ~CSTOPB; // 1 stop bit
	SerialPortSettings.c_cflag |= CS8; // 8 bits per byte
	SerialPortSettings.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control - use only RX and TX
	SerialPortSettings.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

	SerialPortSettings.c_lflag &= ~ICANON;
	SerialPortSettings.c_lflag &= ~ECHO; // Disable echo
	SerialPortSettings.c_lflag &= ~ECHOE; // Disable erasure
	SerialPortSettings.c_lflag &= ~ECHONL; // Disable new-line echo
	SerialPortSettings.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	SerialPortSettings.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

	SerialPortSettings.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	SerialPortSettings.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed


	SerialPortSettings.c_cc[VTIME] = 0; // no delay
	SerialPortSettings.c_cc[VMIN] = 1; // read is blocking of 1 byte


	if((tcsetattr(fd,TCSANOW,&SerialPortSettings)) != 0) // apply settings to serial port
	{
		fprintf(stderr, "\nError in setting attributes: %s", strerror(errno));
		return -1;
	}

        printf("\nSetting serial port attributes OK!");
			
	usleep(1000*1000); // wait for arduino to connect
	tcflush(fd, TCIFLUSH); // flush garbage

	return fd;
}

int emit(int fd, int type, int code, int val)
{
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;
	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	if(write(fd, &ie, sizeof(ie)) < (ssize_t) sizeof(ie))
	{
		fprintf(stderr, "\nWrite event <type:%d, code:%d, value:%d> failed: %s", type, code, val, strerror(errno));
        	return errno;
	}

	//printf("\nwrite event success: %d %d %d", type, code, val);
    	return 0;
}

int key_press(int fd, int key)
{
	int result = 0;

	// key down
	result += emit(fd, EV_MSC, 4, 458761);
	result += emit(fd, EV_KEY, key, 1);
   	result += emit(fd, EV_SYN, SYN_REPORT, 0);

	// key up
	result += emit(fd, EV_MSC, 4, 458761);
   	result += emit(fd, EV_KEY, key, 0);
   	result += emit(fd, EV_SYN, SYN_REPORT, 0);

	if (result != 0)
		fprintf(stderr, "\nError writing key %d", key);
	else
		printf("\nWritten key [%d] on %s", key, dev);

	return result;
}

void closeall(int inputdev, int serialdev)
{
	ioctl(inputdev, UI_DEV_DESTROY);
   	close(inputdev);
	
	close(serialdev);
}

void process_cmd(char cmd[], int inputdev)
{
	for (int i = 0; i < sizeof(KEYSMAP)/sizeof(KEYSMAP[0]); i++) // iterate the keymap
	{
		if (strcmp(cmd, KEYSMAP[i].REMOTE_HEX) != 0) // compare entry to received remote ir code
			continue;

		if (KEYSMAP[i].KEY_CODE > 0) // if a keycode is registered
			key_press(inputdev, KEYSMAP[i].KEY_CODE); // send keypress

		if (strlen(KEYSMAP[i].SYSTEM_CALL) > 0) // if a system call is registered
		{
			printf("\nSystem call: %s", KEYSMAP[i].SYSTEM_CALL);
			system(KEYSMAP[i].SYSTEM_CALL); // send command line
			printf(" ... done!");
		}
	}
}

int main(int argc, char *argv[])
{
	// check correct number of arguments
	if(argc != 2)
	{
        	fprintf(stderr, "\nuse: serialdevice\n");
        	return 1;
    	}

	// open devices
	int inputdev = openinput(); // open input device
	int serialdev = openserial(argv[1]); // open serial port

	// check correct opening of required resources
	if (inputdev < 0 || serialdev < 0)
	{
		fprintf(stderr, "\nPROGRAM ABORTED!");
		closeall(inputdev, serialdev);
		return -1;
	}

	// Main loop
	int bufferpos = 0;
	char recv;	
	char read_buffer[read_buffer_size];

	printf("\nEntering reception loop\n");
	while( read( serialdev, &recv, 1 ) != -1 )
	{
    		if (recv == '\r')
			continue; // skip carret return
		if (recv != '\n')
		{
			read_buffer[bufferpos] = recv; // add received character to the buffer
			bufferpos++;
			continue;
		}

		read_buffer[bufferpos] = '\0'; // finish the string
		bufferpos = 0; // prepare buffer for next message
		
		printf("\nRecv: <%s>", read_buffer);
		process_cmd(read_buffer, inputdev);
	}

	closeall(inputdev, serialdev);

	printf("\nRemote daemon exited!");

	return 0;
}

