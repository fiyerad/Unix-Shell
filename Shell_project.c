																					/**
UNIX Shell Project

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 
#include <string.h>
#include <unistd.h>    //chdir -- cambiar directorio  (cd)
#include <pthread.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------
// OPTATIVO
//------------------------------------------------------------------------
struct termios conf;
int shell_terminal;
struct termios conf_new;
//------------------------------------------------------------------------

job *lista; //Lista -> global (accede sigchld y main)


void* alarmaThread(void *null){
	while(1){
		alarm(1); // envia la señal sigalarm al proceso que llama alarm() en un seg
		sleep(1); //thread bloqueado 1 seg
	}
}


void manejadorSIGALRM(int signal){ //recorrer los jobs para ir decrementando el tiempo
	job* act=lista;
	while(act!=NULL){
		if(act->tiempo==0){
			act=act->next;
		}else if(act->tiempo==1){
			killpg(act->pgid,SIGKILL);
			act=act->next;
		}else{
			act->tiempo=(act->tiempo)-1;
			act=act->next;
		}
	}
}

void respawnear(char *args[]){ //para revivir el proceso
	job *nuevo;
	
	int pid_fork = fork();
	if(pid_fork){
			//Hijo independiente en un grupo nuevo
			new_process_group(pid_fork);//<-- tty lo tiene el shell
			block_SIGCHLD();
			nuevo = new_job(pid_fork, args[0],args, RESPAWNABLE,0);
			
			add_job(lista, nuevo);
			unblock_SIGCHLD();
			printf("Proceso %d (%s) en se ha convertido en RESPAWNABLE\n", pid_fork, args[0]);
	}else{
			//Hijo --> exec
			//Grupo nuevo y coger el terminal por si las moscas el hijo se planifica antes
			//Esta solución evita pensar quién se ejecuta antes
			new_process_group(getpid());
			
			restore_terminal_signals(); //Restaurar las señales por defecto para el hijo
			puts("\e[92m");
			execvp(args[0], args);
			printf("El hijo ha fallado (no existe, fallos de permisos ...)\n");
			exit(EXIT_FAILURE);
	}
}

void my_sigchld(int signum){
	int i, status, info, pid_wait;
	enum status status_res;
	job* jb;
	
	//Manejador de sigchld
	//Recorrer la lista de jobs y ver que ha pasado (waitpid no bloqueante)
	//printf("Llego SIGCHLD ...\n");
	block_SIGCHLD();
	for(i=1; i<=list_size(lista); i++){
		jb=get_item_bypos(lista, i); //Nodo que quiero coger
		pid_wait=waitpid(jb->pgid, &status, WUNTRACED|WNOHANG); //waitpid no bloqueante
		if(pid_wait==jb->pgid){ //Me ha pasado algo
			//Hay que poner 3 IF (hay 3 estados), solo nos da 1 (el de matar), los otros 2 los tenemos que implementar en la Tarea 4
			status_res=analyze_status(status, &info); //Devuelve estado
			print_analyzed_status(status_res, info); //Lo analizo
			printf("Wait realizado para trabajo en background: %s, pid=%i\n", jb->command, pid_wait);
			
			// Actuar segun los posibles casos reportados por status
			// Al menos hay que considerar EXITED, SIGNALED, y SUSPENDED
			// En este ejemplo sólo se consideran los dos primeros
			
			if((status_res == SIGNALED) | (status_res == EXITED)){
				if(jb->state==RESPAWNABLE){ //si es respawnable lo revivimos
					respawnear(jb->args);
				
				}
				delete_job(lista, jb);
				i--; //Ojo! El siguiente ha ocupado la posicion de este en
			}else if(status_res==SUSPENDED){
				jb->state=STOPPED;
			}
			
			print_job_list(lista);
			
		}
	}
	unblock_SIGCHLD();
	
	
	return;
}

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	enum status status2;
	int info;	
				/* info processed by analyze_status() */
	
	
	job *nuevo, *aux; //Punteros auxiliares para usar la macro
	
	lista = new_list("milistadetrabajos");
	signal(SIGCHLD, my_sigchld);   //Instalar SIGCHLD -> ojo hay que crear la lista?
	signal(SIGALRM, manejadorSIGALRM);
	
	pthread_t thread;
	pthread_create(&thread,NULL,alarmaThread,NULL);
	
	
//-----------------------------------------------------------------------------
	shell_terminal = STDIN_FILENO;
	tcgetattr(shell_terminal, &conf);
	tcsetattr(shell_terminal, TCSANOW, &conf);
	conf_new = conf;
//-----------------------------------------------------------------------------	
// -----------------------------------------------------------------------
// Ignora las señales del terminal mientras esta en la shell.
// ----------------------------------------------------------------------- 

	ignore_terminal_signals(); // Soy inmune a ^C, ^Z ... SIGTTIN, SIGTTOUT ...
