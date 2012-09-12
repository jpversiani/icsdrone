/*
icsdroneng, Copyright (c) 2008-2009, Michel Van den Bergh
All rights reserved

This version contains code from polyglot and links against readline. 
Hence it now falls under the GPL2+
*/
/*
icsDrone, Copyright (c) 1996-2001, Henrik Oesterlund Gram
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are 
met:

Redistributions of source code must retain the above copyright 
notice, this list of conditions and the following disclaimer. 

Redistributions in binary form must reproduce the above copyright 
notice, this list of conditions and the following disclaimer in 
the documentation and/or other materials provided with the 
distribution. 

The names of its contributors may not be used to endorse or 
promote products derived from this software without specific 
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "icsdrone.h"

char *welcome=
"   _              _                                  \n"
"  (_)            | |                                 \n"
"   _  ___ ___  __| |_ __ ___  _ __   ___ _ __   __ _ \n"
"  | |/ __/ __|/ _` | '__/ _ \\| '_ \\ / _ \\ '_ \\ / _` |\n"
"  | | (__\\__ \\ (_| | | | (_) | | | |  __/ | | | (_| |\n"
"  |_|\\___|___/\\__,_|_|  \\___/|_| |_|\\___|_| |_|\\__, |\n"
"                                               |___/ \n"
"\n"
;

extern int h_errno;

jmp_buf stackPointer;

RunData runData;


void InitRunData(){
  runData.computerActive=FALSE;
  runData.computerIsWhite=FALSE;
  runData.computerReadFd=-1;
  runData.computerWriteFd=-1;
  runData.computerPid=(pid_t) 0;
  runData.icsReadFd=-1;
  runData.icsWriteFd=-1;
  runData.proxyListenFd=-1;
  runData.proxyFd=-1;
  runData.handle[0]='\0';
  runData.passwd[0]='\0';
  runData.quitPending=FALSE;
  runData.waitingForFirstBoard=FALSE;
  runData.exitValue=0;
  runData.gameID=-1;
  runData.moveNum=-1;
  runData.waitingForPingID=0;
  runData.oppname=NULL;
  runData.forwarding=FALSE;
  runData.forwardingMask=0;
  runData.loggedIn=FALSE;
  runData.registered=TRUE;
  runData.usermove=FALSE;
  runData.engineDrawFeature=TRUE;
  runData.sigint=FALSE;
  runData.computerReady=FALSE;
  runData.lastPlayer[0]='\0';
  runData.numGamesInSeries=0;
  runData.timeOfLastGame=0;
  runData.loginCount=0;
  runData.moveList[0]='\0';
  runData.last_talked_to[0]='\0';
  memset(&runData.icsBoard,0,sizeof(runData.icsBoard));
  runData.lineBoard[0]='\0';
  runData.waitingForMoveList=FALSE;
  runData.promptOnLine=FALSE;
  runData.multiFeedbackDepth=0;
  runData.inExitOn=FALSE;
  runData.blockConsole=TRUE;
  runData.inReadline=FALSE;
  runData.myname[0]='\0';
  runData.prompt[0]='\0';
  runData.historyLoaded=FALSE;
  runData.inhibitPrompt=FALSE;
  runData.color=-1;
  runData.longAlgMoves=TRUE;
  runData.haveCmdResult=TRUE;
  runData.timeOfLastMove=0;
  runData.idleTimeoutTimer=NULL;
  runData.findChallengeTimer=NULL;
  runData.abortTimer=NULL;
  runData.flagTimer=NULL;
  runData.courtesyAdjournTimer=NULL;
  runData.pingTimer=NULL;
  runData.timestring=NULL;
  runData.inGame=FALSE;
  runData.inTourney=FALSE;
  runData.currentTourney=-1;
  runData.lastTourney=-1;
  runData.parsingStandings=FALSE;
  runData.lastGameWasInTourney=0;
  runData.parsingListTourneys=FALSE;
  runData.tournamentOppName[0]='\0';
  runData.registeredDrawOffer=FALSE;
  runData.processingTells=FALSE;
  runData.pingCounter=0;
  runData.timesealPid=(pid_t) 0;
  runData.lastLoginTime=time(NULL);
  runData.tellTimer=NULL;
  runData.tellQueue[0]='\0';
  runData.lastGameWasAbortOrAdjourn=FALSE;
  runData.icsType=ICS_GENERIC;
  runData.parsingMoveList=FALSE;
  runData.processingLastMoves=FALSE;
  runData.internalIcsCommand=0;
  runData.lastIcsPrompt[0]='\0';
  runData.proxyLoginState=PROXY_LOGIN_INIT;
  runData.hideFromProxy=FALSE;
  runData.useMoveList=TRUE;
  runData.icsVariantCount=0;
  /* This done after server detection */
  /* ParseVariantList(appData.variants); */
  runData.chessVariantCount=1;
  strcpy(runData.chessVariants[0],"normal");
  strcpy(runData.chessVariant,"normal");
  runData.noCastle=FALSE;
}

