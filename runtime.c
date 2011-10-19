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

#define _STOPPED 0
#define _RUNNING 1

/* the pids of the background processes */
bgjobL *bgjobs = NULL;
/* done jobs list for CheckJobs() */
bgjobL *donejobs = NULL;

/* alias list */
aliasL *aliaslist = NULL;

/* global variable to hold current directory when runtime funtions called */
char* currentdir;
/* global variable to hold directory of found command file for ResolveExternalCmd and RunExternalCmd communication */
char* filedir;
/* global variable to hold paths from PATH */
char** paths;

pid_t fgpid; // pid of foreground
//pid_t lspid; // pid of last stopped
char* fgcommands; // keeps command string of current foreground so fg and bg can use it
int fgstatus; // for busy loop

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

int addjob(pid_t, int, char*, int);
int removejob(bgjobL*, int, int);
bgjobL* findjobindex(int);
bgjobL* findjobpid(pid_t);
int findindexpid(pid_t);
bgjobL* getmostrecentjob();
int getnewindex();

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
	int bgstatus=0;
	int i;
	char redirfile[MAXLINE];
	if ((cmd->argc - 1 >= 0) && cmd->argv[cmd->argc - 1][0] == '&') { //we need to background
		free(cmd->argv[cmd->argc-1]);
		cmd->argv[cmd->argc-1] = 0;
		cmd->argc--;

		bgstatus = 1;
	}
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
		else if(cmd->argv[i][0] == '~')
		{
			char* tempcmd=malloc(  sizeof(char) * ( strlen(cmd->argv[i]) + strlen(getenv("HOME")) )  );
			strcpy(tempcmd,getenv("HOME"));
			strcat(tempcmd,cmd->argv[i]+1);
			free(cmd->argv[i]);
			cmd->argv[i]=tempcmd;
			
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
	else {
		if (bgstatus)
			RunCmdFork(cmd, FALSE);
		else
			RunCmdFork(cmd, TRUE);
	}
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fg: whether to fg
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0]
 */
