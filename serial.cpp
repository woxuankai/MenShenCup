#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include "serial.h"


enum
{
	OK,
	ERROR,
};

#define REPORT_PACKAGE_HEAD 0x23

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	uint8_t head;
	uint8_t frame_cnt;//帧计数
	uint8_t state;
	int32_t middle_error;
	//离中心线的距离//负值代表线在飞机左边（即飞机需要左移）
	uint8_t checksum;
} report_package_type;
#pragma pack(pop)



report_package_type pkg;
int fd;
int  serial_init(void)
{
    struct termios uart;
    if( (fd=open("/dev/ttyO1", O_RDWR | O_NOCTTY)) < 0 )
        printf("Unable to access uart.\n");

    //get attributes of uart
    if( tcgetattr(fd, &uart) < 0 )
	{
        printf("Failed to get attributes of uart.\n");
		return -1;
    }
    //set Baud rate
    if( cfsetospeed(&uart, B38400) < 0)
	{
        printf("Failed to set baud rate.\n");
		return -1;
	}
    else
        printf("Baud rate: 38400\n");

    //set attributes of uart
    uart.c_iflag = 0;
    uart.c_oflag = 0;
    uart.c_lflag = 0;
    uart.c_cflag &= ~CRTSCTS;
    uart.c_cflag |= CS8;
    tcsetattr(fd, TCSANOW, &uart);
	
	pkg.head = REPORT_PACKAGE_HEAD;
	pkg.frame_cnt = 0;
	
	return 0;
}

int serial_send_int(int x,bool ifget)
{
	pkg.head = REPORT_PACKAGE_HEAD;
	pkg.frame_cnt++;
	if(ifget == true)
	{
		pkg.state = OK;
		pkg.middle_error = x;
	}
	else
	{
		pkg.state = ERROR;
		pkg.middle_error = 0;
	}
	uint8_t sum = 0;
	uint8_t *sum_ptr = (uint8_t *)&pkg;
	for(int i=0;i < sizeof(pkg) - 1;i++)
	{
		sum += *sum_ptr;
		sum_ptr++;
	}
	pkg.checksum = sum;
    write(fd, (uint8_t*)&pkg, sizeof(pkg));
	return 0;
}

int serial_deinti(void)
{
    close(fd);
    return 0; 
}

