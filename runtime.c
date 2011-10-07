/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

typedef struct bgjob_l
{
  pid_t pid;
  struct bgjob_l* next;
} bgjobL;

/* the pids of the background processes */
bgjobL *bgjobs = NULL;

/************Function Prototypes******************************************/
/* run command */
static void
RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void
RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool
ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void
Exec(commandT*, bool);
/* runs a builtin command */
static void
RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool
IsBuiltIn(char*);
/************External Declaration*****************************************/

/**************Implementation***********************************************/


/*
 * RunCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs the given command.
 */
void
RunCmd(commandT* cmd)
{
  RunCmdFork(cmd, TRUE);
wait(NULL);
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0].
 */
void
RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc <= 0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
    {
      RunBuiltInCmd(cmd);
    }
  else
    {
      RunExternalCmd(cmd, fork);
    }
} /* RunCmdFork */


/*
 * RunCmdBg
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a command in the background.
 */
void
RunCmdBg(commandT* cmd)
{
  // TODO
} /* RunCmdBg */


/*
 * RunCmdPipe
 *
 * arguments:
 *   commandT *cmd1: the commandT struct for the left hand side of the pipe
 *   commandT *cmd2: the commandT struct for the right hand side of the pipe
 *
 * returns: none
 *
 * Runs two commands, redirecting standard output from the first to
 * standard input on the second.
 */
void
RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
} /* RunCmdPipe */


/*
 * RunCmdRedirOut
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard output
 *
 * returns: none
 *
 * Runs a command, redirecting standard output to a file.
 */
void
RunCmdRedirOut(commandT* cmd, char* file)
{
} /* RunCmdRedirOut */


/*
 * RunCmdRedirIn
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard input
 *
 * returns: none
 *
 * Runs a command, redirecting a file to standard input.
 */
void
RunCmdRedirIn(commandT* cmd, char* file)
{
}  /* RunCmdRedirIn */


/*
 * RunExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Tries to run an external command.
 */
static void
RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd))
    Exec(cmd, fork);
}  /* RunExternalCmd */


/*
 * ResolveExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: bool: whether the given command exists
 *
 * Determines whether the command to be run actually exists.
 */
