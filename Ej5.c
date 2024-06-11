//
// Created by levixhu on 16/02/24.
//

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

struct nums {
    long *increase;
    long *decrease;
    long total;
    long diff;
    pthread_mutex_t *mutexIncremento;
    pthread_mutex_t *mutexDecremento;
    long iterations;
};

struct args {
    int thread_num;     // application defined thread #
    struct nums *nums;  // pointer to the counters (shared with other threads)
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

void startCounter(struct nums *nums, long total, long iterations, int size) {
    nums->total = total;
    nums->diff = 0;
    nums->iterations = iterations;

    nums->increase = malloc(sizeof(long) * size);
    nums->decrease = malloc(sizeof(long) * size);
    nums->mutexIncremento = malloc(sizeof(pthread_mutex_t) * size);
    nums->mutexDecremento = malloc(sizeof(pthread_mutex_t) * size);

    if (nums->increase == NULL || nums->decrease == NULL || nums->mutexIncremento == NULL || nums->mutexDecremento == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    for (int i = 0; i < size; ++i) {
        nums->increase[i] = 0;
        nums->decrease[i] = total;
        pthread_mutex_init(&nums->mutexIncremento[i], NULL);
        pthread_mutex_init(&nums->mutexDecremento[i], NULL);
    }
}

void deleteCounters(struct nums *nums, int size) {
    for (int i = 0; i < size; ++i) {
        pthread_mutex_destroy(&nums->mutexIncremento[i]);
        pthread_mutex_destroy(&nums->mutexDecremento[i]);
    }
    free(nums->decrease);
    free(nums->increase);
    free(nums->mutexIncremento);
    free(nums->mutexDecremento);
}

// Threads run on this function
void *decrease_increase(void *ptr)
{
    struct args *args = ptr;
    struct nums *n = args->nums;
    long diff;
    int decrement;
    int increment;

    for (long i = 0; i < n->iterations; i++) {
        increment = rand() % 10;
        decrement = rand() % 10;

        if (increment < decrement) {
            pthread_mutex_lock(&n->mutexIncremento[increment]);
            pthread_mutex_lock(&n->mutexDecremento[decrement]);
        } else {
            pthread_mutex_lock(&n->mutexDecremento[decrement]);
            pthread_mutex_lock(&n->mutexIncremento[increment]);
        }

        n->decrease[decrement]--;
        n->increase[increment]++;

        pthread_mutex_unlock(&n->mutexDecremento[decrement]);
        pthread_mutex_unlock(&n->mutexIncremento[increment]);

        diff = (n->decrease[decrement] - n->increase[increment]);

        if (diff != n->diff) {
            n->diff = diff;
            printf("Thread %d increasing %ld decreasing %ld diff %ld\n",
                   args->thread_num, n->increase[increment], n->decrease[decrement], diff);
        }
    }

    return NULL;
}

void *decrease_increase2(void *ptr)
{
    struct args *args = ptr;
    struct nums *n = args->nums;
    long diff;
    int decrement;
    int increment;

    for (long i = 0; i < n->iterations; i++) {
        increment = rand() % 10;
        decrement = rand() % 10;

        if (increment < decrement) {
            pthread_mutex_lock(&n->mutexIncremento[increment]);
            pthread_mutex_lock(&n->mutexDecremento[decrement]);
        } else {
            pthread_mutex_lock(&n->mutexDecremento[decrement]);
            pthread_mutex_lock(&n->mutexIncremento[increment]);
        }

        n->decrease[decrement]--;
        n->decrease[increment]++;

        pthread_mutex_unlock(&n->mutexDecremento[decrement]);
        pthread_mutex_unlock(&n->mutexIncremento[increment]);

        diff = (n->decrease[decrement] - n->increase[increment]);

        if (diff != n->diff) {
            n->diff = diff;
            printf("Thread %d increasing %ld decreasing %ld diff %ld\n",
                   args->thread_num, n->increase[increment], n->decrease[decrement], diff);
        }
    }

    return NULL;
}

// start opt.num_threads threads running on decrease_incresase
struct thread_info *start_threads(struct options opt, struct nums *nums)
{
    int i;
    struct thread_info *threads;

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads * 2);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    long total_operations = nums->iterations * opt.num_threads;

    // Create num_thread threads running decrease_increase
    for (i = 0; i < opt.num_threads * 2; i++) {
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args->thread_num = i;
        threads[i].args->nums       = nums;
        threads[i].args->nums->iterations = total_operations / opt.num_threads;

        if ((0 != pthread_create(&threads[i].id, NULL, decrease_increase, threads[i].args) && (i < opt.num_threads))) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
        if ((0 != pthread_create(&threads[i].id, NULL, decrease_increase2, threads[i].args)) && (i >= opt.num_threads)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    return threads;
}

void print_totals(struct nums *nums, int size, struct options opt)
{
    long totalIncrease = 0;
    long totalDecrease = 0;

    for (int i = 0; i < size; ++i) {
        totalIncrease += nums->increase[i];
        totalDecrease += nums->decrease[i];
    }

    printf("Final: increasing %ld decreasing %ld diff %ld got %ld expected %ld\n",totalIncrease, totalDecrease, (totalDecrease- totalIncrease), totalIncrease + totalDecrease, (nums->total * opt.size));
}

// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct nums *nums, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads * 2; i++)
        pthread_join(threads[i].id, NULL);

    print_totals(nums, opt.size, opt);

    for (int i = 0; i < opt.num_threads * 2; i++)
        free(threads[i].args);

    free(threads);
}

int main (int argc, char **argv)
{
    struct options opt;
    struct nums nums;
    struct thread_info *thrs;
    long totalOperations;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 4;
    opt.iterations   = 100000;
    opt.size         = 10;

    read_options(argc, argv, &opt);

    totalOperations = opt.iterations * opt.num_threads;
    startCounter(&nums, totalOperations, opt.iterations, opt.size);

    thrs = start_threads(opt, &nums);

    wait(opt, &nums, thrs);

    deleteCounters(&nums, opt.size);

    return 0;
}