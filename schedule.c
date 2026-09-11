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


/* Maximum number of arguments and processes are defined as macros for easy modification.
 * Also define process states. 0 - stopped, 1 - running, 2 - terminated.
 */
#define MAX_ARGS 10
#define MAX_PROCESSES 1000
#define STOPPED 0
#define RUNNING 1
#define TERMINATED 2
/*quantum and child processes: in progress or done*/
#define IN_PROGRESS 3
#define DONE 4


/* A struct is used for each process. The name, number of arguments, the arguments themselves,
 * the process IDs, and the time quantum are stored.
 *
 */
typedef struct {
   char *name;
   char *args[MAX_ARGS + 2];
   int arg_count;
   int pid;
   int quantum;
   int status;
} process;

pid_t current_pid = -1;
int num_processes = 0;
process *processes[MAX_PROCESSES];
/*SIGALRM and SIGCHLD set these to DONE*/
volatile sig_atomic_t quantum_status;
volatile sig_atomic_t child_status;

/* Helper function to adjust real time timer. (arg is in milliseconds) */
void set_timer(int time) {
    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_sec = time / 1000;
    timer.it_value.tv_usec = (time % 1000) * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}

/* Have handler function handle SIGALRM and SIGCHLD.
 * SIGALRM sent when quantum is over.
 * SIGCHLD sent when child process is done.
 */
void handler (int signum) {
    if (signum == SIGALRM) {
        quantum_status = DONE;
        if (current_pid > 0) {
            kill(current_pid, SIGSTOP);
        }
    } else if (signum == SIGCHLD) {
        child_status = DONE;
    }
}

/* Have a function to count all living (non-terminated) processes.
 * This will help find the next living process in the queue.
 */

int count_living_processes(process *processes[], int num_processes) {
    int count = 0;
    for (int i = 0; i < num_processes; i++) {
        if (processes[i]->status != TERMINATED) {
            count++;
        }
    }
    return count;
}

/* Look for index of the next living process in the queue afterfrom starting_index. */
int next_living_process(process *processes[], int num_processes, int starting_index) {  
    for (int count = 1; count <= num_processes; count++) {
        int index = (starting_index + count) % num_processes;
        if (processes[index]->status != TERMINATED) {
            return index;
        }
    }
    return -1;
}

void reap_children(process *processes[], int num_processes) {
    pid_t pid;
    int status; 

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (pid <= 0) { break;}
        for (int i = 0; i < num_processes; i++) {
            if (processes[i]->pid != pid) { continue; }

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                processes[i]->status = TERMINATED;
                set_timer(0);
                current_pid = -1;
            } else if (WIFSTOPPED(status)) {
                if (processes[i]->status == RUNNING) {
                    processes[i]->status = STOPPED;
                    set_timer(0);
                }
                if (processes[i]->pid == current_pid) {
                    current_pid = -1;
            }
            }
            break;
        }
    }
}

/* Given a process (process struct), schedule it and have it start running. */
void schedule_process(process *p) {
    current_pid = p->pid;
    p->status = RUNNING;
    set_timer(p->quantum);
    kill(p->pid, SIGCONT);
}

/* Set up the process of forking children, including blocking SIGCHLD to prevent SIGSTOP from being sent to children.*/
void fork_children(void) {
    sigset_t block_child, old_mask;
    sigemptyset(&block_child);
    sigaddset(&block_child, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_child, &old_mask);


    for (int i = 0; i < num_processes; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sigprocmask(SIG_SETMASK, &old_mask, NULL);
            raise(SIGSTOP);

            if (strchr(processes[i]->name, '/') == NULL) {
                char local_path[256];
                snprintf(local_path, sizeof(local_path), "./%s", processes[i]->name);
                execvp(local_path, processes[i]->args);
            } else {
                execvp(processes[i]->name, processes[i]->args);
            }


            execvp(processes[i]->name, processes[i]->args);
            perror(processes[i]->name);
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        else {
            processes[i]->pid = pid;
            int exit_status;
            waitpid(pid, &exit_status, WUNTRACED);
        }
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}


int main(int argc, char *argv[]) {

    /* A loop adds processes from the command line arguments to a queue.
     * Each process is defined according to the struct above.
     * From the cmd line, we can get its name, args, arg count, and quantum.
     * Need to check that arguments are commands or processes (ex. executable or something like ls)
     * The quantum is common among all processes.
     */


    /* TO BE ABLE to run two.c as two not ./two, we need to modify the directory and path environments.
     * 
    */
    char *old_path = getenv("PATH");
    if (old_path) {
        char *new_path = malloc(strlen(old_path) + 3); 
        sprintf(new_path, ".:%s", old_path);
        setenv("PATH", new_path, 1);
        free(new_path);
    } else {
        setenv("PATH", ".", 1);
    }

    int quantum = atoi(argv[1]);
    int current_arg = 2;

    /* Add processes to the queue while there are still command line arguments.
     * Remember the format is:
     * ./schedule [quantum] [process name] [arg1] [arg2] ... : [process name] [arg1] ...
     */
    while (current_arg < argc) {
        if (argc < 3) {
            break;
        }
        process *p = malloc(sizeof(process));
        p->name = argv[current_arg];
        p->arg_count = 0;
        p->status = STOPPED;

        p->args[0] = argv[current_arg];

        current_arg++;

        /* Add arguments to the process until we reach a colon or the max number of args. */
        while (current_arg < argc
               && strcmp(argv[current_arg], ":") != 0
               && p->arg_count < MAX_ARGS) {
            p->args[p->arg_count + 1] = argv[current_arg];
            p->arg_count++;
            current_arg++;
        }

        /* Handle reaching max args BEFORE seeing a colon. */
        while (current_arg < argc && strcmp(argv[current_arg], ":") != 0) {
            current_arg++;
        }

        p->args[p->arg_count + 1] = NULL;

        p->quantum = quantum;

        if (num_processes < MAX_PROCESSES) {
            processes[num_processes] = p;
            num_processes++;
        }

        if (current_arg < argc && strcmp(argv[current_arg], ":") == 0) {
            current_arg++;
        }
    }

/* If no processes to schedule, exit.*/
    if (num_processes == 0) {
        return 0;
    }

    /*register signal handlers*/
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCHLD); /* block SIGCHLD */
    sa.sa_flags   = 0;
    sa.sa_handler = handler;
    sigaction(SIGALRM, &sa, NULL);

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGALRM); /* block SIGALRM while in SIGCHLD handler */
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = handler;
    sigaction(SIGCHLD, &sa, NULL);

    fork_children();

    int last_index = 0;
    schedule_process(processes[last_index]);

    while (count_living_processes(processes, num_processes) > 0) {
        pause();
        if (child_status == DONE) {
            child_status = IN_PROGRESS;
            reap_children(processes, num_processes);
        }

        if (current_pid == -1) {
            int next_index = next_living_process(processes, num_processes, last_index);
            if (next_index == -1) {
                break;
            }
            last_index = next_index;
            schedule_process(processes[last_index]);
        }
    }

    set_timer(0);
    reap_children(processes, num_processes);
    while( waitpid(-1, NULL, 0) > 0) {} /*just wait for all of them*/
    for (int i = 0; i < num_processes; i++) {
        free(processes[i]);
    }
    
    return 0;


}

