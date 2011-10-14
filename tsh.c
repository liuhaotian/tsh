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
pid_t fgpid;

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

	// Initialize global vars
	paths = (char**)malloc(100*sizeof(char*));
	filedir = (char*)malloc(500*sizeof(char));
	currentdir = (char*)malloc(500*sizeof(char));

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

      /* checks the status of background jobs */
      CheckJobs();

      if (strcmp(cmdLine, "exit") == 0)
        break;

      /* interpret command and line
       * includes executing of commands */
      Interpret(cmdLine);
    }

  /* shell termination */
  free(cmdLine);
	free(paths);
	free(filedir);
	free(currentdir);
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
		kill(SIGINT, fgpid);
		PrintNewline();		
	}
} /* sig */
