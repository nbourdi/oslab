#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

void doWrite(int fd, const char *buff, int len) {
    ssize_t wcnt;
    wcnt = write(fd,buff, len);
    if (wcnt == -1){
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
    //print an error if one of the files doesnt exist
    if (fd_in == -1) {
        perror("");
        exit(1);
    }

    for (;;) {
        rcnt = read(fd_in,buff,sizeof(buff)-1);
        if (rcnt == 0) {/* end‐of‐file */
            close(fd_in);
            return;
        }
        if (rcnt == -1){ /* error */
            perror("read");
            exit(1);
        }
        buff[rcnt] = '\0';
        size_t len = strlen(buff);
        doWrite(fd,buff,len);
    }
}

// in this case argv[1] is infile1, argv[2] infile2, argv[3] outfile
int main (int argc, char **argv) {
    // print an error if the user hasn't provided 2 or 3 arguments
    if (!(argc == 3 || argc == 4)) {
        perror("Usage: ./fconc infile1 infile2 [outfile (default:fconc.out)]");
        exit(1);
    }
    if (argc == 3 && (strcmp(argv[1], "fconc.out") == 0 || strcmp(argv[2], "fconc.out") == 0)) {
        perror("Cannot read from the default output file");
        exit(1);
    }

    int oflags, mode;
    oflags = O_CREAT | O_WRONLY | O_TRUNC;
    mode = S_IRUSR | S_IWUSR;

    int fd3; // outfile
    if (argc == 4) {
        if (strcmp(argv[1], argv[3]) == 0 || strcmp(argv[2], argv[3]) == 0) {
            perror("Input and output file are the same.");
            exit(1);
        }
        fd3 = open(argv[3], oflags, mode);
    }
    else fd3 = open("fconc.out", oflags, mode);

    write_file(fd3, argv[1]);
    write_file(fd3, argv[2]);
    close(fd3);
    return 0;
}
