#pragma once
#include "basic.h"
#include "arena.h"

struct Ast;

enum TokenClass
{
  TOK_NONE,
  TOK_SEMICOLON,
  TOK_IDENT,
  TOK_TYPE_IDENT,
  TOK_STRING,
  TOK_INTEGER,
  TOK_WINTEGER,
  TOK_SINTEGER,
  TOK_INTEGER_HEX,
  TOK_WINTEGER_HEX,
  TOK_SINTEGER_HEX,
  TOK_INTEGER_OCT,
  TOK_WINTEGER_OCT,
  TOK_SINTEGER_OCT,
  TOK_INTEGER_BIN,
  TOK_WINTEGER_BIN,
  TOK_SINTEGER_BIN,
  TOK_PARENTH_OPEN,
  TOK_PARENTH_CLOSE,
  TOK_ANGLE_OPEN,
  TOK_ANGLE_CLOSE,
  TOK_BRACE_OPEN,
  TOK_BRACE_CLOSE,
  TOK_DONTCARE,
  TOK_COLON,
  TOK_PERIOD,
  TOK_COMMA,
  TOK_MINUS,
  TOK_PLUS,
  TOK_STAR,
  TOK_SLASH,
  TOK_EQUAL,
  TOK_EQUAL_EQUAL,
  TOK_COMMENT,

  TOK_KW_ACTION,
  TOK_KW_ENUM,
  TOK_KW_IN,
  TOK_KW_PACKAGE,
  TOK_KW_SELECT,
  TOK_KW_SWITCH,
  TOK_KW_TUPLE,
  TOK_KW_VOID,
  TOK_KW_APPLY,
  TOK_KW_CONTROL,
  TOK_KW_ERROR,
  TOK_KW_HEADER,
  TOK_KW_INOUT,
  TOK_KW_PARSER,
  TOK_KW_STATE,
  TOK_KW_TABLE,
  TOK_KW_TYPEDEF,
  TOK_KW_BIT,
  TOK_KW_DEFAULT,
  TOK_KW_EXTERN,
  TOK_KW_HEADER_UNION,
  TOK_KW_INT,
  TOK_KW_OUT,
  TOK_KW_STRING,
  TOK_KW_TRANSITION,
  TOK_KW_VARBIT,
  TOK_KW_ELSE,
  TOK_KW_EXIT,
  TOK_KW_IF,
  TOK_KW_MATCH_KIND,
  TOK_KW_RETURN,
  TOK_KW_STRUCT,
  TOK_KW_VAR,
  TOK_KW_CONST,

  TOK_UNKNOWN,
  TOK_SOI,    // Start Of Input
  TOK_EOI,    // End Of Input
};

enum IdentKind
{
  ID_NONE,
  ID_KEYWORD,
  ID_TYPE,
  ID_TYPEVAR,
  ID_VAR,
};

typedef struct Ident
{
  enum IdentKind ident_kind;
  char* name;
  struct Ast* ast;
  int scope_level;
  struct Ident* next_in_scope;
}
Ident;  // ID_TYPE
        // ID_TYPEVAR
        // ID_VAR

typedef struct Ident_Keyword
{
  Ident;
  enum TokenClass token_klass;
}
Ident_Keyword;  // ID_KEYWORD

typedef struct Namespace_Entry
{
  char* name;
  Ident* ns_global;
  Ident* ns_type;
  struct Namespace_Entry* next;
}
Namespace_Entry;

typedef struct
{
  enum TokenClass klass;
  char* lexeme;
  int line_nr;
}
Token;

enum Typexpr_TypeCtor
{
  TYP_NONE,
  TYP_UNKNOWN,
  TYP_BASIC,
  TYP_TYPE_PARAMETER,
  TYP_PARAMETER,
  TYP_FUNCTION,
  TYP_ENUM_FIELD,
  TYP_ENUM,
  TYP_PARSER,
  TYP_CONTROL,
  TYP_PACKAGE,
  TYP_TYPEDEF,
  TYP_STRUCT_FIELD,
  TYP_HEADER,
  TYP_STRUCT,
  TYP_EXTERN_OBJECT,
  TYP_BINARY_EXPR,
  TYP_ARGUMENT,
  TYP_FUNCTION_CALL,
  TYP_PACKAGE_INSTANTIATION,
};

