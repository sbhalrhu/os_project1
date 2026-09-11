#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>


/* Schedule command should look like this :
 * ./schedule 1000 two 1 : two 2 : two 3
 * ...assuming two is a process that should run and 1,2,3 are its arguments
 * and that 1000 is the quantum - the time allotted to each process in MILLISECONDS.
 * Do not test a quantum below 400 ms when using two.c.
 * The max number of processes should be >100, and the max args should be 10.
 *
 */


/* Maximum number of arguments and processes are defined as macros for easy modification.*/
#define MAX_ARGS 10
#define MAX_PROCESSES 1000


/* A struct is used for each process. The name, number of arguments, the arguments themselves,
 * the process IDs, and the time quantum are stored.
 *
 */
typedef struct {
   char *name;
   char *args[MAX_ARGS];
   int arg_count;
   int pid;
   int quantum;
} process;

pid_t current_pid;

int main(int argc, char *argv[]) {

    /* A loop adds processes from the command line arguments to a queue.
     * Each process is defined according to the struct above.
     * From the cmd line, we can get its name, args, arg count, and quantum.
     * Need to check that arguments are commands or processes (ex. executable or something like ls)
     * The quantum is common among all processes.
     */

     int quantum = atoi(argv[1]);
     process *processes[MAX_PROCESSES]; 
     int process_queue_index = 0;
     /* Add processes to the queue while there are still command line arguments.
      * Remember the format is:
      * ./schedule [quantum] [process name] [arg1] [arg2] ... : [process name] [arg1] ...
      */
    int current_arg = 2;
    while (current_arg < argc) {
        if (argc < 3) {
            break;
        }
        process *p = malloc(sizeof(process));
        p->name = argv[current_arg];
        p->arg_count = 0;
        /*
         * Add arguments to the process until we reach a colon or the max number of args.
         */
        current_arg++;
        for (int i = current_arg; i < argc && p->arg_count < (MAX_ARGS+1) && strcmp(argv[i], ":") != 0; i++) {
            p->args[p->arg_count++] = argv[i];
            current_arg++;
        }
        /* Handle reaching max args BEFORE seeing a colon.*/
        while (current_arg < argc && strcmp(argv[current_arg], ":") !=0) {
            current_arg++;
        }
        p->quantum = quantum;
        if (process_queue_index < MAX_PROCESSES) {
            processes[process_queue_index] = p;
            process_queue_index++;
        }
    }


}

