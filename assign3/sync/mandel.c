#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include "mandel-lib.h"
#define MANDEL_MAX_ITERATION 100000

/*
 * Taken from simplesync.c:
 * POSIX thread functions do not return error numbers in errno,
 * but in the actual return value of the function call instead.
 * This macro helps with error reporting in this case.
 */
#define perror_pthread(ret, msg) \
        do { errno = ret; perror(msg); } while (0)
int NTHREADS; // number of threads
pthread_t *tid;  //array of NTHREADS thread IDs
int *nlines;     //
sem_t *nsem;     //semaphore for each thread

int safe_atoi(char *s, int *val)
{
        long l;
        char *endp;

        l = strtol(s, &endp, 10);
        if (s != endp && *endp == '\0') {
                *val = l;
                return 0;
        } else
                return -1;
}

void *safe_malloc(size_t size)
{
        void *p;

        if ((p = malloc(size)) == NULL) {
                fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n",
                        size);
                exit(1);
        }

        return p;
}

int y_chars = 50;
int x_chars = 90;

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;
/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep;
double ystep;
/*
 * This function computes a line of output
 * as an array of x_char color values.
 */
void compute_mandel_line(int line, int color_val[])
{
        /*
         * x and y traverse the complex plane.
         */
        double x, y;

        int n;
        int val;

        /* Find out the y value corresponding to this line */
        y = ymax - ystep * line;

        /* and iterate for all points on this line */
        for (x = xmin, n = 0; n < x_chars; x+= xstep, n++) {

                /* Compute the point's color value */
                val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
                val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
                if (val > 255)
                        val = 255;

                /* And store it in the color_val[] array */
                val = xterm_color(val);
                color_val[n] = val;
        }
}

/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */
void output_mandel_line(int fd, int color_val[])
{
        int i;

        char point ='@';
        char newline='\n';

        for (i = 0; i < x_chars; i++) {
                /* Set the current color, then output the point */
                set_xterm_color(fd, color_val[i]);
                if (write(fd, &point, 1) != 1) {
                        perror("compute_and_output_mandel_line: write point");
                        exit(1);
                }
        }

        /* Now that the line is done, output a newline character */
        if (write(fd, &newline, 1) != 1) {
                perror("compute_and_output_mandel_line: write newline");
                exit(1);
        }
}

void *compute_and_output_mandel_line(void *arg)
{
        int line = *(int*)arg;
        int j;

        //
        for (j = line; j < y_chars; j += NTHREADS) {
        /*
        * A temporary array, used to hold color values for the line being drawn
        */
            int color_val[x_chars];
            compute_mandel_line(j, color_val);
            /*
             * All but the current line's thread
             * will be blocked here.
             */
            sem_wait(&nsem[j % NTHREADS]);
            output_mandel_line(1, color_val);
            reset_xterm_color(1);
             /*
             * Unblocking the next line's semaphore so
             * it can print it.
             */
            sem_post(&nsem[(j + 1) % NTHREADS]);
        }
        return NULL;
}

int main(int argc, char *argv[])
{
        int i, ret;
        xstep = (xmax - xmin) / x_chars;
        ystep = (ymax - ymin) / y_chars;
        /* Ask for desired number of threads
         * and save in NTHREADS */
        if (argc != 2) {
                printf("Usage: ./mandel <NTHREADS>\n");
                exit(1);
        }
        safe_atoi(argv[1], &NTHREADS);

        if (NTHREADS > 50 || NTHREADS < 1) {
            printf("Number of threads must be between 1 and 50\n");
            exit(1);
        }

        tid = safe_malloc( sizeof(pthread_t)*NTHREADS);
        nsem = safe_malloc( sizeof(sem_t)*NTHREADS);
        nlines = safe_malloc (sizeof(int)*NTHREADS);
        /*
         * Initialize all semaphores to 0
         * except for the first thread's
         */
        sem_init(&nsem[0], 0, 1);
        for (i = 1; i < NTHREADS; i++) {
            sem_init(&nsem[i], 0, 0);
            nlines[i] = i;
        }
        /*
         * create NTHREAD threads
         */
        for (i = 0; i < NTHREADS; i++) {
            ret = pthread_create(&tid[i], NULL, compute_and_output_mandel_line, &nlines[i]);
            if (ret) {
                perror_pthread(ret, "pthread_create failed.\n");
                exit(1);
            }
        }

        /*
         * Wait for all threads to terminate
         */
        for (i = 0; i < NTHREADS; i++) {
            ret = pthread_join(tid[i], NULL);
            if (ret) {
                perror_pthread(ret, "pthread_join failed.\n");
                exit(1);
            }
            sem_destroy(&nsem[i]);
        }
        free(tid);
        free(nsem);
        free(nlines);
        reset_xterm_color(1);
        return 0;
}