enum TypeBasic_Kind
{
  BASTYP_NONE,
  BASTYP_INT,
  BASTYP_VOID,
  BASTYP_BOOL,
  BASTYP_BIT,
  BASTYP_VARBIT,
  BASTYP_STRING,
};

typedef struct Typexpr 
{
  enum Typexpr_TypeCtor kind;
  bool is_prototype;
  char* name;
}
Typexpr;

typedef struct Typexpr_Unknown
{
  Typexpr;
  struct Ast* ast;
}
Typexpr_Unknown;

typedef struct Typexpr_Basic
{
  Typexpr;
  enum TypeBasic_Kind basic_type;
  bool is_signed;
}
Typexpr_Basic;

typedef struct Typexpr_EnumField
{
  Typexpr;
  struct Typexpr_EnumField* next_field;
}
Typexpr_EnumField;  // TYP_ENUM_FIELD

typedef struct Typexpr_Enum
{
  Typexpr;

  Typexpr_EnumField* sentinel_field;
  Typexpr_EnumField* last_field;
  int field_count;
}
Typexpr_Enum;  // TYP_ENUM

typedef struct Typexpr_TypeParameter
{
  Typexpr;
  struct Typexpr_TypeParameter* next_type_parameter;
}
Typexpr_TypeParameter;  // TYP_TYPE_PARAMETER

typedef struct Typexpr_Parameter
{
  Typexpr;
  struct Typexpr_Parameter* next_parameter;
  Typexpr* type;
}
Typexpr_Parameter;  // TYP_PARAMETER

typedef struct Typexpr_Function
{
  Typexpr;
  struct Typexpr_Function* next_function;

  Typexpr_TypeParameter* sentinel_type_parameter;
  Typexpr_TypeParameter* last_type_parameter;
  int type_parameter_count;

  Typexpr_Parameter* sentinel_parameter;
  Typexpr_Parameter* last_parameter;
  int parameter_count;

  Typexpr* return_type;
}
Typexpr_Function;  // TYP_FUNCTION

typedef struct Typexpr_ExternObject
{
  Typexpr;

  Typexpr_Function* sentinel_function;
  Typexpr_Function* last_function;
  int method_count;
}
Typexpr_ExternObject;  // TYP_EXTERN_OBJECT

typedef struct Typexpr_Parser
{
  Typexpr;

  Typexpr_TypeParameter* sentinel_type_parameter;
  Typexpr_TypeParameter* last_type_parameter;
  int type_parameter_count;

  Typexpr_Parameter* sentinel_parameter;
  Typexpr_Parameter* last_parameter;
  int parameter_count;
}
Typexpr_Parser;  // TYP_PARSER

typedef struct Typexpr_Control
{
  Typexpr;

  Typexpr_TypeParameter* sentinel_type_parameter;
  Typexpr_TypeParameter* last_type_parameter;
  int type_parameter_count;

  Typexpr_Parameter* sentinel_parameter;
  Typexpr_Parameter* last_parameter;
  int parameter_count;
}
Typexpr_Control;  // TYP_CONTROL

typedef struct Typexpr_Package
{
  Typexpr;

  Typexpr_TypeParameter* sentinel_type_parameter;
  Typexpr_TypeParameter* last_type_parameter;
  int type_parameter_count;

  Typexpr_Parameter* sentinel_parameter;
  Typexpr_Parameter* last_parameter;
  int parameter_count;
}
Typexpr_Package;  // TYP_PACKAGE

typedef struct Typexpr_Typedef
{
  Typexpr;
  Typexpr* type;
}
Typexpr_Typedef;  // TYP_TYPEDEF

typedef struct Typexpr_StructField
{
  Typexpr;
  struct Typexpr_StructField* next_field;
  Typexpr* type;
}
Typexpr_StructField;  // TYP_STRUCT_FIELD

typedef struct Typexpr_Header
{
  Typexpr;

  Typexpr_StructField* sentinel_field;
  Typexpr_StructField* last_field;
  int field_count;
}
Typexpr_Header;  // TYP_HEADER

typedef struct Typexpr_Struct
{
  Typexpr;

  Typexpr_StructField* sentinel_field;
  Typexpr_StructField* last_field;
  int field_count;
}
Typexpr_Struct;  // TYP_STRUCT

typedef struct Typexpr_BinaryExpr
{
  Typexpr;

  Typexpr* l_type;
  Typexpr* r_type;
}
Typexpr_BinaryExpr;  // TYP_BINARY_EXPR

