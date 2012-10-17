#ifndef EVAL_H
#define EVAL_H

/*
 * symbol table flags
 */

#define SY_RO      1

/*
 * value types
 */

#define V_BOOLEAN     0
#define V_NUMERIC     1
#define V_STRING      2
#define V_CFUNC       3
#define V_NONE        4
#define V_ERROR       5
#define V_UNDEFINED   6

/*
 * errors
 */

#define E_INVALID_TOKEN                                 1
#define E_TOKENS_AFTER_EXPRESSION                       2
#define E_NO_MATCHING_RIGHT_PARENTHESIS                 3
#define E_NOT_IMPLEMENTED                               4
#define E_UNRECOGNIZED_PRIMARY                          5
#define E_BINARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERANDS 6
#define E_UNARY_OPERATOR_IS_INCOMPATIBLE_WITH_OPERAND   7
#define E_BINARY_OPERATION_NOT_IMPLEMENTED              8
#define E_UNDEFINED_SYMBOL                              9
#define E_SELECTOR_IN_TERNARY_IS_NON_BOOLEAN           10
#define E_TERNARY_DOES_NOT_CONTAIN_COLON               11
#define E_SYMBOL_IS_READ_ONLY                          12
#define E_INVALID_SLICE_SYNTAX                         13
#define E_INVALID_SLICE_OPERATION                      14
#define E_BAD_FUNCTION_CALL_SYNTAX                     15
#define E_SYMBOL_NOT_CALLABLE                          16
#define E_INVALID_SIGNATURE                            17
#define E_STRING_DOES_NOT_REPRESENT_A_NUMBER           18
#define E_STRING_IS_NOT_ONE_CHARACTER                  19
#define E_UNKNOWN_ERROR                                20
#define E_UNKNOWN_TYPE                                 21

typedef struct {
    int refcount;
    char *data;
} string_t;

struct value_;
typedef int (*cfunc)(struct value_ * result, struct value_ ** value);
typedef struct value_ {
    int type;
    union {
	double  value;
	string_t* string_value;
	cfunc  cfunc_value;
    };
} value_t;

extern const char *eval_errmsg[];
extern int eval_debug;

/*
 * api
 */ 

int value_init(value_t * value, int type, ... );
int (*value_clear)(value_t * value);

void eval_init();
void (*eval_clear)();

int eval_set(char *name, int type, char flags,...);
int eval_memory();
/*
 * The contents of value remains owned by the interpreter.
 */
int eval(value_t *value, char *line, ...);

#endif
