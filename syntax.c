#include "dp4c.h"
#include "lex.h"
#include "syntax.h"

external Arena arena;

external Token* tokenized_input;
internal Token* token = 0;
internal Token* prev_token = 0;
external int tokenized_input_len;

external Namespace_Entry** symtable;
external int max_symtable_len;
external int scope_level;

external Ast_P4Program* p4program;

Ident_Keyword* error_kw = 0;
Ident_Keyword* apply_kw = 0;

Ast_TypeIdent* error_type_ast = 0;
Ident* error_type_ident = 0;

Ast_TypeIdent* void_type_ast = 0;
Ident* void_type_ident = 0;

Ast_TypeIdent* bool_type_ast = 0;
Ident* bool_type_ident = 0;

Ast_TypeIdent* bit_type_ast = 0;
Ident* bit_type_ident = 0;

Ast_TypeIdent* varbit_type_ast = 0;
Ident* varbit_type_ident;

Ast_TypeIdent* int_type_ast = 0;
Ident* int_type_ident;

Ast_TypeIdent* string_type_ast = 0;
Ident* string_type_ident;

Ast_Integer* bool_true_ast = 0;
Ident* bool_true_ident = 0;
Ast_Integer* bool_false_ast = 0;
Ident* bool_false_ident = 0;

internal Ast_TypeExpression* build_type_expression();
internal Ast* build_expression(int priority_threshold);
internal Ast_BlockStmt* build_block_statement();

internal uint32_t
name_hash(char* name)
{
  uint32_t result = 0;
  uint32_t sum = 0;
  char* c = name;
  while (*c)
    sum += (uint32_t)(*c++);
  result = sum % max_symtable_len;
  return result;
}

int
scope_push_level()
{
  int new_scope_level = ++scope_level;
  printf("push scope %d\n", new_scope_level);
  return new_scope_level;
}

int
scope_pop_level(int to_level)
{
  if (scope_level <= to_level)
    return scope_level;

  int i = 0;
  while (i < max_symtable_len)
  {
    Namespace_Entry* ns = symtable[i];
    while (ns)
    {
      Ident* ident = ns->ns_global;
      if (ident && ident->scope_level > to_level)
      {
        ns->ns_global = ident->next_in_scope;
        if (ident->next_in_scope)
          assert (ident->next_in_scope->scope_level <= to_level);
        ident->next_in_scope = 0;
      }
      ident = ns->ns_type;
      if (ident && ident->scope_level > to_level)
      {
        ns->ns_type = ident->next_in_scope;
        if (ident->next_in_scope)
          assert (ident->next_in_scope->scope_level <= to_level);
        ident->next_in_scope = 0;
      }
      ns = ns->next;
    }
    i++;
  }
  printf("pop scope %d\n", to_level);
  scope_level = to_level;
  return scope_level;
}

bool
sym_ident_is_declared(Ident* ident)
{
  bool is_declared = (ident && ident->scope_level >= scope_level);
  return is_declared;
}

internal Namespace_Entry*
sym_get_namespace(char* name)
{
  uint32_t h = name_hash(name);
  Namespace_Entry* name_info = symtable[h];
  while(name_info)
  {
    if (cstr_match(name_info->name, name))
      break;
    name_info = name_info->next;
  }
  if (!name_info)
  {
    name_info = arena_push_struct(&arena, Namespace_Entry);
    name_info->name = name;
    name_info->next = symtable[h];
    symtable[h] = name_info;
  }
  return name_info;
}

Ident*
sym_get_var(char* name)
{
  Namespace_Entry* ns = sym_get_namespace(name);
  Ident* ident_var = (Ident*)ns->ns_global;
  if (ident_var)
    assert (ident_var->ident_kind == ID_VAR);
  return ident_var;
}

Ident*
sym_new_var(char* name, Ast* ast)
{
  Namespace_Entry* ns = sym_get_namespace(name);
  Ident* ident = arena_push_struct(&arena, Ident);
  ident->ast = ast;
  ident->name = name;
  ident->scope_level = scope_level;
  ident->ident_kind = ID_VAR;
  ident->next_in_scope = ns->ns_global;
  ns->ns_global = (Ident*)ident;
  printf("new var '%s'\n", ident->name);
  return ident;
}

void
sym_import_var(Ident* var_ident)
{
  Namespace_Entry* ns = sym_get_namespace(var_ident->name);

  if (ns->ns_global)
  {
    assert (ns->ns_global == (Ident*)var_ident);
    return;
  }

  var_ident->next_in_scope = ns->ns_global;
  ns->ns_global = (Ident*)var_ident;
}

void
sym_unimport_var(Ident* var_ident)
{
  Namespace_Entry* ns = sym_get_namespace(var_ident->name);

  if (!ns->ns_global)
    return;

  assert (ns->ns_global == (Ident*)var_ident);
  ns->ns_global = ns->ns_global->next_in_scope;
}

Ident*
sym_get_type(char* name)
{
  Namespace_Entry* ns = sym_get_namespace(name);
  Ident* result = (Ident*)ns->ns_type;
  if (result)
    assert (result->ident_kind == ID_TYPE || result->ident_kind == ID_TYPEVAR);
  return result;
}

Ident*
sym_new_type(char* name, Ast* ast)
{
  Namespace_Entry* ns = sym_get_namespace(name);
  Ident* ident = arena_push_struct(&arena, Ident);
  ident->ast = ast;
  ident->name = name;
  ident->scope_level = scope_level;
  ident->ident_kind = ID_TYPE;
  ident->next_in_scope = ns->ns_type;
  ns->ns_type = (Ident*)ident;
  printf("new type '%s'\n", ident->name);
  return ident;
}

Ident*
sym_new_typevar(char* name, Ast* ast)
{
  Namespace_Entry* ns = sym_get_namespace(name);
  Ident* ident = arena_push_struct(&arena, Ident);
  ident->ast = ast;
  ident->name = name;
  ident->scope_level = scope_level;
  ident->ident_kind = ID_TYPEVAR;
  ident->next_in_scope = ns->ns_type;
  ns->ns_type = (Ident*)ident;
  printf("new typevar '%s'\n", ident->name);
  return ident;
}

void
sym_import_type(Ident* type_ident)
{
  Namespace_Entry* ns = sym_get_namespace(type_ident->name);

  if (ns->ns_type)
  {
    assert (ns->ns_type == (Ident*)type_ident);
    return;
  }

  type_ident->next_in_scope = ns->ns_type;
  ns->ns_type = (Ident*)type_ident;
}

void
sym_unimport_type(Ident* type_ident)
{
  Namespace_Entry* ns = sym_get_namespace(type_ident->name);

  if (!ns->ns_type)
    return;

  assert (ns->ns_type == (Ident*)type_ident);
  ns->ns_type = ns->ns_type->next_in_scope;
}

