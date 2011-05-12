#include <assert.h>
#include <time.h>

int main(int argc, char* argv[])
{
	time_t	t;

	t = time(NULL);
	assert (t != ((time_t)-1));

	return 0;
}
