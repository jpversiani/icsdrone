#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "eval.h"
#include "math.h"


typedef struct {
    char *data;
    int length;
    int type;
} token_t;

typedef struct {
    char *name;
    char flags;
    value_t *value;
} symbol_t;

#define MAXSYMBOLS 1024
static symbol_t ** symbol_table;
static int eval_memory_=0;

// hidden symbol table flag

#define SY_HONOR_RO 2


const char * eval_errmsg[]={
    "No error.",                                      // 0
    "Invalid token.",                                 // 1
    "Tokens after expression.",                       // 2  
    "No matching right parenthesis.",                 // 3
    "Not implemented.",                               // 4
    "Unrecognized primary.",                          // 5
    "Binary operator is incompatible with operands.", // 6
    "Unary operator is incompatible with operand.",   // 7
    "Binary operation not implemented.",              // 8
    "Undefined symbol.",                              // 9
    "Selector in ternary is non boolean.",            //10
    "Ternary does not contain colon.",                //11
    "Symbol is read only.",                           //12
    "Invalid slice syntax.",                          //13
    "Invalid slice operation.",                       //14
    "Bad function call syntax.",                      //15
    "Symbol not callable.",                           //16
    "Invalid signature.",                             //17
    "String does not represent a number.",            //18
    "String is not one character.",                   //19
    "Unknown error.",                                 //20
    "Unknown type."                                   //21
};

#define MAXERROR 21


// tokenizer internal states

#define S_START           0
#define S_PARSINGNUMERIC  1
#define S_PARSINGVARIABLE 2
#define S_LT_             3
#define S_GT_             4
#define S_VERT_           5
#define S_AMP_            6
#define S_EQ_             7
#define S_PLUS_           8
#define S_MINUS_          9
#define S_QUOTE_         10
#define S_NEG_           11
#define S_QUOTE_QUOTE_   12
#define S_PARSINGINT     13
#define S_MULT_          14

// exit states (token types)

#define T_INVALID      99
#define T_EQ          100
#define T_LT          101
#define T_GT          102
#define T_LE          103
#define T_GE          104
#define T_OR          105
#define T_AND         106
#define T_LP          107
#define T_RP          108
#define T_VAR         109
#define T_NUM         110
#define T_INV         111
#define T_PLUS        112
#define T_PLUSPLUS    114
#define T_MINUS       115
#define T_MINUSMINUS  116
#define T_TIMES       117
#define T_DIV         118
#define T_NEG         119
#define T_STRING      120
#define T_LB          121
#define T_RB          122
#define T_LBR         123
#define T_RBR         124
#define T_COMMA       125
#define T_SC          126
#define T_COLON       127
#define T_QUEST       128
#define T_NEQ         129
#define T_ASSIGN      130
#define T_POWER       131
#define T_NONE        132

#define MAXSTATES 20
#define MAXINPUTS 128
#define MAXARGS   10

static int tok[MAXSTATES][MAXINPUTS];
static int pushback[MAXSTATES][MAXINPUTS];

int eval_debug=0;


// prototypes
static int transfer(value_t *value, value_t *value1);
static int eval_commaexp(char **line, value_t *value);
static int parse_commaexp(char **line);
static int eval_ternary(char ** line, value_t *value);
static int parse_ternary(char ** line);
static int eval_assign(char ** line, value_t *value);
static int parse_assign(char ** line);
static int incref(value_t *value);
static int decref(value_t *value);
static int string_len_wrap(value_t *result,value_t **value);
static int str_wrap(value_t *result,value_t **value);
static int int_wrap(value_t *result,value_t **value);
static int float_wrap(value_t *result,value_t **value);
static int bool_wrap(value_t *result,value_t **value);
static int eval_wrap(value_t *result,value_t **value);
static int memory_wrap(value_t *result,value_t **value);
static int abs_wrap(value_t *result,value_t **value);
static int ord_wrap(value_t *result,value_t **value);
static int chr_wrap(value_t *result,value_t **value);
static int lower_wrap(value_t *result, value_t **value);
static int upper_wrap(value_t *result, value_t **value);
static int find_wrap(value_t *result, value_t **value);
static int time_wrap(value_t *result, value_t **value);
static int symbol_table_set(char *name, value_t *value, char flags);
static string_t * string_create(char *data, int size);

static void fatal_error(char *s){
    fprintf(stderr,"%s\n",s);
    abort();
}

static void * eval_malloc(int size){
    void * p;
    p=malloc(size+8);
    *((int *)p)=size;
    eval_memory_+=(size+8);
    return p+8;
}

static char * eval_strdup(char *s){
    int l;
    char *ret;
    l=strlen(s);
    ret=eval_malloc(l+1);
    strcpy(ret,s);
    return ret;
}

static void eval_free(void *p){
    int size;
    size=*((int *)(p-8));
    eval_memory_-=(size+8);
    free(p-8);
}

