/***************************************************************************
 *  Title: tsh
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.4 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
#define __MYSS_IMPL__
#define _GNU_SOURCE

/************System include***********************************************/
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80

/************Global Variables*********************************************/
char** paths;
char* filedir;
char* currentdir;
char* cmdLine;
pid_t fgpid; // pid of fg
//pid_t lspid; // pid of last stopped
char* fgcommands; // remember foreground's commands
bgjobL* bgjobs;
bgjobL* donejobs;
int fgstatus; // status for busy while loop of foreground

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */
static void sig(int);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

/*
 * main
 *
 * arguments:
 *   int argc: the number of arguments provided on the command line
 *   char *argv[]: array of strings provided on the command line
 *
 * returns: int: 0 = OK, else error
 *
 * This sets up signal handling and implements the main loop of tsh.
 */
int
main(int argc, char *argv[])
{
  /* Initialize command buffer */
  cmdLine = malloc(sizeof(char*) * BUFSIZE);

  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR)
    PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR)
    PrintPError("SIGTSTP");
	if (signal(SIGCHLD, sig) == SIG_ERR)
		PrintPError("SIGCHLD");

	// Initialize global vars
	paths = (char**)malloc(100*sizeof(char*));
	filedir = (char*)malloc(500*sizeof(char));
	currentdir = (char*)malloc(500*sizeof(char));
	
	fgcommands = (char*)malloc(500*sizeof(char));

	char pathsWithTokens[1024];
	strcpy(pathsWithTokens, getenv("PATH"));
	// Using strtok to parse : out of pathsWithTokens and put each directory string into paths
	int i = 0;
	char* temp = strtok(pathsWithTokens, ":");
	if (temp)
	{
		do
		{
			paths[i] = temp;
			i++;
		}
		while ((temp = strtok(NULL, ":")) != NULL);
	}

  while (TRUE) /* repeat forever */
    {
			// Update currentdir
			char *getCwdReturn = getcwd(currentdir, 500*sizeof(char));
			if (getCwdReturn == NULL)
				PrintPError("Error in finding current working directory");
      /* read command line */
      getCommandLine(&cmdLine, BUFSIZE);

      if (strcmp(cmdLine, "exit") == 0)
        break;

			/* checks the status of background jobs */
      CheckJobs();

      /* interpret command and line
       * includes executing of commands */
      Interpret(cmdLine);
    }

  /* shell termination */
  free(cmdLine);
	free(paths);
	free(filedir);
	free(currentdir);

	free(fgcommands);
  return 0;
} /* main */

/*
 * sig
 *
 * arguments:
 *   int signo: the signal being sent
 *
 * returns: none
 *
 * This should handle signals sent to tsh.
 */
static void
sig(int signo)
{
	if (signo == SIGINT) // Handle SIGINT
	{
		kill(-fgpid, SIGINT);	
	}
	if (signo == SIGTSTP) // Handle SIGTSTP
	{
		//lspid = fgpid;
		int i = addjob(fgpid, _STOPPED, fgcommands, _JOBLIST); // foreground becomes background
		kill(-fgpid, SIGTSTP);
		if (i == 0)
			PrintPError("Error adding job");
	}
	if (signo == SIGCHLD) // Handle SIGCHLD
	{
		pid_t chldpid;
		int status;
		chldpid = waitpid(-1, &status, WUNTRACED | WNOHANG); // wait on child with options for stopping childs
		
		if (chldpid == fgpid) // the foreground sent a SIGCHLD
		{
			fgstatus = _STOPPED; // set fgstatus to 0 so that the sleep loop in Exec can exit
		}
		else if (WIFEXITED(status) || WIFSIGNALED(status)) // SIGCHLD from bg process
		{
			if ( findjobpid(chldpid) == NULL) // not in list
				return;
			int index = findindexpid(chldpid);
 
			if (WIFSIGNALED(status)) // bg child terminated with error status, notify user
				printf("[%d] terminated with status %d  PID: %d\n", index,  WTERMSIG(status), chldpid);

			if ( !removejob( findjobpid( chldpid ), _JOBLIST ) ) //Find job in list and remove it
				PrintPError("SIGCHLD from child that is not a job");
		}		 
	} 
} /* sig */
