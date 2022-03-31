/*--------------------------------------------------------
UNIX Shell Project

--------------------------------------------------------*/

#ifndef _JOB_CONTROL_H
#define _JOB_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

// ----------- ENUMERATIONS ---------------------------------------------
enum status { SUSPENDED, SIGNALED, EXITED}; //Situaciones en las que se genera un SIGSHLD
enum job_state { FOREGROUND, BACKGROUND, STOPPED, RESPAWNABLE };
static char* status_strings[] = { "Suspended","Signaled","Exited" };
static char* state_strings[] = { "Foreground","Background","Stopped", "Respawnable" };

// ----------- JOB TYPE FOR JOB LIST ------------------------------------
typedef struct job_
{
	pid_t pgid; /* group id = process lider id */
	char * command; /* program name */
	char * args[128];
	enum job_state state;
	struct job_ *next; /* next job in the list */
	int tiempo;
} job;

// -----------------------------------------------------------------------
//      PUBLIC FUNCTIONS
// -----------------------------------------------------------------------

void get_command(char inputBuffer[], int size, char *args[],int *background);

job * new_job(pid_t pid, const char * command,char * args[], enum job_state state, int tiempo);

void add_job (job * list, job * item);

int delete_job(job * list, job * item);

job * get_item_bypid  (job * list, pid_t pid);

job * get_item_bypos( job * list, int n);

enum status analyze_status(int status, int *info);

// -----------------------------------------------------------------------
//      PRIVATE FUNCTIONS: BETTER USED THROUGH MACROS BELOW
// -----------------------------------------------------------------------

void print_item(job * item);

void print_list(job * list, void (*print)(job *));

void terminal_signals(void (*func) (int));

void block_signal(int signal, int block);

void print_analyzed_status(enum status s, int info);

// -----------------------------------------------------------------------
//      PUBLIC MACROS
// -----------------------------------------------------------------------

#define list_size(list) 	 list->pgid   // number of jobs in the list
#define empty_list(list) 	 !(list->pgid)  // returns 1 (true) if the list is empty

#define new_list(name) 			 new_job(0,name,NULL,FOREGROUND,0)  // name must be const char *

#define print_job_list(list) 	 print_list(list, print_item)

#define restore_terminal_signals()   terminal_signals(SIG_DFL)
#define ignore_terminal_signals() terminal_signals(SIG_IGN)

#define set_terminal(pgid)        tcsetpgrp (STDIN_FILENO,pgid) //Sólo cede a la lectura(entrada, da problemas), pero no a la salida estándar
#define new_process_group(pid)   setpgid (pid, pid)

#define block_SIGCHLD()   	 block_signal(SIGCHLD, 1)
#define unblock_SIGCHLD() 	 block_signal(SIGCHLD, 0)

// macro for debugging----------------------------------------------------
// to debug integer i, use:    debug(i,%d);
// it will print out:  current line number, function name and file name, and also variable name, value and type
#define debug(x,fmt) fprintf(stderr,"\"%s\":%u:%s(): --> %s= " #fmt " (%s)\n", __FILE__, __LINE__, __FUNCTION__, #x, x, #fmt)

// -----------------------------------------------------------------------
#endif