static void init_tokenizer(){
    int state;
    int input;


    // first everything invalid, no pushback
    for(state=0;state<MAXSTATES;state++){
	for(input=0;input<MAXINPUTS;input++){
	    tok[state][input]=T_INVALID;
	    pushback[state][input]=0;
	}
    }

    // space is a noop

    tok[S_START][' ']=S_START;

    // if we encounter the end of the string then there is no more token

    tok[S_START]['\0']=T_NONE;
    pushback[S_START]['\0']=1;

    // 1 char tokens
    tok[S_START]['(']=T_LP;
    tok[S_START][')']=T_RP;
    tok[S_START]['/']=T_DIV;
    tok[S_START]['-']=T_MINUS;
    tok[S_START]['{']=T_LB;
    tok[S_START]['}']=T_RB;
    tok[S_START]['[']=T_LBR;
    tok[S_START][']']=T_RBR;
    tok[S_START][',']=T_COMMA;    
    tok[S_START][';']=T_SC;
    tok[S_START][':']=T_COLON;
    tok[S_START]['?']=T_QUEST;

    // strings

    for(input=0;input<MAXINPUTS;input++){
	if(input=='\"'){
	    tok[S_START][input]=S_QUOTE_;
	    tok[S_QUOTE_][input]=T_STRING;
	    tok[S_QUOTE_QUOTE_][input]=S_QUOTE_;
	}else if(input=='\\'){
	    tok[S_QUOTE_][input]=S_QUOTE_QUOTE_;
	    tok[S_QUOTE_QUOTE_][input]=S_QUOTE_;
	}else if(input=='\0'){
	    tok[S_QUOTE_][input]=T_INVALID;
	    tok[S_QUOTE_QUOTE_][input]=T_INVALID;
	    pushback[S_QUOTE_][input]=1;
	}else{
	    tok[S_QUOTE_][input]=S_QUOTE_;
	    tok[S_QUOTE_QUOTE_][input]=S_QUOTE_;
	}
    }

    // numeric

    for(input=0;input<MAXINPUTS;input++){
	if(isdigit(input)){
	    tok[S_START][input]=S_PARSINGINT;
	    tok[S_PARSINGINT][input]=S_PARSINGINT;
	    tok[S_PARSINGNUMERIC][input]=S_PARSINGNUMERIC;
	}else if(input=='.'){
	    tok[S_PARSINGINT][input]=S_PARSINGNUMERIC;
	}else{
	    tok[S_PARSINGINT][input]=T_NUM;
	    tok[S_PARSINGNUMERIC][input]=T_NUM;
	    pushback[S_PARSINGNUMERIC][input]=1;
	    pushback[S_PARSINGINT][input]=1;
	}
    }

    // variable

    for(input=0;input<MAXINPUTS;input++){
	if(isalpha(input) || input=='_'){
	    tok[S_START][input]=S_PARSINGVARIABLE;
	    tok[S_PARSINGVARIABLE][input]=S_PARSINGVARIABLE;
	}else if(isdigit(input) || input=='.'){
	    tok[S_PARSINGVARIABLE][input]=S_PARSINGVARIABLE;
	}else{
	    tok[S_PARSINGVARIABLE][input]=T_VAR;
	    pushback[S_PARSINGVARIABLE][input]=1;
	}
    }

    // < vs. <=

    tok[S_START]['<']=S_LT_;
    for(input=0;input<MAXINPUTS;input++){
	if(input=='='){
	    tok[S_LT_][input]=T_LE;
	}else{
	    tok[S_LT_][input]=T_LT;
	    pushback[S_LT_][input]=1;
	}
    }

    // > vs. >=

    tok[S_START]['>']=S_GT_;
    for(input=0;input<MAXINPUTS;input++){
	if(input=='='){
	    tok[S_GT_][input]=T_GE;
	}else{
	    tok[S_GT_][input]=T_GT;
	    pushback[S_GT_][input]=1;
	}
    }

    // + vs. ++

    tok[S_START]['+']=S_PLUS_;
    for(input=0;input<MAXINPUTS;input++){
	if(input=='+'){
	    tok[S_PLUS_][input]=T_PLUSPLUS;
	}else{
	    tok[S_PLUS_][input]=T_PLUS;
	    pushback[S_PLUS_][input]=1;
	}
    }

    // - vs. --

    tok[S_START]['-']=S_MINUS_;
    for(input=0;input<MAXINPUTS;input++){
	if(input=='-'){
	    tok[S_MINUS_][input]=T_MINUSMINUS;
	}else{
	    tok[S_MINUS_][input]=T_MINUS;
	    pushback[S_MINUS_][input]=1;
	}
    }


    // = vs ==

    tok[S_START]['=']=S_EQ_;
    for(input=0;input<MAXINPUTS;input++){
	if(input=='='){
	    tok[S_EQ_][input]=T_EQ;
	}else{
	    tok[S_EQ_][input]=T_ASSIGN;
	    pushback[S_EQ_][input]=1;
	}
    }

    // ! vs !=

    tok[S_START]['!']=S_NEG_;
    for(input=0;input<MAXINPUTS;input++){
	if(input=='='){
	    tok[S_NEG_][input]=T_NEQ;
	}else{
	    tok[S_NEG_][input]=T_NEG;
	    pushback[S_NEG_][input]=1;
	}
    }

    tok[S_START]['*']=T_TIMES;

    // * vs **

    tok[S_START]['*']=S_MULT_;
    for(input=0;input<MAXINPUTS;input++){
	if(input=='*'){
	    tok[S_MULT_][input]=T_POWER;
	}else{
	    tok[S_MULT_][input]=T_TIMES;
	    pushback[S_MULT_][input]=1;
	}
    }



    // ||

    tok[S_START]['|']=S_VERT_;
    tok[S_VERT_]['|']=T_OR;

    // &&

    tok[S_START]['&']=S_AMP_;
    tok[S_AMP_]['&']=T_AND;

}

static void string_decref(string_t *string);

static symbol_t * symbol_create(char *name){
    symbol_t *ret;
    ret=eval_malloc(sizeof(symbol_t));
    ret->name=eval_strdup(name);
    ret->flags=0;
    ret->value=eval_malloc(sizeof(value_t));
    return ret;
}

static void symbol_table_init(){
    value_t value[1];
    symbol_table=eval_malloc((MAXSYMBOLS+1)*sizeof(symbol_t *));
    symbol_table[0]=NULL;
    value->type=V_BOOLEAN;
    value->value=1;
    symbol_table_set("true",value,SY_RO);
    value->value=0;
    symbol_table_set("false",value,SY_RO);
    value->type=V_NONE;
    symbol_table_set("none",value,SY_RO);
    value->type=V_UNDEFINED;
    symbol_table_set("undefined",value,SY_RO);
    value->type=V_CFUNC;
    value->cfunc_value=string_len_wrap;
    symbol_table_set("len",value,SY_RO);
    value->cfunc_value=str_wrap;
    symbol_table_set("str",value,SY_RO);
    value->cfunc_value=int_wrap;
    symbol_table_set("int",value,SY_RO);
    value->cfunc_value=float_wrap;
    symbol_table_set("float",value,SY_RO);
    value->cfunc_value=bool_wrap;
    symbol_table_set("bool",value,SY_RO);
    value->cfunc_value=eval_wrap;
    symbol_table_set("eval",value,SY_RO);
    value->cfunc_value=memory_wrap;
    symbol_table_set("memory",value,SY_RO);
    value->cfunc_value=abs_wrap;
    symbol_table_set("abs",value,SY_RO);
    value->cfunc_value=ord_wrap;
    symbol_table_set("ord",value,SY_RO);
    value->cfunc_value=chr_wrap;
    symbol_table_set("chr",value,SY_RO);
    value->cfunc_value=lower_wrap;
    symbol_table_set("lower",value,SY_RO);
    value->cfunc_value=upper_wrap;
    symbol_table_set("upper",value,SY_RO);
    value->cfunc_value=find_wrap;
    symbol_table_set("find",value,SY_RO);
    value->cfunc_value=time_wrap;
    symbol_table_set("time",value,SY_RO);
}

static void symbol_table_clear(){
    int i;
    symbol_t *entry;
    for(i=0;i<MAXSYMBOLS;i++){
	entry=symbol_table[i];
	if(entry==NULL){
	    break;
	}
	decref(entry->value);
	eval_free(entry->name);
	eval_free(entry->value);
	eval_free(entry);
    }
    eval_free(symbol_table);
}

// external interface

void (*eval_clear)()=symbol_table_clear;

static int symbol_table_set(char *name, value_t *value, char flags){
    int i;
    symbol_t *entry;
    if(eval_debug){
	if(value->type==V_STRING){
	    printf("symbol_table_set(): name=%s value=[%s]\n",name,value->string_value->data);
	}else{
	    printf("symbol_table_set(): name=%s value=%f\n",name,value->value);
	}
    }
    for(i=0;i<MAXSYMBOLS;i++){
	entry=symbol_table[i];
	if(entry==NULL){
	    entry=symbol_create(name);
	    entry->flags=~SY_HONOR_RO & flags;
	    transfer(entry->value,value);
	    symbol_table[i]=entry;
	    symbol_table[i+1]=NULL;
	    return 0;
	}else if(!strcmp(entry->name,name)){
	    decref(entry->value);
	    if((entry->flags & SY_RO) && (flags & SY_HONOR_RO)){
		return E_SYMBOL_IS_READ_ONLY;
	    }else{
		entry->flags=flags;
		transfer(entry->value,value);
	    }
	    return 0;
	}
    }    
    fatal_error("Symbol table overflow.");
    return 0;
}

int eval_memory(){
    return eval_memory_;
}

// external interface

