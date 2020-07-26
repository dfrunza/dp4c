#include "dp4c.h"
#include "symtab.h"

Arena arena = {};
Token token_at = {};
char* input_text = 0;
uint32_t input_size = 0;
int symtab_len = 997;
NamespaceInfo** symtab = 0;
int scope_level = 0;
int typetable_len = 1000;
uint64_t* typetable = 0;
Ast_P4Program* p4program = 0;

internal Ast_Typeref* syn_typeref();
internal Ast_Expression* syn_expression(int priority_threshold);

internal Ast_StructField*
syn_struct_field()
{
  assert (token_at.klass == TOK_TYPE_IDENT);
  Ast_StructField* result = arena_push_struct(&arena, Ast_StructField);
  zero_struct(result, Ast_StructField);
  result->kind = AST_STRUCT_FIELD;
  result->typeref = syn_typeref();
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    IdentInfo_Selector* id_info = sym_get_selector(result->name);
    if (id_info && id_info->scope_level >= scope_level)
      error("at line %d: selector '%s' has been previously declared", lex_line_nr(), result->name);
    result->selector = sym_add_selector(result->name);
    lex_next_token(&token_at);
    if (token_at.klass == TOK_SEMICOLON)
      lex_next_token(&token_at);
    else
      error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal bool
token_is_declaration(Token* token)
{
  bool result = token_at.klass == TOK_KW_STRUCT || token_at.klass == TOK_KW_HEADER ||
      token_at.klass == TOK_KW_ERROR || token_at.klass == TOK_KW_TYPEDEF ||
      token_at.klass == TOK_KW_PARSER || token_at.klass == TOK_KW_CONTROL ||
      token_at.klass == TOK_TYPE_IDENT || token_at.klass == TOK_KW_EXTERN || token_at.klass == TOK_KW_PACKAGE;
  return result;
}

internal Ast_HeaderTypeDecl*
syn_header_type_decl()
{
  assert (token_at.klass == TOK_KW_HEADER);
  lex_next_token(&token_at);
  Ast_HeaderTypeDecl* result = arena_push_struct(&arena, Ast_HeaderTypeDecl);
  zero_struct(result, Ast_HeaderTypeDecl);
  result->kind = AST_HEADER_TYPE_DECL;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    result->type = sym_add_type(result->name);
    lex_next_token(&token_at);
    if (token_at.klass == TOK_BRACE_OPEN)
    {
      scope_push_level();
      lex_next_token(&token_at);
      if (token_at.klass == TOK_TYPE_IDENT)
      {
        Ast_StructField* field = syn_struct_field();
        result->field = field;
        result->field_count++;
        while (token_at.klass == TOK_TYPE_IDENT)
        {
          Ast_StructField* next_field = syn_struct_field();
          field->next = (Ast_Ident*)next_field;
          field = next_field;
          result->field_count++;
        }

        field = result->field;
        IdentInfo_Selector* selector = field->selector;
        IdentInfo_Type* type = result->type;
        type->selector = field->selector;
        field = (Ast_StructField*)field->next;
        while (field)
        {
          selector->next_selector = field->selector;
          selector = selector->next_selector;
          field = (Ast_StructField*)field->next;
        }
      }

      if (token_at.klass == TOK_BRACE_CLOSE)
        lex_next_token(&token_at);
      else if (token_at.klass == TOK_IDENT)
        error("at line %d: unknown type '%s'", lex_line_nr(), token_at.lexeme);
      else
        error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      scope_pop_level();
    }
    else
      error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
    error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_StructTypeDecl*
syn_struct_type_decl()
{
  assert (token_at.klass == TOK_KW_STRUCT);
  lex_next_token(&token_at);
  Ast_StructTypeDecl* result = arena_push_struct(&arena, Ast_StructTypeDecl);
  zero_struct(result, Ast_StructTypeDecl);
  result->kind = AST_STRUCT_TYPE_DECL;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    result->type = sym_add_type(result->name);
    lex_next_token(&token_at);
    if (token_at.klass == TOK_BRACE_OPEN)
    {
      scope_push_level();
      lex_next_token(&token_at);
      if (token_at.klass == TOK_TYPE_IDENT)
      {
        Ast_StructField* field = syn_struct_field();
        result->field = field;
        result->field_count++;
        while (token_at.klass == TOK_TYPE_IDENT)
        {
          Ast_StructField* next_field = syn_struct_field();
          field->next = (Ast_Ident*)next_field;
          field = next_field;
          result->field_count++;
        }

        field = result->field;
        IdentInfo_Selector* selector = field->selector;
        IdentInfo_Type* type = result->type;
        type->selector = field->selector;
        field = (Ast_StructField*)field->next;
        while (field)
        {
          selector->next_selector = field->selector;
          selector = selector->next_selector;
          field = (Ast_StructField*)field->next;
        }
      }

      if (token_at.klass == TOK_BRACE_CLOSE)
        lex_next_token(&token_at);
      else if (token_at.klass == TOK_IDENT)
        error("at line %d: unknown type '%s'", lex_line_nr(), token_at.lexeme);
      else
        error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      scope_pop_level();
    }
    else
      error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
    error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_ErrorCode*
syn_error_code()
{
  assert (token_at.klass == TOK_IDENT);
  Ast_ErrorCode* id = arena_push_struct(&arena, Ast_ErrorCode);
  zero_struct(id, Ast_ErrorCode);
  id->kind = AST_ERROR_CODE;
  id->name = token_at.lexeme;
  IdentInfo_Selector* id_info = sym_get_selector(id->name);
  if (id_info && id_info->scope_level >= scope_level)
    error("at line %d: selector '%s' has been previously declared", lex_line_nr(), id->name);
  id->selector = sym_add_selector(id->name);
  lex_next_token(&token_at);
  return id;
}

internal Ast_ErrorTypeDecl*
syn_error_type_decl()
{
  assert (token_at.klass == TOK_KW_ERROR);
  lex_next_token(&token_at);
  Ast_ErrorTypeDecl* result = arena_push_struct(&arena, Ast_ErrorTypeDecl);
  zero_struct(result, Ast_ErrorTypeDecl);
  result->kind = AST_ERROR_TYPE_DECL;
  result->type = sym_get_error_type();
  if (token_at.klass == TOK_BRACE_OPEN)
  {
    scope_push_level();
    lex_next_token(&token_at);
    if (token_at.klass == TOK_IDENT)
    {
      Ast_ErrorCode* field = syn_error_code();
      result->error_code = field;
      result->code_count++;
      while (token_at.klass == TOK_COMMA)
      {
        lex_next_token(&token_at);
        if (token_at.klass == TOK_IDENT)
        {
          Ast_ErrorCode* next_code = syn_error_code();
          field->next = (Ast_Ident*)next_code;
          field = next_code;
          result->code_count++;
        }
        else if (token_at.klass == TOK_COMMA)
          error("missing parameter at line %d", lex_line_nr());
        else
          error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      }

      field = result->error_code;
      IdentInfo_Selector* selector = field->selector;
      IdentInfo_Type* type = result->type;
      type->selector = field->selector;
      field = (Ast_ErrorCode*)field->next;
      while (field)
      {
        selector->next_selector = field->selector;
        selector = selector->next_selector;
        field = (Ast_ErrorCode*)field->next;
      }
    }
    else
      error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    if (token_at.klass == TOK_BRACE_CLOSE)
      lex_next_token(&token_at);
    else
      error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    scope_pop_level();
  }
  else
    error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal void
syn_type_parameter_list()
{
  assert (token_at.klass == TOK_ANGLE_OPEN);
  lex_next_token(&token_at);
  if (token_at.klass == TOK_IDENT)
  {
    sym_add_typevar(token_at.lexeme);
    lex_next_token(&token_at);
    while (token_at.klass == TOK_COMMA)
    {
      lex_next_token(&token_at);
      if (token_at.klass == TOK_IDENT)
      {
        sym_add_typevar(token_at.lexeme);
        lex_next_token(&token_at);
      }
      else if (token_at.klass == TOK_INTEGER)
      {
        // TODO
        lex_next_token(&token_at);
      }
      else if (token_at.klass == TOK_COMMA)
        error("missing parameter at line %d", lex_line_nr());
      else if (token_at.klass == TOK_TYPE_IDENT)
        error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
      else
        error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
  }
  else if (token_at.klass == TOK_INTEGER)
  {
    // TODO
    lex_next_token(&token_at);
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
    error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);

  if (token_at.klass == TOK_ANGLE_CLOSE)
    lex_next_token(&token_at);
  else
    error("'>' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
}

internal void
syn_typeref_argument_list()
{
  assert (token_at.klass == TOK_ANGLE_OPEN);
  lex_next_token(&token_at);
  if (token_at.klass == TOK_TYPE_IDENT)
  {
    //TODO
    lex_next_token(&token_at);
    while (token_at.klass == TOK_COMMA)
    {
      lex_next_token(&token_at);
      if (token_at.klass == TOK_TYPE_IDENT)
      {
        //TODO:
        lex_next_token(&token_at);
      }
      else if (token_at.klass == TOK_INTEGER)
      {
        // TODO
        lex_next_token(&token_at);
      }
      else if (token_at.klass == TOK_COMMA)
        error("missing parameter at line %d", lex_line_nr());
      else if (token_at.klass == TOK_IDENT)
        error("at line %d: unknown type '%s'", lex_line_nr(), token_at.lexeme);
      else
        error("type identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
  }
  else if (token_at.klass == TOK_INTEGER)
  {
    // TODO
    lex_next_token(&token_at);
  }
  else if (token_at.klass == TOK_IDENT)
    error("at line %d: unknown type '%s'", lex_line_nr(), token_at.lexeme);
  else
    error("type identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);

  if (token_at.klass == TOK_ANGLE_CLOSE)
    lex_next_token(&token_at);
  else
    error("'>' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
}

internal Ast_Typeref*
syn_typeref()
{
  assert (token_at.klass == TOK_TYPE_IDENT);
  Ast_Typeref* result = arena_push_struct(&arena, Ast_Typeref);
  zero_struct(result, Ast_Typeref);
  result->kind = AST_TYPEREF;
  result->name = token_at.lexeme;
  lex_next_token(&token_at);
  if (token_at.klass == TOK_ANGLE_OPEN)
    syn_typeref_argument_list();
  return result;
}

internal Ast_TypedefDecl*
syn_typedef_decl()
{
  assert (token_at.klass == TOK_KW_TYPEDEF);
  lex_next_token(&token_at);
  Ast_TypedefDecl* result = arena_push_struct(&arena, Ast_TypedefDecl);
  zero_struct(result, Ast_TypedefDecl);
  result->kind = AST_TYPEDEF_DECL;
  if (token_at.klass == TOK_TYPE_IDENT)
  {
    result->typeref = syn_typeref();
    if (token_at.klass == TOK_IDENT)
    {
      result->name = token_at.lexeme;
      result->type = sym_add_type(result->name);
      lex_next_token(&token_at);
      if (token_at.klass == TOK_SEMICOLON)
        lex_next_token(&token_at);
      else
        error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else if (token_at.klass == TOK_TYPE_IDENT)
      error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
    else
      error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  return result;
}

internal bool
token_is_direction(Token* token)
{
  bool result = token->klass == TOK_KW_IN || token->klass == TOK_KW_OUT || token->klass == TOK_KW_INOUT;
  return result;
}

internal bool
token_is_parameter(Token* token)
{
  bool result = token_is_direction(token) || token->klass == TOK_TYPE_IDENT;
  return result;
}

internal enum AstDirection
syn_direction()
{
  assert (token_is_direction(&token_at));
  enum AstDirection result = 0;
  if (token_at.klass == TOK_KW_IN)
    result = DIR_IN;
  else if (token_at.klass == TOK_KW_OUT)
    result = DIR_OUT;
  else if (token_at.klass == TOK_KW_INOUT)
    result = DIR_INOUT;
  lex_next_token(&token_at);
  return result;
}

internal Ast_Parameter*
syn_parameter()
{
  assert(token_is_parameter(&token_at));
  Ast_Parameter* result = arena_push_struct(&arena, Ast_Parameter);
  zero_struct(result, Ast_Parameter);
  result->kind = AST_PARAMETER;
  if (token_is_direction(&token_at))
    result->direction = syn_direction();
  if (token_at.klass == TOK_TYPE_IDENT)
    result->typeref = syn_typeref();
  else
    error("at line %d: unknown type '%s'", lex_line_nr(), token_at.lexeme);
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    lex_next_token(&token_at);
  }
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_Parameter*
syn_parameter_list()
{
  assert (token_is_parameter(&token_at));
  Ast_Parameter* parameter = syn_parameter();
  Ast_Parameter* result = parameter;
  while (token_at.klass == TOK_COMMA)
  {
    lex_next_token(&token_at);
    if (token_is_parameter(&token_at))
    {
      Ast_Parameter* next_parameter = syn_parameter();
      parameter->next = next_parameter;
      parameter = next_parameter;
    }
    else if (token_at.klass == TOK_COMMA)
      error("missing parameter at line %d", lex_line_nr());
    else
      error("type identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  return result;
}

internal Ast_ParserTypeDecl*
syn_parser_type_decl()
{
  assert (token_at.klass == TOK_KW_PARSER);
  lex_next_token(&token_at);
  Ast_ParserTypeDecl* result = arena_push_struct(&arena, Ast_ParserTypeDecl);
  zero_struct(result, Ast_ParserTypeDecl);
  result->kind = AST_PARSER_TYPE_DECL;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    result->type = sym_add_type(result->name);
    lex_next_token(&token_at);
    scope_push_level();
    if (token_at.klass == TOK_ANGLE_OPEN)
      syn_type_parameter_list();
    if (token_at.klass == TOK_PARENTH_OPEN)
    {
      lex_next_token(&token_at);
      if (token_is_parameter(&token_at))
        result->parameter = syn_parameter_list();
      if (token_at.klass == TOK_PARENTH_CLOSE)
        lex_next_token(&token_at);
      else
      {
        if (token_at.klass == TOK_IDENT)
          error("at line %d: unknown type '%s'", lex_line_nr(), token_at.lexeme);
        else
          error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      }
    }
    else
      error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    scope_pop_level();
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
    error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
  else
      error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal bool
token_is_sinteger(Token* token)
{
  bool result = token->klass == TOK_SINTEGER || token->klass == TOK_SINTEGER_HEX
    || token->klass == TOK_SINTEGER_OCT || token->klass == TOK_SINTEGER_BIN;
  return result;
}

internal bool
token_is_winteger(Token* token)
{
  bool result = token->klass == TOK_WINTEGER || token->klass == TOK_WINTEGER_HEX
    || token->klass == TOK_WINTEGER_OCT || token->klass == TOK_WINTEGER_BIN;
  return result;
}

internal bool
token_is_integer(Token* token)
{
  bool result = token->klass == TOK_INTEGER || token->klass == TOK_INTEGER_HEX
    || token->klass == TOK_INTEGER_OCT || token->klass == TOK_INTEGER_BIN;
  return result;
}

internal bool
token_is_expression(Token* token)
{
  bool result = token->klass == TOK_IDENT || token->klass == TOK_TYPE_IDENT ||
    token_is_integer(token) || token_is_winteger(token) || token_is_sinteger(token) ||
    //token->klass == TOK_KW_TRUE || token->klass == TOK_KW_FALSE ||
    token->klass == TOK_STRING || token->klass == TOK_PARENTH_OPEN;
  return result;
}

internal bool
token_is_expression_operator(Token* token)
{
  bool result = token_at.klass == TOK_PERIOD || token_at.klass == TOK_EQUAL_EQUAL
    || token_at.klass == TOK_PARENTH_OPEN || token_at.klass == TOK_EQUAL
    || token_at.klass == TOK_MINUS || token_at.klass == TOK_PLUS;
  return result;
}

internal Ast_Expression*
syn_expression_primary()
{
  assert (token_is_expression(&token_at));
  Ast_Expression* result = 0;
  if (token_at.klass == TOK_IDENT)
  {
    Ast_IdentExpr* expression = arena_push_struct(&arena, Ast_IdentExpr);
    zero_struct(expression, Ast_IdentExpr);
    expression->kind = AST_IDENT_EXPR;
    expression->name = token_at.lexeme;
    result = (Ast_Expression*)expression;
    lex_next_token(&token_at);
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
  {
    Ast_IdentExpr* expression = arena_push_struct(&arena, Ast_IdentExpr);
    zero_struct(expression, Ast_IdentExpr);
    expression->kind = AST_IDENT_EXPR;
    expression->name = token_at.lexeme;
    result = (Ast_Expression*)expression;
    lex_next_token(&token_at);
  }
  else if (token_is_integer(&token_at))
  {
    Ast_IntegerExpr* expression = arena_push_struct(&arena, Ast_IntegerExpr);
    zero_struct(expression, Ast_IntegerExpr);
    expression->kind = AST_INTEGER_EXPR;
    expression->value = 0; //TODO
    result = (Ast_Expression*)expression;
    lex_next_token(&token_at);
  }
  else if (token_is_winteger(&token_at))
  {
    Ast_WIntegerExpr* expression = arena_push_struct(&arena, Ast_WIntegerExpr);
    zero_struct(expression, Ast_WIntegerExpr);
    expression->kind = AST_WINTEGER_EXPR;
    expression->value = 0; //TODO
    result = (Ast_Expression*)expression;
    lex_next_token(&token_at);
  }
  else if (token_is_sinteger(&token_at))
  {
    Ast_SIntegerExpr* expression = arena_push_struct(&arena, Ast_SIntegerExpr);
    zero_struct(expression, Ast_SIntegerExpr);
    expression->kind = AST_SINTEGER_EXPR;
    expression->value = 0; //TODO
    result = (Ast_Expression*)expression;
    lex_next_token(&token_at);
  }
  else if (token_at.klass == TOK_KW_ERROR)
  {
    Ast_ErrorExpr* expression = arena_push_struct(&arena, Ast_ErrorExpr);
    zero_struct(expression, Ast_ErrorExpr);
    expression->kind = AST_ERROR_EXPR;
    result = (Ast_Expression*)expression;
    lex_next_token(&token_at);
  }
  else if (token_at.klass == TOK_PARENTH_OPEN)
  {
    lex_next_token(&token_at);
    if (token_is_expression(&token_at))
      result = syn_expression(1);
    if (token_at.klass == TOK_PARENTH_CLOSE)
      lex_next_token(&token_at);
    else
      error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
//  else if (token_at.klass == TOK_KW_TRUE || token_at.klass == TOK_KW_FALSE)
//  {
//    Ast_Bool* boolean = arena_push_struct(&arena, Ast_Bool);
//    *boolean = (Ast_Bool){};
//    boolean->kind = AST_BOOL;
//    if (token_at.klass == TOK_KW_TRUE)
//      boolean->value = true;
//    token_at++;
//  }
  else if (token_at.klass == TOK_KW_APPLY)
  {
    error("TODO");
    //Ast_IdentExpr* expression = arena_push_struct(&arena, Ast_IdentExpr);
    //zero_struct(expression, Ast_IdentExpr);
    //expression->kind = AST_IDENT_EXPR;
    //if (token_at.klass == TOK_KW_APPLY)
    //  expression->name = "apply";
    //else if (token_at.klass == TOK_KW_VERIFY)
    //  expression->name = "verify";
    //result = (Ast_Expression*)expression;
    //lex_next_token(&token_at);
  }
  else
    assert (false);
  return result;
}

internal enum AstExprOperator
syn_expression_operator()
{
  assert (token_is_expression_operator(&token_at));
  enum AstExprOperator result = 0;
  if (token_at.klass == TOK_PERIOD)
    result = OP_MEMBER_SELECTOR;
  else if (token_at.klass == TOK_EQUAL)
    result = OP_ASSIGN;
  else if (token_at.klass == TOK_EQUAL_EQUAL)
    result = OP_LOGIC_EQUAL;
  else if (token_at.klass == TOK_PARENTH_OPEN)
    result = OP_FUNCTION_CALL;
  else if (token_at.klass == TOK_MINUS)
    result = OP_SUBTRACT;
  else if (token_at.klass == TOK_PLUS)
    result = OP_ADDITION;
  else
    assert (false);
  return result;
}

internal int
op_get_priority(enum AstExprOperator op)
{
  int result = 0;
  if (op == OP_ASSIGN)
    result = 1;
  else if (op == OP_LOGIC_EQUAL)
    result = 2;
  else if (op == OP_ADDITION || op == OP_SUBTRACT)
    result = 3;
  else if (op == OP_FUNCTION_CALL)
    result = 4;
  else if (op == OP_MEMBER_SELECTOR)
    result = 5;
  else
    assert (false);
  return result;
}

internal bool
op_is_binary(enum AstExprOperator op)
{
  bool result = op == OP_LOGIC_EQUAL || op == OP_MEMBER_SELECTOR || op == OP_ASSIGN
    || op == OP_ADDITION || op == OP_SUBTRACT;
  return result;
}

internal Ast_Expression*
syn_expression(int priority_threshold)
{
  assert (token_is_expression(&token_at));
  Ast_Expression* primary = syn_expression_primary();
  Ast_Expression* result = primary;
  while (token_is_expression_operator(&token_at))
  {
    enum AstExprOperator op = syn_expression_operator();
    int priority = op_get_priority(op);
    if (priority >= priority_threshold)
    {
      lex_next_token(&token_at);
      if (op_is_binary(op))
      {
        Ast_BinaryExpr* binary_expr = arena_push_struct(&arena, Ast_BinaryExpr);
        zero_struct(binary_expr, Ast_BinaryExpr)
        binary_expr->kind = AST_BINARY_EXPR;
        binary_expr->l_operand = result;
        binary_expr->op = op;
        if (token_is_expression(&token_at))
          binary_expr->r_operand = syn_expression(priority_threshold + 1);
        else
          error("expression term expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
        result = (Ast_Expression*)binary_expr;
      }
      else if (op == OP_FUNCTION_CALL)
      {
        Ast_FunctionCallExpr* function_call = arena_push_struct(&arena, Ast_FunctionCallExpr);
        zero_struct(function_call, Ast_FunctionCallExpr);
        function_call->kind = AST_FUNCTION_CALL;
        if (token_is_expression(&token_at))
        {
          Ast_Expression* argument = syn_expression(1);
          function_call->argument = argument;
          function_call->argument_count++;
          while (token_at.klass == TOK_COMMA)
          {
            lex_next_token(&token_at);
            if (token_is_expression(&token_at))
            {
              Ast_Expression* next_argument = syn_expression(1);
              argument->next = next_argument;
              argument = next_argument;
              function_call->argument_count++;
            }
            else if (token_at.klass == TOK_COMMA)
              error("missing parameter at line %d", lex_line_nr());
            else
              error("at line %d: expected an expression term, got '%s'", lex_line_nr(), token_at.lexeme);
          }
        }
        if (token_at.klass == TOK_PARENTH_CLOSE)
          lex_next_token(&token_at);
        else
          error("at line %d: '}' expected, got '%s'", lex_line_nr(), token_at.lexeme);
        result = (Ast_Expression*)function_call;
      }
      else
        assert (false);
    }
    else
      break;
  }
  return result;
}

internal Ast_IdentState*
syn_ident_state()
{
  assert (token_at.klass == TOK_IDENT);
  Ast_IdentState* result = arena_push_struct(&arena, Ast_IdentState);
  zero_struct(result, Ast_IdentState);
  result->kind = AST_IDENT_STATE;
  result->name = token_at.lexeme;
  lex_next_token(&token_at);
  if (token_at.klass == TOK_SEMICOLON)
    lex_next_token(&token_at);
  else
    error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal bool
token_is_select_case(Token* token)
{
  bool result = token_is_expression(&token_at) || token_at.klass == TOK_KW_DEFAULT;
  return result;
}

internal Ast_SelectCase*
syn_select_case()
{
  assert (token_is_select_case(&token_at));
  Ast_SelectCase* result = 0;
  if (token_is_expression(&token_at))
  {
    Ast_ExprSelectCase* expr_select = arena_push_struct(&arena, Ast_ExprSelectCase);
    zero_struct(expr_select, Ast_ExprSelectCase);
    expr_select->kind = AST_EXPR_SELECT_CASE;
    expr_select->key_expr = syn_expression(1);
    result = (Ast_SelectCase*)expr_select;
  }
  else if (token_at.klass = TOK_KW_DEFAULT)
  {
    lex_next_token(&token_at);
    Ast_DefaultSelectCase* default_select = arena_push_struct(&arena, Ast_DefaultSelectCase);
    zero_struct(default_select, Ast_DefaultSelectCase);
    default_select->kind = AST_DEFAULT_SELECT_CASE;
    result = (Ast_SelectCase*)default_select;
  }
  else
    assert (false);
  if (token_at.klass == TOK_COLON)
  {
    lex_next_token(&token_at);
    if (token_at.klass == TOK_IDENT)
    {
      result->end_state = token_at.lexeme;
      lex_next_token(&token_at);
      if (token_at.klass == TOK_SEMICOLON)
        lex_next_token(&token_at);
      else
        error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("':' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_SelectCase*
syn_select_case_list()
{
  assert (token_is_select_case(&token_at));
  Ast_SelectCase* select_case = syn_select_case();
  Ast_SelectCase* result = select_case;
  while (token_is_select_case(&token_at))
  {
    Ast_SelectCase* next_select_case = syn_select_case();
    select_case->next = next_select_case;
    select_case = next_select_case;
  }
  return result;
}

internal Ast_SelectState*
syn_select_state()
{
  assert (token_at.klass == TOK_KW_SELECT);
  lex_next_token(&token_at);
  Ast_SelectState* result = arena_push_struct(&arena, Ast_SelectState);
  zero_struct(result, Ast_SelectState);
  result->kind = AST_SELECT_STATE;
  if (token_at.klass == TOK_PARENTH_OPEN)
  {
    lex_next_token(&token_at);
    if (token_is_expression(&token_at))
      result->expression = syn_expression(1);
    if (token_at.klass == TOK_PARENTH_CLOSE)
      lex_next_token(&token_at);
    else
      error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  if (token_at.klass == TOK_BRACE_OPEN)
  {
    lex_next_token(&token_at);
    result->select_case = syn_select_case_list();
    if (token_at.klass == TOK_BRACE_CLOSE)
      lex_next_token(&token_at);
    else
      error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_TransitionStmt*
syn_transition_stmt()
{
  assert (token_at.klass == TOK_KW_TRANSITION);
  lex_next_token(&token_at);
  Ast_TransitionStmt* result = arena_push_struct(&arena, Ast_TransitionStmt);
  zero_struct(result, Ast_TransitionStmt);
  result->kind = AST_TRANSITION_STMT;
  if (token_at.klass == TOK_IDENT)
    result->state_expr = (Ast_StateExpr*)syn_ident_state();
  else if (token_at.klass == TOK_KW_SELECT)
    result->state_expr = (Ast_StateExpr*)syn_select_state();
  else
    error("transition stmt expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_Expression*
syn_statement_list()
{
  Ast_Expression* result = 0;
  if (token_is_expression(&token_at))
  {
    Ast_Expression* expression = syn_expression(1);
    result = expression;
    if (token_at.klass == TOK_SEMICOLON)
    {
      lex_next_token(&token_at);
      while (token_is_expression(&token_at))
      {
        Ast_Expression* next_expression = syn_expression(1);
        expression->next = next_expression;
        expression = next_expression;
        if (token_at.klass == TOK_SEMICOLON)
          lex_next_token(&token_at);
        else
          error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      }
    }
    else
      error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  return result;
}

internal Ast_ParserState*
syn_parser_state()
{
  assert (token_at.klass == TOK_KW_STATE);
  lex_next_token(&token_at);
  Ast_ParserState* result = arena_push_struct(&arena, Ast_ParserState);
  zero_struct(result, Ast_ParserState);
  result->kind = AST_PARSER_STATE;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    lex_next_token(&token_at);
    if (token_at.klass == TOK_BRACE_OPEN)
    {
      lex_next_token(&token_at);
      result->statement = syn_statement_list();
      if (token_at.klass == TOK_KW_TRANSITION)
        result->transition_stmt = syn_transition_stmt();
      else
        error("'transition' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      if (token_at.klass == TOK_BRACE_CLOSE)
        lex_next_token(&token_at);
      else
        error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  return result;
}

internal Ast_ParserDecl*
syn_parser_decl()
{
  assert (token_at.klass == TOK_KW_PARSER);
  sym_remove_error_kw();
  Ast_ParserDecl* result = arena_push_struct(&arena, Ast_ParserDecl);
  zero_struct(result, Ast_ParserDecl);
  result->kind = AST_PARSER_DECL;
  result->parser_type_decl = syn_parser_type_decl();
  if (token_at.klass == TOK_BRACE_OPEN)
  {
    lex_next_token(&token_at);
    scope_push_level();
    sym_add_error_var();
    if (token_at.klass == TOK_KW_STATE)
    {
      Ast_ParserState* state = syn_parser_state();
      result->parser_state = state;
      while (token_at.klass == TOK_KW_STATE)
      {
        Ast_ParserState* next_state = syn_parser_state();
        state->next = next_state;
        state = next_state;
      }
    }
    else
      error("'state' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    if (token_at.klass == TOK_BRACE_CLOSE)
      lex_next_token(&token_at);
    else
      error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    sym_remove_error_var();
    scope_pop_level();
  }
  else if (token_at.klass == TOK_SEMICOLON)
    lex_next_token(&token_at);
  else
    error("'{' or ';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  sym_add_error_kw();
  return result;
}

internal Ast_ControlTypeDecl*
syn_control_type_decl()
{
  assert (token_at.klass == TOK_KW_CONTROL);
  lex_next_token(&token_at);
  Ast_ControlTypeDecl* result = arena_push_struct(&arena, Ast_ControlTypeDecl);
  zero_struct(result, Ast_ControlTypeDecl);
  result->kind = AST_CONTROL_TYPE_DECL;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    result->type = sym_add_type(result->name);
    lex_next_token(&token_at);
    scope_push_level();
    if (token_at.klass == TOK_ANGLE_OPEN)
      syn_type_parameter_list();
    if (token_at.klass == TOK_PARENTH_OPEN)
    {
      lex_next_token(&token_at);
      if (token_is_parameter(&token_at))
        result->parameter = syn_parameter_list();
      if (token_at.klass == TOK_PARENTH_CLOSE)
        lex_next_token(&token_at);
      else
        error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    scope_pop_level();
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
    error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_BlockStmt*
syn_block_statement()
{
  assert (token_at.klass == TOK_BRACE_OPEN);
  lex_next_token(&token_at);
  Ast_BlockStmt* result = arena_push_struct(&arena, Ast_BlockStmt);
  zero_struct(result, Ast_BlockStmt);
  result->kind = AST_BLOCK_STMT;
  if (token_is_expression(&token_at))
    result->statement = syn_statement_list();
  if (token_at.klass == TOK_BRACE_CLOSE)
    lex_next_token(&token_at);
  else
    error("block stmt expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

bool
token_is_control_local_decl(Token* token)
{
  bool result = token->klass == TOK_KW_ACTION || token->klass == TOK_KW_TABLE ||
    token->klass == TOK_TYPE_IDENT;
  return result;
}

internal Ast_ActionDecl*
syn_action_decl()
{
  assert (token_at.klass == TOK_KW_ACTION);
  lex_next_token(&token_at);
  Ast_ActionDecl* result = arena_push_struct(&arena, Ast_ActionDecl);
  zero_struct(result, Ast_ActionDecl);
  result->kind = AST_ACTION_DECL;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    lex_next_token(&token_at);
    if (token_at.klass == TOK_PARENTH_OPEN)
    {
      lex_next_token(&token_at);
      if (token_is_parameter(&token_at))
        result->parameter = syn_parameter_list();
      if (token_at.klass == TOK_PARENTH_CLOSE)
        lex_next_token(&token_at);
      else
        error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    if (token_at.klass == TOK_BRACE_OPEN)
      result->action_body = syn_block_statement();
    else
      error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_Key*
syn_key_elem()
{
  assert (token_is_expression(&token_at));
  Ast_Key* result = arena_push_struct(&arena, Ast_Key);
  zero_struct(result, Ast_Key);
  result->kind = AST_TABLE_KEY;
  result->expression = syn_expression(1);
  if (token_at.klass == TOK_COLON)
  {
    lex_next_token(&token_at);
    if (token_at.klass == TOK_IDENT)
    {
      result->name = syn_expression(1);
      if (token_at.klass == TOK_SEMICOLON)
        lex_next_token(&token_at);
      else
        error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("':' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_SimpleProp*
syn_simple_prop()
{
  assert (token_is_expression(&token_at));
  Ast_SimpleProp* result = arena_push_struct(&arena, Ast_SimpleProp);
  zero_struct(result, Ast_SimpleProp);
  result->kind = AST_SIMPLE_PROP;
  result->expression = syn_expression(1);
  if (token_at.klass == TOK_SEMICOLON)
    lex_next_token(&token_at);
  else
    error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_ActionRef*
syn_action_ref()
{
  assert (token_at.klass == TOK_IDENT);
  Ast_ActionRef* result = arena_push_struct(&arena, Ast_ActionRef);
  zero_struct(result, Ast_ActionRef);
  result->kind = AST_ACTION_REF;
  result->name = token_at.lexeme;
  lex_next_token(&token_at);
  if (token_at.klass == TOK_PARENTH_OPEN)
  {
    lex_next_token(&token_at);
    if (token_is_expression(&token_at))
    {
      Ast_Expression* argument = syn_expression(1);
      result->argument = argument;
      while (token_at.klass == TOK_COMMA)
      {
        lex_next_token(&token_at);
        if (token_is_expression(&token_at))
        {
          Ast_Expression* next_argument = syn_expression(1);
          argument->next = next_argument;
          argument = next_argument;
        }
        else if (token_at.klass == TOK_COMMA)
          error("missing parameter at line %d", lex_line_nr());
        else
          error("expression term expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      }
    }
    if (token_at.klass == TOK_PARENTH_CLOSE)
      lex_next_token(&token_at);
    else
      error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);

  if (token_at.klass == TOK_SEMICOLON)
    lex_next_token(&token_at);
  else
    error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_TableProperty*
syn_table_property()
{
  assert (token_at.klass == TOK_IDENT);
  Ast_TableProperty* result = 0;
  if (cstr_match(token_at.lexeme, "key"))
  {
    lex_next_token(&token_at);
    if (token_at.klass == TOK_EQUAL)
    {
      lex_next_token(&token_at);
      if (token_at.klass == TOK_BRACE_OPEN)
      {
        lex_next_token(&token_at);
        if (token_is_expression(&token_at))
        {
          Ast_Key* key_elem = syn_key_elem();
          result = (Ast_TableProperty*)key_elem;
          while (token_is_expression(&token_at))
          {
            Ast_Key* next_key_elem = syn_key_elem();
            key_elem->next_key = next_key_elem;
            key_elem = next_key_elem;
          }
        }
        else
          error("expression term expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
        if (token_at.klass == TOK_BRACE_CLOSE)
          lex_next_token(&token_at);
        else
          error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      }
      else
        error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'=' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else if (cstr_match(token_at.lexeme, "actions"))
  {
    lex_next_token(&token_at);
    if (token_at.klass == TOK_EQUAL)
    {
      lex_next_token(&token_at);
      if (token_at.klass == TOK_BRACE_OPEN)
      {
        lex_next_token(&token_at);
        if (token_at.klass == TOK_IDENT)
        {
          Ast_ActionRef* action_ref = syn_action_ref();
          result = (Ast_TableProperty*)action_ref;
          while (token_at.klass == TOK_IDENT)
          {
            Ast_ActionRef* next_action_ref = syn_action_ref();
            action_ref->next_action = next_action_ref;
            action_ref = next_action_ref;
          }
        }
        if (token_at.klass == TOK_BRACE_CLOSE)
          lex_next_token(&token_at);
        else
          error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      }
      else
        error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'=' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
  {
    Ast_SimpleProp* prop = syn_simple_prop();
    result = (Ast_TableProperty*)prop;
    while (token_at.klass == TOK_IDENT)
    {
      Ast_SimpleProp* next_prop = syn_simple_prop();
      prop->next = (Ast_TableProperty*)next_prop;
      prop = next_prop;
    }
  }
  return result;
}

internal Ast_TableDecl*
syn_table_decl()
{
  assert (token_at.klass == TOK_KW_TABLE);
  lex_next_token(&token_at);
  Ast_TableDecl* result = arena_push_struct(&arena, Ast_TableDecl);
  zero_struct(result, Ast_TableDecl);
  result->kind = AST_TABLE_DECL;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    lex_next_token(&token_at);
    if (token_at.klass == TOK_BRACE_OPEN)
    {
      lex_next_token(&token_at);
      if (token_at.klass == TOK_IDENT)
      {
        Ast_TableProperty* property = syn_table_property();
        result->property = property;
        while (token_at.klass == TOK_IDENT)
        {
          Ast_TableProperty* next_property = syn_table_property();
          property->next = next_property;
          property = next_property;
        }
      }
      else
        error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      if (token_at.klass == TOK_BRACE_CLOSE)
        lex_next_token(&token_at);
      else
        error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_VarDecl*
syn_var_decl()
{
  assert (token_at.klass == TOK_TYPE_IDENT);
  Ast_VarDecl* result = arena_push_struct(&arena, Ast_VarDecl);
  zero_struct(result, Ast_VarDecl);
  result->kind = AST_VAR_DECL;
  syn_typeref();
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    lex_next_token(&token_at);
    if (token_at.klass == TOK_EQUAL)
    {
      lex_next_token(&token_at);
      if (token_is_expression(&token_at))
        result->initializer = syn_expression(1);
      else
        error("expression term expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }

    if (token_at.klass == TOK_SEMICOLON)
      lex_next_token(&token_at);
    else
      error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_Declaration*
syn_control_local_decl()
{
  assert (token_is_control_local_decl(&token_at));
  Ast_Declaration* result = 0;
  if (token_at.klass == TOK_KW_ACTION)
  {
    Ast_ActionDecl* action_decl = syn_action_decl();
    result = (Ast_Declaration*)action_decl;
  }
  else if (token_at.klass == TOK_KW_TABLE)
  {
    Ast_TableDecl* table_decl = syn_table_decl();
    result = (Ast_Declaration*)table_decl;
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
  {
    Ast_VarDecl* var_decl = syn_var_decl();
    result = (Ast_Declaration*)var_decl;
  }
  else
    assert (false);
  return result;
}

internal Ast_ControlDecl*
syn_control_decl()
{
  assert (token_at.klass == TOK_KW_CONTROL);
  sym_remove_error_kw();
  Ast_ControlDecl* result = arena_push_struct(&arena, Ast_ControlDecl);
  zero_struct(result, Ast_ControlDecl);
  result->kind = AST_CONTROL_DECL;
  result->type_decl = syn_control_type_decl();
  if (token_at.klass == TOK_BRACE_OPEN)
  {
    lex_next_token(&token_at);
    scope_push_level();
    sym_add_error_var();
    if (token_is_control_local_decl(&token_at))
    {
      Ast_Declaration* local_decl = syn_control_local_decl();
      result->local_decl = local_decl;
      result->local_decl_count++;
      while (token_is_control_local_decl(&token_at))
      {
        Ast_Declaration* next_local_decl = syn_control_local_decl(result);
        local_decl->next_decl = local_decl;
        local_decl = next_local_decl;
        result->local_decl_count++;
      }
    }
    if (token_at.klass == TOK_KW_APPLY)
    {
      lex_next_token(&token_at);
      if (token_at.klass == TOK_BRACE_OPEN)
        result->control_body = syn_block_statement();
      else
        error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      if (token_at.klass == TOK_BRACE_CLOSE)
        lex_next_token(&token_at);
      else
        error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    sym_remove_error_var();
    scope_pop_level();
  }
  else if (token_at.klass == TOK_SEMICOLON)
    lex_next_token(&token_at);
  else
    error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  sym_add_error_kw();
  return result;
}

internal Ast_PackageTypeDecl*
syn_package_type_decl()
{
  assert (token_at.klass == TOK_KW_PACKAGE);
  lex_next_token(&token_at);
  Ast_PackageTypeDecl* result = arena_push_struct(&arena, Ast_PackageTypeDecl);
  zero_struct(result, Ast_PackageTypeDecl);
  result->kind = AST_PACKAGE_TYPE_DECL;
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    result->type = sym_add_type(result->name);
    lex_next_token(&token_at);
    scope_push_level();
    if (token_at.klass == TOK_ANGLE_OPEN)
      syn_type_parameter_list();
    if (token_at.klass == TOK_PARENTH_OPEN)
    {
      lex_next_token(&token_at);
      if (token_is_parameter(&token_at))
        result->parameter = syn_parameter_list();
      if (token_at.klass == TOK_PARENTH_CLOSE)
        lex_next_token(&token_at);
      else
        error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    scope_pop_level();
  }
  else if (token_at.klass == TOK_TYPE_IDENT)
    error("at line %d: type '%s' has been previously declared", lex_line_nr(), token_at.lexeme);
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);

  if (token_at.klass == TOK_SEMICOLON)
    lex_next_token(&token_at);
  else
    error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_Instantiation*
syn_instantiation()
{
  assert (token_at.klass == TOK_TYPE_IDENT);
  Ast_Instantiation* result = arena_push_struct(&arena, Ast_Instantiation);
  zero_struct(result, Ast_Instantiation);
  result->kind = AST_INSTANTIATION;
  result->typeref = syn_typeref();
  if (token_at.klass == TOK_PARENTH_OPEN)
  {
    lex_next_token(&token_at);
    if (token_is_expression(&token_at))
    {
      Ast_Expression* argument = syn_expression(1);
      result->argument = argument;
      while (token_at.klass == TOK_COMMA)
      {
        lex_next_token(&token_at);
        if (token_is_expression(&token_at))
        {
          Ast_Expression* next_argument = syn_expression(1);
          argument->next = next_argument;
          argument = next_argument;
        }
        else if (token_at.klass == TOK_COMMA)
          error("missing parameter at line %d", lex_line_nr());
      }
    }
    if (token_at.klass == TOK_PARENTH_CLOSE)
      lex_next_token(&token_at);
    else
      error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    if (token_at.klass == TOK_IDENT)
    {
      result->name = token_at.lexeme;
      lex_next_token(&token_at);
      if (token_at.klass == TOK_SEMICOLON)
        lex_next_token(&token_at);
      else
        error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_FunctionPrototype*
syn_function_prototype_decl()
{
  assert (token_at.klass == TOK_TYPE_IDENT);
  Ast_FunctionPrototype* result = arena_push_struct(&arena, Ast_FunctionPrototype);
  zero_struct(result, Ast_FunctionPrototype);
  result->kind = AST_FUNCTION_PROTOTYPE_DECL;
  result->return_type = sym_get_type(token_at.lexeme);
  lex_next_token(&token_at);
  if (token_at.klass == TOK_IDENT)
  {
    result->name = token_at.lexeme;
    lex_next_token(&token_at);
    scope_push_level();
    if (token_at.klass == TOK_ANGLE_OPEN)
      syn_type_parameter_list();
    if (token_at.klass == TOK_PARENTH_OPEN)
    {
      lex_next_token(&token_at);
      if (token_is_parameter(&token_at))
        result->parameter = syn_parameter_list();
      if (token_at.klass == TOK_PARENTH_CLOSE)
        lex_next_token(&token_at);
      else
        error("')' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
      if (token_at.klass == TOK_SEMICOLON)
        lex_next_token(&token_at);
      else
        error("';' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    }
    else
      error("'(' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    scope_pop_level();
  }
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_ExternObjectDecl*
syn_extern_object_decl()
{
  assert (token_at.klass == TOK_IDENT);
  Ast_ExternObjectDecl* result = arena_push_struct(&arena, Ast_ExternObjectDecl);
  zero_struct(result, Ast_ExternObjectDecl);
  result->kind = AST_EXTERN_OBJECT_DECL;
  result->name = token_at.lexeme;
  result->type = sym_add_type(result->name);
  lex_next_token(&token_at);
  if (token_at.klass == TOK_ANGLE_OPEN)
    syn_type_parameter_list();
  if (token_at.klass == TOK_BRACE_OPEN)
  {
    scope_push_level();
    lex_next_token(&token_at);
    if (token_at.klass == TOK_TYPE_IDENT)
    {
      Ast_FunctionPrototype* method = syn_function_prototype_decl();
      result->method = method;
      result->method_count++;
      while (token_at.klass == TOK_TYPE_IDENT)
      {
        Ast_FunctionPrototype* next_method = syn_function_prototype_decl();
        method->next_decl = (Ast_Declaration*)next_method;
        method = next_method;
        result->method_count++;
      }
    }
    else
      error("at line %d: unknown type '%s'", lex_line_nr(), token_at.lexeme);
    if (token_at.klass == TOK_BRACE_CLOSE)
      lex_next_token(&token_at);
    else
      error("'}' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
    scope_pop_level();
  }
  else
    error("'{' expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal Ast_ExternFunctionDecl*
syn_extern_function_decl()
{
  assert (token_at.klass == TOK_TYPE_IDENT);
  Ast_ExternFunctionDecl* result = (Ast_ExternFunctionDecl*)syn_function_prototype_decl();
  return result;
}

internal Ast_Declaration*
syn_extern_decl()
{
  assert (token_at.klass == TOK_KW_EXTERN);
  sym_remove_error_kw();
  Ast_Declaration* result = 0;
  lex_next_token(&token_at);
  if (token_at.klass == TOK_IDENT)
    result = (Ast_Declaration*)syn_extern_object_decl();
  else if (token_at.klass == TOK_TYPE_IDENT)
    result = (Ast_Declaration*)syn_extern_function_decl();
  else
    error("identifier expected at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  sym_add_error_kw();
  return result;
}

internal Ast_Declaration*
syn_declaration()
{
  Ast_Declaration* result = 0;
  assert (token_is_declaration(&token_at));
  if (token_at.klass == TOK_KW_STRUCT)
    result = (Ast_Declaration*)syn_struct_type_decl();
  else if (token_at.klass == TOK_KW_HEADER)
    result = (Ast_Declaration*)syn_header_type_decl();
  else if (token_at.klass == TOK_KW_ERROR)
    result = (Ast_Declaration*)syn_error_type_decl();
  else if (token_at.klass == TOK_KW_TYPEDEF)
    result = (Ast_Declaration*)syn_typedef_decl();
  else if (token_at.klass == TOK_KW_PARSER)
    result = (Ast_Declaration*)syn_parser_decl();
  else if (token_at.klass == TOK_KW_CONTROL)
    result = (Ast_Declaration*)syn_control_decl();
  else if (token_at.klass == TOK_KW_ACTION)
    result = (Ast_Declaration*)syn_action_decl();
  else if (token_at.klass == TOK_KW_PACKAGE)
    result = (Ast_Declaration*)syn_package_type_decl();
  else if (token_at.klass == TOK_KW_EXTERN)
    result = (Ast_Declaration*)syn_extern_decl();
  else if (token_at.klass == TOK_TYPE_IDENT)
    result = (Ast_Declaration*)syn_instantiation();
  else
    assert (false);
  return result;
}

internal Ast_P4Program*
syn_p4program()
{
  Ast_P4Program* result = 0;
  if (token_is_declaration(&token_at))
  {
    result = arena_push_struct(&arena, Ast_P4Program);
    zero_struct(result, Ast_P4Program);
    result->kind = AST_P4PROGRAM;
    Ast_Declaration* declaration = syn_declaration();
    result->declaration = declaration;
    result->decl_count++;
    while (token_is_declaration(&token_at))
    {
      Ast_Declaration* next_declaration = syn_declaration();
      declaration->next_decl = next_declaration;
      declaration = next_declaration;
      result->decl_count++;
    }
    if (token_at.klass != TOK_EOI)
      error("expected end of input at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  }
  else
    error("expected a declaration at line %d, got '%s'", lex_line_nr(), token_at.lexeme);
  return result;
}

internal void
read_input(char* filename)
{
  FILE* f_stream = fopen(filename, "rb");
  fseek(f_stream, 0, SEEK_END);
  input_size = ftell(f_stream);
  fseek(f_stream, 0, SEEK_SET);
  input_text = arena_push_array(&arena, char, input_size + 1);
  fread(input_text, sizeof(char), input_size, f_stream);
  input_text[input_size] = '\0';
  fclose(f_stream);
  arena_print_usage(&arena, "Arena (read_input): ");
}

int
main(int arg_count, char* args[])
{
  arena_new(&arena, 128*KILOBYTE);

  symtab = arena_push_array(&arena, NamespaceInfo*, symtab_len);
  int i = 0;
  while (i < symtab_len)
    symtab[i++] = 0;
  typetable = arena_push_array(&arena, uint64_t, typetable_len);

  read_input("test.p4");
  sym_init();
  lex_lexeme_init(input_text);

  scope_push_level();
  lex_next_token(&token_at);
  p4program = syn_p4program();
  scope_pop_level();
  arena_print_usage(&arena, "Arena (main): ");

  return 0;
}