PersistentData persistentData;

void InitPersistentData(){
  persistentData.startTime=time(NULL);
  persistentData.firsttime=TRUE;
  persistentData.wasLoggedIn=FALSE;
  persistentData.games=0;
}


AppData appData = {
    NULL,            /* icsHost */
    23,              /* icsPort */
    5000,            /* proxyPort */
    NULL,            /* proxyHost */
    0,               /* proxy */
    FALSE,           /* proxyLogin */ 
    0,               /* searchDepth */
    0,               /* secPerMove */
    255,             /* logLevel */
    NULL,            /* logFile */
    NULL,            /* owner */
    TRUE,            /* console */               
    FALSE,           /* easyMode */
    NULL,            /* book */
    NULL,            /* program */
    NULL,            /* sendGameEnd */
    NULL,            /* sendTimeOut */ 
    0,               /* limitRematches */
    FALSE,           /* haveCmnPing */
    NULL,            /* loginScript */
    FALSE,           /* issueRematch */
    NULL,            /* sendGameStart */
    NULL,            /* acceptOnly */
    NULL,            /* timeseal */
    FALSE,           /* daemonize */
    NULL,            /* handle */
    NULL,            /* passwd */
    TRUE,            /* logging */
    FALSE,           /* logAppend */
    FALSE,           /* feedback */
    TRUE,            /* feedbackOnlyFinal */
    TRUE,            /* proxyFeedback */
    TRUE,            /* dontReuseEngine */
#ifdef __CYGWIN__
    5,               /* nice */
#else
    0,
#endif
    NULL,            /* pgnFile */
    TRUE,            /* pgnLogging */
    TRUE,            /* resign */
    FALSE,           /* scoreForWhite */
    NULL,            /* shortLogFile */
    TRUE,            /* shortLogging */
    TRUE,            /* autoJoin (experimental) */
    NULL,            /* sendLogin */
    NULL,            /* noplay */
    COLOR_ALERT,     /* colorAlert */
    COLOR_TELL,      /* colorTell */
    COLOR_DEFAULT,   /* colorDefault */
    0,               /* hardLimit */
    TRUE,            /* acceptDraw */
    TRUE,            /* autoReconnect */
    FALSE,           /* ownerQuiet */
    NULL,            /* feedbackCommand */
    FALSE,           /* engineQuiet */
    NULL             /* variants */
};

int  ProcessRawInput P((int, char *, int, void (*)(char *, char*)));

void MainLoop P(());
void TerminateProg P((int));
void BrokenPipe P((int));
void Usage P(());

void BlockConsole(){
  runData.blockConsole=TRUE;
#ifdef HAVE_LIBREADLINE
  rl_callback_handler_remove ();
#endif
}

void ProcessConsoleLine(char *line, char *queue) 
{
  logcomm("console","icsdrone",line);
  line=strtok(line,"\r\n");
  if(line!=NULL && line[0]!='\0'){
    runData.promptOnLine=0;
    runData.inReadline=FALSE;
    BlockConsole();
#ifdef HAVE_LIBREADLINE
    add_history(line);
#endif
    ExecCommand(line,CONSOLE,FALSE);
  }
#ifdef HAVE_LIBREADLINE
  if(line!=NULL){
    free(line);
  }
#endif
}

