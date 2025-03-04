#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// optional: use these functions to add debug or error prints to your application
#define debug_log(msg,...)
//#define debug_log(msg,...) printf("threading: " msg "\n" , ##__va_args__)
#define error_log(msg,...) printf("threading error: " msg "\n" , ##__va_args__)

void* threadfunc(void* thread_param)
{
    struct thread_data *param = (struct thread_data*)thread_param;

    // todo: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    usleep(param->wait_to_obtain_ms*1000); 
    int ret;
    ret = pthread_mutex_lock(param->mutex);
    if(ret) {
        param->thread_complete_success = false;
        return NULL;
    }
    usleep(param->wait_to_release_ms * 1000);
    ret = pthread_mutex_unlock(param->mutex);
     if(ret) {
        param->thread_complete_success = false;
        return NULL;
    }   
    param->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * todo: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * see implementation details in threading.h file comment block
     */

    //struct thread_data data = (struct thread_data){.wait_to_obtain_ms = wait_to_obtain_ms, .wait_to_release_ms= wait_to_release_ms, .mutex= mutex};

    struct thread_data *data = malloc(sizeof(struct thread_data));
    if (!data) {
        return false;
    }
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->mutex = mutex;
    int ret = pthread_create(thread, NULL , threadfunc, data);
    if(ret) {
      free(data);
      return false;
    }
    //pthread_join(*thread, NULL);
    return true;
}