typedef struct Typexpr_Argument
{
  Typexpr;
  struct Typexpr_Argument* next_argument;
  Typexpr* type;
}
Typexpr_Argument;  // TYP_ARGUMENT

typedef struct Typexpr_FunctionCall
{
  Typexpr;

  Typexpr* function;

  Typexpr_Argument* sentinel_argument;
  Typexpr_Argument* last_argument;
  int argument_count;
}
Typexpr_FunctionCall;  // TYP_FUNCTION_CALL

typedef struct Typexpr_PackageInstantiation
{
  Typexpr;

  Typexpr* ctor_type;
}
Typexpr_PackageInstantiation;  // TYP_PACKAGE_INSTANTIATION

#define ast_cast(TYPE, KIND, EXPR) ({\
  if ((EXPR)) assert((EXPR)->kind == KIND); \
  (TYPE* )(EXPR);})

enum AstKind
{
  AST_NONE,
  AST_P4PROGRAM,
  AST_ERROR_TYPE,
  AST_STRUCT_PROTOTYPE,
  AST_STRUCT_DECL,
  AST_HEADER_PROTOTYPE,
  AST_HEADER_DECL,
  AST_TYPEDEF,
  AST_TYPE_EXPRESSION,
  AST_STRUCT_FIELD,
  AST_ERROR_CODE,
  AST_PARAMETER,
  AST_TYPE_PARAMETER,
  AST_PARSER_PROTOTYPE,
  AST_PARSER_DECL,
  AST_PARSER_STATE,
  AST_IDENT,
  AST_TYPE_IDENT,
  AST_INTEGER,
  AST_WINTEGER,
  AST_SINTEGER,
  AST_BINARY_EXPR,
  AST_FUNCTION_CALL,
  AST_IDENT_STATE,
  AST_SELECT_STATE,
  AST_SELECT_CASE_EXPR,
  AST_DEFAULT_SELECT_CASE,
  AST_TRANSITION_STMT,
  AST_CONTROL_PROTOTYPE,
  AST_CONTROL_DECL,
  AST_BLOCK_STMT,
  AST_BOOL,
  AST_ACTION,
  AST_TABLE,
  AST_TABLE_KEY,
  AST_ACTION_REF,
  AST_SIMPLE_PROP,
  AST_VAR_DECL,
  AST_PACKAGE_PROTOTYPE,
  AST_PACKAGE_INSTANTIATION,
  AST_EXTERN_OBJECT_PROTOTYPE,
  AST_EXTERN_FUNCTION_PROTOTYPE,
  AST_FUNCTION_PROTOTYPE,
};

enum Ast_ParameterDirection
{
  AST_DIR_NONE,
  AST_DIR_IN,
  AST_DIR_OUT,
  AST_DIR_INOUT,
};

enum Ast_ExprOperator
{
  AST_OP_NONE,
  AST_OP_MEMBER_SELECTOR,
  AST_OP_LOGIC_EQUAL,
  AST_OP_FUNCTION_CALL,
  AST_OP_ASSIGN,
  AST_OP_ADDITION,
  AST_OP_SUBTRACT,
};

enum Ast_TypeParameterKind
{
  AST_TYPPARAM_NONE,
  AST_TYPPARAM_VAR,
  AST_TYPPARAM_INT,
};

typedef struct Ast
{
  enum AstKind kind;
  int line_nr;
  char* lexeme;
  Typexpr* typexpr;
  bool is_builtin;
}
Ast;

typedef struct Ast_Declaration
{
  Ast;
  struct Ast_Declaration* next_decl;
}
Ast_Declaration;

typedef struct Ast_ErrorCode
{
  Ast;
  struct Ast_ErrorCode* next_code;
  char* name;
  Ident* var_ident;
}
Ast_ErrorCode;  // AST_ERROR_CODE

typedef struct Ast_TypeParameter
{
  Ast;
  enum Ast_TypeParameterKind parameter_kind;
  struct Ast_TypeParameter* first_type_parameter;
  struct Ast_TypeParameter* next_parameter;
  Ident* type_ident;

  union
  {
    char* name;
    char* width;
  };
}
Ast_TypeParameter;  // AST_TYPE_PARAMETER

typedef struct Ast_TypeExpression
{
  Ast;
  enum Ast_TypeParameterKind argument_kind;
  struct Ast_TypeExpression* first_type_argument;
  struct Ast_TypeExpression* next_argument;
  
  char* name;
  Ast* type_ast;
  Ident* type_ident;
}
Ast_TypeExpression;  // AST_TYPE_EXPRESSION

