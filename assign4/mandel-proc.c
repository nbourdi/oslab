
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/wait.h>
/*TODO header file for m(un)map*/ //OK
#include <sys/mman.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

/***************************
 * Compile-time parameters *
 ***************************/

int NPROCS;
sem_t *sem_shared_buf;
//int *lines;

/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
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

void sigint_handler(signal) {
        printf("\n");
        reset_xterm_color(1);
        exit(1);
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

void compute_and_output_mandel_line(int fd, int line)
{
        /*
         * A temporary array, used to hold color values for the line being drawn
         */
        int color_val[x_chars];
        int j;
        for (j = line; j < y_chars; j += NPROCS) {
                compute_mandel_line(j, color_val);
                /*
                 * All but the current line's proc
                 * will be blocked here.
                 */
                sem_wait(&sem_shared_buf[j % NPROCS]);
                output_mandel_line(fd, color_val);
                /*
                 * Unblocking the next line's semaphore so
                 * it can print it.
                 */
                sem_post(&sem_shared_buf[(j + 1) % NPROCS]);
        }
        exit(1); //terminate child
}

/*
 * Create a shared memory area, usable by all descendants of the calling
 * process.
 */
void *create_shared_memory_area(unsigned int numbytes)
{
        int pages;
        void *addr;

        if (numbytes == 0) {
                fprintf(stderr, "%s: internal error: called for numbytes == 0\n", __func__);
                exit(1);
        }

        /*
         * Determine the number of pages needed, round up the requested number of
         * pages
         */
        pages = (numbytes - 1) / sysconf(_SC_PAGE_SIZE) + 1;

        /* Create a shared, anonymous mapping for this number of pages */
        /* TODO: */
        addr = mmap(NULL, pages * sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (addr == MAP_FAILED) {
            perror("create_shared_memory_area: mmap failed");
            exit(1);
        }
        return addr;
}

void destroy_shared_memory_area(void *addr, unsigned int numbytes) {
        int pages;

        if (numbytes == 0) {
                fprintf(stderr, "%s: internal error: called for numbytes == 0\n", __func__);
                exit(1);
        }

        /*
         * Determine the number of pages needed, round up the requested number of
         * pages
         */
        pages = (numbytes - 1) / sysconf(_SC_PAGE_SIZE) + 1;

        if (munmap(addr, pages * sysconf(_SC_PAGE_SIZE)) == -1) {
                perror("destroy_shared_memory_area: munmap failed");
                exit(1);
        }
}

int main(int argc, char **argv)
{
        signal(SIGINT, sigint_handler);
        if (argc != 2) {
                printf("Usage: ./mandel <NPROCS>\n");
                exit(1);
        }

        

        safe_atoi(argv[1], &NPROCS);

        if (NPROCS > 50 || NPROCS < 1) {
            printf("Number of processes must be between 1 and 50\n");
            exit(1);
        }

        xstep = (xmax - xmin) / x_chars;
        ystep = (ymax - ymin) / y_chars;
        //nat
        /*
         * Create a shared memory area for each processe's 
         * semaphore to be accessible to all processes.
         */
        
        sem_shared_buf = create_shared_memory_area( NPROCS*sizeof(sem_t) ); 

        /* 
         * First line's process shouldn't get blocked. => Initialize sem to 1.
         */
        sem_init(&sem_shared_buf[0], 1, 1);
        /*
         * All other processes should get blocked at first. => Initialize sem to 0.
         */
        int i;
        
        
        pid_t pid_arr[NPROCS];
        //lines = safe_malloc (sizeof(int)*NPROCS);
        
        for (i = 1; i < NPROCS; i++) {
                sem_init(&sem_shared_buf[i], 1, 0);
                //lines[i] = i;
        }


        //for (i = 0; i < NPROCS; i++) { lines[i] = i; }

        for (i = 0; i < NPROCS; i++) { 
            pid_t c = fork();
            pid_arr[i] = c;
            if (c > 0) {
                /* Parent does nothing */
            }
            if (c == 0) {
                /* Child */
                compute_and_output_mandel_line(1, i);
                
            }
            if (c < 0) { 
                perror("Fork failed");
                exit(1);
            }
        }

        /*
         * Wait for all children to terminate.
         */
        int status;
        
        for (i = 0; i < NPROCS; i++) {
                pid_arr[i] = wait(&status);
        }

        /*
         * draw the Mandelbrot Set, one line at a time.
         * Output is sent to file descriptor '1', i.e., standard output.
         */
        //for (line = 0; line < y_chars; line++) {
          //      compute_and_output_mandel_line(1, line);
        //}
        // is this necessary or covered by destroy shared mem area?
        for (i = 0; i < NPROCS; i++) sem_destroy(&sem_shared_buf[i]);

        destroy_shared_memory_area(sem_shared_buf, NPROCS*sizeof(sem_t));

        reset_xterm_color(1);
        return 0;
}