static bool
ResolveExternalCmd(commandT* cmd)
{
	int ret=0,i,j;
	char pathtmp[MAXLINE];
	char pathcwd[MAXLINE*2];
	char pathenv[MAXLINE*3];
	char* path[10];
	strcpy(pathenv, getenv("PATH"));
	path[0]=pathenv;
	getcwd(pathcwd,sizeof(pathcwd));
	for(i=0,j=1;*(path[0]+i)!='\0';i++)
	{
	 	if(*(path[0]+i)==':')
		{
			*(path[0]+i)='\0';
			path[j++]=path[0]+i+1;
		}
	}
	path[j]=pathcwd;

	if(*(cmd->argv[0])=='/')
	{
		if((j=open(cmd->argv[0], O_RDONLY))>=0)
		{
			close(j);
			ret=1;
		}
	}
	else if(*(cmd->argv[0])=='.')
	{
		strcpy(pathtmp, pathcwd);
		strcat(pathtmp, "/");
		strcat(pathtmp, cmd->argv[0]);
		if((j=open(pathtmp, O_RDONLY))>=0)
		{
			close(j);
			ret=1;
		}
	}
	else
	{
		for(i=0;(path[i]!=NULL)&&(*path[i]=='/');i++)
		{
			strcpy(pathtmp, path[i]);
			strcat(pathtmp, "/");
			strcat(pathtmp, cmd->argv[0]);
			if((j=open(pathtmp, O_RDONLY))>=0)
			{
				close(j);
			}
			ret=(j>0||ret>0);
		}
	}
	if(ret<=0)
		perror("exec");
	return ret;
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *
 * returns: none
 *
 * Executes a command.
 */
static void
Exec(commandT* cmd, bool forceFork)
{
	if(forceFork==TRUE){
			pid_t forkreturn;
			forkreturn=fork();
			if(forkreturn==0){
				//printf("%s\n",cmd->argv[0]);
				execvp(cmd->argv[0],cmd->argv);
				PrintPError("exec");
				forceExit = TRUE;
			}
		}
		else{
			printf("It's not fork!\n");
		}
} /* Exec */


/*
 * IsBuiltIn
 *
 * arguments:
 *   char *cmd: a command string (e.g. the first token of the command line)
 *
 * returns: bool: TRUE if the command string corresponds to a built-in
 *                command, else FALSE.
 *
 * Checks whether the given string corresponds to a supported built-in
 * command.
 */
static bool
IsBuiltIn(char* cmd)
{
	int i;
	if(strcmp(cmd, "exit")==0)
		return TRUE;
	else if(strcmp(cmd, "cd")==0)
		return TRUE;
//	else if(strcmp(cmd, "echo")==0)
//		return TRUE;
	else{
		for(i=0;cmd[i]!=0;i++){
			if(cmd[i]=='=')
				return TRUE;
		}
	}
	return FALSE;
} /* IsBuiltIn */


/*
 * RunBuiltInCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a built-in command.
 */
static void
RunBuiltInCmd(commandT* cmd)
{
	int i;
	if(strcmp(cmd->argv[0], "cd")==0){
		if(cmd->argv[1]==NULL)
		{
			chdir(getenv("HOME"));
		}
		else if(*(cmd->argv[1])=='.')
		{
			char pathcwd[MAXLINE],pathtmp[MAXLINE];
			getcwd(pathcwd,sizeof(pathcwd));
			strcpy(pathtmp, pathcwd);
			strcat(pathtmp, "/");
			strcat(pathtmp, cmd->argv[1]);
			chdir(pathtmp);
		}
		else if (chdir(cmd->argv[1]) != 0)
			perror("cd");
	}
	else if(strcmp(cmd->argv[0], "echo")==0){
		for(i=1;i<cmd->argc;i++){
			if(*(cmd->argv[i])=='$')
			{	
				printf("%s ",getenv(cmd->argv[i]+1));
			}
			else
				printf("%s ",cmd->argv[i]);
		}
		putchar('\n');
	}
	else{
		int i,j;
		for(i=0;cmd->name[i]!=0;i++){
			if(cmd->name[i]=='='){
				cmd->name[i]=0;// we break the string cmd-name, so it will be easy to find the variable in envtable
				char tmp;
				for(j=0;j<envtable->argc;j++){
					if(strlen(envtable->argv[j])>=strlen(cmd->name))
					{
						tmp=*(envtable->argv[j]+i);
						*(envtable->argv[j]+i)=0;
						if(strcmp(envtable->argv[j],cmd->name)==0){
						/*	free(envtable->argv[j]);
							envtable->argv[envtable->argc]=malloc(sizeof(char)*(strlen(cmd->name)+1));
							strcpy(envtable->argv[envtable->argc],cmd->name);
							putenv(envtable->argv[envtable->argc]);
							*/
							*(envtable->argv[j]+i)=tmp;
							break;
						}
						*(envtable->argv[j]+i)=tmp;
					}	
				}//find the envtable index of cmd->name
				cmd->name[i]='=';//restore the '='
				
				if(j<envtable->argc){// we find the env variable in the envtable
					free(envtable->argv[j]);
					envtable->argv[j]=malloc(sizeof(char)*(strlen(cmd->name)+1));
					strcpy(envtable->argv[j],cmd->name);
					putenv(envtable->argv[j]);
					//printf("We repalce!\n");
				}
				else if(j==envtable->argc)// we cannot find it, so create another
				{
					envtable->argv[envtable->argc]=malloc(sizeof(char)*(strlen(cmd->name)+1));
					strcpy(envtable->argv[envtable->argc],cmd->name);
					putenv(envtable->argv[envtable->argc]);
					envtable->argc++;
					envtable->argv[envtable->argc] = 0;
					//printf("We create!\n");
				}
				
				//printf("%s,%d",envtable->name,envtable->argc);
				
				break;
			}
		}
	}
} /* RunBuiltInCmd */


/*
 * CheckJobs
 *
 * arguments: none
 *
 * returns: none
 *
 * Checks the status of running jobs.
 */
void
CheckJobs()
{
} /* CheckJobs */
