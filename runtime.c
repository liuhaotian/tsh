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

/* global variable to hold current directory when runtime funtions called */
char* currentdir;
/* global variable to hold directory of found command file for ResolveExternalCmd and RunExternalCmd communication */
char* filedir;
/* global variable to hold paths from PATH */
char** paths;

pid_t pid;

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
static void
handler(int);
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
 * Runs the given command. Initializes globals.
 */
void
RunCmd(commandT* cmd)
{
	int redirstatus=0;
	int pipestatus=0;
	int i;
	char redirfile[MAXLINE];
	if( ( cmd->argc-2>=0 ) && (cmd->argv[cmd->argc-2][0] == '>') ){//we get the redir out.
		strcpy(redirfile,cmd->argv[cmd->argc-1]);
		
		free(cmd->argv[cmd->argc-2]);//wo don't need it anymore
		free(cmd->argv[cmd->argc-1]);
		cmd->argv[cmd->argc-2] = 0;
		cmd->argv[cmd->argc-1] = 0;
		cmd->argc=cmd->argc-2;
		
		RunCmdRedirOut(cmd, redirfile);//redirect output
		redirstatus=1;
	}
	else if( ( cmd->argc-2>=0 ) && ( cmd->argv[cmd->argc-2][0] == '<') ){//we get the redir in
		strcpy(redirfile,cmd->argv[cmd->argc-1]);
		
		free(cmd->argv[cmd->argc-2]);//wo don't need it anymore
		free(cmd->argv[cmd->argc-1]);
		cmd->argv[cmd->argc-2] = 0;
		cmd->argv[cmd->argc-1] = 0;
		cmd->argc=cmd->argc-2;
		
		RunCmdRedirIn(cmd, redirfile);//redirect input
		redirstatus=1;
	}
	else
	for(i = 0; i < cmd->argc; ++i)
	{
		if(cmd->argv[i][0] == '|')
		{
			pipestatus=i;
			break;
		}
	}
	
	if(redirstatus==1)
	{
		;
	}
	else if(pipestatus > 0)
	{
		commandT* cmd1 = malloc(sizeof(commandT) + sizeof(char*) * cmd->argc);
		commandT* cmd2 = malloc(sizeof(commandT) + sizeof(char*) * cmd->argc);
		char* tmp = cmd->argv[pipestatus];
		cmd->argv[pipestatus]=0;
		
		/*	copy the cmd before '|' to cmd1	*/
		cmd1->name = cmd->argv[0];
		cmd1->argc = pipestatus;
		for(i = 0; cmd->argv[i] != 0; ++i)
		{
			cmd1->argv[i] = cmd->argv[i];
		}
		cmd1->argv[i]=0;
		
		/*	copy the cmd after '|' to cmd2	*/
		cmd2->name = cmd->argv[pipestatus+1];
		cmd2->argc = cmd->argc-pipestatus-1;
		for(i = pipestatus +1; cmd->argv[i] != 0; ++i)
		{
			cmd2->argv[i-pipestatus-1] = cmd->argv[i];
		}
		cmd2->argv[i-pipestatus-1] = 0;
		
		cmd->argv[pipestatus]=tmp; //restore the cmd->argv[pipestatus]
		
		RunCmdPipe(cmd1,cmd2);
	}
	else  
		RunCmdFork(cmd, TRUE);
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
 * depending on cmd->argv[0]
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
  else if (ResolveExternalCmd(cmd))
	{
		RunExternalCmd(cmd, fork);
	}
	else
	{
		char* temp = (char*)malloc(500*sizeof(char));
		char* unfoundCommand = cmd->argv[0];
		char* unresolvedCommand = "line 1: "; // does line 1 refer to error in first argument?
		strcpy(temp, unresolvedCommand);
		strcat(temp, unfoundCommand);
		PrintPError(temp);
		free(temp);
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
	int pipefp[2];
	int standardout = dup(1);
	int standardin  = dup(0);
	
	if(pipe(pipefp)==-1)PrintPError("Pipe Create Error");

	/*	redirect the cmd1 output to pipe	*/
	close(1);	//close standard output
	if(dup(pipefp[1]) != 1){
		PrintPError("Pipe Write");
		return;
	}
	close(pipefp[1]);
	
	RunCmd(cmd1);
	
	close(1);	//close redir out, and back to normal
	if(dup(standardout)!=1){
		PrintPError("DupStdOutFile");
		return;
	}
	
	/*	redirect the pipe to cmd2 input		*/
	close(0);	//close standard output
	if(dup(pipefp[0]) != 0){
		PrintPError("Pipe read");
		return;
	}
	close(pipefp[0]);
	
	RunCmd(cmd2);
	
	close(0);	//close redir out, and back to normal
	if(dup(standardin)!=0){
		PrintPError("DupStdInFile");
		return;
	}
	
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
	int standardout = dup(1);
	int fileout = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
	//write only. if not exists, create it. if exists, delete it. rwxr--r--
	
	if(fileout < 0){
		PrintPError("OpenOutFile");
		return;
	}
	close(1);	//close standard output
	if(dup(fileout) != 1){
		PrintPError("DupOutFile");
		return;
	}
	close(fileout);
	
	RunCmd(cmd);
	
	close(1);	//close redir out, and back to normal
	if(dup(standardout)!=1){
		PrintPError("DupStdOutFile");
		return;
	}
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
	int standardin = dup(0);
	int filein = open(file, O_RDONLY);
	//write only. if not exists, create it. if exists, delete it. rwxr--r--
	
	if(filein < 0){
		PrintPError("OpenInFile");
		return;
	}
	close(0);	//close standard output
	if(dup(filein) != 0){
		PrintPError("DupInFile");
		return;
	}
	close(filein);
	
	RunCmd(cmd);
	
	close(0);	//close redir out, and back to normal
	if(dup(standardin)!=0){
		PrintPError("DupStdInFile");
		return;
	}
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
 * Determines whether the command to be run actually exists, and changes global var filedir to its location
 * if it does for RunExternalCmd.
 */
static bool
ResolveExternalCmd(commandT* cmd)
{
	char* filepath= (char*)malloc(500*sizeof(char)); //buffer for building filepaths ,memory leak issue possible here

	if (*cmd->argv[0] == '.') //executable in currentdir
	{
		// get rid of . and add on currentdir before it and check if command exists, change filedir and return TRUE
		char* temp = cmd->argv[0]; //move pt up to next char to rid .
		temp++;
		strcpy(filepath, currentdir); //move current directory to filepath buffer
		filepath = strcat(filepath, temp); //append on the executable
		
		if (access(filepath, X_OK) == 0) // we can access it
		{
			strcpy(filedir, filepath);
			free(filepath);
			return TRUE;
		}
	}
	else if (*cmd->argv[0] == '/') //absolute path
	{
		if (access(cmd->argv[0], X_OK) == 0) // we can access it
		{
			strcpy(filedir, cmd->argv[0]);
			free(filepath);
			return TRUE;
		}
	}
	else // search through directories in PATH
	{
		int i = 0;
		const char* argument = cmd->argv[0];
		while (paths[i] != NULL) // While loop over each directory in PATHS
		{
			strcpy(filepath, paths[i]);
			//add / + argv[0] to paths[i]
			filepath = strcat(filepath, "/"); //possible source for memory leaks when overloading temp, better concatenating process should go here
			filepath = strcat(filepath, argument);

			// use filepath to check if exists, change filedir to filepath and return TRUE
			if (access(filepath, X_OK) == 0) // we can access it
			{
				strcpy(filedir, filepath);
				free(filepath);
				return TRUE;
			}
			i++;
		}
	}
	free(filepath);
	return FALSE;
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
void
Exec(commandT* cmd, bool forceFork)
{
	// Fork and execute the command using filedir as command and cmd->argv as arguments
	int status;
	
	pid = fork();
	if (pid >= 0) // fork succeeds
	{
		if (pid == 0) // child process
		{
			signal(SIGINT, handler);
			// Child will execv the command
			//setpgid(0, 0); only for backgrounded processes
			if (execv(filedir, cmd->argv) < 0)
				PrintPError("Error in executing the command");
			exit(0);
		}
		else // parent
		{
			while ((pid = wait(&status)))
			{
				if (errno == ECHILD)
					break;
			}
		}
	}
	else // fork errors
	{
		PrintPError("Error in fork");
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
	  if (strcmp(cmd, "pwd") == 0 || strcmp(cmd, "cd") == 0)
			return TRUE;
		else
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
	if (strcmp(cmd->argv[0], "pwd") == 0)
	{
		if (cmd->argc != 1) //something other than just pwd entered
			PrintPError("The command pwd takes no arguments.");
		else
			Print(currentdir);	
	}

	if (strcmp(cmd->argv[0], "cd") == 0) //cd needs to account for no argument, absolutes, and ..
	{
		// use cmd->argc (number or args including command cd) to switch between cases
		if (cmd->argc >= 3) // too many args
			PrintPError("The command cd takes one argument.");
		else if (cmd->argc == 1) // no arguments given, chdir to HOME directory
			{
				if (chdir(getenv("HOME")) < 0)
					PrintPError("Could not change directory.");
			}
		else
			if (*cmd->argv[1] == '/') // absolute path
				{
					if (chdir(cmd->argv[1]) < 0)
						PrintPError("Could not change directory.");
				}
			else if (*cmd->argv[1] == '.' && cmd->argv[1][1] == '.') // go back directories
			{
				char* tempPtr;
				tempPtr = cmd->argv[1]; // ptr to walk over argument counting how many directories to go back
				while (*tempPtr != '\0') // kind of a weak way to count, since we assume the user is putting periods
				{
					if (*tempPtr == '/') // skip the slashes
						tempPtr++;
					else
					{
						tempPtr += 2;
						if (chdir("..") < 0) //go back one directory
							PrintPError("Could not change directory.");
					}
				}
			}
			else // argument without . or / so chdir to currentdir + it
			{
				char* temp = (char*)malloc(500*sizeof(char));
				strcpy(temp, currentdir);
				strcat(temp, "/");
				strcat(temp, cmd->argv[1]);
				if (chdir(temp) < 0)
					PrintPError("Could not change directory.");
				free(temp);
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

static void
handler(int signo)
{
	if (signo == SIGINT)
	{
		kill(pid, SIGINT);
		exit(0);	
	}
}