typedef struct Ast_BitType
{
  Ast_TypeExpression;
  int width;
}
Ast_BitType;  // AST_BIT_TYPE

typedef struct Ast_IntType
{
  Ast_TypeExpression;
  int width;
}
Ast_IntType;  // AST_INT_TYPE

typedef struct Ast_Typedef
{
  Ast_Declaration;
  Ast_TypeExpression* type;
  char* name;
  Ident* type_ident;
}
Ast_Typedef;  // AST_TYPEDEF

typedef struct Ast_StructField
{
  Ast;
  struct Ast_StructField* next_field;
  char* name;
  Ast_TypeExpression* member_type;
  Ident* member_ident;
}
Ast_StructField;  // AST_STRUCT_FIELD

typedef struct Ast_StructDecl
{
  Ast_Declaration;
  char* name;
  Ast_TypeParameter* first_type_parameter;
  Ast_StructField* first_field;
  Ident* type_ident;
}
Ast_StructDecl;  // AST_STRUCT_PROTOTYPE
                 // AST_STRUCT_DECL

typedef struct Ast_HeaderDecl
{
  Ast_Declaration;
  Ast_TypeParameter* first_type_parameter;
  char* name;
  Ast_StructField* first_field;
  Ident* type_ident;
}
Ast_HeaderDecl;  // AST_HEADER_PROTOTYPE
                 // AST_HEADER_DECL

typedef struct Ast_ErrorType
{
  Ast_Declaration;
  Ast_ErrorCode* error_code;
  Ident* type_ident;
}
Ast_ErrorType;  // AST_ERROR_TYPE

typedef struct Ast_Parameter
{
  Ast;
  enum Ast_ParameterDirection direction;
  Ast_TypeExpression* param_type;
  char* name;
  Ident* var_ident;
  struct Ast_Parameter* next_parameter;
}
Ast_Parameter;  // AST_PARAMETER

typedef struct Ast_FunctionCall
{
  Ast_Declaration;
  Ast* function;
  Ast_Declaration* first_argument;
}
Ast_FunctionCall;  // AST_FUNCTION_CALL

typedef struct Ast_BinaryExpr
{
  Ast_Declaration;
  enum Ast_ExprOperator op;
  struct Ast* l_operand;
  struct Ast* r_operand;
}
Ast_BinaryExpr;  // AST_BINARY_EXPR

typedef struct Ast_Ident
{
  Ast_Declaration;
  Ast_TypeExpression* first_type_argument;
  char* name;
  Ident* var_ident;
}
Ast_Ident;  // AST_IDENT

typedef struct Ast_TypeIdent
{
  Ast_Declaration;
  Ast_TypeExpression* first_type_argument;
  char* name;
  Ident* type_ident;
}
Ast_TypeIdent;  // AST_TYPE_IDENT

typedef struct Ast_Integer
{
  Ast_Declaration;
  int value;
}
Ast_Integer;  // AST_INTEGER

typedef struct Ast_WInteger
{
  Ast_Declaration;
  int value;
}
Ast_WInteger;  // AST_WINTEGER

typedef struct Ast_SInteger
{
  Ast_Declaration;
  int value;
}
Ast_SInteger;  // AST_SINTEGER

typedef struct Ast_VarDecl
{
  Ast_Declaration;

  Ast_TypeExpression* var_type;
  Ast_Ident* name;
  Ident* var_ident;
  Ast* init_expr;
  bool is_const;
}
Ast_VarDecl;  // AST_VAR_DECL

typedef struct Ast_IdentState
{
  Ast;
  char* name;
}
Ast_IdentState;  // AST_IDENT_STATE

typedef struct Ast_SelectCase
{
  Ast;
  char* end_state;
  struct Ast_SelectCase* next;
}
Ast_SelectCase;  // AST_SELECT_CASE

typedef struct Ast_SelectCase_Expr
{
  Ast_SelectCase;
  Ast* key_expr;
}
Ast_SelectCase_Expr;  // AST_SELECT_CASE_EXPR

typedef struct Ast_SelectCase_Default
{
  Ast_SelectCase;
}
Ast_SelectCase_Default;  // AST_SELECT_CASE_DEFAULT

