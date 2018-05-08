#include <pthread.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#include <math.h>
#include <time.h>

#define TIMER_COUNTER 7
#define MILLION 1000000
struct timeval start_time;

void err_abort(int status, char *message)
{
	fprintf(stderr, "%s\n", message);
	exit(status);
}

void errno_abort(char *message)
{
	perror(message);
	exit(EXIT_FAILURE);
}
typedef struct _WaitingMsg
{
	int mid;
	int timeout_count;
	int *streams;
} WaitingMsg;
#define EVENT_TIMEOUT 1
#define EVENT_ACK 2
int event;

#define MAX_WAITING_MSG_NUM 10
WaitingMsg wm[MAX_WAITING_MSG_NUM];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int counter = 0;

/*
 * Thread start routine to notify the application when the
 * timer expires. This routine is run "as if" it were a new
 * thread, each time the timer expires.
 *
 * When the timer has expired 5 times, the main thread will
 * be awakened, and will terminate the program.
 */
void timer_thread(union sigval arg)
{
	int status;

	status = pthread_mutex_lock(&mutex);
	if (status != 0)
		err_abort(status, "Lock mutex");

	counter +=1;
	if (counter >= TIMER_COUNTER)
	{
		status = pthread_cond_signal(&cond);
		if (status != 0)
			err_abort(status, "Signal condition");
	}
	
	status = pthread_mutex_unlock(&mutex);
	if (status != 0)
		err_abort(status, "Unlock mutex");
	
	struct timeval stop_time;
	gettimeofday(&stop_time, NULL);
	printf("\t Timer thread: Timer %d\t stop time: %ld.%-6ld time span: %ld.%ld\n", counter, stop_time.tv_sec,stop_time.tv_usec, (stop_time.tv_sec - start_time.tv_sec),(stop_time.tv_usec-start_time.tv_usec));
}

void create_timer(float i)
{
	timer_t timer_id;
	int status;
	struct itimerspec ts;
	struct sigevent se;
	long long unsigned nanosecs = MILLION * 1000 * i;

	/*
   * Set the sigevent structure to cause the signal to be
   * delivered by creating a new thread.
   */
	se.sigev_notify = SIGEV_THREAD;
	se.sigev_value.sival_ptr = &timer_id;
	se.sigev_notify_function = timer_thread;
	se.sigev_notify_attributes = NULL;

	ts.it_value.tv_sec = floor(i);
	ts.it_value.tv_nsec = (i-floor(i)) * 1000000000;
	printf("i:%f tv_sec: %ld tv_nsec: %ld\n",i,ts.it_value.tv_sec, ts.it_value.tv_nsec);
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;

	status = timer_create(CLOCK_REALTIME, &se, &timer_id);
	if (status == -1)
		errno_abort("Create timer");

	// TODO maybe we'll need to have an array of itimerspec
	status = timer_settime(timer_id, 0, &ts, 0);
	if (status == -1)
		errno_abort("Set timer");
}

int main()
{
	int status;
	unsigned i = 1;
	gettimeofday(&start_time, NULL);
	printf("start time: %ld,%ld\n", start_time.tv_sec, start_time.tv_usec);

	for (; i <= TIMER_COUNTER ; i++)
		create_timer(i*0.1);

	status = pthread_mutex_lock(&mutex);
	if (status != 0)
		err_abort(status, "Lock mutex");

	while (counter < 5)
	{
		printf("Main thread \tBlocked!\n");
		status = pthread_cond_wait(&cond, &mutex);
		if (status != 0)
			err_abort(status, "Wait on condition");
		printf("Main thread \tUnblocked!\n");
	}
	status = pthread_mutex_unlock(&mutex);
	if (status != 0)
		err_abort(status, "Unlock mutex");

	return 0;
}