int eval_set(char *name, int type, char flags, ...){
    value_t value[1];
    int num;
    char *str;
    cfunc cf;
    va_list a_list;
    int ret;
    va_start(a_list,flags);
    switch(type){
    case V_NUMERIC:
	num=va_arg(a_list,int);
	ret=value_init(value,type,num);
	break;
    case V_BOOLEAN:
	num=(va_arg(a_list,int)!=0);
	ret=value_init(value,type,num);
	break;
    case V_NONE:
	ret=value_init(value,type);
	break;
    case V_UNDEFINED:
	ret=value_init(value,type);
	break;
    case V_ERROR:
	num=va_arg(a_list,int);
	ret=value_init(value,type,num);
	break;
    case V_STRING:
	str=va_arg(a_list,char *);
	ret=value_init(value,type,str);
	break;
    case V_CFUNC:
	cf=va_arg(a_list,cfunc);
	ret=value_init(value,type,cf);
	break;
    default:
	ret=E_UNKNOWN_TYPE;
    }
    va_end(a_list);
    if(ret){
	return ret;
    }
    ret=symbol_table_set(name, value, flags);
    if(ret){
	decref(value);
    }
    return ret;

}

int symbol_table_get(char *name, value_t *value){
    int i;
    symbol_t *entry;
    if(eval_debug){
	printf("symbol_table_get(): name=[%s]\n",name);
    }
    for(i=0;i<MAXSYMBOLS;i++){
	entry=symbol_table[i];
	if(entry==NULL){
	    value->type=V_UNDEFINED;
	    value->value=0;
	    return 0;
	}
	if(!strcmp(name,entry->name)){
	    transfer(value,entry->value);
	    return 0;
	}
    }
    return 0;
}


static string_t * string_create(char *data, int size){
    string_t * ret;
    int i;
    if(size<0){
	size=strlen(data);
    }
    ret=eval_malloc(sizeof(string_t));
    ret->refcount=1;
    ret->data=eval_malloc(size+1);
    for(i=0;i<size;i++){
	ret->data[i]=data[i];
	if(data[i]==0){
	    break;
	}
    }
    ret->data[i]='\0';
    return ret;
}

static void string_dequote(string_t *string){
    char *in;
    char *out;
    char c;
    int quote=0;
    in=string->data;
    out=string->data;
    while((c=*(in++))){
	switch(c){
	case '\\':
	    if(quote){
		quote=0;
		*(out++)='\\';
	    }else{
		quote=1;
	    }
	    break;
	case 'n':
	    if(quote){
		quote=0;
		*(out++)='\n';
	    }else{
		*(out++)=c;
	    }
	    break;
	case 'r':
	    if(quote){
		quote=0;
		*(out++)='\r';
	    }else{
		*(out++)=c;
	    }
	    break;	    
	case 't':
	    if(quote){
		quote=0;
		*(out++)='\t';
	    }else{
		*(out++)=c;
	    }
	    break;
	case '"':
	    if(quote){
		quote=0;
		*(out++)='"';
	    }else{
		// this should be impossible
		*(out++)=c;
	    }
	    break;
	default:
	    *(out++)=c;
	}
    }
    *out='\0';
}

static void string_decref(string_t *string){
    if(string->refcount<=0){
	fatal_error("Internal error. String should not exist.");
    }
    string->refcount--;
    if(string->refcount==0){
	eval_free(string->data);
	eval_free(string);
    }
}

static void string_incref(string_t *string){
    string->refcount++;
}

static string_t * string_add(string_t *string1, string_t *string2){
    int i;
    int l1,l2;
    string_t *ret;
    l1=strlen(string1->data);
    l2=strlen(string2->data);
    ret=string_create("",l1+l2);
    for(i=0;i<l1;i++){
	ret->data[i]=string1->data[i];
    }
    for(i=0;i<l2;i++){
	ret->data[i+l1]=string2->data[i];
    }
    ret->data[l1+l2]='\0';
    return ret;
}

static string_t * string_slice(string_t *string, int first, int second){
    string_t *ret;
    int i;
    if(second<first){
	second=first;
    }
    ret=eval_malloc(sizeof(string_t));
    ret->refcount=1;
    ret->data=eval_malloc(second-first+1);
    for(i=0;i<second-first;i++){
	ret->data[i]=string->data[i+first];
	if(ret->data[i]==0){
	    break;
	}
    }
    ret->data[i]=0;
    return ret;
}

static int decref(value_t *value){
    if(value->type==V_STRING){
	string_decref(value->string_value);
    }
    return 0;
}

// public interface

int (*value_clear)(value_t *value)=decref;

static int incref(value_t *value){
    if(value->type==V_STRING){
	string_incref(value->string_value);
    }
    return 0;
}

static int transfer(value_t *value, value_t *value1){
    value->type=value1->type;
    if(value1->type==V_STRING){
	value->string_value=value1->string_value;
    }else if(value1->type==V_CFUNC){
	value->cfunc_value=value1->cfunc_value;
    }else{
	value->value=value1->value;
    }
    return 0;
}

static int unop(value_t *value, value_t *value1, int op){
    switch(op){
    case T_PLUS:
	if(value1->type!=V_NUMERIC){
	    return E_UNARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERAND;
	}
	value->type=V_NUMERIC;
	value->value=value1->value;
	break;
    case T_MINUS:
	if(value1->type!=V_NUMERIC){
	    return E_UNARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERAND;
	}
	value->type=V_NUMERIC;
	value->value=-value1->value;
	break;
    case T_NEG:
	if(value1->type!=V_BOOLEAN){
	    return E_UNARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERAND;
	}
	value->type=V_BOOLEAN;
	value->value=!value1->value;
	break;
    default:
	fatal_error("Non existant unary operation.");
	break;
    }
    return 0;
}