void SetInterface(){
  char *uptime=strdup(asctime(gmtime(&persistentData.startTime)));
  uptime=strtok(uptime,"\r\n");
  logme(LOG_DEBUG,"Setting interface. runData.icsType=%d\n",runData.icsType);
  if(runData.icsType==ICS_FICS){
      if(runData.myname[0]){
	  SendToIcs("set interface %s-%s + %s. Online since %s GMT (%d games played).\n",PACKAGE_NAME,VERSION,runData.myname,uptime,persistentData.games);
      }else{
	  SendToIcs("set interface %s-%s. Online since: %s GMT (%d games played).\n",PACKAGE_NAME,VERSION,uptime,persistentData.games);
      }
  }else{
      if(runData.myname[0]){
	  SendToIcs("set interface %s-%s + %s. Online since %s GMT.\n",PACKAGE_NAME,VERSION,runData.myname,uptime);
      }else{
	  SendToIcs("set interface %s-%s. Online since: %s GMT.\n",PACKAGE_NAME,VERSION,uptime);
      }
  }
  free(uptime);
}

void SetScoreForWhite(){
    if(!strncasecmp(runData.myname,"Crafty",6)){
        logme(LOG_DEBUG,"Crafty detected. Setting scoreForWhite to true.");
        appData.scoreForWhite=TRUE;
    }
}

void CloseIcs(){
  if(appData.timeseal && runData.timesealPid){
    kill(runData.timesealPid,SIGKILL);
    wait(NULL);
  }
  if(runData.icsReadFd!=0){ // if something goes wrong these are zero=console! 
    close(runData.icsReadFd);
  }
  if(runData.icsReadFd!=runData.icsWriteFd && runData.icsWriteFd!=0){
    close(runData.icsWriteFd); // these two Fd's might be the same... 
  }
  runData.timesealPid=(pid_t) 0;
}

void BlockSignals(){
  sigset_t mask;
  sigfillset(&mask);
  sigprocmask(SIG_BLOCK, &mask, NULL);
}

void UnblockSignals(){
  sigset_t mask;
  sigfillset(&mask);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}


void OpenTimeseal(){
  struct hostent *hp;
  char *ip_addr;
  char port[20];
  int from_fds[2];
  int to_fds[2];
  int fd;
  hp=gethostbyname(appData.icsHost);
  if(hp==NULL){
      ExitOn(EXIT_QUITRESTART,"Cannot resolve hostname");
  }
  ip_addr=inet_ntoa(*((struct in_addr*)(hp->h_addr)));
  snprintf(port,10,"%d",appData.icsPort);
  if ((pipe(from_fds) < 0)|| pipe(to_fds) < 0)
    ExitOn(EXIT_HARDQUIT,"Unable to open pipe.");
  runData.timesealPid=fork();
  if(runData.timesealPid==0){  /* child process */
    close(from_fds[0]);
    close(to_fds[1]);
    dup2(from_fds[1], STDOUT_FILENO);
    dup2(to_fds[0], STDIN_FILENO);
    close(from_fds[1]);
    close(to_fds[0]);
    if(setsid()<0){
      ExitOn(EXIT_HARDQUIT,"Could not put timeseal in its own session.");
    }
    if ((fd = open ("/dev/null", O_RDWR, 0)) >= 0) {
      if (isatty (STDERR_FILENO)) dup2(fd,STDERR_FILENO);
      if (fd > 2) close (fd);
    }
    printf("Starting timeseal binary.\n");
    UnblockSignals();  /* necessary for FICS timeseal */
    if(execlp(appData.timeseal,appData.timeseal,
	      ip_addr,port,NULL)){
      printf("Unable to start timeseal.\n");
      ExitOn(EXIT_HARDQUIT,"Unable to start timeseal binary.");
    }
  }else if(runData.timesealPid>0){
    close(from_fds[1]);
    close(to_fds[0]);
    //    printf("timeseal: closed %d %d\n",from_fds[1],to_fds[0]);
    //printf("timeseal: inuse %d %d\n",from_fds[0],to_fds[1]);
    runData.icsReadFd=from_fds[0];
    runData.icsWriteFd=to_fds[1];
  }else{
    ExitOn(EXIT_HARDQUIT,"Unable to fork."); 
  }
}

void UpdateTimeString(){
  time_t t = time(0);
  if(runData.timestring){
      free(runData.timestring);
  }
  runData.timestring=strdup(ctime(&t));
  runData.timestring=strtok(runData.timestring,"\r\n");
}

