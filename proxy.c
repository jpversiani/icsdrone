#include "icsdrone.h"

int StartProxy(){
    struct sockaddr_in server_addr;
    int optval;
    logme(LOG_DEBUG,"Starting proxy");
    if((runData.proxyListenFd=socket(AF_INET,SOCK_STREAM,0))==-1){
	logme(LOG_ERROR,"Could not create proxy socket.");
	return FALSE;
    }
    optval=1;
    if(setsockopt(runData.proxyListenFd, 
	       SOL_SOCKET, 
	       SO_REUSEADDR, 
	       &optval,
		  sizeof(optval))==-1){
	logme(LOG_ERROR,"Could not set socket option REUSEADDR.");
    }
    if(setsockopt(runData.proxyListenFd, 
		  IPPROTO_TCP,    
		  TCP_NODELAY,
		  &optval,
		  sizeof(optval))==-1){
	logme(LOG_ERROR,"Could not set socket option TCP_NODELAY.");
    }
    memset(&server_addr,0,sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;         
    server_addr.sin_port = htons(appData.proxyPort);     
    //     server_addr.sin_addr.s_addr = INADDR_ANY
    if(appData.proxyHost){
	server_addr.sin_addr.s_addr = inet_addr(appData.proxyHost); 
    }else{
	server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
    }
    memset(&server_addr.sin_zero,0,sizeof(server_addr.sin_zero));
    if (bind(runData.proxyListenFd, 
	     (struct sockaddr *)&server_addr, 
	     sizeof(struct sockaddr))
	== -1) {
	logme(LOG_ERROR,"Unable to bind proxy socket.");
	return FALSE;
    }
    if (listen(runData.proxyListenFd, 2) == -1) {
	logme(LOG_ERROR,"Unable to bind proxy socket.");
	return FALSE;
    }
    return TRUE;
}

void CloseProxy(){
    logme(LOG_DEBUG,"Closing proxy.");
    runData.proxyLoginState=PROXY_LOGIN_INIT;
    if(runData.proxyListenFd!=-1){
	close(runData.proxyListenFd);
	runData.proxyListenFd=-1;
    }
    if(runData.proxyFd!=-1){
	close(runData.proxyFd);
	runData.proxyFd=-1;
    }
}

void DisconnectProxy(){
    logme(LOG_DEBUG,"Disconnecting proxy.");
    if(runData.proxyFd!=-1){
	close(runData.proxyFd);
	runData.proxyFd=-1;
    }
    runData.proxyLoginState=PROXY_LOGIN_INIT;
}

void SendToProxy(char *format, ... )
{
  char buf[16384] = "";
  va_list ap;
  
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  buf[sizeof(buf)-1]='\0';
  if(runData.proxyFd!=-1 && runData.proxyLoginState==PROXY_LOGGED_IN){
      // HACK
      void (*sighandler_org)(int);
      logcomm("icsdrone","proxy", buf);
      sighandler_org=signal(SIGPIPE,SIG_IGN);
      // The signal should be delivered right here.
      UnblockSignals();
      if(write(runData.proxyFd,buf,strlen(buf))==-1){
	  logme(LOG_DEBUG,"Unable to write to proxy.");
      };
      BlockSignals();
      signal(SIGPIPE,sighandler_org);
  }
  va_end(ap);
}

void SendToProxyLogin(char *format, ... )
{
  char buf[16384] = "";
  va_list ap;
  
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  buf[sizeof(buf)-1]='\0';
  if(runData.proxyFd!=-1){
      // HACK
      void (*sighandler_org)(int);
      logcomm("icsdrone","proxy", buf);
      sighandler_org=signal(SIGPIPE,SIG_IGN);
      // The signal should be delivered right here.
      UnblockSignals();
      if(write(runData.proxyFd,buf,strlen(buf))==-1){
	  logme(LOG_DEBUG,"Unable to write to proxy.");
      };
      BlockSignals();
      signal(SIGPIPE,sighandler_org);
  }
  va_end(ap);
}

void ProcessProxyLine(char * line, char * queue){
  char* strip_line;
  static char saved_handle[20];
  static char saved_password[20];
  logcomm("proxy","icsdrone", line);
  
  // login handling
  if(runData.proxyLoginState!=PROXY_LOGGED_IN){
      strip_line=strtok(line,"\r\n");
      if(strip_line){
	  line=strip_line;
      }
      switch(runData.proxyLoginState){
      case PROXY_LOGIN_PROMPT:
	  strncpy(saved_handle,line,18);
	  runData.proxyLoginState=PROXY_PASSWORD_PROMPT;
	  SendToProxyLogin("%s%c%c%c","\r\npassword: ",IAC,WILL,TELOPT_ECHO);
	  return;
      case PROXY_PASSWORD_PROMPT:
	  SendToProxyLogin("%c%c%c",IAC,WONT,TELOPT_ECHO);
	  strncpy(saved_password,line,18);
	  logme(LOG_DEBUG,"Comparing (%s,%s) to (%s,%s)",
		saved_handle,
		saved_password,
		appData.handle,
		appData.passwd);
	  if(!strcmp(saved_handle,appData.handle) && 
	     !strcmp(saved_password,appData.passwd)){
	      runData.proxyLoginState=PROXY_LOGGED_IN;
	      SendToProxy("\r\n");
	      if(runData.inGame){
		  SendToProxy("%s\r\n",runData.lineBoard);
	      }
	      SendMarker(PROXYPROMPT);
	      logme(LOG_INFO,"Proxy login succeeded.");
	  }else{
	      SendToProxyLogin("**** Login incorrect! ****\n");
	      SendToProxyLogin("login: ");
	      runData.proxyLoginState=PROXY_LOGIN_PROMPT;
	  }
	  return;
	  
      }
  }
  
  // Make sure "quit" does not kill icsdrone.
  if(!strncmp("quit",line,4)){
     SendToProxy("Bye!\r\n");
     DisconnectProxy();
     return;
  }  
  // Do not allow xboard to set things.
  if(!strncmp("$set",line,4) || !strncmp("$iset",line,5)){
      return;
  }
  // Resetting the style resends the board. Confusing icsdrone.
  if(!strncmp("$style",line,5)){
      return;
  }
  // Unfortunately the $ wizardry works only on FICS.
  if(line[0]=='$'){
      line++;
  }
  ExecCommand(line,PROXY);
  //SendMarker(PROXYPROMPT);
  return;
}
