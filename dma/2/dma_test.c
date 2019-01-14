#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int main(int argc , char **argv)
{

	int fd = open("/dev/dma", O_RDWR);

	int arg = atoi(argv[1]);


	ioctl(fd, arg);

	close(fd);

}