internal Ident_Keyword*
add_keyword(char* name, enum TokenClass token_klass)
{
  Namespace_Entry* namespace = sym_get_namespace(name);
  assert (namespace->ns_global == 0);
  Ident_Keyword* ident = arena_push_struct(&arena, Ident_Keyword);
  ident->name = name;
  ident->scope_level = scope_level;
  ident->token_klass = token_klass;
  ident->ident_kind = ID_KEYWORD;
  namespace->ns_global = (Ident*)ident;
  return ident;
}

internal Token*
next_token()
{
  assert(token < tokenized_input + tokenized_input_len);
  prev_token = token++;
  while (token->klass == TOK_COMMENT)
    token++;

  if (token->klass == TOK_IDENT)
  {
    Namespace_Entry* ns = sym_get_namespace(token->lexeme);
    if (ns->ns_global)
    {
      Ident* ident = ns->ns_global;
      if (ident->ident_kind == ID_KEYWORD)
      {
        token->klass = ((Ident_Keyword*)ident)->token_klass;
        return token;
      }
    }

    /*
    if (ns->ns_type)
    {
      Ident* ident = ns->ns_type;
      if (ident->ident_kind == ID_TYPE || ident->ident_kind == ID_TYPEVAR)
      {
        token->klass = TOK_TYPE_IDENT;
        return token;
      }
    }
    */
  }
  return token;
}

internal Token*
peek_token()
{
  prev_token = token;
  Token* peek_token = next_token();
  token = prev_token;
  return peek_token;
}

internal void
copy_tokenattr_to_ast(Token* token, Ast* ast)
{
  ast->line_nr = token->line_nr;
  ast->lexeme = token->lexeme;
}

internal void
expect_semicolon()
{
  if (token->klass == TOK_SEMICOLON)
    next_token();
  else
    error("at line %d: ';' expected, got '%s'", token->line_nr, token->lexeme);
}

internal bool
token_is_ident(Token* token)
{
  bool result = token->klass == TOK_IDENT || token->klass == TOK_TYPE_IDENT;
  return result;
}

internal bool
token_is_type_parameter(Token* token)
{
  bool result = token_is_ident(token) || token->klass == TOK_INTEGER;
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
  bool result = token_is_direction(token) || token_is_ident(token) || token->klass == TOK_INTEGER;
  return result;
}

internal Ast_TypeExpression*
build_type_parameter_list()
{
  assert(token->klass == TOK_ANGLE_OPEN);

  Ast_TypeExpression* result = 0;

  next_token();

  if (token_is_type_parameter(token))
  {
    Ast_TypeExpression* argument = build_type_expression();
    result = argument;

    while (token->klass == TOK_COMMA)
    {
      next_token();

      if (token_is_type_parameter(token))
      {
        Ast_TypeExpression* next_argument = build_type_expression();
        argument->next_argument = next_argument;
        argument = next_argument;
      }
      else if (token->klass == TOK_COMMA)
        error("at line %d: missing type argument", token->line_nr);
      else
        error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
    }
  }

  if (token->klass == TOK_ANGLE_CLOSE)
    next_token();
  else
    error("at line %d: '>' expected, got '%s'", token->line_nr, token->lexeme);

  return result;
}

internal Ast_TypeExpression*
build_type_expression()
{
  assert(token_is_type_parameter(token));

  Ast_TypeExpression* result = arena_push_struct(&arena, Ast_TypeExpression);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_TYPE_EXPRESSION;

  if (token_is_ident(token))
  {
    result->argument_kind = AST_TYPPARAM_VAR;
    result->name = token->lexeme;
    next_token();
  }
  else if (token->klass == TOK_INTEGER)
  {
    result->argument_kind = AST_TYPPARAM_INT;

    Ast* size_ast = build_expression(1);
    result->type_ast = size_ast;
    if (size_ast->kind != AST_INTEGER)
      error("at line %d: type width must be a integer literal, got '%s'", size_ast->line_nr, size_ast->lexeme);
  }
  else
    assert(false);

  if (token->klass == TOK_ANGLE_OPEN)
    result->first_type_argument = build_type_parameter_list();

  return result;
}