static int binop(value_t *value, value_t *value1, value_t *value2, int op){
    // note: value may be equal to value1 or value2;
    // first deal with errors and set result type
    int c;
    int i;
    char *m;
    int result_type=V_NUMERIC; // make old version of gcc happy
    string_t *ret;
    switch(op){
    case T_LT:
    case T_GT:
    case T_LE:
    case T_GE:
	if(value1->type!=value2->type){
	    return E_BINARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERANDS;
	}
	// fall through
    case T_EQ:
    case T_NEQ:
	result_type=V_BOOLEAN;
	break;
    case T_PLUS:
	if((value1->type==V_STRING) && (value2->type==V_STRING)){
	    result_type=V_STRING;
	    break;
	}
	// fall through
    case T_TIMES:
	if((value1->type==V_NUMERIC) && (value2->type==V_STRING)){
	    result_type=V_STRING;
	    break;
	}
	if((value1->type==V_STRING) && (value2->type==V_NUMERIC)){
	    result_type=V_STRING;
	    break;
	}
	// fall through
    case T_DIV:
    case T_MINUS:
    case T_POWER:
	if((value1->type!=V_NUMERIC) ||(value2->type!=V_NUMERIC)){
	    return E_BINARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERANDS;
	}
	result_type=V_NUMERIC;
	break;
	
    case T_OR:
    case T_AND:
	if((value1->type!=V_BOOLEAN) ||(value2->type!=V_BOOLEAN)){
	    return E_BINARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERANDS;
	}
	result_type=V_BOOLEAN;
	break;	
    default:
	fatal_error("Non existant binary operation");
	break;
    }

    // now compute

    switch(op){
    case T_LT:
	switch(value1->type){
	case V_NUMERIC:
	case V_BOOLEAN:
	case V_NONE:
	case V_UNDEFINED:
	    value->value=(value1->value<value2->value);
	    value->type=result_type;
	    break;
	case V_STRING:
	    value->value=(strcmp(value1->string_value->data,value2->string_value->data)<0);
	    decref(value1);
	    decref(value2);
	    value->type=result_type;
	    break;
	default:
	    return E_BINARY_OPERATION_NOT_IMPLEMENTED;
	}
	break;
    case T_GT:
	switch(value1->type){
	case V_NUMERIC:
	case V_BOOLEAN:
	case V_NONE:
	case V_UNDEFINED:
	    value->value=(value1->value>value2->value);
	    value->type=result_type;
	    break;
	case V_STRING:
	    value->value=(strcmp(value1->string_value->data,value2->string_value->data)>0);
	    decref(value1);
	    decref(value2);
	    value->type=result_type;
	    break;
	default:
	    return E_BINARY_OPERATION_NOT_IMPLEMENTED;
	}
	break;
    case T_LE:
	switch(value1->type){
	case V_NUMERIC:
	case V_BOOLEAN:
	case V_NONE:
	case V_UNDEFINED:
	    value->value=(value1->value<=value2->value);
	    value->type=result_type;
	    break;
	case V_STRING:
	    value->value=(strcmp(value1->string_value->data,value2->string_value->data)<=0);
	    decref(value1);
	    decref(value2);
	    value->type=result_type;
	    break;
	default:
	    return E_BINARY_OPERATION_NOT_IMPLEMENTED;
	}
	break;
    case T_GE:
	switch(value1->type){
	case V_NUMERIC:
	case V_BOOLEAN:
	case V_NONE:
	case V_UNDEFINED:
	    value->value=(value1->value>=value2->value);
	    value->type=result_type;
	    break;
	case V_STRING:
	    value->value=(strcmp(value1->string_value->data,value2->string_value->data)>=0);
	    decref(value1);
	    decref(value2);
	    value->type=result_type;
	    break;
	default:
	    return E_BINARY_OPERATION_NOT_IMPLEMENTED;
	}
	break;
    case T_EQ:
	switch(value1->type){
	case V_NUMERIC:
	case V_BOOLEAN:
	case V_NONE:
	case V_UNDEFINED:
	case V_ERROR:
	    value->value=(value1->type==value2->type) &&
		(value1->value==value2->value);
	    value->type=result_type;
	    break;
	case V_STRING:
	    value->value=(strcmp(value1->string_value->data,value2->string_value->data)==0) && 	(value1->type==value2->type);
	    decref(value1);
	    decref(value2);
	    value->type=result_type;
	    break;
	default:
	    return E_BINARY_OPERATION_NOT_IMPLEMENTED;
	}
	break;
    case T_NEQ:
	switch(value1->type){
	case V_NUMERIC:
	case V_BOOLEAN:
	case V_NONE:
	case V_UNDEFINED:
	case V_ERROR:
	    value->value=(value1->type!=value2->type) ||
		(value1->value!=value2->value);
	    value->type=result_type;
	    break;
	case V_STRING:
	    value->value=(strcmp(value1->string_value->data,value2->string_value->data)!=0) || (value1->type!=value2->type);
	    decref(value1);
	    decref(value2);
	    value->type=result_type;
	    break;
	default:
	    return E_BINARY_OPERATION_NOT_IMPLEMENTED;
	}
	break;
    case T_TIMES:
	if(result_type==V_NUMERIC){
	    value->value=value1->value*value2->value;
	    value->type=result_type;
	    break;
	}
	if(result_type==V_STRING){
	    if(value1->type==V_NUMERIC){
		c=(int)value1->value;
		m=value2->string_value->data;
	    }else{
		c=(int)value2->value;
		m=value1->string_value->data;
	    }
	    ret=string_create("",c*strlen(m));
	    for(i=0;i<c;i++){
		strcat(ret->data,m);
	    }
	    decref(value1);
	    decref(value2);
	    value->string_value=ret;
	    value->type=result_type;
	}
	break;
    case T_DIV:
	value->value=value1->value/value2->value;
	value->type=result_type;
	break;
    case T_PLUS:
	if(result_type==V_STRING){
	    string_t *ret;
	    ret=string_add(value1->string_value,value2->string_value);
	    decref(value1);
	    decref(value2);
	    value->string_value=ret;
	}else{
	    value->value=value1->value+value2->value;
	}
	value->type=result_type;
	break;
    case T_MINUS:
	value->value=value1->value-value2->value;
	value->type=result_type;
	break;
    case T_POWER:
	value->value=pow(value1->value,value2->value);
	value->type=result_type;
	break;
    case T_AND:
	value->value=value1->value&&value2->value;
	value->type=result_type;
	break;
    case T_OR:
	value->value=value1->value||value2->value;
	value->type=result_type;
	break;
    default:
	fatal_error("Non existant binary operation.");
	break;
    }
    return 0;
}

static int peektoken(char **line, token_t *token){
    int state;
    int input;
    char *line1;
    if(eval_debug){
	printf("peektoken(): line=[%s]\n",*line);
    }
    token->length=0;
    state=S_START;
    while(isspace(**line)){
    	(*line)++;
    }
    token->data=*line;
    line1=*line;
    while(state<99){
	input=*(line1++);
	if(!pushback[state][input]){
	    token->length++;
	}
	state=tok[state][input];
    }

    token->type=state;
    
    if(state==T_INVALID){
	return E_INVALID_TOKEN;
    }



    return 0;

}

static int skiptoken(char **line, token_t *token){
    if(eval_debug){
	printf("skiptoken(): line=[%s]\n",*line);
    }
    (*line)+=token->length;
    return 0;
}

static int gettoken(char **line, token_t *token){
    int ret;
    if(eval_debug){
	printf("gettoken(): line=[%s]\n",*line);
    }
    ret=peektoken(line,token);
    skiptoken(line,token);
    return ret;
}

static int eval_function(char **line, value_t *value){
    token_t token[1];
    value_t value1[1];
    struct value_ * values[MAXARGS+1];
    int ret, i;
    char *name;
    int args;
    if(eval_debug){
	printf("eval_function(): line=[%s]\n",*line);
    }
    args=0;
    values[0]=NULL;
    if((ret=gettoken(line,token))){
	return ret;
    }
    if(token->type!=T_VAR){
	return E_BAD_FUNCTION_CALL_SYNTAX;
    }
    name=eval_malloc(token->length+1);
    for(i=0;i<token->length;i++){
	name[i]=token->data[i];
    }
    name[i]='\0';
    if((ret=symbol_table_get(name,value1))){
	eval_free(name);
	return ret;
    }
    eval_free(name);
    if(value1->type!=V_CFUNC){
	return E_SYMBOL_NOT_CALLABLE;
    }
    gettoken(line,token);
    if(token->type!=T_LP){
	decref(value1);
	return E_BAD_FUNCTION_CALL_SYNTAX;
    }
    while(1){
	if(args>=MAXARGS){
	    ret=E_BAD_FUNCTION_CALL_SYNTAX;
	    goto bailout;
	}
	peektoken(line,token);
	if(token->type!=T_RP){
	    values[args]=eval_malloc(sizeof(value_t));
	    values[args+1]=NULL;
	    ret=eval_assign(line,values[args]);
	    if(ret){
		eval_free(values[args]);
		goto bailout;
	    }
	    args++;
	}	    
	gettoken(line,token);
	switch(token->type){
	case T_COMMA:
	    peektoken(line,token);
	    if(token->type==T_RP){
		ret=E_BAD_FUNCTION_CALL_SYNTAX;
		goto bailout;
	    }
	    continue;
	case T_RP:
	    ret=(value1->cfunc_value)(value,values);
	    goto bailout;
	default:
	    ret=E_BAD_FUNCTION_CALL_SYNTAX;
	    goto bailout;
	}
    }
    return 0;
 bailout:
    decref(value1);
    for(i=0;i<args;i++){
	decref(values[i]);
	eval_free(values[i]);
    }
    return ret;

}
static int parse_function(char **line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_function(): line=[%s]\n",*line);
    }
    if((ret=gettoken(line,token))){
	return ret;
    }
    if(token->type!=T_VAR){
	return E_BAD_FUNCTION_CALL_SYNTAX;
    }
    gettoken(line,token);
    if(token->type!=T_LP){
	return E_BAD_FUNCTION_CALL_SYNTAX;
    }
    while(1){
	peektoken(line,token);
	if(token->type!=T_RP){
	    if((ret=parse_assign(line))){
		goto bailout;
	    }
	}
	gettoken(line,token);
	switch(token->type){
	case T_COMMA:
	    peektoken(line,token);
	    if(token->type==T_RP){
		ret=E_BAD_FUNCTION_CALL_SYNTAX;
		goto bailout;
	    }
	    continue;
	case T_RP:
	    goto bailout;
	default:
	    ret=E_BAD_FUNCTION_CALL_SYNTAX;
	    goto bailout;
	}
    }
    return 0;
 bailout:
    return ret;

}