void MainLoop()
{
  fd_set readfds;
  int maxfd;
  struct timeval timeout;
  char cBuf[2*BUF_SIZE], sBuf[2*BUF_SIZE];
  char conBuf[2*BUF_SIZE];
  char proxyBuf[2*BUF_SIZE];
  int selectVal;
  int events;
  struct sockaddr_in client_addr;  
  unsigned int sin_size;
  //  InitRunData();
  BlockSignals();
  sBuf[0] = '\0';
  cBuf[0] = '\0';
  conBuf[0]='\0';
  proxyBuf[0]='\0';
  while (1) {
    UpdateTimeString();
    maxfd=-1;
    FD_ZERO(&readfds);
    FD_SET(runData.icsReadFd, &readfds);
    maxfd=runData.icsReadFd;
    if (appData.console && !runData.blockConsole){
      FD_SET(0, &readfds);
    }
    if (runData.computerActive) {
      FD_SET(runData.computerReadFd, &readfds);
      maxfd=MAX(maxfd,runData.computerReadFd);
    }
    if (runData.proxyListenFd!=-1){
	FD_SET(runData.proxyListenFd, &readfds);
	maxfd=MAX(maxfd,runData.proxyListenFd);
    }
    if (runData.proxyFd!=-1){
    	FD_SET(runData.proxyFd, &readfds);
    	maxfd=MAX(maxfd,runData.proxyFd);
    }


    timeout = sched_idle_time();
    time_add(timeout,1);  /* add 1 msec for safety */
    UnblockSignals();
    selectVal=select(maxfd+1,&readfds, NULL, NULL, &timeout);
    BlockSignals();  
    if (selectVal < 0 && errno == EINTR){
      logme(LOG_DEBUG,"Select call interrupted by alarm.");
      continue; 
    }
    if (selectVal < 0){
      logme(LOG_DEBUG,"Error in select: %s",strerror(errno));
      ExitOn(EXIT_HARDQUIT,"Unknown error in select.");
    }
    events=process_timer_list();
    if(selectVal==0){
      logme(LOG_DEBUG,"Select interrupted without data. A timer fired?");
      logme(LOG_DEBUG,"%d timer events processed",events);
    }else if(events>0){
      logme(LOG_DEBUG,"%d timer events processed",events);
    }
    if (appData.console && FD_ISSET(0, &readfds)){
#ifdef HAVE_LIBREADLINE
      rl_callback_read_char();
#else
      ProcessRawInput(0, conBuf, sizeof(conBuf), ProcessConsoleLine);
#endif
    }
    if (runData.computerActive && FD_ISSET(runData.computerReadFd, &readfds)){
      if (ProcessRawInput(runData.computerReadFd, cBuf, sizeof(cBuf), 
			  ProcessComputerLine) == ERROR){
	  //	ExitOn(EXIT_HARDQUIT,"Lost contact with computer.");
	  logme(LOG_DEBUG,"Lost contact with computer.");
	  logme(LOG_DEBUG,"Bailing out.");
	  SendToIcs("resign\n");
	  RawKillComputer();
	  StartComputer();
      }
    }
    if (runData.proxyFd!=-1 && FD_ISSET(runData.proxyFd, &readfds)){
      if (ProcessRawInput(runData.proxyFd, proxyBuf, sizeof(proxyBuf), 
			  ProcessProxyLine) == ERROR){
	  logme(LOG_DEBUG,"EOF on proxy.");
	  DisconnectProxy();
      }
    }
    if (FD_ISSET(runData.icsReadFd, &readfds)) {
      if (ProcessRawInput(runData.icsReadFd, sBuf, sizeof(sBuf), 
			  ProcessIcsLine) == ERROR){
	  ExitOn(EXIT_QUITRESTART,"Disconnected from ics.");
      }
    }
    if (runData.proxyListenFd!=-1 && FD_ISSET(runData.proxyListenFd, &readfds)) {
	sin_size=sizeof(struct sockaddr_in);
	if(runData.proxyFd!=-1){
	    logme(LOG_WARNING,"New proxy connection. Closing prior one.\n");
	    DisconnectProxy();
	}
	if((runData.proxyFd=accept(runData.proxyListenFd, 
			       (struct sockaddr *)&client_addr,
				   &sin_size))==-1){
	    logme(LOG_ERROR,"Unable to accept proxy connection.");
	}else{
	    SendToProxyLogin("%s",welcome);
	    if(appData.proxyLogin){
		SendToProxyLogin("%s","login: ");
		runData.proxyLoginState=PROXY_LOGIN_PROMPT;
		logme(LOG_INFO,"Proxy connected.");
	    }else{
		runData.proxyLoginState=PROXY_LOGGED_IN;
		if(runData.inGame){
		    SendToProxy("%s\r\n",runData.lineBoard);
		}
		SendMarker(PROXYPROMPT);
	    }
	}

    }
  }
}