// -----------------------------------------------------------------------

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   	
		printf("\e[0m");
		puts("\e[96m");
		
		printf("COMMAND->");
		fflush(stdout);
		printf("\e[0m");
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		if(args[0]==NULL) continue;   // if empty command

// -----------------------------------------------------------------------
// Imprimir por consola al escribir por teclado. 
// -----------------------------------------------------------------------

		if(strcmp(args[0], "hello") == 0){
			//Ejemplo de comando interno (built-in command)
			printf("Hello world!\n");
			continue;
		}
		
		
		if(strcmp(args[0], "cd") == 0){
			if(args[1]){
				if(chdir(args[1])!=0){		
					puts("\e[91m");							
					printf("Especifica un argumento valido\n");
				}
			}else{
				chdir(getenv("HOME"));
			}
			continue;
		}
		if(strcmp(args[0], "jobs") == 0){
			print_job_list(lista);
			
			continue;
		}
		if(!strcmp(args[0],"time-out")){
			if(args[1]==NULL || args[2]==NULL){ //si no se le pasan bien los argumentos
				printf("time-out sin argumentos");
				continue;
			}
			pid_fork=fork();
			if(pid_fork){
			//Hijo independiente en un grupo nuevo
			new_process_group(pid_fork);//<-- tty lo tiene el shell
			
			//Padre = shell
			if(background==1){
				///Background -> (1) no tienen tty, (2) el shell no espera
				
				 block_SIGCHLD(); // Bloquear SIGCHLD al acceder a estructura de datos
                 add_job(lista, new_job(pid_fork, args[2],NULL, BACKGROUND, atoi(args[1]))); // Se le pone NULL porque no es respawnable
                 unblock_SIGCHLD();
				/*Habría que comprobar que el hijo está vivo antes de meterlo en la lista */
				
				if(info!=1){
					printf("Proceso %d (%s) en background\n", pid_fork, args[0]);
				}
				
			}else if(background==2){
				nuevo = new_job(pid_fork, args[0],args, RESPAWNABLE,0);
				block_SIGCHLD();
				
				/*Habría que comprobar que el hijo está vivo antes de meterlo en la lista */
				add_job(lista, nuevo);
				unblock_SIGCHLD();
				printf("Proceso %d (%s) en se ha convertido en RESPAWNABLE\n", pid_fork, args[0]);
			}else{
				///Foreground
				//Ceder el terminal al hijo
				block_SIGCHLD();
				add_job(lista,new_job(pid_fork,args[2],NULL,BACKGROUND,atoi(args[1])));
				unblock_SIGCHLD();
				set_terminal(pid_fork);//Se le da el terminal al proceso
				pid_wait=waitpid(pid_fork, &status, WUNTRACED);
				set_terminal(getpid());
				status2=analyze_status(status,&info);
				//INSERTAR JOB EN LA LISTA DE JOBS SI FG SUSPENDIDO
				/*Insertar en la lista*/
				
				if(status2 == SUSPENDED){
					nuevo = new_job(pid_fork, args[0],NULL, STOPPED,0); //cambiado de suspended
					block_SIGCHLD();					
					/*Habría que comprobar que el hijo está vivo antes de meterlo en la lista */
					
					add_job(lista, nuevo);
					unblock_SIGCHLD(); 
				}else{
					 delete_job(lista,get_item_bypid(lista,pid_fork)); // Quitar trabajo de la lista
				}
				
				
			}
		} else{
			//Hijo --> exec
			//Grupo nuevo y coger el terminal por si las moscas el hijo se planifica antes
			//Esta solución evita pensar quién se ejecuta antes
			char **argTime=args;
			argTime=argTime+2;
			restore_terminal_signals(); //Restaurar las señales por defecto para el hijo
			puts("\e[92m");
			execvp(args[2], argTime);
			printf("El hijo ha fallado (no existe, fallos de permisos ...)\n");
			exit(EXIT_FAILURE);
		}
			
			
			continue;
			
			
			
		}
		
		if(!strcmp(args[0],"bg")){
			int posicion;
			if(args[1]==NULL){
				posicion=1;
			}else{
				posicion=atoi(args[1]);
			}
			job* aux=get_item_bypos(lista,posicion);
			if(aux==NULL){
				puts("\e[91m");
				printf("ERROR: No se encuentra el job");
			}else{
				if(aux->state==STOPPED || aux->state==RESPAWNABLE){					
					printf("Puesto en background el job %d que estaba suspendido, el job era: %s\n",posicion,aux->command);
					killpg(aux->pgid,SIGCONT);
					aux->state=BACKGROUND;
				}
			}
			continue;
		}
		if(!strcmp(args[0],"fg")){
			int posicion;
			enum status statusfg;
			if(args[1]==NULL){
				posicion=1;
			}else{
				posicion=atoi(args[1]);
			}
			job* aux=get_item_bypos(lista,posicion);
			if(aux==NULL){
				puts("\e[91m");
				printf("ERROR: No se encuentra el job");
				
			}else{
				if(aux->state==STOPPED || aux->state==BACKGROUND || aux->state==RESPAWNABLE){
					printf("Puesto en foreground el job %d que estaba suspendido o en background, el job era: %s\n",posicion,aux->command);
					set_terminal(aux->pgid);
					killpg(aux->pgid,SIGCONT); //manda una señal al grupo de proceso para que continue
					aux->state=FOREGROUND;								
					waitpid(aux->pgid,&status,WUNTRACED);
					statusfg=analyze_status(status,&info);
					
					set_terminal(getpid());
					if(statusfg==SUSPENDED){
						aux->state=STOPPED;
					}else{
						delete_job(lista,aux);
					}
				}else{
					printf("El proceso no estaba en background o suspendido");
				}
			}
			continue;
		}
		
