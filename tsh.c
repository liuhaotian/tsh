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

/************System include***********************************************/
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

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

#define BUFSIZE 512

/************Global Variables*********************************************/

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */
static void
sig(int);

/************External Declaration*****************************************/

/**************Implementation***********************************************/
/*
 * ReadConfig
 *
 * arguments: none
 *
 * returns: none
 *
 * Read the config in ~/.tshrc. If not exists, it will be created.
 * All the variables should be defined as a global variables in the config.h
 */
void
ReadConfig()
{
	char pathtshrc[MAXLINE];
	strcpy(pathtshrc, getenv("HOME"));
	strcat(pathtshrc, "/.tshrc");
	FILE *fp;
	char tmpline[MAXLINE];
	if(NULL==(fp=fopen(pathtshrc, "r"))){
	//	printf("not exists!\n");
		return;
	}
	else{
		while(fgets(tmpline,MAXLINE,fp)!=NULL)
		{
			if(*tmpline=='#'||*tmpline=='\n'||*tmpline==EOF||*tmpline==0)continue;
			if(tmpline[strlen(tmpline)-1]=='\n')tmpline[strlen(tmpline)-1]=0;
			Interpret(tmpline);
		}		
		fclose(fp);
	}	
}  /* ReadConfig */

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
  char* cmdLine = malloc(sizeof(char*) * BUFSIZE);

envtable = malloc(sizeof(commandT) + sizeof(char*) * 512);
envtable->argv[0]=0;
envtable->name=0;
envtable->argc=0;

  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR)
    PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR)
    PrintPError("SIGTSTP");
	
	ReadConfig();
  while (!forceExit) /* repeat forever */
	{
		PrintNewline();
      /* read command line */
      getCommandLine(&cmdLine, BUFSIZE);

      /* checks the status of background jobs */
      CheckJobs();

      /* interpret command and line
       * includes executing of commands */
      Interpret(cmdLine);

      if (strcmp(cmdLine, "exit") == 0)
        forceExit = TRUE;
    }

  /* shell termination */
  free(cmdLine);
freeCommand(envtable);
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
	printf("\r");
} /* sig */