void LoadHistory(){
#ifdef HAVE_LIBREADLINE
  char buf[BUF_SIZE];
  char * currentDir;
  char * home;
  if(!(currentDir=getcwd(buf,BUF_SIZE))){
    ExitOn(EXIT_HARDQUIT,"Buffer too small to hold working directory.");
  }
  logme(LOG_DEBUG,"Current directory = %s",currentDir);
  if((home=getenv("HOME"))){
    logme(LOG_DEBUG,"Home directory = %s",home);
    if(chdir(home)<0){
      logme(LOG_ERROR,"Could not chdir to home directory");
    }
  }
  read_history(".icsdrone_history");
  if(chdir(currentDir)<0){
    logme(LOG_ERROR,"Could not chdir to current directory");
  }
  runData.historyLoaded=TRUE;
#endif
}

void SaveHistory(){
#ifdef HAVE_LIBREADLINE
  char buf[BUF_SIZE];
  char * currentDir;
  char * home;
  if(!runData.historyLoaded){
    return;
  }
  if(!(currentDir=getcwd(buf,BUF_SIZE))){
    ExitOn(EXIT_HARDQUIT,"Buffer too small to hold working directory.");
  }
  if((home=getenv("HOME"))){
    if(chdir(home)<0){
      logme(LOG_ERROR,"Could not chdir to home directory");
    }
  }
  write_history(".icsdrone_history");
  history_truncate_file (".icsdrone_history", 200);
  if(chdir(currentDir)<0){
      logme(LOG_ERROR,"Could not chdir to current directory");
  }
#endif
}


void ExitOn(int exitValue, char *errmsg)
{
  SetColor(appData.colorAlert);
  if (!errmsg) {
#ifndef NO_STRERROR
    logme(LOG_ERROR, "%s", strerror(errno));
#else
    logme(LOG_ERROR, "Fatal error (errno %d)", errno);
#endif
  } else {
    logme(LOG_ERROR, "%s", errmsg);
  }
  if(runData.inExitOn){
    logme(LOG_DEBUG,"Recursive call to ExitOn.");
    return; 
  }
  runData.inExitOn=TRUE;
  BlockConsole();
  SaveHistory();
  book_close();
  if (runData.computerActive) {
    KillComputer();
  }
  CloseIcs();
  CloseProxy();
  StopLogging();
  printf("%s\n",errmsg);
  ResetColor();
  delete_timer(&runData.pingTimer);
  delete_timer(&runData.tellTimer);
  CancelTimers();
  if(exitValue==EXIT_QUITRESTART){
    if(persistentData.wasLoggedIn && !runData.quitPending){
      logme(LOG_DEBUG,"We have been logged in and are not quitting so we try to restart\n");
      exitValue=EXIT_RESTART;
    }else{
      logme(LOG_DEBUG,"No succesful login so far so we quit\n");
      exitValue=EXIT_HARDQUIT;
    }
  }
  longjmp(stackPointer,exitValue);
}  

void TerminateProc(int sig)
{
  EchoOn();
  ExitOn(EXIT_HARDQUIT,"Got SIGINT/SIGTERM/SIGHUP signal.");
}

void BrokenPipe(int sig)
{
  EchoOn();
  ExitOn(EXIT_HARDQUIT,"Got SIGPIPE signal.");
}  

void Usage()
{
  printf("%s %s\n(c) 1996-2001 by Henrik Oesterlund Gram. \n" 
"(c) 2008-2009 Michel Van den Bergh.\n"
"Consult the README or the manpage for instructions.\n", PACKAGE_NAME, VERSION);
  exit(0);
}