static int eval_primary(char **line, value_t *value){
    token_t token[1];
    token_t token1[1];
    value_t value1[1];
    value_t value2[1];
    value_t value3[1];
    string_t *string;
    char *line1;
    int ret;
    int i;
    char *name;
    int nocolon=0;
    int l1;
    if(eval_debug){
	printf("eval_primary(): line=[%s]\n",*line);
    }
    line1=*line;
    if((ret=gettoken(line,token))){
	return ret;
    }
    switch(token->type){
    case T_LP:
	if((ret=eval_commaexp(line,value1))){
	    return ret;
	}
	if((ret=gettoken(line,token))){
	    decref(value1);
	    return ret;
	}
	if(token->type!=T_RP){
	    decref(value1);
	    return E_NO_MATCHING_RIGHT_PARENTHESIS;
	}
	break;
    case T_NEG:
    case T_PLUS:
    case T_MINUS:
	if((ret=eval_primary(line,value1))){
	    return ret;
	}
	if((ret=unop(value,value1,token->type))){
	    decref(value1);
	    return ret;
	}
	return 0;
    case T_NUM:
	string=string_create(token->data,token->length);
	value->type=V_NUMERIC;
	value->value=atof(string->data);
	string_decref(string);
	return 0;
    case T_STRING:
	value1->string_value=string_create(token->data+1,token->length-2);
	string_dequote(value1->string_value);
	value1->type=V_STRING;
	break;
    case T_VAR:
	if((ret=peektoken(line,token1))){
	    return ret;
	}
	if(token1->type==T_LP){
	    if((ret=eval_function(&line1,value1))){
		*line=line1;
		return ret;
	    }
	    *line=line1;
	}else{
	    name=eval_malloc(token->length+1);
	    for(i=0;i<token->length;i++){
		name[i]=token->data[i];
	    }
	    name[i]='\0';
	    if((ret=symbol_table_get(name,value1))){
		eval_free(name);
		return ret;
	    }
	    eval_free(name);
	    incref(value1);
	}
	break;
    default:
	return E_UNRECOGNIZED_PRIMARY;
	break;
    }
    if((ret=peektoken(line,token))){
	decref(value1);
	return ret;
    }
    if(token->type==T_LBR){
	skiptoken(line,token);
	if(value1->type!=V_STRING){
	    decref(value1);
	    return E_INVALID_SLICE_OPERATION;
	}
	l1=strlen(value1->string_value->data);
	if((ret=peektoken(line,token))){
	    decref(value1);
	    return ret;
	}
	if(token->type==T_COLON){
	    skiptoken(line,token);
	    value2->type=V_NUMERIC;
	    value2->value=0;
	}else{
	    if((ret=eval_commaexp(line,value2))){
		decref(value1);
		return ret;
	    }
	    if(value2->type!=V_NUMERIC){
		decref(value1);
		decref(value2);
		return E_INVALID_SLICE_OPERATION;
	    }
	    if(value2->value<0){
		value2->value+=l1;
	    }
	    if((ret=peektoken(line,token))){
		decref(value1);
		decref(value2);
		return ret;
	    }
	    if(token->type!=T_COLON){
		if(token->type==T_RBR){
		    nocolon=1;
		}else{
		    decref(value1);
		    decref(value2);
		    return E_INVALID_SLICE_SYNTAX;
		}
	    }else{
		skiptoken(line,token);
	    }
	}
	if((ret=peektoken(line,token))){
	    decref(value1);
	    decref(value2);
	    return ret;
	}
	if(token->type==T_RBR){
	    skiptoken(line,token);
	    if(nocolon){
		value3->type=V_NUMERIC;
		value3->value=value2->value+1;
	    }else{
		value3->type=V_NUMERIC;
		value3->value=l1+1;
	    }
	}else{
	    if((ret=eval_commaexp(line,value3))){
		decref(value1);
		decref(value2);
		return ret;
	    }
	    if(value3->type!=V_NUMERIC){
		decref(value1);
		decref(value2);
		decref(value3);
		return E_INVALID_SLICE_OPERATION;
	    }
	    if(value3->value<0){
		value3->value+=l1;
	    }
	    if((ret=gettoken(line,token))){
		decref(value1);
		decref(value2);
		decref(value3);
		return ret;
	    }
	    if(token->type!=T_RBR){
		decref(value1);
		decref(value2);
		decref(value3);
		return E_INVALID_SLICE_SYNTAX;
	    }
	}
	value->type=V_STRING;
	value->string_value=string_slice(value1->string_value,value2->value,value3->value);
	decref(value1);
	decref(value2);
	decref(value3);
    }else{	    
	transfer(value,value1);
    }
    return 0;
}

static int parse_primary(char **line){
    token_t token[1];
    token_t token1[1];
    int ret;
    char *line1;
    if(eval_debug){
	printf("parse_primary(): line=[%s]\n",*line);
    }
    line1=*line;
    if((ret=gettoken(line,token))){
	return ret;
    }
    switch(token->type){
    case T_LP:
	if((ret=parse_commaexp(line))){
	    return ret;
	}
	if((ret=gettoken(line,token))){
	    return ret;
	}
	if(token->type!=T_RP){
	    return E_NO_MATCHING_RIGHT_PARENTHESIS;
	}
	break;
    case T_NEG:
    case T_PLUS:
    case T_MINUS:
	if((ret=parse_primary(line))){
	    return ret;
	}
	return 0;
    case T_NUM:
	return 0;
    case T_STRING:
	break;
    case T_VAR:
	if((ret=peektoken(line,token1))){
	    return ret;
	}
	if(token1->type==T_LP){
	    if((ret=parse_function(&line1))){
		*line=line1;
		return ret;
	    }
	    *line=line1;
	}
	break;
    default:
	return E_UNRECOGNIZED_PRIMARY;
	break;
    }
    if((ret=peektoken(line,token))){
	return ret;
    }
    if(token->type==T_LBR){
	skiptoken(line,token);
	if((ret=peektoken(line,token))){
	    return ret;
	}
	if(token->type==T_COLON){
	    skiptoken(line,token);
	}else{
	    if((ret=parse_commaexp(line))){
		return ret;
	    }
	    if((ret=peektoken(line,token))){
		return ret;
	    }
	    if(token->type!=T_COLON){
		if(token->type==T_RBR){
		}else{
		    return E_INVALID_SLICE_SYNTAX;
		}
	    }else{
		skiptoken(line,token);
	    }
	}
	if((ret=peektoken(line,token))){
	    return ret;
	}
	if(token->type==T_RBR){
	    skiptoken(line,token);
	}else{
	    if((ret=parse_commaexp(line))){
		return ret;
	    }
	    if((ret=gettoken(line,token))){
		return ret;
	    }
	    if(token->type!=T_RBR){
		return E_INVALID_SLICE_SYNTAX;
	    }
	}
    }else{	    

    }
    return 0;
}


