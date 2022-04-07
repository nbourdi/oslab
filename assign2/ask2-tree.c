
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "tree.h"
#include "proc-common.h"

#define SLEEP_PROC_SEC  10
#define SLEEP_TREE_SEC  3


void fork_procs(struct tree_node *node)

{
        int status;
        change_pname(node->name);
        if (node->nr_children > 0) {
                int i;
                for(i = 0; i < node->nr_children; i++) {
                        pid_t p;
                        printf("%s: creating...\n", (node->children+i)->name);
                        p = fork();
                        
                        if (p < 0) {
                                perror("recurs: fork");
                                exit(1);
                        }
                        if (p == 0) {
                                fork_procs(node->children+i);
                        }
                        if (p > 0) {
                                printf("%s: Waiting for child...\n", node->name);
                                p = wait(&status);
                                explain_wait_status(p, status);
                        }
                }
                printf("%s: Exiting...\n", node->name);
                exit(16);

        }
        else {
                //leaf
                printf("%s: Leaf is sleeping...\n", node->name);
                sleep(SLEEP_PROC_SEC);
                printf("%s: Exiting...\n", node->name);
                exit(10);
        }
}



int main(int argc, char **argv)
{       
        struct tree_node *root;
       

        if (argc != 2) {
                fprintf(stderr, "Usage: %s <input_tree_file>\n\n", argv[0]);
                exit(1);
        }
        root = get_tree_from_file(argv[1]);
        print_tree(root);
        

        pid_t pid;
        int status;

        /* Fork root of process tree */
        printf("%s: creating...\n", root->name);
        pid = fork();
        if (pid < 0) {
                perror("main: fork");
                exit(1);
        }
        if (pid == 0) {
                /* root */
                fork_procs(root);
                exit(1);
        }

        sleep(SLEEP_TREE_SEC);
        /* Print the process tree root at pid */
        show_pstree(pid);
        /* Wait for the root of the process tree to terminate */
        pid = wait(&status);
        explain_wait_status(pid, status);

        return 0;
}
