#include <pthread.h>
#include <semaphore.h>

#define NUM_THR	4

pthread_t	thr[NUM_THR];
sem_t		sem;

static void* f(void* x)
{
	sem_wait(&sem);
	return NULL;
}

int main(void)
{
	unsigned	i;

	sem_init(&sem, 0, 0);

	for (i = 0; i < NUM_THR; i++)
		pthread_create(&thr[i], NULL, f, NULL);

	sleep(5);

	/* release all threads */
	for (i = 0; i < NUM_THR; i++)
		sem_post(&sem);

	for (i = 0; i < NUM_THR; i++)
		pthread_join(thr[i], NULL);

	return 0;
}