void
RunCmdFork(commandT* cmd, bool fg)
{
  if (cmd->argc <= 0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
	{
		RunBuiltInCmd(cmd);
	}
  else if (ResolveExternalCmd(cmd))
	{
		RunExternalCmd(cmd, fg);
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
  // Not used, we use Exec with the fg bool given through RunExternalCmd
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
 *   bool fg: whether to fg
 *
 * returns: none
 *
 * Tries to run an external command.
 */
static void
RunExternalCmd(commandT* cmd, bool fg)
{
	Exec(cmd, fg);
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
 *   bool fg: whether to fg
 *
 * returns: none
 *
 * Executes a command.
 */
void
Exec(commandT* cmd, bool fg)
{
	// Fork and execute the command using filedir as command and cmd->argv as arguments
	int pid;
	sigset_t chldsigset; // Initialize and set sigset for procmask
	sigemptyset(&chldsigset);
	sigaddset(&chldsigset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chldsigset, NULL); // mask SIGCHLDs

	char* commands = (char*)malloc(500*sizeof(char));
	int i = 0;
	while (cmd->argv[i] != 0) // traverse commands and copy into one new jobcommands string
	{
	  int j = 0; int insertquotes = 0;
	  while (cmd->argv[i][j] != 0)
	  {
	    if (cmd->argv[i][j] == ' ') // if there are spaces in a single command
	      insertquotes = 1;
	    j++;
	  }
	  if (i == 0)
		  strcpy(commands, cmd->argv[i]);
	  else
		{
		 	strcat(commands, " ");
			if (insertquotes)
			  strcat(commands, "\"");
			strcat(commands, cmd->argv[i]);
			if (insertquotes)
			  strcat(commands, "\"");
		}
		i++;
	}

	pid = fork();
	if (pid >= 0) // fork succeeds
	{
		if (pid == 0) // child process
		{
			setpgid(0,0);
			// Unblock SIGCHLD first
			sigprocmask(SIG_UNBLOCK, &chldsigset, NULL);			
			// Child will execv the command
			if (execv(filedir, cmd->argv) < 0)
				PrintPError("Error in executing the command");
			exit(0);
		}
		else // parent wait if fg set
		{
			if (fg) {
				fgpid = pid; //set global fgpid to current pid
				strcpy(fgcommands, commands);
				fgstatus = _RUNNING; // start fgstatus to _RUNNING

				// Unblock SIGCHLD before busy loop
				sigprocmask(SIG_UNBLOCK, &chldsigset, NULL);

				while (fgstatus)
				{
					sleep(1);
				}
			}
			else // bg
			{
				char* ampersand = " &";
				strcat(commands, ampersand);
					
				int jobnumber = addjob(pid, _RUNNING, commands, _JOBLIST);
				if (jobnumber == 0)
					PrintPError("Error in adding job");
			
				// Unblock SIGCHLD after adding to job list
				sigprocmask(SIG_UNBLOCK, &chldsigset, NULL);
			}
		}
	}
	else // fork errors
	{
		PrintPError("Error in fork");
	}
	free(commands);	
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
	  if (strcmp(cmd, "pwd") == 0 || strcmp(cmd, "cd") == 0 || strcmp(cmd, "jobs") == 0 || strcmp(cmd, "fg") == 0 || strcmp(cmd, "bg") == 0 || strcmp(cmd, "alias") == 0 || strcmp(cmd, "unalias") == 0)
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
	if (strcmp(cmd->argv[0], "jobs") == 0)
	{
		bgjobL* temp = bgjobs;
		while (temp != NULL)
		{
			if (temp->status) // running
			{
				char* commands = (char*)malloc(500*sizeof(char));
				strcpy(commands, temp->commands);
				if (commands[strlen(commands) - 1] != '&')
				  strcat(commands, " &");
				printf("[%d]   Running                  %s\n", temp->index, commands);
				fflush(stdout);
				free(commands);
			}
			else
			{
				//char* commands = (char*)malloc(500*sizeof(char));
				//strcpy(commands, temp->commands);
				printf("[%d]   Stopped                  %s\n", temp->index, temp->commands);
				fflush(stdout);
				//free(commands);
			}
			temp = temp->next;
		}
	}
	if (strcmp(cmd->argv[0], "bg") == 0)
	{
		if (cmd->argc > 2) // too many args
		{
			PrintPError("bg takes one or no arguments");
			return;
		}

		//sigset_t chldsigset; // Initialize and set sigset for procmask
		//sigemptyset(&chldsigset);
		//sigaddset(&chldsigset, SIGCHLD);
		//sigprocmask(SIG_BLOCK, &chldsigset, NULL); // mask SIGCHLDs

		if (bgjobs != NULL)
		{
			bgjobL* temp;
			if (cmd->argc == 1) // default
				temp = getmostrecentjob();
			else if (findjobindex(atoi(cmd->argv[1])))
				temp = findjobindex( atoi(cmd->argv[1]) );
			else
			{
				PrintPError("Index outside of job bounds");
				return;
			}
			

			pid_t pid = temp->pid;
			kill(-pid, SIGCONT); // Send SIGCONT to the job and all processes
			temp->status = _RUNNING; // Set status to running
			
			
			//sigprocmask(SIG_UNBLOCK, &chldsigset, NULL); //unmask
		}	
	}
	if (strcmp(cmd->argv[0], "fg") == 0)
	{
		if (cmd->argc > 2) // too many args
		{
			PrintPError("fg takes one or no arguments");
			return;
		}

		//sigset_t chldsigset; // Initialize and set sigset for procmask
		//sigemptyset(&chldsigset);
		//sigaddset(&chldsigset, SIGCHLD);
		//sigprocmask(SIG_BLOCK, &chldsigset, NULL); // mask SIGCHLDs

		if (bgjobs != NULL)
		{
			bgjobL* temp;
			if (cmd->argc == 1) // default
				temp = getmostrecentjob();
			else if (findjobindex(atoi(cmd->argv[1])))
				temp = findjobindex( atoi(cmd->argv[1]) );
			else
			{
				PrintPError("Index outside of job bounds");
				return;
			}
			
			fgpid = temp->pid; // set fg to pid of job
			strcpy(fgcommands, temp->commands); // copy commands to fgcommands
			fgcommands[strlen(fgcommands) - 1] = 0;
			
			if (temp->status == _STOPPED) // if stopped, SIGCONT it to get it running again
				kill(-fgpid, SIGCONT);

			// remove from job list
			if ( !removejob(temp, _JOBLIST, 1) )
				PrintPError("Not a job");

			//sigprocmask(SIG_UNBLOCK, &chldsigset, NULL); //unmask
			fgstatus = _RUNNING;
			// Now wait on it with a busy loop
			while (fgstatus)
			{
				sleep(1);
			}
		}	
	}
	if (strcmp(cmd->argv[0], "alias") == 0){
		aliasL* temp=aliaslist;
		if(cmd->argc==1)/*	show all the alias	*/
		{
			if (temp == NULL) // start of list
			{
				;
			}
			else // something at start of list
			{
				while (TRUE)
				{
					if(temp!=NULL){
						int i;
						for( i = 0; temp->aliascmdline[i] != 0; ++i)
						{
							if(i==0)printf("alias ");
							putchar(temp->aliascmdline[i]);
							if(temp->aliascmdline[i]=='=')putchar('\'');
							if(temp->aliascmdline[i+1]==0)puts("\'");
						}
						//printf("alias %s\n",temp->aliascmdline);
					}
					if (temp->next == NULL)
					{
						break;
					}
					temp = temp->next;
				}
			}
			
		}
		else{
			aliasL* newalias = malloc(sizeof(aliasL)); // malloc memory for new alias
			if (newalias == NULL){
				PrintPError("Error in allocating memory for alias");
				return;
			}

			// Initialize new alias
			newalias->aliascmdline = (char*)malloc(strlen(cmd->argv[1]+1));
			if (newalias->aliascmdline == NULL){
				PrintPError("Error in allocating memory for alias");
				return;
			}
			strcpy(newalias->aliascmdline, cmd->argv[1]);
			newalias->next = NULL;

			if (temp == NULL) // start of list
			{
				aliaslist=newalias;
			}
			else // something at start of list
			{
				while (TRUE)
				{
					if (temp->next == NULL)
					{
						temp->next = newalias;
						break;
					}
					temp = temp->next;
				}
			}
		}
		
	}
	if (strcmp(cmd->argv[0], "unalias") == 0){
		aliasL* temp=aliaslist;
		aliasL* previousalias=aliaslist;
		if(cmd->argc==1)// do nothing
		{
			;
		}
		else
		{
			while (TRUE)
			{
				if( ( temp != NULL ) && (strncmp(cmd->argv[1],temp->aliascmdline,strlen(cmd->argv[1]))==0) )// we find the alias, so unalias it
				{
					previousalias->next = temp->next;
					if(temp==aliaslist)aliaslist=temp->next;
					free(temp->aliascmdline);
					free(temp);
					break;
				}
				if ( ( temp == NULL ) || (temp->next == NULL) )
				{
					printf("%s: unalias: %s: not found\n",SHELLNAME,cmd->argv[1]);
					//PrintPError(cmd->argv[1]);
					break;
				}
				previousalias = temp;
				temp = temp->next;
			}
		}
	}
} /* RunBuiltInCmd */

/*
 * Job Functions
 */
int addjob(pid_t jobpid, int jobstatus, char* jobcommands, int whichlist) // Makes and adds job to the job list or done list and returns index of job
{
	int i = getnewindex(); // grab what the index should be
	bgjobL* temp;
	if (whichlist == _JOBLIST)
		temp = bgjobs; // temporary header
	else
		temp = donejobs;

	bgjobL* newJob = malloc(sizeof(bgjobL)); // malloc memory for new job
	if (newJob == NULL)
		PrintPError("Error in allocating memory for job");
	// Initialize new job
	newJob->pid = jobpid;
	newJob->status = jobstatus;
	newJob->commands = (char*)malloc(500*sizeof(char));
	newJob->index = i;
	strcpy(newJob->commands, jobcommands);
	newJob->next = NULL;

	if (temp == NULL) // start of list
	{
		if (whichlist == _JOBLIST)
			bgjobs = newJob; // move header
		else
			donejobs = newJob;
		return i;
	}
	else // something at start of list
	{
		while (TRUE)
		{
			if (temp->next == NULL)
			{
				temp->next = newJob;
				break;
			}
			temp = temp->next;
		}
		return i;
	}
}

int removejob(bgjobL* job, int whichlist, int fg) // Removes job from either list, keeping integrity of list, returns 0 if empty list or not found 1 if success
{
	if (whichlist == _JOBLIST && !fg)
	{
		//Add done job to donelist so checkjobs can print it out in main
		char* donecommands = (char*)malloc(500*sizeof(char));
		char* jobcommands = job->commands;
		strcpy(donecommands, jobcommands);
		addjob(job->pid, job->index, donecommands , _DONELIST); // status doubling as index
		free(donecommands);
	}

	bgjobL* temp;
	if (whichlist == _JOBLIST)
		temp = bgjobs; // set temp pointer to head of jobs list
	else
		temp = donejobs;

	if (temp == NULL) // noone in list
	{
		free(job->commands);
		free(job); 
		return 0;
	}
	// Find the node in list and delete and maintain list integrity
	if (temp == job) // if first in list is the job
	{
		if (whichlist == _JOBLIST)
			bgjobs = temp->next; // move header
		else
			donejobs = temp->next;

		free(job->commands); // free command pointer
		free(job); // delete node
		return 1;
	}
	else // search through remembering previous to set its next to job->next and delete job
	{
		while (temp->next != job && temp->next != NULL)
			temp = temp->next;
		if (temp->next == NULL) // we couldn't find it, not in list
		{
			free(job->commands);
			free(job);
			return 0;
		}
		
		temp->next = job->next;
		free(job->commands);
		free(job);
		
		return 1;
	}
}

bgjobL* findjobindex(int index) // Looks for job of given index and returns it, else return NULL
{
	// Changed due to change of the way index is handled
	/*
	if (index < 1) // no such index
		return NULL;

	bgjobL* temp = bgjobs; // grab header
	int i;
	for (i = 1; i <= index; i++)
	{
		if (temp == NULL) // we reached end of list before finding index
			return NULL;
		
		if (i == index) // we are at index and it is not NULL
			return temp;

		temp = temp->next;
	}
	return NULL; // shouldn't get here */
	
	bgjobL* temp = bgjobs;
	while (temp != NULL)
	{
	  if (temp->index == index)
	      return temp;
	  
	  temp = temp->next;
	}
	
	return NULL;
	
}

bgjobL* findjobpid(pid_t jobpid) // Looks for job of given pid and returns i, else return NULL
{
	bgjobL* temp = bgjobs; // grab header
	while (temp != NULL)
	{
		if (temp->pid == jobpid)
			return temp;
		
		temp = temp->next;
	}

	return NULL; // we couldn't find it
		
}

int findindexpid(pid_t jobpid) // Look for index of given pid in job list
{
	// ORIGINALLY had index be just the order number in list but changed to absolute number to pass test cases
	/*
	bgjobL* temp = bgjobs;
	if (temp == NULL) // no one in list
		return 0;

	int i = 1;
	while (temp != NULL)
	{
		if (temp->pid == jobpid)
			return i;

		temp = temp->next;
		i++;
	}

	return 0; // we couldn't find it */
	
	return ((findjobpid(jobpid))->index);
}

bgjobL* getmostrecentjob() // Returns the last backgrounded job
{
	bgjobL* temp = bgjobs; // grab header
	if (temp == NULL) // no one in list
		return NULL;
	
	while (temp->next != NULL)
	{
		temp = temp->next;
	}

	return temp;
}

int getnewindex() // Gets the new index for addjob
{
  
    // Ideally, this would set the new index to the next largest only if the list has no inconsistencies (ie. a list with jobs of indices 1,2,4 would be inconsistent because we could add a job and make it index 3)
    bgjobL* temp = bgjobs; // grab header
    int i = 1;
    if (temp == NULL) // no one in list
	return i;
    
    if (temp->next == NULL && temp->index > 1) // only one in list and it's got an index greater than 1, so make new one 1
      return i;
    
    i = temp->index;
    while (temp->next != NULL)
    {
      temp = temp->next;
      if (temp->index > i)
	i = temp->index;
    }
    i++;
    return i;
}
      

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
	bgjobL* temp = donejobs; // grab header
	if (temp == NULL) // no one in list
		return;
	
	do
	{
		temp->commands[strlen(temp->commands)] = 0;
		temp->commands[strlen(temp->commands) - 1] = 0; // rid the ampersand
		printf("[%d]   Done                     %s\n", temp->status, temp->commands); //status doubling as index
		fflush(stdout);
		temp = temp->next;
	} while (temp != NULL);
	
	
	temp = donejobs;
	do
	{
		removejob(temp, _DONELIST, 0);
		temp = donejobs;
	}
	while (temp != NULL);

	donejobs = NULL;
} /* CheckJobs */