// -----------------------------------------------------------------------
		

		/* the steps are:
			 (1) fork a child process using fork()
			 (2) the child process will invoke execvp()
			 (3) if background == 0, the parent will wait, otherwise continue 
			 (4) Shell shows a status message for processed command 
			 (5) loop returns to get_commnad() function
		*/

// -----------------------------------------------------------------------
//  Implementación de un proceso(padre e hijo). 
// -----------------------------------------------------------------------

		pid_fork = fork();
		
		//Cuidado que no falle el fork dando un -1, que imprima un mensaje de error.
		if(pid_fork == -1){
			puts("\e[91m");
			printf("Error, pid negativo\n");
			exit(EXIT_FAILURE);
		}
		
		if(pid_fork){
			//Hijo independiente en un grupo nuevo
			new_process_group(pid_fork);//<-- tty lo tiene el shell
			
			//Padre = shell
			if(background==1){
				///Background -> (1) no tienen tty, (2) el shell no espera
				
				
				//INSERTAR JOB EN LA LISTA DE JOBS SI FG SUSPENDIDO
				/*Insertar en la lista*/
				nuevo = new_job(pid_fork, args[0],NULL, BACKGROUND,0);
				block_SIGCHLD();
				
				/*Habría que comprobar que el hijo está vivo antes de meterlo en la lista */
				add_job(lista, nuevo);
				unblock_SIGCHLD();
				if(info!=1){
					printf("Proceso %d (%s) en background\n", pid_fork, args[0]);
				}
				
			}else if(background==2){ //si es respawnable
				nuevo = new_job(pid_fork, args[0],args, RESPAWNABLE,0); //lo metemos con estado respawnable
				block_SIGCHLD();
				
			
				add_job(lista, nuevo);
				unblock_SIGCHLD();
				printf("Proceso %d (%s) en se ha convertido en RESPAWNABLE\n", pid_fork, args[0]);
			}
			 else{
				///Foreground
				//Ceder el terminal al hijo
				printf("Foreground pid:%d, command: %s, %s, info: %d\n",pid_fork, args[0],status_strings[status_res],info);
				set_terminal(pid_fork);
				
				//Este wait es bloqueante --> no poner WNOHAG
				pid_wait = waitpid(pid_fork, &status, WUNTRACED);
				puts("\e[0m");
				//Ojo pid_wait == pid_fork ...!
				if(pid_wait != pid_fork){
					printf("Error, pid_wait != pid_fork\n");
					exit(EXIT_FAILURE);
				}
				
				//Recuperar el terminal (el shell), cuando el proceso en fg termina o se suspende
				//Zona libre de SIGTTIN, SIGTTOUT (por el ignore_terminal_signals()) ... podemos quitar el tty
				set_terminal(getpid());
				
				//Imprimir qué pasa
				 status2=analyze_status(status, &info);
				 tcsetattr(shell_terminal, TCSANOW, &conf);
				 if(info!=1){
					printf("Foreground pid:%d, command: %s, %s, info: %d\n",pid_fork, args[0],status_strings[status2],info);
				}
				 print_analyzed_status(status2, info); //Esto imprime mensajes en función de status e info  
				 
				//INSERTAR JOB EN LA LISTA DE JOBS SI FG SUSPENDIDO
				/*Insertar en la lista*/
				if(status2 == SUSPENDED){
					nuevo = new_job(pid_fork, args[0],NULL, STOPPED,0); //cambiado de suspended
					block_SIGCHLD();					
					/*Habría que comprobar que el hijo está vivo antes de meterlo en la lista */
					
					add_job(lista, nuevo);
					unblock_SIGCHLD(); 
				}
				
				
			}
		} else{
			//Hijo --> exec
			//Grupo nuevo y coger el terminal por si las moscas el hijo se planifica antes
			//Esta solución evita pensar quién se ejecuta antes
			new_process_group(getpid());
			
			if(!background){ //Coger el tty --> solo en fg
				set_terminal(getpid());
			}
			
			restore_terminal_signals(); //Restaurar las señales por defecto para el hijo
			puts("\e[92m");
			execvp(args[0], args);
			printf("El hijo ha fallado (no existe, fallos de permisos ...)\n");
			exit(EXIT_FAILURE);
		}
// -----------------------------------------------------------------------
		

	} // end while
}