static int eval_factor(char **line, value_t *value){
    token_t token[1];
    value_t value1[1], value2[1];
    int ret;
    if(eval_debug){
	printf("eval_factor(): line=[%s]\n",*line);
    }
    if((ret=eval_primary(line,value1))){
	return ret;
    }
    if((ret=peektoken(line,token))){
	decref(value1);
	return ret;
    }
    switch(token->type){
    case T_POWER:
	skiptoken(line,token);
	break;
    default:
	transfer(value,value1);
	return 0;
    }
    if((ret=eval_factor(line,value2))){
	decref(value1);
	return ret;
    }
    if((ret=binop(value,value1,value2,token->type))){
	decref(value1);
	decref(value2);
	return ret;
    }
    return 0;
}


static int parse_factor(char **line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_factor(): line=[%s]\n",*line);
    }
    if((ret=parse_primary(line))){
	return ret;
    }
    if((ret=peektoken(line,token))){
	return ret;
    }
    switch(token->type){
    case T_POWER:
	skiptoken(line,token);
	break;
    default:
	return 0;
    }
    if((ret=parse_factor(line))){
	return ret;
    }
    return 0;
}






static int eval_term(char **line, value_t *value){
    token_t token[1];
    value_t value1[1], value2[2];
    int ret;
    if(eval_debug){
	printf("eval_term(): line=[%s]\n",*line);
    }
    if((ret=eval_factor(line,value1))){
	return ret;
    }
    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_TIMES:
	case T_DIV:
	    if((ret=skiptoken(line,token))){
		decref(value1);
		return ret;
	    }
	    if((ret=eval_factor(line,value2))){
		decref(value1);
		return ret;
	    }
	    if((ret= binop(value1,value1,value2,token->type))){
		decref(value1);
		decref(value2);
		return ret;
	    }
	    break;
	default:
	    transfer(value,value1);
	    return 0;
	}
    }
    return 0;
}

static int parse_term(char **line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_term(): line=[%s]\n",*line);
    }
    if((ret=parse_factor(line))){
	return ret;
    }
    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_TIMES:
	case T_DIV:
	    if((ret=skiptoken(line,token))){
		return ret;
	    }
	    if((ret=parse_factor(line))){
		return ret;
	    }
	    break;
	default:
	    return 0;
	}
    }
    return 0;
}


static int eval_expr(char **line, value_t *value){
    token_t token[1];
    value_t value1[1], value2[2];
    int ret;
    if(eval_debug){
	printf("eval_expr(): line=[%s]\n",*line);
    }
    if((ret=eval_term(line,value1))){
	return ret;
    }
    while(1){
	if((ret=peektoken(line,token))){
	    decref(value1);
	    return ret;
	}
	switch(token->type){
	case T_PLUS:
	case T_MINUS:
	    skiptoken(line,token);
	    if((ret=eval_term(line,value2))){
		decref(value1);
		return ret;
	    }
	    if((ret=binop(value1,value1,value2,token->type))){
		decref(value1);
		decref(value2);
		return ret;
	    }
	    break;
	default:
	    transfer(value,value1);
	    return 0;
	}
    }
    return 0;
}

static int parse_expr(char **line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_expr(): line=[%s]\n",*line);
    }
    if((ret=parse_term(line))){
	return ret;
    }
    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_PLUS:
	case T_MINUS:
	    skiptoken(line,token);
	    if((ret=parse_term(line))){
		return ret;
	    }
	    break;
	default:
	    return 0;
	}
    }
    return 0;
}

// despite appearances this can return a numeric result

static int eval_boolterm(char **line, value_t *value){
    token_t token[1];
    value_t value1[1], value2[1];
    int ret;
    if(eval_debug){
	printf("eval_boolterm(): line=[%s]\n",*line);
    }
    if((ret=eval_expr(line,value1))){
	return ret;
    }
    if((ret=peektoken(line,token))){
	decref(value1);
	return ret;
    }
    switch(token->type){
    case T_LT:
    case T_GT:
    case T_LE:
    case T_GE:
    case T_EQ:
    case T_NEQ:
	skiptoken(line,token);
	break;
    default:
	transfer(value,value1);
	return 0;
    }
    if((ret=eval_expr(line,value2))){
	decref(value1);
	return ret;
    }
    if((ret=binop(value,value1,value2,token->type))){
	decref(value1);
	decref(value2);
	return ret;
    }
    return 0;
}

static int parse_boolterm(char **line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_boolterm(): line=[%s]\n",*line);
    }
    if((ret=parse_expr(line))){
	return ret;
    }
    if((ret=peektoken(line,token))){
	return ret;
    }
    switch(token->type){
    case T_LT:
    case T_GT:
    case T_LE:
    case T_GE:
    case T_EQ:
    case T_NEQ:
	skiptoken(line,token);
	break;
    default:
	return 0;
    }
    if((ret=parse_expr(line))){
	return ret;
    }
    return 0;
}

// despite appearances this can return a numeric result

static int eval_andexp(char ** line, value_t *value){
    token_t token[1];
    value_t value1[1];
    int ret;
    if(eval_debug){
	printf("eval_andexp(): line=[%s]\n",*line);
    }
    if((ret=eval_boolterm(line,value1))){
	return ret;
    }

    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_AND:
	    skiptoken(line,token);
	    if(value1->type!=V_BOOLEAN){
		return E_BINARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERANDS;
	    }
	    if(!value1->value){
		if((ret=parse_boolterm(line))){
		    return ret;
		}
	    }else{
		if((ret=eval_boolterm(line,value1))){
		    return ret;
		}
	    }
	    break;
	default:
	    transfer(value,value1);
	    return 0;
	}
    }
    return 0;
}

static int parse_andexp(char ** line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_andexp(): line=[%s]\n",*line);
    }
    if((ret=parse_boolterm(line))){
	return ret;
    }

    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_AND:
	    skiptoken(line,token);
	    if((ret=parse_boolterm(line))){
		return ret;
	    }
	    break;
	default:
	    return 0;
	}
    }
    return 0;
}


// despite appearances this can return a numeric result

static int eval_orexp(char ** line, value_t *value){
    token_t token[1];
    value_t value1[1];
    int ret;
    if(eval_debug){
	printf("eval_orexp(): line=[%s]\n",*line);
    }
    if((ret=eval_andexp(line,value1))){
	return ret;
    }

    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_OR:
	    skiptoken(line,token);
	    if(value1->type!=V_BOOLEAN){
		return E_BINARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERANDS;
	    }
	    if(value1->value){
		if((ret=parse_andexp(line))){
		    return ret;
		}
	    }else{
		if((ret=eval_andexp(line,value1))){
		    return ret;
		}
	    }
	    break;
	default:
	    transfer(value,value1);
	    return 0;
	}
    }
    return 0;
}