void SendPing(void *data){
  SendMarker(PING);
  runData.pingCounter++;
  logme(LOG_DEBUG,"ping counter is now: %d",runData.pingCounter);
  if(runData.pingCounter>=PINGWINDOW){
    ExitOn(EXIT_QUITRESTART,"Too many pings missed.");
  }
  create_timer(&(runData.pingTimer),PINGINTERVAL,SendPing,NULL);
}

void Daemonize(){
  int  pid,fd;
  logme(LOG_INFO,"Going into background...");
  appData.console=0;
  BlockConsole();
  if((pid=fork())>0){  /* parent */
    exit(0);
  }else if(pid<0){  /* error */
    ExitOn(EXIT_HARDQUIT,"Could not fork.\n");
  }else{
    if(setsid()<0){ /*child */
      ExitOn(EXIT_HARDQUIT,"setsid call failed.\n");
    }
  }
  /* taken from old setsid in cygwin */
  if ((fd = open ("/dev/null", O_RDWR, 0)) >= 0) {
    if (isatty (STDIN_FILENO))
      dup2 (fd, STDIN_FILENO);
    if (isatty (STDOUT_FILENO))
      dup2 (fd, STDOUT_FILENO);
    if (isatty (STDERR_FILENO))
      dup2 (fd, STDERR_FILENO);
    if (fd > 2)
      close (fd);
  }
}



