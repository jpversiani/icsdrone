#include "icsdrone.h"

int StartProxy(){
    struct sockaddr_in server_addr;
    logme(LOG_DEBUG,"Starting proxy");
    if((runData.proxyListenFd=socket(AF_INET,SOCK_STREAM,0))==-1){
	logme(LOG_ERROR,"Could not create proxy socket.");
	return FALSE;
    }
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
    if (listen(runData.proxyListenFd, 1) == -1) {
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
