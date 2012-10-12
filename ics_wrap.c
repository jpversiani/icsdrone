#include "icsdrone.h"

int log_wrap(value_t *result,value_t **value){
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }   
    logme(LOG_DEBUG,"%s",value[0]->string_value->data);
    value_init(result,V_NONE);
    return 0;
}

int send_to_ics_wrap(value_t *result,value_t **value){
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }   
    SendToIcs("%s",value[0]->string_value->data);
    value_init(result,V_NONE);
    return 0;
}

int set_option_wrap(value_t *result,value_t **value){
    int ret;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[2]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }   
    if(value[1]->type!=V_STRING && value[1]->type!=V_NONE){
	return E_INVALID_SIGNATURE;
    }   
    if(value[1]->type==V_NONE){
	ret=SetOption(value[0]->string_value->data,CLEAR,0,"");
    }else{
	ret=SetOption(value[0]->string_value->data,
		      0,
		      0,
		      "%s",
		      value[1]->string_value->data);
    }
    value_init(result,V_BOOLEAN,ret);
    return 0;
}

int get_option_wrap(value_t *result,value_t **value){
    int ret;
    char buffer[8192]; // buffer overflow alert!
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }   
    ret=GetOption(value[0]->string_value->data,buffer);
    if(ret){
	value_init(result,V_STRING,buffer);
    }else{
	value_init(result,V_NONE);
    }
    return 0;
}

void ics_wrap_init(){
    value_t value[1];
    eval_init();
    value_init(value,V_CFUNC,log_wrap);
    eval_set("log",value,SY_RO);
    value_init(value,V_CFUNC,send_to_ics_wrap);
    eval_set("send_to_ics",value,SY_RO);
    value_init(value,V_CFUNC,set_option_wrap);
    eval_set("set_option",value,SY_RO);
    value_init(value,V_CFUNC,get_option_wrap);
    eval_set("get_option",value,SY_RO);
}