typedef struct Ast_SelectState
{
  Ast;
  Ast* expression;
  Ast_SelectCase* select_case;
}
Ast_SelectState;  // AST_SELECT_STATE

typedef struct Ast_TransitionStmt
{
  Ast;
  Ast* state_expr;
}
Ast_TransitionStmt;  // AST_TRANSITION_STMT

typedef struct Ast_ParserState
{
  Ast;
  char* name;
  Ast_Declaration* first_statement;
  struct Ast_ParserState* next_state;
  Ast_TransitionStmt* transition_stmt;
  Ident* var_ident;
}
Ast_ParserState;  // AST_PARSER_STATE

typedef struct Ast_ExprStmt
{
  Ast;
  Ast* expression;
}
Ast_ExprStmt;  // AST_EXPR_STMT

typedef struct Ast_BlockStmt
{
  Ast_Declaration;
  Ast_Declaration* first_statement;
}
Ast_BlockStmt;  // AST_BLOCK_STMT

typedef struct Ast_Bool
{
  Ast;
  bool value;
}
Ast_Bool;  // AST_BOOL

typedef struct Ast_TableProperty
{
  Ast;
  struct Ast_TableProperty* next;
}
Ast_TableProperty;

typedef struct Ast_Key
{
  Ast_TableProperty;
  Ast* expression;
  Ast* name;
  struct Ast_Key* next_key;
}
Ast_Key;  // AST_TABLE_KEY

typedef struct Ast_ActionRef
{
  Ast_TableProperty;
  char* name;
  Ast_Declaration* first_argument;
  struct Ast_ActionRef* next_action;
}
Ast_ActionRef;  // AST_ACTION_REF

typedef struct Ast_SimpleProp
{
  Ast_TableProperty;
  Ast* expression;
}
Ast_SimpleProp;  // AST_SIMPLE_PROP

typedef struct Ast_ActionDecl
{
  Ast_Declaration;
  char* name;
  Ast_Parameter* parameter;
  Ast_BlockStmt* action_body;
}
Ast_ActionDecl;  // AST_ACTION

typedef struct Ast_TableDecl
{
  Ast_Declaration;
  char* name;
  Ast_TableProperty* property;
}
Ast_TableDecl;  // AST_TABLE

typedef struct Ast_ControlDecl
{
  Ast_Declaration;
  char* name;
  Ast_TypeParameter* first_type_parameter;
  Ast_Parameter* first_parameter;
  Ast_Declaration* first_local_decl;
  Ast_BlockStmt* apply_block;
  Ident* type_ident;
}
Ast_ControlDecl;  // AST_CONTROL_PROTOTYPE
                  // AST_CONTROL_DECL

typedef struct Ast_ParserDecl
{
  Ast_Declaration;
  char* name;
  Ast_TypeParameter* first_type_parameter;
  Ast_Parameter* first_parameter;
  Ast_ParserState* first_parser_state;
  Ident* type_ident;
}
Ast_ParserDecl;  // AST_PARSER_PROTOTYPE
                 // AST_PARSER_DECL

typedef struct Ast_PackageDecl
{
  Ast_Declaration;
  char* name;
  Ast_TypeParameter* first_type_parameter;
  Ast_Parameter* first_parameter;
  Ident* type_ident;
}
Ast_PackageDecl;  // AST_PACKAGE_PROTOTYPE

typedef struct Ast_PackageInstantiation
{
  Ast_Declaration;
  Ast* package_ctor;
  char* name;
  Ident* var_ident;
}
Ast_PackageInstantiation;  // AST_PACKAGE_INSTANTIATION

typedef struct Ast_FunctionPrototype
{
  Ast_Declaration;
  char* name;
  Ast_TypeParameter* first_type_parameter;
  Ast_Parameter* first_parameter;
  Ast* return_type_ast;
  Ident* type_ident;
}
Ast_FunctionDecl;  // AST_FUNCTION_PROTOTYPE
                   // AST_EXTERN_FUNCTION_PROTOTYPE

typedef struct Ast_ExternObjectDecl
{
  Ast_Declaration;
  char* name;
  Ast_TypeParameter* first_type_parameter;
  Ast_FunctionDecl* first_method;
  Ident* type_ident;
}
Ast_ExternObjectDecl;  // AST_EXTERN_OBJECT_PROTOTYPE

typedef struct Ast_P4Program
{
  Ast;
  Ast_Declaration* first_declaration;
}
Ast_P4Program;  // AST_P4PROGRAM