static int parse_orexp(char ** line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_orexp(): line=[%s]\n",*line);
    }
    if((ret=parse_andexp(line))){
	return ret;
    }

    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_OR:
	    skiptoken(line,token);
	    if((ret=parse_andexp(line))){
		return ret;
	    }
	    break;
	default:
	    return 0;
	}
    }
    return 0;
}


static int eval_ternary(char **line, value_t *value){
    token_t token[1];
    value_t value1[1];
    int ret;

    if(eval_debug){
	printf("eval_ternary(): line=[%s]\n",*line);
    }

    if((ret=eval_orexp(line,value1))){
	return ret;
    }
    if((ret=peektoken(line,token))){
	return ret;
    }
    if(token->type!=T_QUEST){
	transfer(value,value1);
	return 0;
    }
    skiptoken(line,token);
    if(value1->type!=V_BOOLEAN){
	decref(value1);
	return E_SELECTOR_IN_TERNARY_IS_NON_BOOLEAN;
    }
    // no need to do decref on boolean values
    if(value1->value){
	if((ret=eval_assign(line,value))){
	    return ret;
	}
    }else{
	if((ret=parse_assign(line))){
	    return ret;
	}
    }
    if((ret=gettoken(line,token))){
	return ret;
    }
    if(token->type!=T_COLON){
	return E_TERNARY_DOES_NOT_CONTAIN_COLON;
    }
    if(value1->value){
	if((ret=parse_assign(line))){
	    return ret;
	}
    }else{
	if((ret=eval_assign(line,value))){
	    return ret;
	}
    }
    return 0;
}

static int parse_ternary(char **line){
    token_t token[1];
    int ret;

    if(eval_debug){
	printf("parse_ternary(): line=[%s]\n",*line);
    }

    if((ret=parse_orexp(line))){
	return ret;
    }
    if((ret=peektoken(line,token))){
	return ret;
    }
    if(token->type!=T_QUEST){
	return 0;
    }
    skiptoken(line,token);
    if((ret=parse_assign(line))){
	return ret;
    }
    if((ret=gettoken(line,token))){
	return ret;
    }
    if(token->type!=T_COLON){
	return E_TERNARY_DOES_NOT_CONTAIN_COLON;
    }
    if((ret=parse_assign(line))){
	return ret;
    }
    return 0;
}


static int eval_assign(char **line, value_t *value){
    token_t var[1];
    token_t token[1];
    char *line1;
    char *name;
    int ret;
    int i;
    line1=*line;
    if(eval_debug){
	printf("eval_assign(): line=[%s]\n",*line);
    }
    if((ret=gettoken(line,var))){
	return ret;
    }
    if(var->type!=T_VAR){
	if((ret=eval_ternary(&line1,value))){
	    *line=line1;
	    return ret;
	}
	*line=line1;
	return 0;
    }
    if((ret=gettoken(line,token))){
	return ret;
    }
    if(token->type!=T_ASSIGN){
	if((ret=eval_ternary(&line1,value))){
	    *line=line1;
	    return ret;
	}
	*line=line1;
	return 0;
    }
    if((ret=eval_assign(line,value))){
	return ret;
    }
    name=eval_malloc(var->length+1);
    for(i=0;i<var->length;i++){
	name[i]=var->data[i];
    }
    name[i]='\0';
    if((ret=symbol_table_set(name,value,SY_HONOR_RO))){
	eval_free(name);
	decref(value);
	return ret;
    }

    eval_free(name);
    incref(value);
    return 0;
}

static int parse_assign(char **line){
    token_t var[1];
    token_t token[1];
    char *line1;
    int ret;
    line1=*line;
    if(eval_debug){
	printf("parse_assign(): line=[%s]\n",*line);
    }
    if((ret=gettoken(line,var))){
	return ret;
    }
    if(var->type!=T_VAR){
	if((ret=parse_ternary(&line1))){
	    *line=line1;
	    return ret;
	}
	*line=line1;
	return 0;
    }
    if((ret=gettoken(line,token))){
	return ret;
    }
    if(token->type!=T_ASSIGN){
	if((ret=parse_ternary(&line1))){
	    *line=line1;
	    return ret;
	}
	*line=line1;
	return 0;
    }
    if((ret=parse_assign(line))){
	return ret;
    }
    return 0;
}


static int eval_commaexp(char **line, value_t *value){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("eval_commaexp(): line=[%s]\n",*line);
    }
    if((ret=eval_assign(line,value))){
	return ret;
    }
    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_COMMA:
	    skiptoken(line,token);
	    decref(value);
	    if((ret=eval_assign(line,value))){
		return ret;
	    }
	    break;
	default:
	    return 0;
	}
    }   
    return 0;
}

static int parse_commaexp(char **line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse_commaexp(): line=[%s]\n",*line);
    }
    if((ret=parse_assign(line))){
	return ret;
    }
    while(1){
	if((ret=peektoken(line,token))){
	    return ret;
	}
	switch(token->type){
	case T_COMMA:
	    skiptoken(line,token);
	    if((ret=parse_assign(line))){
		return ret;
	    }
	    break;
	default:
	    return 0;
	}
    }   
    return 0;
}


// A string_value returned lives in the symbol table
// so it should not be freed externally.

int eval(value_t *value, char *format, ... ){
    token_t token[1];
    int ret;
    va_list alist;
    char *line;
    //    char *line1;
    char *buf;
    va_start(alist,format);
    ret=vsnprintf(NULL,0,format,alist);
    va_end(alist);
    buf=eval_malloc(ret+1);
    va_start(alist,format);
    vsprintf(buf,format,alist);
    va_end(alist);

    line=buf;
    if(eval_debug){
	printf("eval(): line=[%s]\n",line);
    }

    if((ret=eval_commaexp(&line,value))){
	value->type=V_ERROR;
	value->value=ret;
	//	printf("rest=[%s]\n",line);
	symbol_table_set("_",value,0);
	eval_free(buf);
	return ret;
    }
    //    line1=line;
    if((ret=peektoken(&line,token))){
	decref(value);
	value->type=V_ERROR;
	value->value=ret;
	//	printf("rest=[%s]\n",line1);
	symbol_table_set("_",value,0);
	eval_free(buf);
	return ret;
    }
    if(token->type!=T_NONE){
	decref(value);
	value->type=V_ERROR;
	value->value=E_TOKENS_AFTER_EXPRESSION;
	symbol_table_set("_",value,0);
	//	printf("rest=[%s]\n",line1);
	eval_free(buf);
	return E_TOKENS_AFTER_EXPRESSION;
    }
    if((ret=symbol_table_set("_",value,0))){
	decref(value);
	value->type=V_ERROR;
	value->value=ret;
	//	printf("rest=[%s]\n",line1);
	eval_free(buf);
	symbol_table_set("_",value,0);
	return ret;
    }
    eval_free(buf);
    return 0;
}

int parse(char *line){
    token_t token[1];
    int ret;
    if(eval_debug){
	printf("parse(): line=[%s]\n",line);
    }
    if((ret=parse_commaexp(&line))){
	return ret;
    }
    if((ret=peektoken(&line,token))){
	return ret;
    }
    if(token->type!=T_NONE){
	return E_TOKENS_AFTER_EXPRESSION;
    }
    return 0;
}



