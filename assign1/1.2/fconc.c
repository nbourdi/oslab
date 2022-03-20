#include <sys/types.h>
#include <sys/stat.h> //need
#include <fcntl.h> //need for O CREAT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// writes in fd file
void doWrite(int fd, const char *buff, int len) {
    ssize_t wcnt;
    wcnt = write(fd,buff, len);
    if (wcnt == -1){ /* error */
        perror("write");
        exit (1);
    }
}
// writes the contents of the file named infile in the descriptor of fd, uses doWrite()
void write_file(int fd, const char *infile) {
    //create buffer
    char buff[1024];
    size_t rcnt;
    int fd_in = open(infile,  O_RDONLY);
    //print an error if one of the files doesnt exit
    if (fd_in == -1) {
        perror("No such file or directory");
        exit(1);
    }

    for (;;) {
        rcnt = read(fd_in,buff,sizeof(buff)-1);
        if (rcnt == 0) /* end‐of‐file */
            return;
        if (rcnt == -1){ /* error */
            perror("read");
            exit(1);
        }
        buff[rcnt] = '\0';
        size_t len = strlen(buff);

        doWrite(fd,buff,len);
    }
    close(fd_in);
}

// in this case argv[1] is infile1, argv[2] infile2, argv[3] outfile
int main (int argc, char **argv) {
    // print an error if the user hasn't provided 2 or 3 arguments
    if (!(argc == 3 || argc == 4)) {
        perror("Usage: ./fconc infile1 infile2 [outfile (default:fconc.out)]");
        exit(1);
    }

    int oflags, mode;
    oflags = O_CREAT | O_WRONLY | O_TRUNC;
    mode = S_IRUSR | S_IWUSR;

    int fd3; // outfile
    if (argc == 4) fd3 = open(argv[3], oflags, mode);
    else fd3 = open("fconc.out", oflags, mode); //create?


    write_file(fd3, argv[1]);
    write_file(fd3, argv[2]);
    close(fd3);
    return 0;
}

