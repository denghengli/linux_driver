#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	unsigned int times = 0;

	while (1)
	{
		printf("runging times: %d \r\n", times);
		times++;
		sleep(1);
	}
}