internal Ast_StructField*
build_struct_field()
{
  assert(token_is_ident(token));
  Ast_StructField* result = arena_push_struct(&arena, Ast_StructField);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_STRUCT_FIELD;
  result->member_type = build_type_expression();

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();
    expect_semicolon();
  }
  else
    error("at line %d: non-type identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal bool
token_is_p4declaration(Token* token)
{
  bool result = token->klass == TOK_KW_STRUCT || token->klass == TOK_KW_HEADER \
      || token->klass == TOK_KW_ERROR || token->klass == TOK_KW_TYPEDEF \
      || token->klass == TOK_KW_PARSER || token->klass == TOK_KW_CONTROL \
      || token_is_ident(token) || token->klass == TOK_KW_EXTERN \
      || token->klass == TOK_KW_CONST || token->klass == TOK_KW_PACKAGE;
  return result;
}

internal Ast_HeaderDecl*
build_header_decl()
{
  Ast_StructField* field;

  assert(token->klass == TOK_KW_HEADER);
  next_token();
  Ast_HeaderDecl* result = arena_push_struct(&arena, Ast_HeaderDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_HEADER_PROTOTYPE;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      result->first_type_parameter = build_type_parameter_list();

    if (token->klass == TOK_BRACE_OPEN)
    {
      result->kind = AST_HEADER_DECL;
      next_token();

      if (token_is_ident(token))
      {
        field = build_struct_field();
        result->first_field = field;
        while (token_is_ident(token))
        {
          Ast_StructField* next_field = build_struct_field();
          field->next_field = next_field;
          field = next_field;
        }
      }

      if (token->klass == TOK_BRACE_CLOSE)
        next_token(token);
      else if (token->klass == TOK_IDENT)
        error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
      else
        error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_StructDecl*
build_struct_decl()
{
  assert(token->klass == TOK_KW_STRUCT);
  next_token();
  Ast_StructDecl* result = arena_push_struct(&arena, Ast_StructDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_STRUCT_PROTOTYPE;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;

    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      result->first_type_parameter = build_type_parameter_list();

    if (token->klass == TOK_BRACE_OPEN)
    {
      result->kind = AST_STRUCT_DECL;

      next_token();

      if (token_is_ident(token))
      {
        Ast_StructField* field = build_struct_field();
        result->first_field = field;
        while (token_is_ident(token))
        {
          Ast_StructField* next_field = build_struct_field();
          field->next_field = next_field;
          field = next_field;
        }
      }

      if (token->klass == TOK_BRACE_CLOSE)
        next_token();
      else if (token->klass == TOK_IDENT)
        error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
      else
        error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);

  return result;
}

internal Ast_ErrorCode*
build_error_code()
{
  assert(token_is_ident(token));

  Ast_ErrorCode* code = arena_push_struct(&arena, Ast_ErrorCode);
  copy_tokenattr_to_ast(token, (Ast*)code);
  code->kind = AST_ERROR_CODE;
  code->name = token->lexeme;
  next_token();
  return code;
}

internal Ast_ErrorType*
build_error_type_decl()
{
  assert(token->klass == TOK_KW_ERROR);
  next_token();
  Ast_ErrorType* result = arena_push_struct(&arena, Ast_ErrorType);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_ERROR_TYPE;
  result->line_nr = token->line_nr;

  if (token->klass == TOK_BRACE_OPEN)
  {
    next_token();

    if (token_is_ident(token))
    {
      Ast_ErrorCode* field = build_error_code();
      result->error_code = field;
      while (token->klass == TOK_COMMA)
      {
        next_token();
        if (token_is_ident(token))
        {
          Ast_ErrorCode* next_code = build_error_code();
          field->next_code = next_code;
          field = next_code;
        }
        else if (token->klass == TOK_COMMA)
          error("at line %d: missing parameter", token->line_nr);
        else
          error("at line %d: non-type identifier expected, got '%s'", token->line_nr, token->lexeme);
      }
    }

    if (token->klass == TOK_BRACE_CLOSE)
      next_token();
    else
      error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_Typedef*
build_typedef_decl()
{
  assert(token->klass == TOK_KW_TYPEDEF);

  next_token();

  Ast_Typedef* result = arena_push_struct(&arena, Ast_Typedef);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_TYPEDEF;

  if (token_is_type_parameter(token))
  {
    result->type = build_type_expression();

    if (token_is_ident(token))
    {
      result->name = token->lexeme;
      next_token();
    }
    else
      error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d : unexpected token '%s'", token->line_nr, token->lexeme);

  expect_semicolon();

  return result;
}

internal enum Ast_ParameterDirection
build_direction()
{
  assert(token_is_direction(token));
  enum Ast_ParameterDirection result = 0;
  if (token->klass == TOK_KW_IN)
    result = AST_DIR_IN;
  else if (token->klass == TOK_KW_OUT)
    result = AST_DIR_OUT;
  else if (token->klass == TOK_KW_INOUT)
    result = AST_DIR_INOUT;
  next_token();
  return result;
}

internal Ast_Parameter*
build_parameter()
{
  assert(token_is_parameter(token));

  Ast_Parameter* result = arena_push_struct(&arena, Ast_Parameter);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_PARAMETER;

  if (token_is_direction(token))
    result->direction = build_direction();

  if (token_is_ident(token))
    result->param_type = build_type_expression();
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_Parameter*
build_parameter_list()
{
  assert(token_is_parameter(token));

  Ast_Parameter* parameter = build_parameter();
  Ast_Parameter* result = parameter;

  while (token->klass == TOK_COMMA)
  {
    next_token();
    if (token_is_parameter(token))
    {
      Ast_Parameter* next_parameter = build_parameter();
      parameter->next_parameter = next_parameter;
      parameter = next_parameter;
    }
    else if (token->klass == TOK_COMMA)
      error("at line %d: missing parameter", token->line_nr);
    else
      error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
  }
  return result;
}

internal Ast_ParserDecl*
build_parser_prototype()
{
  assert(token->klass == TOK_KW_PARSER);
  next_token();
  Ast_ParserDecl* result = arena_push_struct(&arena, Ast_ParserDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_PARSER_PROTOTYPE;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      result->first_type_parameter = build_type_parameter_list();

    if (token->klass == TOK_PARENTH_OPEN)
    {
      next_token();
      if (token_is_parameter(token))
        result->first_parameter = build_parameter_list();

      if (token->klass == TOK_PARENTH_CLOSE)
        next_token();
      else
      {
        if (token->klass == TOK_IDENT)
          error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
        else
          error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);
      }
    }
    else
      error("at line %d: '(' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
      error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
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
  bool result = token_is_ident(token) \
    || token_is_integer(token) || token_is_winteger(token) || token_is_sinteger(token) \
    || token->klass == TOK_STRING || token->klass == TOK_PARENTH_OPEN;
  return result;
}

internal bool
token_is_expression_operator(Token* token)
{
  bool result = token->klass == TOK_PERIOD || token->klass == TOK_EQUAL_EQUAL
    || token->klass == TOK_PARENTH_OPEN || token->klass == TOK_EQUAL
    || token->klass == TOK_MINUS || token->klass == TOK_PLUS;
  return result;
}

internal Ast*
build_expression_primary()
{
  Ast* result = 0;

  assert(token_is_expression(token));

  if (token->klass == TOK_IDENT)
  {
    Ast_Ident* ident_ast = arena_push_struct(&arena, Ast_Ident);
    copy_tokenattr_to_ast(token, (Ast*)ident_ast);
    ident_ast->kind = AST_IDENT;
    ident_ast->name = token->lexeme;
    result = (Ast*)ident_ast;
    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      ident_ast->first_type_argument = build_type_parameter_list();
  }
  else if (token->klass == TOK_TYPE_IDENT)
  {
    Ast_TypeIdent* ident_ast = arena_push_struct(&arena, Ast_TypeIdent);
    copy_tokenattr_to_ast(token, (Ast*)ident_ast);
    ident_ast->kind = AST_TYPE_IDENT;
    ident_ast->name = token->lexeme;
    result = (Ast*)ident_ast;
    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      ident_ast->first_type_argument = build_type_parameter_list();
  }
  else if (token_is_integer(token))
  {
    Ast_Integer* expression = arena_push_struct(&arena, Ast_Integer);
    copy_tokenattr_to_ast(token, (Ast*)expression);
    expression->kind = AST_INTEGER;
    expression->lexeme = token->lexeme;
    expression->value = 0; //TODO

    result = (Ast*)expression;
    next_token();
  }
  else if (token_is_winteger(token))
  {
    Ast_WInteger* expression = arena_push_struct(&arena, Ast_WInteger);
    copy_tokenattr_to_ast(token, (Ast*)expression);
    expression->kind = AST_WINTEGER;
    expression->lexeme = token->lexeme;
    expression->value = 0; //TODO

    result = (Ast*)expression;
    next_token();
  }
  else if (token_is_sinteger(token))
  {
    Ast_SInteger* expression = arena_push_struct(&arena, Ast_SInteger);
    copy_tokenattr_to_ast(token, (Ast*)expression);
    expression->kind = AST_SINTEGER;
    expression->lexeme = token->lexeme;
    expression->value = 0; //TODO

    result = (Ast*)expression;
    next_token();
  }
  else if (token->klass == TOK_PARENTH_OPEN)
  {
    next_token();

    if (token_is_expression(token))
      result = build_expression(1);
    if (token->klass == TOK_PARENTH_CLOSE)
      next_token();
    else
      error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    assert(false);
  return result;
}

internal enum Ast_ExprOperator
build_expression_operator()
{
  assert(token_is_expression_operator(token));
  enum Ast_ExprOperator result = 0;

  switch (token->klass)
  {
    case TOK_PERIOD:
      result = AST_OP_MEMBER_SELECTOR;
      break;
    case TOK_EQUAL:
      result = AST_OP_ASSIGN;
      break;
    case TOK_EQUAL_EQUAL:
      result = AST_OP_LOGIC_EQUAL;
      break;
    case TOK_PARENTH_OPEN:
      result = AST_OP_FUNCTION_CALL;
      break;
    case TOK_MINUS:
      result = AST_OP_SUBTRACT;
      break;
    case TOK_PLUS:
      result = AST_OP_ADDITION;
      break;

    default: assert(false);
  }

  return result;
}

internal int
op_get_priority(enum Ast_ExprOperator op)
{
  int result = 0;

  switch (op)
  {
    case AST_OP_ASSIGN:
      result = 1;
      break;
    case AST_OP_LOGIC_EQUAL:
      result = 2;
      break;
    case AST_OP_ADDITION:
    case AST_OP_SUBTRACT:
      result = 3;
      break;
    case AST_OP_FUNCTION_CALL:
      result = 4;
      break;
    case AST_OP_MEMBER_SELECTOR:
      result = 5;
      break;

    default: assert(false);
  }

  return result;
}

internal bool
op_is_binary(enum Ast_ExprOperator op)
{
  bool result = op == AST_OP_LOGIC_EQUAL || op == AST_OP_MEMBER_SELECTOR || op == AST_OP_ASSIGN
    || op == AST_OP_ADDITION || op == AST_OP_SUBTRACT;
  return result;
}

internal Ast*
build_expression(int priority_threshold)
{
  assert(token_is_expression(token));

  Ast* expr = build_expression_primary();

  while (token_is_expression_operator(token))
  {
    enum Ast_ExprOperator op = build_expression_operator();
    int priority = op_get_priority(op);

    if (priority >= priority_threshold)
    {
      next_token();

      if (op_is_binary(op))
      {
        Ast_BinaryExpr* binary_expr = arena_push_struct(&arena, Ast_BinaryExpr);
        copy_tokenattr_to_ast(token, (Ast*)binary_expr);
        binary_expr->kind = AST_BINARY_EXPR;
        binary_expr->l_operand = expr;
        binary_expr->op = op;

        if (token_is_expression(token))
          binary_expr->r_operand = build_expression(priority_threshold + 1);
        else
          error("at line %d: expression term expected, got '%s'", token->line_nr, token->lexeme);
        expr = (Ast*)binary_expr;
      }
      else if (op == AST_OP_FUNCTION_CALL)
      {
        Ast_FunctionCall* function_call = arena_push_struct(&arena, Ast_FunctionCall);
        copy_tokenattr_to_ast(token, (Ast*)function_call);
        function_call->kind = AST_FUNCTION_CALL;
        function_call->function = expr;

        if (token_is_expression(token))
        {
          Ast_Declaration* argument = (Ast_Declaration*)build_expression(1);
          function_call->first_argument = argument;
          while (token->klass == TOK_COMMA)
          {
            next_token();
            if (token_is_expression(token))
            {
              Ast_Declaration* next_argument = (Ast_Declaration*)build_expression(1);
              argument->next_decl = next_argument;
              argument = next_argument;
            }
            else if (token->klass == TOK_COMMA)
              error("at line %d: missing argument", token->line_nr);
            else
              error("at line %d: expression term expected, got '%s'", token->line_nr, token->lexeme);
          }
        }

        if (token->klass == TOK_PARENTH_CLOSE)
          next_token();
        else
          error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);

        expr = (Ast*)function_call;
      }
      else assert(false);
    }
    else break;
  }
  return expr;
}

internal Ast_VarDecl*
build_var_decl()
{
  Ast_Ident* name_ast = 0;
  Ast* init_expr = 0;

  assert(token->klass == TOK_KW_VAR || token->klass == TOK_KW_CONST);
  next_token();
  Ast_VarDecl* var_decl = arena_push_struct(&arena, Ast_VarDecl);
  copy_tokenattr_to_ast(token, (Ast*)var_decl);
  var_decl->kind = AST_VAR_DECL;
  var_decl->var_type = build_type_expression();
  Token* name_token = token;

  if (token_is_expression(token))
  {
    init_expr = build_expression(1);
    if (!init_expr)
      error("at line %d: identifier expected, got '%s'", name_token->line_nr, name_token->lexeme);
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);

  if (init_expr->kind == AST_BINARY_EXPR)
  {
    var_decl->init_expr = ((Ast_BinaryExpr*)init_expr)->r_operand;
    init_expr = ((Ast_BinaryExpr*)init_expr)->l_operand;
  }

  if (init_expr->kind == AST_IDENT)
  {
    name_ast = (Ast_Ident*)init_expr;
    var_decl->name = name_ast;
  }
  else
    error("at line %d: identifier expected", init_expr->line_nr);
  expect_semicolon();
  return var_decl;
}

internal Ast_VarDecl*
build_const_decl()
{
  assert (token->klass == TOK_KW_CONST);

  Ast_VarDecl* const_decl = build_var_decl();
  const_decl->is_const = true;

  return const_decl;
}

internal Ast_IdentState*
build_ident_state()
{
  assert(token_is_ident(token));

  Ast_IdentState* result = arena_push_struct(&arena, Ast_IdentState);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_IDENT_STATE;
  result->name = token->lexeme;

  next_token();
  expect_semicolon();

  return result;
}

internal bool
token_is_select_case(Token* token)
{
  bool result = token_is_expression(token) || token->klass == TOK_KW_DEFAULT;
  return result;
}

internal Ast_SelectCase*
build_select_case()
{
  assert(token_is_select_case(token));
  Ast_SelectCase* result = 0;

  if (token_is_expression(token))
  {
    Ast_SelectCase_Expr* select_expr = arena_push_struct(&arena, Ast_SelectCase_Expr);
    copy_tokenattr_to_ast(token, (Ast*)select_expr);
    select_expr->kind = AST_SELECT_CASE_EXPR;
    select_expr->key_expr = build_expression(1);
    result = (Ast_SelectCase*)select_expr;
  }
  else if (token->klass = TOK_KW_DEFAULT)
  {
    next_token();

    Ast_SelectCase_Default* default_select = arena_push_struct(&arena, Ast_SelectCase_Default);
    copy_tokenattr_to_ast(token, (Ast*)default_select);
    default_select->kind = AST_DEFAULT_SELECT_CASE;
    result = (Ast_SelectCase*)default_select;
  }
  else
    assert(false);

  if (token->klass == TOK_COLON)
  {
    next_token();

    if (token_is_ident(token))
    {
      result->end_state = token->lexeme;
      next_token();
      expect_semicolon();
    }
    else
      error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: ':' expected, got '%s'", token->line_nr, token->lexeme);

  return result;
}

internal Ast_SelectCase*
build_select_case_list()
{
  assert(token_is_select_case(token));
  Ast_SelectCase* select_case = build_select_case();
  Ast_SelectCase* result = select_case;
  while (token_is_select_case(token))
  {
    Ast_SelectCase* next_select_case = build_select_case();
    select_case->next = next_select_case;
    select_case = next_select_case;
  }
  return result;
}

internal Ast_SelectState*
build_select_state()
{
  assert(token->klass == TOK_KW_SELECT);

  next_token();

  Ast_SelectState* result = arena_push_struct(&arena, Ast_SelectState);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_SELECT_STATE;

  if (token->klass == TOK_PARENTH_OPEN)
  {
    next_token();
    if (token_is_expression(token))
      result->expression = build_expression(1);
    if (token->klass == TOK_PARENTH_CLOSE)
      next_token();
    else
      error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: '(' expected, got '%s'", token->line_nr, token->lexeme);

  if (token->klass == TOK_BRACE_OPEN)
  {
    next_token();
    result->select_case = build_select_case_list();
    if (token->klass == TOK_BRACE_CLOSE)
      next_token();
    else
      error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_TransitionStmt*
build_transition_stmt()
{
  assert(token->klass == TOK_KW_TRANSITION);

  Ast_TransitionStmt* result = arena_push_struct(&arena, Ast_TransitionStmt);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_TRANSITION_STMT;

  next_token();
  if (token_is_ident(token))
    result->state_expr = (Ast*)build_ident_state();
  else if (token->klass == TOK_KW_SELECT)
    result->state_expr = (Ast*)build_select_state();
  else
    error("at line %d: transition stmt expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal bool
token_is_statement(Token* token)
{
  bool result = token_is_expression(token) || token->klass == TOK_KW_VAR \
                || token->klass == TOK_BRACE_OPEN;
  return result;
}

internal Ast_Declaration*
build_statement()
{
  Ast_Declaration* result = 0;

  assert (token_is_statement(token));

  if (token_is_expression(token))
  {
    result = (Ast_Declaration*)build_expression(1);
    expect_semicolon();
  }
  else if (token->klass == TOK_KW_VAR)
    result = (Ast_Declaration*)build_var_decl();
  else if (token->klass == TOK_BRACE_OPEN)
    result = (Ast_Declaration*)build_block_statement();
  else
    assert(false);

  return result;
}

internal Ast_Declaration*
build_statement_list()
{
  Ast_Declaration* result = 0;

  if (token_is_statement(token))
  {
    Ast_Declaration* expression = (Ast_Declaration*)build_statement();
    result = expression;

    while (token_is_statement(token))
    {
      Ast_Declaration* next_expression = (Ast_Declaration*)build_statement();
      expression->next_decl = next_expression;
      expression = next_expression;
    }
  }

  return result;
}

internal Ast_ParserState*
build_parser_state()
{
  assert(token->klass == TOK_KW_STATE);
  next_token();
  Ast_ParserState* result = arena_push_struct(&arena, Ast_ParserState);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_PARSER_STATE;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    sym_unimport_var((Ident*)apply_kw);
    next_token();

    if (token->klass == TOK_BRACE_OPEN)
    {
      next_token();
      result->first_statement = build_statement_list();

      if (token->klass == TOK_KW_TRANSITION)
        result->transition_stmt = build_transition_stmt();
      else
        error("at line %d: 'transition' expected, got '%s'", token->line_nr, token->lexeme);

      if (token->klass == TOK_BRACE_CLOSE)
        next_token();
      else
        error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);

    sym_import_var((Ident*)apply_kw);
  }
  return result;
}

internal bool
token_is_parser_local_decl(Token* token)
{
  bool result = token->klass == TOK_KW_STATE || token->klass == TOK_KW_VAR \
                || token_is_ident(token);
  return result;
}

internal Ast_Declaration*
build_parser_local_decl()
{
  Ast_Declaration* result = 0;

  assert(token_is_parser_local_decl(token));
  
  if (token->klass == TOK_KW_STATE)
  {
    Ast_ParserState* state_decl = build_parser_state();
    result = (Ast_Declaration*)state_decl;
  }
  else if (token->klass == TOK_KW_VAR)
  {
    Ast_VarDecl* var_decl = build_var_decl();
    result = (Ast_Declaration*)var_decl;
  }
  else if (token_is_ident(token))
  {
    Ast_Declaration* expression = (Ast_Declaration*)build_expression(1);
    result = (Ast_Declaration*)expression;

    expect_semicolon();
  }
  else
    assert(false);

  return result;
}

internal Ast_ParserDecl*
build_parser_decl()
{
  assert(token->klass == TOK_KW_PARSER);

  Ast_ParserDecl* result = build_parser_prototype();

  if (token->klass == TOK_BRACE_OPEN)
  {
    result->kind = AST_PARSER_DECL;
    sym_unimport_var((Ident*)error_kw);
    next_token();

    if (token_is_parser_local_decl(token))
    {
      Ast_Declaration* local_decl = build_parser_local_decl();
      result->first_local_decl = local_decl;
      while (token_is_parser_local_decl(token))
      {
        Ast_Declaration* next_local_decl = build_parser_local_decl();
        local_decl->next_decl = next_local_decl;
        local_decl = next_local_decl;
      }
    }

    if (token->klass == TOK_BRACE_CLOSE)
    {
      sym_import_var((Ident*)error_kw);
      next_token();
    }
    else
      error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else if (token->klass == TOK_SEMICOLON)
    next_token();
  else
    error("at line %d: '{' or ';' expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_ControlDecl*
build_control_prototype()
{
  assert(token->klass == TOK_KW_CONTROL);
  next_token();
  Ast_ControlDecl* result = arena_push_struct(&arena, Ast_ControlDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_CONTROL_PROTOTYPE;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      result->first_type_parameter = build_type_parameter_list();

    if (token->klass == TOK_PARENTH_OPEN)
    {
      next_token();

      if (token_is_parameter(token))
        result->first_parameter = build_parameter_list();

      if (token->klass == TOK_PARENTH_CLOSE)
        next_token();
      else
      {
        if (token->klass == TOK_IDENT)
          error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
        else
          error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);
      }
    }
    else
      error("at line %d: '(' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_BlockStmt*
build_block_statement()
{
  assert(token->klass == TOK_BRACE_OPEN);
  next_token();
  Ast_BlockStmt* result = arena_push_struct(&arena, Ast_BlockStmt);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_BLOCK_STMT;

  if (token_is_statement(token))
    result->first_statement = build_statement_list();

  if (token->klass == TOK_BRACE_CLOSE)
    next_token();
  else
    error("at line %d: statement expected, got '%s'", token->line_nr, token->lexeme);

  return result;
}

bool
token_is_control_local_decl(Token* token)
{
  bool result = token->klass == TOK_KW_ACTION || token->klass == TOK_KW_TABLE \
                || token_is_ident(token) || token->klass == TOK_KW_VAR;
  return result;
}

internal Ast_ActionDecl*
build_action_decl()
{
  assert(token->klass == TOK_KW_ACTION);

  next_token();

  Ast_ActionDecl* result = arena_push_struct(&arena, Ast_ActionDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_ACTION;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();

    if (token->klass == TOK_PARENTH_OPEN)
    {
      next_token();
      if (token_is_parameter(token))
        result->parameter = build_parameter_list();
      if (token->klass == TOK_PARENTH_CLOSE)
        next_token();
      else
        error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '(' expected, got '%s'", token->line_nr, token->lexeme);

    if (token->klass == TOK_BRACE_OPEN)
      result->action_body = build_block_statement();
    else
      error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: non-type identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_Key*
build_key_elem()
{
  assert(token_is_expression(token));

  Ast_Key* result = arena_push_struct(&arena, Ast_Key);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_TABLE_KEY;
  result->expression = build_expression(1);

  if (token->klass == TOK_COLON)
  {
    next_token();
    if (token_is_ident(token))
    {
      result->name = build_expression(1);
      expect_semicolon();
    }
    else
      error("at line %d: non-type identifier expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: ':' expected, got '%s'", token->line_nr, token->lexeme);

  return result;
}

internal Ast_SimpleProp*
build_simple_prop()
{
  assert(token_is_expression(token));

  Ast_SimpleProp* result = arena_push_struct(&arena, Ast_SimpleProp);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_SIMPLE_PROP;
  result->expression = build_expression(1);
  expect_semicolon();

  return result;
}

internal Ast_ActionRef*
build_action_ref()
{
  assert(token_is_ident(token));

  Ast_ActionRef* result = arena_push_struct(&arena, Ast_ActionRef);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_ACTION_REF;
  result->name = token->lexeme;

  next_token();

  if (token->klass == TOK_PARENTH_OPEN)
  {
    next_token();
    if (token_is_expression(token))
    {
      Ast_Declaration* argument = (Ast_Declaration*)build_expression(1);
      result->first_argument = argument;
      while (token->klass == TOK_COMMA)
      {
        next_token();
        if (token_is_expression(token))
        {
          Ast_Declaration* next_argument = (Ast_Declaration*)build_expression(1);
          argument->next_decl = next_argument;
          argument = next_argument;
        }
        else if (token->klass == TOK_COMMA)
          error("at line %d: missing parameter", token->line_nr);
        else
          error("at line %d: expression term expected, got '%s'", token->line_nr, token->lexeme);
      }
    }
    if (token->klass == TOK_PARENTH_CLOSE)
      next_token();
    else
      error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: '(' expected, got '%s'", token->line_nr, token->lexeme);

  expect_semicolon();

  return result;
}

internal Ast_TableProperty*
build_table_property()
{
  assert(token_is_ident(token));
  Ast_TableProperty* result = 0;
  // FIXME: WTF is this nonsense?
  if (cstr_match(token->lexeme, "key"))
  {
    next_token();
    if (token->klass == TOK_EQUAL)
    {
      next_token();
      if (token->klass == TOK_BRACE_OPEN)
      {
        next_token();
        if (token_is_expression(token))
        {
          Ast_Key* key_elem = build_key_elem();
          result = (Ast_TableProperty*)key_elem;
          while (token_is_expression(token))
          {
            Ast_Key* next_key_elem = build_key_elem();
            key_elem->next_key = next_key_elem;
            key_elem = next_key_elem;
          }
        }
        else
          error("at line %d: expression term expected, got '%s'", token->line_nr, token->lexeme);
        if (token->klass == TOK_BRACE_CLOSE)
          next_token();
        else
          error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
      }
      else
        error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '=' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else if (cstr_match(token->lexeme, "actions"))
  {
    next_token();
    if (token->klass == TOK_EQUAL)
    {
      next_token();
      if (token->klass == TOK_BRACE_OPEN)
      {
        next_token();
        if (token_is_ident(token))
        {
          Ast_ActionRef* action_ref = build_action_ref();
          result = (Ast_TableProperty*)action_ref;
          while (token_is_ident(token))
          {
            Ast_ActionRef* next_action_ref = build_action_ref();
            action_ref->next_action = next_action_ref;
            action_ref = next_action_ref;
          }
        }
        if (token->klass == TOK_BRACE_CLOSE)
          next_token();
        else
          error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
      }
      else
        error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '=' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
  {
    Ast_SimpleProp* prop = build_simple_prop();
    result = (Ast_TableProperty*)prop;
    while (token_is_ident(token))
    {
      Ast_SimpleProp* next_prop = build_simple_prop();
      prop->next = (Ast_TableProperty*)next_prop;
      prop = next_prop;
    }
  }
  return result;
}

internal Ast_TableDecl*
build_table_decl()
{
  assert(token->klass == TOK_KW_TABLE);

  next_token();

  Ast_TableDecl* result = arena_push_struct(&arena, Ast_TableDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_TABLE;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();
    if (token->klass == TOK_BRACE_OPEN)
    {
      next_token();
      if (token_is_ident(token))
      {
        Ast_TableProperty* property = build_table_property();
        result->property = property;
        while (token_is_ident(token))
        {
          Ast_TableProperty* next_property = build_table_property();
          property->next = next_property;
          property = next_property;
        }
      }
      else
        error("at line %d: non-type identifier expected, got '%s'", token->line_nr, token->lexeme);
      if (token->klass == TOK_BRACE_CLOSE)
        next_token();
      else
        error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: non-type identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_Declaration*
build_control_local_decl()
{
  Ast_Declaration* result = 0;

  assert(token_is_control_local_decl(token));

  if (token->klass == TOK_KW_ACTION)
  {
    Ast_ActionDecl* action_decl = build_action_decl();
    result = (Ast_Declaration*)action_decl;
  }
  else if (token->klass == TOK_KW_TABLE)
  {
    Ast_TableDecl* table_decl = build_table_decl();
    result = (Ast_Declaration*)table_decl;
  }
  else if (token->klass == TOK_KW_VAR)
  {
    Ast_VarDecl* var_decl = build_var_decl();
    result = (Ast_Declaration*)var_decl;
  }
  else if (token_is_ident(token))
  {
    Ast_Declaration* expression = (Ast_Declaration*)build_expression(1);
    result = (Ast_Declaration*)expression;

    expect_semicolon();
  }
  else
    assert(false);

  return result;
}

internal Ast_ControlDecl*
build_control_decl()
{
  assert(token->klass == TOK_KW_CONTROL);
  Ast_ControlDecl* result = build_control_prototype();

  if (token->klass == TOK_BRACE_OPEN)
  {
    result->kind = AST_CONTROL_DECL;
    sym_unimport_var((Ident*)error_kw);
    next_token();

    if (token_is_control_local_decl(token))
    {
      Ast_Declaration* local_decl = build_control_local_decl();
      result->first_local_decl = local_decl;
      while (token_is_control_local_decl(token))
      {
        Ast_Declaration* next_local_decl = build_control_local_decl(result);
        local_decl->next_decl = next_local_decl;
        local_decl = next_local_decl;
      }
    }

    if (token->klass == TOK_KW_APPLY)
    {
      sym_unimport_var((Ident*)apply_kw);
      next_token();

      if (token->klass == TOK_BRACE_OPEN)
        result->apply_block = build_block_statement();
      else
        error("at line %d: '{' expected, got '%s'", token->line_nr, token->lexeme);

      sym_import_var((Ident*)apply_kw);
    }

    if (token->klass == TOK_BRACE_CLOSE)
    {
      sym_import_var((Ident*)error_kw);
      next_token();
    }
    else
      error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else if (token->klass == TOK_SEMICOLON)
    next_token();
  else
    error("at line %d: '{' or ';' expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_PackageDecl*
build_package_prototype()
{
  assert(token->klass == TOK_KW_PACKAGE);
  next_token();
  Ast_PackageDecl* result = arena_push_struct(&arena, Ast_PackageDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_PACKAGE_PROTOTYPE;

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      result->first_type_parameter = build_type_parameter_list();

    if (token->klass == TOK_PARENTH_OPEN)
    {
      next_token();

      if (token_is_parameter(token))
        result->first_parameter = build_parameter_list();

      if (token->klass == TOK_PARENTH_CLOSE)
        next_token();
      else
      {
        if (token->klass == TOK_IDENT)
          error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
        else
          error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);
      }
    }
    else
      error("at line %d: '(' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);

  if (token->klass == TOK_SEMICOLON)
    next_token();
  else
    error("at line %d: ';' expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_PackageInstantiation*
build_package_instantiation()
{
  assert(token_is_ident(token));
  Ast_PackageInstantiation* result = arena_push_struct(&arena, Ast_PackageInstantiation);
  copy_tokenattr_to_ast(token, (Ast*)result);
  zero_struct(result, Ast_PackageInstantiation);
  result->kind = AST_PACKAGE_INSTANTIATION;
  result->package_ctor = build_expression(1);

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();
  }
  else
    error("at line %d: non-type identifier expected, got '%s'", token->line_nr, token->lexeme);
  expect_semicolon();
  return result;
}

internal Ast_FunctionDecl*
build_function_prototype()
{
  assert(token_is_ident);
  Ast_FunctionDecl* result = arena_push_struct(&arena, Ast_FunctionDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_FUNCTION_PROTOTYPE;

  if (token_is_ident(peek_token()))
    result->return_type = build_type_expression();

  if (token_is_ident(token))
  {
    result->name = token->lexeme;
    next_token();

    if (token->klass == TOK_ANGLE_OPEN)
      result->first_type_parameter = build_type_parameter_list();

    if (token->klass == TOK_PARENTH_OPEN)
    {
      sym_unimport_var((Ident*)error_kw);
      next_token();

      if (token_is_parameter(token))
        result->first_parameter = build_parameter_list();

      if (token->klass == TOK_PARENTH_CLOSE)
      {
        sym_import_var((Ident*)error_kw);
        next_token();
      }
      else
        error("at line %d: ')' expected, got '%s'", token->line_nr, token->lexeme);

      if (token->klass == TOK_SEMICOLON)
        next_token();
      else
        error("at line %d: ';' expected, got '%s'", token->line_nr, token->lexeme);
    }
    else
      error("at line %d: '(' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_ExternObjectDecl*
build_extern_object_prototype()
{
  Ast_FunctionDecl* method;
  assert(token_is_ident(token));
  Ast_ExternObjectDecl* result = arena_push_struct(&arena, Ast_ExternObjectDecl);
  copy_tokenattr_to_ast(token, (Ast*)result);
  result->kind = AST_EXTERN_OBJECT_PROTOTYPE;
  result->name = token->lexeme;
  next_token();

  if (token->klass == TOK_ANGLE_OPEN)
    result->first_type_parameter = build_type_parameter_list();

  if (token->klass == TOK_BRACE_OPEN)
  {
    sym_unimport_var((Ident*)error_kw);
    next_token();

    if (token_is_ident(token))
    {
      method = build_function_prototype();
      result->first_method = method;

      while (token_is_ident(token))
      {
        Ast_FunctionDecl* next_method = build_function_prototype();
        method->next_decl = (Ast_Declaration*)next_method;
        method = next_method;
      }
    }

    sym_import_var((Ident*)error_kw);
    if (token->klass == TOK_BRACE_CLOSE)
      next_token();
    else if (token->klass == TOK_IDENT)
      error("at line %d: unknown type '%s'", token->line_nr, token->lexeme);
    else
      error("at line %d: '}' expected, got '%s'", token->line_nr, token->lexeme);
  }
  else if (token->klass == TOK_SEMICOLON)
    next_token();
  else
    error("at line %d: '{' or ';' expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_FunctionDecl*
build_extern_function_prototype()
{
  assert(token_is_ident(token));
  Ast_FunctionDecl* result = build_function_prototype();
  result->kind = AST_EXTERN_FUNCTION_PROTOTYPE;
  return result;
}

internal Ast_Declaration*
build_extern_decl()
{
  Ast_Declaration* result = 0;

  assert(token->klass == TOK_KW_EXTERN);
  next_token();

  if (token_is_ident(token))
  {
    if (token_is_ident(peek_token()))
      result = (Ast_Declaration*)build_extern_function_prototype();
    else
      result = (Ast_Declaration*)build_extern_object_prototype();
  }
  else
    error("at line %d: identifier expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

internal Ast_Declaration*
build_p4declaration()
{
  Ast_Declaration* result = 0;
  assert(token_is_p4declaration(token));

  switch (token->klass)
  {
    case TOK_KW_STRUCT:
      result = (Ast_Declaration*)build_struct_decl();
      break;
    case TOK_KW_HEADER:
      result = (Ast_Declaration*)build_header_decl();
      break;
    case TOK_KW_ERROR:
      result = (Ast_Declaration*)build_error_type_decl();
      break;
    case TOK_KW_TYPEDEF:
      result = (Ast_Declaration*)build_typedef_decl();
      break;
    case TOK_KW_PARSER:
      result = (Ast_Declaration*)build_parser_decl();
      break;
    case TOK_KW_CONTROL:
      result = (Ast_Declaration*)build_control_decl();
      break;
    case TOK_KW_ACTION:
      result = (Ast_Declaration*)build_action_decl();
      break;
    case TOK_KW_PACKAGE:
      result = (Ast_Declaration*)build_package_prototype();
      break;
    case TOK_KW_EXTERN:
      result = (Ast_Declaration*)build_extern_decl();
      break;
    case TOK_KW_CONST:
      result = (Ast_Declaration*)build_const_decl();
      break;
    case TOK_IDENT:
    case TOK_TYPE_IDENT:
      result = (Ast_Declaration*)build_package_instantiation();
      break;

    default: assert(false);
  }

  return result;
}

internal Ast_P4Program*
build_p4program()
{
  Ast_P4Program* result = 0;
  if (token_is_p4declaration(token))
  {
    result = arena_push_struct(&arena, Ast_P4Program);
    copy_tokenattr_to_ast(token, (Ast*)result);
    result->kind = AST_P4PROGRAM;

    Ast_Declaration* declaration = build_p4declaration();

    result->first_declaration = declaration;
    while (token_is_p4declaration(token))
    {
      Ast_Declaration* next_declaration = build_p4declaration();
      declaration->next_decl = next_declaration;
      declaration = next_declaration;
    }
    if (token->klass != TOK_EOI)
      error("at line %d: unexpected token '%s'", token->line_nr, token->lexeme);
  }
  else
    error("at line %d: declaration expected, got '%s'", token->line_nr, token->lexeme);
  return result;
}

void
build_ast()
{
  /*
  error_type_ast = arena_push_struct(&arena, Ast_TypeIdent);
  error_type_ast->kind = AST_TYPE_IDENT;
  error_type_ast->name = "error";
  error_type_ast->lexeme = error_type_ast->name;
  error_type_ast->is_builtin = true;

  void_type_ast = arena_push_struct(&arena, Ast_TypeIdent);
  void_type_ast->kind = AST_TYPE_IDENT;
  void_type_ast->name = "void";
  void_type_ast->lexeme = void_type_ast->name;
  void_type_ast->is_builtin = true;

  bool_type_ast = arena_push_struct(&arena, Ast_TypeIdent);
  bool_type_ast->kind = AST_TYPE_IDENT;
  bool_type_ast->name = "bool";
  bool_type_ast->lexeme = bool_type_ast->name;
  bool_type_ast->is_builtin = true;

  bit_type_ast = arena_push_struct(&arena, Ast_TypeIdent);
  bit_type_ast->kind = AST_TYPE_IDENT;
  bit_type_ast->name = "bit";
  bit_type_ast->lexeme = bit_type_ast->name;
  bit_type_ast->is_builtin = true;

  varbit_type_ast = arena_push_struct(&arena, Ast_TypeIdent);
  varbit_type_ast->kind = AST_TYPE_IDENT;
  varbit_type_ast->name = "varbit";
  varbit_type_ast->lexeme = varbit_type_ast->name;
  varbit_type_ast->is_builtin = true;

  int_type_ast = arena_push_struct(&arena, Ast_TypeIdent);
  int_type_ast->kind = AST_TYPE_IDENT;
  int_type_ast->name = "int";
  int_type_ast->lexeme = int_type_ast->name;
  int_type_ast->is_builtin = true;

  string_type_ast = arena_push_struct(&arena, Ast_TypeIdent);
  string_type_ast->kind = AST_TYPE_IDENT;
  string_type_ast->name = "string";
  string_type_ast->lexeme = string_type_ast->name;
  string_type_ast->is_builtin = true;

  bool_true_ast = arena_push_struct(&arena, Ast_Integer);
  bool_true_ast->kind = AST_INTEGER;
  bool_true_ast->lexeme = "true";
  bool_true_ast->value = 1;

  bool_false_ast = arena_push_struct(&arena, Ast_Integer);
  bool_false_ast->kind = AST_INTEGER;
  bool_false_ast->lexeme = "false";
  bool_false_ast->value = 0;
  */

  add_keyword("action", TOK_KW_ACTION);
  add_keyword("enum", TOK_KW_ENUM);
  add_keyword("in", TOK_KW_IN);
  add_keyword("package", TOK_KW_PACKAGE);
  add_keyword("select", TOK_KW_SELECT);
  add_keyword("switch", TOK_KW_SWITCH);
  add_keyword("tuple", TOK_KW_TUPLE);
  add_keyword("control", TOK_KW_CONTROL);
  error_kw = add_keyword("error", TOK_KW_ERROR);
  add_keyword("header", TOK_KW_HEADER);
  add_keyword("inout", TOK_KW_INOUT);
  add_keyword("parser", TOK_KW_PARSER);
  add_keyword("state", TOK_KW_STATE);
  add_keyword("table", TOK_KW_TABLE);
  add_keyword("typedef", TOK_KW_TYPEDEF);
  add_keyword("default", TOK_KW_DEFAULT);
  add_keyword("extern", TOK_KW_EXTERN);
  add_keyword("header_union", TOK_KW_HEADER_UNION);
  add_keyword("out", TOK_KW_OUT);
  add_keyword("transition", TOK_KW_TRANSITION);
  add_keyword("else", TOK_KW_ELSE);
  add_keyword("exit", TOK_KW_EXIT);
  add_keyword("if", TOK_KW_IF);
  add_keyword("match_kind", TOK_KW_MATCH_KIND);
  add_keyword("return", TOK_KW_RETURN);
  add_keyword("struct", TOK_KW_STRUCT);
  apply_kw = add_keyword("apply", TOK_KW_APPLY);
  add_keyword("var", TOK_KW_VAR);
  add_keyword("const", TOK_KW_CONST);

  /*
  error_type_ident = sym_new_type(error_type_ast->name, (Ast*)error_type_ast);
  void_type_ident = sym_new_type(void_type_ast->name, (Ast*)void_type_ast);
  bool_type_ident = sym_new_type("bool", (Ast*)bool_type_ast);
  bit_type_ident = sym_new_type("bit", (Ast*)bit_type_ast);
  varbit_type_ident = sym_new_type("varbit", (Ast*)varbit_type_ast);
  int_type_ident = sym_new_type("int", (Ast*)int_type_ast);
  string_type_ident = sym_new_type("string", (Ast*)string_type_ast);

  bool_true_ident = sym_new_var("true", (Ast*)bool_true_ast);
  bool_false_ident = sym_new_var("false", (Ast*)bool_false_ast);
  */

  token = tokenized_input;
  next_token();
  p4program = build_p4program();
}