int main(int argc, char *argv[]) 
{
    FILE *fp;
    char buf[256];
    static int exitValue=EXIT_RESTART;
    InitPersistentData();
#ifndef HAVE_LIBREADLINE
    printf("No readline support.\n");
#endif
#ifndef HAVE_LIBZ
    printf("No zlib support.\n");
#endif
        /* 
         *  Set default string options.
         */
    SetOption("icsHost",LOGIN,0,"%s","freechess.org");
    SetOption("program",LOGIN,0,"%s","gnuchess");
    SetOption("sendTimeout",LOGIN,0,"%s","resume");
    SetOption("feedbackCommand",LOGIN,0,"%s","whisper");
    if (ParseArgs(argc, argv) == ERROR)
        Usage();
    signal(SIGINT, TerminateProc);
    signal(SIGTERM, TerminateProc);
    signal(SIGHUP, TerminateProc);
    signal(SIGPIPE, BrokenPipe);
    
    while(exitValue==EXIT_RESTART){
        switch(setjmp(stackPointer)){
            case 0:
                exitValue=0;
                fp=NULL;
                InitRunData();
#ifdef HAVE_LIBREADLINE
                rl_catch_signals=0;
                setupterm((char *)0, 1, (int *)0);
#endif
                srandom(time(NULL));
                create_timer(&(runData.pingTimer),PINGINTERVAL,SendPing,NULL);
                    /* this code must be cleaned up */
                if(appData.loginScript){
                    fp = fopen(appData.loginScript, "r");
                }
                if(appData.handle){
                    strncpy(buf,appData.handle,sizeof(buf));
                }else if(getenv("FICSHANDLE")){
                    strncpy(buf,getenv("FICSHANDLE"),sizeof(buf));
                }else if(getenv("ICSHANDLE")){
                    strncpy(buf,getenv("ICSHANDLE"),sizeof(buf));
                }else {
                    if (!fp || !myfgets(buf, sizeof(buf), fp)) {
                        if(appData.console){
                            PromptInput(buf,sizeof(buf),"ICS handle: ",1);
                        }else{
                            ExitOn(EXIT_HARDQUIT,"You must specify the handle. "
                                   "See README file for help.");
                        }
                    } else {
                        buf[ strlen(buf)-1 ] = '\0';
                    }
                }
                strncpy(runData.handle,buf,sizeof(runData.handle));
                runData.handle[sizeof(runData.handle)-1]='\0';
		if(runData.handle[0]!='\0'){
		    SetOption("handle",LOGIN,0,"%s",runData.handle);
		}else{
		    SetOption("handle",LOGIN,0,"%s","guest");

		}
                if(appData.passwd){
                    strncpy(buf,appData.passwd,sizeof(buf));
                }else if(getenv("FICSPASSWD")){
                    strncpy(buf,getenv("FICSPASSWD"),sizeof(buf));
                }else if(getenv("ICSPASSWD")){
                    strncpy(buf,getenv("ICSPASSWD"),sizeof(buf));
                }else {
                    if (!fp || !myfgets(buf, sizeof(buf), fp)) {
                        if(appData.console){
                            PromptInput(buf,sizeof(buf),"ICS password: ",0);
                        }else{
                            ExitOn(EXIT_HARDQUIT,"You must specify the password. See README file for help.");
                        }
                    } else {
                        buf[ strlen(buf)-1 ] = '\0';
                    }
                }
                strncpy(runData.passwd,buf,sizeof(runData.passwd));
                runData.passwd[sizeof(runData.passwd)-1]='\0';
		if(runData.passwd[0]!='\0'){
		    SetOption("password",LOGIN,0,"%s",runData.passwd);
		}else{
		    SetOption("password",LOGIN,0,"%s","xxx");
		}
                if(runData.handle[0]=='\0'){
                    strcpy(runData.handle,"guest");
                }
                if(!persistentData.firsttime){
                    appData.logAppend=TRUE;
                }
                if(!appData.logFile){
                    SetOption("logFile",LOGIN,0,"%s.log",runData.handle);
                }
                StartLogging();

                if(!persistentData.firsttime){
                    UnblockSignals();
                    sleep(RECONNECTINTERVAL);
                    BlockSignals();
                }
                    /* for polyglot board support */
                if(appData.book){
                    book_open(appData.book);
                }
                if(appData.timeseal){
                    OpenTimeseal();
                }else if((runData.icsReadFd =
                          OpenTCP(appData.icsHost,appData.icsPort))>0){ 
                    runData.icsWriteFd=runData.icsReadFd;
                }else{
                    ExitOn(EXIT_QUITRESTART,"Failed to connect to server.\n");
                }
                    /* I don't know why this is necessary,
                     * as sending 2 '\n''s is at the end of the
                     * password is necessary. If we don't do it
                     * the first command
                     * 'set style 12' gets lost. Perhaps it is a timing issue?
                     */
                if(!runData.passwd || runData.passwd[0]=='\0'){
                    strcpy(runData.passwd,"xxx");
                }
                SendToIcs("%s\n%s\n\n", runData.handle, runData.passwd);
                if(appData.daemonize){
                    Daemonize();
                }
		//                StartComputer();
                SendToIcs("set style 12\n"
                          "set shout 0\n"
                          "set cshout 0\n"
                          "set chanof 1\n"
                          "set open 1\n"
                          "set highlight 0\n"
                          "set prompt fics%%\n"
                          "set bell 0\n" 
                          "set seek 0\n"
                          "set width 240\n"
                          "set pin 0\n"
                          "set gin 0\n"
                          "iset movecase 1\n"
                          "iset nohighlight 1\n"
			  "iset nowrap 1\n"
			  "iset fr 1\n"
			  "iset losers 1\n"
			  "iset suicide 1\n"
                          "iset lock 1\n"
                          "td set tourneyinfo on\n"
                          "td set autostart yes\n"
			  "finger\n"
			  );
		SendMarker(ENDLOGIN);
                if(appData.owner){
                    SendToIcs("+notify %s\n",appData.owner);
                }
		/* ICC does not allow setting the interface variable
		 * multiple times.
		 * The interfaces variable will be set in computer.c
		 * one we know the "myname" option.
		 *                SetInterface();  */
                SetTimeZone();
                SetScoreForWhite();
                    /*
                     * Send the rest of the lines in loginscript as
                     * commands to the server
                     */
                if (fp) {
                    while(myfgets(buf, sizeof(buf), fp)){
			/* On relogin we do not reexecute set commands
			 * the user might have changed the variables manually.
			 */
			ExecCommand(buf,0,persistentData.firsttime?FALSE:TRUE);
                    }
                    fclose(fp);
                }
                LoadHistory();
                snprintf(runData.prompt,30,"%s> ",runData.handle);
                SetFeedbackColor(appData.colorDefault);
                MainLoop();
                break;
            case EXIT_RESTART:
                exitValue=EXIT_RESTART;
                break;
            case EXIT_HARDQUIT:
                exitValue=EXIT_HARDQUIT;
                break;
            default:
                logme(LOG_DEBUG,"Unknown exit value of setjmp");
        }
        persistentData.firsttime=FALSE;
        StopLogging();
        if(!appData.autoReconnect){
            break;
        }
    }
    return exitValue;
}