static int string_len_wrap(value_t *result,value_t **value){
    result->type=V_NUMERIC;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }
    result->value=strlen(value[0]->string_value->data);
    return 0;
}

static int str_wrap(value_t *result,value_t **value){
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    result->type=V_STRING;
    switch(value[0]->type){
    case V_STRING:
	transfer(result,value[0]);
	result->string_value->refcount++;
	break;
    case V_BOOLEAN:
	result->string_value=value[0]->value?string_create("true",-1):
	                                     string_create("false",-1);
					     break;
    case V_NUMERIC:
	result->string_value=string_create("",100);
	snprintf(result->string_value->data,99,"%.16g",value[0]->value);
	break;
    case V_CFUNC:
	result->string_value=string_create("",100);
	snprintf(result->string_value->data,99,"<C function: %p>",value[0]->cfunc_value);
	break;
    case V_NONE:
	result->string_value=string_create("none",-1);
	break;
    case V_UNDEFINED:
	result->string_value=string_create("undefined",-1);
	break;
    case V_ERROR:
	if(value[0]->value<0 || value[0]->value>MAXERROR){
	    result->string_value=string_create("",100);
	    snprintf(result->string_value->data,99,"<Error: %d>",(int)value[0]->value);
	}else{
	    result->string_value=string_create((char *)eval_errmsg[(int) value[0]->value],-1);
	}
	break;
    default:
	return E_NOT_IMPLEMENTED;
    }
    return 0;
}

static int float_wrap(value_t *result,value_t **value){
    token_t token[1];
    token_t token1[1];
    char * input;
    int ret;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    result->type=V_NUMERIC;
    switch(value[0]->type){
    case V_STRING:
	input=value[0]->string_value->data;
	if((ret=gettoken(&input,token))){
	    return ret;
	}
	if(token->type==T_MINUS){
	    if((ret=gettoken(&input,token))){
		return ret;
	    }
	}
	if(token->type!=T_NUM){
	    return E_STRING_DOES_NOT_REPRESENT_A_NUMBER;
	}
	if((ret=gettoken(&input,token1))){
	    return ret;
	}
	if(token1->type!=T_NONE){
	    return E_STRING_DOES_NOT_REPRESENT_A_NUMBER;
	}
	result->value=atoi(value[0]->string_value->data);
	break;
    case V_BOOLEAN:
    case V_NUMERIC:
    case V_NONE:
    case V_UNDEFINED:
    case V_ERROR:
	result->value=value[0]->value;
	break;
    case V_CFUNC:
    default:
	return E_NOT_IMPLEMENTED;
    }
    return 0;
}

static int int_wrap(value_t *result,value_t **value){
    value_t *tmp[2];
    value_t tmp1;
    int ret;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    tmp[0]=eval_malloc(sizeof(value_t));
    transfer(tmp[0],value[0]);
    tmp[1]=NULL;
    if((ret=float_wrap(&tmp1,tmp))){
	eval_free(tmp[0]);
	return ret;
    }
    eval_free(tmp[0]);
    result->type=V_NUMERIC;
    result->value=(int) tmp1.value;
    return 0;
}


static int bool_wrap(value_t *result,value_t **value){
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    result->type=V_BOOLEAN;
    switch(value[0]->type){
    case V_STRING:
	if(!strcmp(value[0]->string_value->data,"")){
	    result->value=0;
	}else{
	    result->value=1;
	}
	break;
    case V_BOOLEAN:
    case V_NUMERIC:
    case V_NONE:
    case V_UNDEFINED:
	if(value[0]->value==0){
	    result->value=0;
	}else{
	    result->value=1;
	}
	break;
    case V_ERROR:
    case V_CFUNC:
    default:
	return E_NOT_IMPLEMENTED;
    }
    return 0;
}

static int eval_wrap(value_t *result,value_t **value){
    int ret;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }
    if((ret=eval(result,"%s",value[0]->string_value->data))){
	return ret;
    }
    incref(result);
    return 0;
}

int memory_wrap(value_t *result,value_t **value){
    int ret;
    if(value[0]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    ret=value_init(result,V_NUMERIC,eval_memory());
    return ret;
}

static int abs_wrap(value_t *result,value_t **value){
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_NUMERIC){
	return E_INVALID_SIGNATURE;
    }
    transfer(result,value[0]);
    if(result->value<0){
	result->value=-result->value;
    }
    return 0;
}

static int ord_wrap(value_t *result, value_t **value){
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }
    if(strlen(value[0]->string_value->data)!=1){
	return E_STRING_IS_NOT_ONE_CHARACTER;
    }
    result->type=V_NUMERIC;
    result->value=value[0]->string_value->data[0];
    return 0;
}

static int chr_wrap(value_t *result, value_t **value){
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_NUMERIC){
	return E_INVALID_SIGNATURE;
    }
    result->type=V_STRING;
    result->string_value=string_create("",1);
    result->string_value->data[0]=value[0]->value;
    result->string_value->data[1]='\0';
    return 0;
}

static int lower_wrap(value_t *result, value_t **value){
    int l1,i;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }
    result->type=V_STRING;
    l1=strlen(value[0]->string_value->data);
    result->string_value=string_create("",l1);
    for(i=0;i<l1;i++){
	result->string_value->data[i]=tolower(value[0]->string_value->data[i]);
    }
    result->string_value->data[i]='\0';
    return 0;;
}

static int upper_wrap(value_t *result, value_t **value){
    int l1,i;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }
    result->type=V_STRING;
    l1=strlen(value[0]->string_value->data);
    result->string_value=string_create("",l1);
    for(i=0;i<l1;i++){
	result->string_value->data[i]=toupper(value[0]->string_value->data[i]);
    }
    result->string_value->data[i]='\0';
    return 0;;
}

static int find_wrap(value_t *result, value_t **value){
    char *index;
    if(value[0]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[1]==NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[2]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    if(value[0]->type!=V_STRING || value[1]->type!=V_STRING){
	return E_INVALID_SIGNATURE;
    }
    result->type=V_NUMERIC;

    index=strstr(value[0]->string_value->data,value[1]->string_value->data);
    if(index==NULL){
	result->value=-1;
    }else{
	result->value=index-value[0]->string_value->data;
    }
    return 0;;
}

static int time_wrap(value_t *result, value_t **value){
    struct timeval tv;
    if(value[0]!=NULL){
	return E_INVALID_SIGNATURE;
    }
    gettimeofday(&tv,NULL);
    result->type=V_NUMERIC;
    result->value=tv.tv_sec+tv.tv_usec/1000000.0;
    //    result->value=tv.tv_sec;
    return 0;
}

void eval_init(){
    init_tokenizer();
    symbol_table_init();
}

int value_init( value_t * value, int type, ...){
    va_list a_list;
    int ret=0;
    va_start(a_list,type);
    value->type=type;
    switch(type){
    case V_NUMERIC:
	value->value=va_arg(a_list,int);
	break;
    case V_BOOLEAN:
	value->value=va_arg(a_list,int);
	break;
    case V_NONE:
	value->value=0;
	break;
    case V_UNDEFINED:
	value->value=0;
	break;
    case V_ERROR:
	value->value=va_arg(a_list,int);
	break;
    case V_STRING:
	value->string_value=string_create(va_arg(a_list,char *),-1);
	break;
    case V_CFUNC:
	value->cfunc_value=va_arg(a_list,cfunc);
	break;
    default:
	ret=E_UNKNOWN_TYPE;
    }
    va_end(a_list);
    return ret;
}

