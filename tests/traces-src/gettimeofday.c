#include <assert.h>
#include <sys/time.h>
#include <time.h>

int main(int argc, char* argv[])
{
	struct timeval	tv;
	int		rc;
	time_t		t;

	rc = gettimeofday(&tv, NULL);
	assert (rc == 0 && "Bad return value");

	t = time(NULL);
	assert (tv.tv_sec <= t);

	return 0;
}
