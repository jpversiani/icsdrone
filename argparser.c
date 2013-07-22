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


ArgList argList = { 
    {"-icsHost",              ArgString, &appData.icsHost           , 0 }, 
    {"-icsPort",              ArgInt,    &appData.icsPort           , 0 },
    {"-proxyPort",            ArgInt,    &appData.proxyPort         , 0 },
    {"-proxyHost",            ArgString, &appData.proxyHost         , 0 },
    {"-proxy",                ArgBool,   &appData.proxy             , 0 },
    {"-proxyLogin",           ArgBool,   &appData.proxyLogin        , 0 },
    {"-searchDepth",          ArgInt,    &appData.searchDepth       , 1 },
    {"-secPerMove",           ArgInt,    &appData.secPerMove        , 1 }, 
    {"-logLevel",             ArgInt,    &appData.logLevel          , 0 },
    {"-logFile",              ArgString, &appData.logFile           , 0 },
    {"-owner",                ArgString, &appData.owner             , 0 },
    {"-console",              ArgBool,   &appData.console           , 0 },
    {"-easyMode",             ArgBool,   &appData.easyMode          , 1 },
    {"-book",                 ArgString, &appData.book              , 0 },
    {"-program",              ArgString, &appData.program           , 0 },
    {"-sendGameEnd",          ArgString, &appData.sendGameEnd       , 1 },
    {"-sendTimeout",          ArgString, &appData.sendTimeout       , 1 },
    {"-limitRematches",       ArgInt,    &appData.limitRematches    , 1 },
    {"-haveCmdPing",          ArgBool,   &appData.haveCmdPing       , 0 },
    {"-loginScript",          ArgString, &appData.loginScript       , 0 },
    {"-issueRematch",         ArgBool,   &appData.issueRematch      , 1 },
    {"-sendGameStart",        ArgString, &appData.sendGameStart     , 1 },
    {"-acceptOnly",           ArgString, &appData.acceptOnly        , 1 },
    {"-timeseal",             ArgString, &appData.timeseal          , 0 },
    {"-daemonize",            ArgBool,   &appData.daemonize         , 0 },
    {"-handle",               ArgString, &appData.handle            , 0 },
    {"-password",             ArgString, &appData.passwd            , 0 },
    {"-logging",              ArgBool,   &appData.logging           , 0 },
    {"-logAppend",            ArgBool,   &appData.logAppend         , 0 },
    {"-feedback",             ArgBool,   &appData.feedback          , 1 },
    {"-feedbackOnlyFinal",    ArgBool,   &appData.feedbackOnlyFinal , 1 },
    {"-proxyFeedback",        ArgBool,   &appData.proxyFeedback     , 1 },
    {"-dontReuseEngine",      ArgBool,   &appData.dontReuseEngine   , 0 },
    {"-nice",                 ArgInt,    &appData.nice              , 0 },
    {"-pgnFile",              ArgString, &appData.pgnFile           , 0 },
    {"-pgnLogging",           ArgBool,   &appData.pgnLogging        , 1 },
    {"-resign",               ArgBool,   &appData.resign            , 1 },
    {"-scoreForWhite",        ArgBool,   &appData.scoreForWhite     , 1 },
    {"-shortLogFile",         ArgString, &appData.shortLogFile      , 0 },
    {"-shortLogging",         ArgBool,   &appData.shortLogging      , 1 },
    {"-autoJoin",             ArgBool,   &appData.autoJoin          , 1 },
    {"-sendLogin",            ArgString, &appData.sendLogin         , 1 },
    {"-noplay",               ArgString, &appData.noplay            , 1 },
    {"-colorAlert",           ArgInt,    &appData.colorAlert        , 1 },
    {"-colorTell",            ArgInt,    &appData.colorTell         , 1 },
    {"-colorDefault",         ArgInt,    &appData.colorDefault      , 1 },
    {"-hardLimit",            ArgInt,    &appData.hardLimit         , 1 },
    {"-acceptDraw",           ArgBool,   &appData.acceptDraw        , 1 },
    {"-autoReconnect",        ArgBool,   &appData.autoReconnect     , 1 },
    {"-ownerQuiet",           ArgBool,   &appData.ownerQuiet        , 1 },
    {"-feedbackCommand",      ArgString, &appData.feedbackCommand   , 1 },
    {"-engineQuiet",          ArgBool,   &appData.engineQuiet       , 1 },
    {"-variants",             ArgString, &appData.variants          , 0 },
    {"-engineKnowsSAN",       ArgBool,   &appData.engineKnowsSAN    , 0 },
    {"-tourneyFilter",        ArgString, &appData.tourneyFilter     , 1 },
    {"-matchFilter",          ArgString, &appData.matchFilter       , 1 },
    {NULL,                    ArgNull,   NULL                       , 0 } 
};

int ParseArgs(int argc, char* argv[])
{
    int i, j, flag;

    for (j = 1; (j < argc); j++) {
	flag = 0;
	for (i = 0; (argList[i].argType != ArgNull && !flag); i++) {
	    if (!strcmp(argv[j], argList[i].argName)) {
		flag = 1;
		switch(argList[i].argType) {
		case ArgString:
		    j++;
		    if (!argv[j] || argv[j][0] == '-') {
			fprintf(stderr, "Missing value for \"%s\"\n", argv[j-1]);
			return ERROR;
		    } else {
                SetOption(argList[i].argName+1,LOGIN,0,"%s",argv[j]);
		    }
		    break;
		case ArgInt:
		    j++;
		    if (!argv[j] || argv[j][0] == '-') {
			fprintf(stderr, "Missing value for \"%s\"\n", argv[j-1]);
			return ERROR;
		    } else {
			int a;
			for (a = 0; (argv[j][a] && isdigit(argv[j][a])); a++)
			    ;
			if (argv[j][a] != '\0') {
			    fprintf(stderr, "Bad value given for \"%s\"\n", argv[j-1]);
			    return ERROR;
			} else {
#if 0
			    fprintf(stderr, "%s %s\n", argv[j-1], argv[j]);
#endif
			    *((int *) argList[i].argValue) = atoi(argv[j]);
			}
		    }
		    break;
		case ArgBool:
		    j++;
		    if (!argv[j] || argv[j][0] == '-') {
			fprintf(stderr, "Missing value for \"%s\"\n", argv[j-1]);
			return ERROR;
		    } else {
			if (!strcasecmp(argv[j], "true") || 
			    !strcasecmp(argv[j], "on"))
			    *((int *) argList[i].argValue) = TRUE;
			else if (!strcasecmp(argv[j], "false") || 
				 !strcasecmp(argv[j], "off"))
			    *((int *) argList[i].argValue) = FALSE;
			else {
			    fprintf(stderr, "Bad value given for \"%s\"\n", argv[j-1]);
			    return ERROR;
			}
		    }
		    break;
		case ArgNull:
		default:
		    fprintf(stderr, "Internal error: Wrong argparser configuration!");
		    return ERROR;
		}
	    }
	}
	if (!flag) {
	    fprintf(stderr, "Unknown option \"%s\"\n", argv[j]);
	    return ERROR;
	}
    }
    return NOERROR;
}
