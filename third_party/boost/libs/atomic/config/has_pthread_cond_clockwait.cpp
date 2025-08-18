/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

#include <errno.h>
#include <time.h>
#include <pthread.h>

int main(int, char*[])
{
    pthread_mutex_t mut;
    pthread_mutex_init(&mut, nullptr);

    pthread_cond_t cond;
    pthread_cond_init(&cond, nullptr);

    pthread_mutex_lock(&mut);

    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec += 1;
    ts.tv_nsec += (int)0;

    int err = pthread_cond_clockwait(&cond, &mut, CLOCK_REALTIME, &ts);
    if (err == ETIMEDOUT)
    {
        (void)0;
    }

    pthread_mutex_unlock(&mut);

    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mut);

    return 0;
}
