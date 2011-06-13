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
    setsockopt(runData.proxyListenFd, 
	       SOL_SOCKET, 
	       SO_REUSEADDR, 
	       &optval,
	       sizeof(optval));
    
    server_addr.sin_family = AF_INET;         
    server_addr.sin_port = htons(5000);     
    server_addr.sin_addr.s_addr = INADDR_ANY; 
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
    if(runData.proxyListenFd!=-1){
	close(runData.proxyListenFd);
    }
    if(runData.proxyFd!=-1){
	close(runData.proxyFd);
    }
}

void SendToProxy(char *format, ... )
{
  char buf[16384] = "";
  va_list ap;
  
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  buf[sizeof(buf)-1]='\0';
  if(runData.proxyFd!=-1){
      // HACK
      void (*sighandler_org)(int);
      logme(LOG_DEBUG, "icsdrone->proxy: %s", buf);
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

void ProcessProxyLine(char * line){
  logme(LOG_DEBUG, "proxy->icdrone: %s", line);
  SendToIcs("%s\n",line);
  return;
}
