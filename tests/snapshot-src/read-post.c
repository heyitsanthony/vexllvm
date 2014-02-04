#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char* argv[])
{
	int	n;
	int	fd;
	char	x[32];

	fd = open(argv[1], O_RDONLY);
	n = read(fd, x, 32);
	printf("%d\n", n);
	return n;
}