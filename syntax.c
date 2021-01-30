#define DEBUG_ENABLED 1

#include "syntax.h"

external Arena arena;

external struct Token* tokenized_input;
internal struct Token* token = 0;
internal struct Token* prev_token = 0;
external int tokenized_input_len;

external struct Namespace_Entry** symtable;
external int max_symtable_len;
external int scope_level;

external struct Cst_P4Program* p4program;

internal struct Cst* build_expression(int priority_threshold);
internal struct Cst* build_typeRef();
internal struct Cst* build_blockStatement();
internal struct Cst* build_statement();
internal struct Cst* build_parserStatement();

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

internal int
scope_push_level()
{
  int new_scope_level = ++scope_level;
  DEBUG("push scope %d\n", new_scope_level);
  return new_scope_level;
}

internal int
scope_pop_level(int to_level)
{
  if (scope_level <= to_level)
    return scope_level;

  int i = 0;
  while (i < max_symtable_len) {
    struct Namespace_Entry* ns = symtable[i];
    while (ns) {
      struct Ident* ident = ns->ns_global;
      if (ident && ident->scope_level > to_level) {
        ns->ns_global = ident->next_in_scope;
        if (ident->next_in_scope)
          assert (ident->next_in_scope->scope_level <= to_level);
        ident->next_in_scope = 0;
      }
      ident = ns->ns_type;
      if (ident && ident->scope_level > to_level) {
        ns->ns_type = ident->next_in_scope;
        if (ident->next_in_scope)
          assert (ident->next_in_scope->scope_level <= to_level);
        ident->next_in_scope = 0;
      }
      ns = ns->next;
    }
    i++;
  }
  DEBUG("pop scope %d\n", to_level);
  scope_level = to_level;
  return scope_level;
}

internal bool
ident_is_declared(struct Ident* ident)
{
  bool is_declared = (ident && ident->scope_level >= scope_level);
  return is_declared;
}

internal struct Namespace_Entry*
get_namespace(char* name)
{
  uint32_t h = name_hash(name);
  struct Namespace_Entry* name_info = symtable[h];
  while(name_info) {
    if (cstr_match(name_info->name, name))
      break;
    name_info = name_info->next;
  }
  if (!name_info) {
    name_info = arena_push_struct(&arena, Namespace_Entry);
    name_info->name = name;
    name_info->next = symtable[h];
    symtable[h] = name_info;
  }
  return name_info;
}

struct Ident*
new_type(char* name, int line_nr)
{
  struct Ident* ident = 0;
  struct Namespace_Entry* ns = get_namespace(name);
  if (ident_is_declared(ident)) {
    error("at line %d: redeclaration of type `%s`.", line_nr, ident->name);
  } else {
    struct Ident* ident = arena_push_struct(&arena, Ident);
    ident->name = name;
    ident->scope_level = scope_level;
    ident->ident_kind = Ident_Type;
    ident->next_in_scope = ns->ns_type;
    ns->ns_type = (struct Ident*)ident;
    DEBUG("new type `%s`\n", ident->name);
  }
  return ident;
}

internal struct Ident_Keyword*
add_keyword(char* name, enum TokenClass token_klass)
{
  struct Namespace_Entry* namespace = get_namespace(name);
  assert (namespace->ns_global == 0);
  struct Ident_Keyword* ident = arena_push_struct(&arena, Ident_Keyword);
  ident->name = name;
  ident->scope_level = scope_level;
  ident->token_klass = token_klass;
  ident->ident_kind = Ident_Keyword;
  namespace->ns_global = (struct Ident*)ident;
  return ident;
}

internal struct Token*
next_token()
{
  assert(token < tokenized_input + tokenized_input_len);
  prev_token = token++;
  while (token->klass == Token_Comment)
    token++;

  if (token->klass == Token_Identifier) {
    struct Namespace_Entry* ns = get_namespace(token->lexeme);
    if (ns->ns_global) {
      struct Ident* ident = ns->ns_global;
      if (ident->ident_kind == Ident_Keyword) {
        token->klass = ((struct Ident_Keyword*)ident)->token_klass;
        return token;
      }
    }

    if (ns->ns_type) {
      struct Ident* ident = ns->ns_type;
      if (ident->ident_kind == Ident_Type) {
        token->klass = Token_TypeIdentifier;
        return token;
      }
    }
  }
  return token;
}

internal struct Token*
peek_token()
{
  prev_token = token;
  struct Token* peek_token = next_token();
  token = prev_token;
  return peek_token;
}

#define new_cst_node(type) ({ \
  struct type* node = arena_push(&arena, sizeof(struct type)); \
  *node = (struct type){}; \
  node->kind = type; \
  node->line_nr = token->line_nr; \
  node; })

internal bool
token_is_typeName(struct Token* token)
{
  return token->klass == Token_TypeIdentifier;
}

internal bool
token_is_prefixedType(struct Token* token)
{
  return token->klass == Token_TypeIdentifier;
}

internal bool
token_is_baseType(struct Token* token)
{
  bool result = token->klass == Token_Bool || token->klass == Token_Error || token->klass == Token_Int
    || token->klass == Token_Bit || token->klass == Token_Varbit;
  return result;
}

internal bool
token_is_typeRef(struct Token* token)
{
  bool result = token_is_baseType(token) || token->klass == Token_TypeIdentifier || token->klass == Token_Tuple;
  return result;
}

internal bool
token_is_direction(struct Token* token)
{
  bool result = token->klass == Token_In || token->klass == Token_Out || token->klass == Token_InOut;
  return result;
}

internal bool
token_is_parameter(struct Token* token)
{
  bool result = token_is_direction(token) || token_is_typeRef(token);
  return result;
}

internal bool
token_is_functionPrototype()
{
  bool result = token_is_typeRef(token) || token->klass == Token_Void || token->klass == Token_Identifier;
  return result;
}

internal bool
token_is_derivedTypeDeclaration(struct Token* token)
{
  bool result = token->klass == Token_Header || token->klass == Token_HeaderUnion || token->klass == Token_Struct
    || token->klass == Token_Enum;
  return result;
}

internal bool
token_is_typeDeclaration(struct Token* token)
{
  bool result = token_is_derivedTypeDeclaration(token) || token->klass == Token_Typedef || token->klass == Token_Type
    || token->klass == Token_Parser || token->klass == Token_Control || token->klass == Token_Package;
  return result;
}

internal bool
token_is_nonTypeName(struct Token* token)
{
  bool result = token->klass == Token_Identifier || token->klass == Token_Apply || token->klass == Token_Key
    || token->klass == Token_Actions || token->klass == Token_State || token->klass == Token_Entries || token->klass == Token_Type;
  return result;
}

internal bool
token_is_name(struct Token* token)
{
  bool result = token_is_nonTypeName(token) || token->klass == Token_TypeIdentifier;
  return result;
}

internal bool
token_is_nonTableKwName(struct Token* token)
{
  bool result = token->klass == Token_Identifier || token->klass == Token_TypeIdentifier
    || token->klass == Token_Apply || token->klass == Token_State || token->klass == Token_Type;
  return result;
}

internal bool
token_is_typeArg(struct Token* token)
{
  bool result = token->klass == Token_Dontcare || token_is_typeRef(token) || token_is_nonTypeName(token);
  return result;
}

internal bool
token_is_typeParameterList(struct Token* token)
{
  return token_is_name(token);
}

internal bool
token_is_typeOrVoid(struct Token* token)
{
  bool result = token_is_typeRef(token) || token->klass == Token_Void || token->klass == Token_Identifier;
  return result;
}

internal bool
token_is_actionRef(struct Token* token)
{
  bool result = token->klass == Token_Dotprefix || token_is_nonTypeName(token)
    || token->klass == Token_ParenthOpen;
  return result;
}

internal bool
token_is_tableProperty(struct Token* token)
{
  bool result = token->klass == Token_Key || token->klass == Token_Actions
    || token->klass == Token_Const || token->klass == Token_Entries
    || token_is_nonTableKwName(token);
  return result;
}

internal bool
token_is_switchLabel(struct Token* token)
{
  bool result = token_is_name(token) || token->klass == Token_Default;
  return result;
}

internal bool
token_is_expressionPrimary(struct Token* token)
{
  bool result = token->klass == Token_Integer || token->klass == Token_True || token->klass == Token_False
    || token->klass == Token_StringLiteral || token->klass == Token_Dotprefix || token_is_nonTypeName(token)
    || token->klass == Token_BraceOpen || token->klass == Token_ParenthOpen || token->klass == Token_LogicNot
    || token->klass == Token_BitwiseNot || token->klass == Token_UnaryMinus || token_is_typeName(token)
    || token->klass == Token_Error || token_is_prefixedType(token);
  return result;
}

internal bool
token_is_expression(struct Token* token)
{
  return token_is_expressionPrimary(token);
}

internal struct Cst*
build_nonTypeName(bool is_type)
{
  struct Cst_NonTypeName* name = 0;
  if (token_is_nonTypeName(token)) {
    name = new_cst_node(Cst_NonTypeName);
    name->name = token->lexeme;
    if (is_type) {
      new_type(name->name, token->line_nr);
    }
    next_token();
  } else error("at line %d: non-type name was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)name;
}

internal struct Cst*
build_name(bool is_type)
{
  struct Cst_Name* name = 0;
  if (token_is_name(token)) {
    if (token_is_nonTypeName(token)) {
      name = (struct Cst_Name*)build_nonTypeName(is_type);
    } else if (token->klass == Token_TypeIdentifier) {
      struct Cst_TypeName* type_name = new_cst_node(Cst_TypeName);
      type_name->name = token->lexeme;
      name = (struct Cst_Name*)type_name;
      next_token();
    } else assert(false);
  } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)name;
}

internal struct Cst*
build_typeParameterList()
{
  struct Cst* params = 0;
  if (token_is_typeParameterList(token)) {
    struct Cst_TypeParam* prev_param = new_cst_node(Cst_TypeParam);
    params = (struct Cst*)prev_param;
    prev_param->name = build_name(true);
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst_TypeParam* next_param = new_cst_node(Cst_TypeParam);
      next_param->name = build_name(true);
      list_append_item(&prev_param->link, &next_param->link);
      prev_param = next_param;
    }
  } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  return params;
}

internal struct Cst*
build_optTypeParameters()
{
  struct Cst* params = 0;
  if (token->klass == Token_AngleOpen) {
    next_token();
    if (token_is_typeParameterList(token)) {
      params = build_typeParameterList();
      if (token->klass == Token_AngleClose) {
        next_token();
      } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  }
  return params;
}

internal struct Cst*
build_typeArg()
{
  struct Cst* arg = 0;
  if (token_is_typeArg(token))
  {
    if (token->klass == Token_Dontcare) {
      struct Cst_Dontcare* dontcare = new_cst_node(Cst_Dontcare);
      arg = (struct Cst*)dontcare;
      next_token();
    } else if (token_is_typeRef(token)) {
      build_typeRef();
    } else if (token_is_nonTypeName(token)) {
      build_nonTypeName(false);
    } else assert(false);
  } else error("at line %d: type argument was expected, got `%s`.", token->line_nr, token->lexeme);
  return arg;
}

internal bool
token_is_methodPrototype(struct Token* token)
{
  return token_is_functionPrototype() | token->klass == Token_TypeIdentifier;
}

internal struct Cst*
build_direction()
{
  if (token_is_direction(token)) {
    next_token();
  }
  return 0;
}

internal struct Cst*
build_parameter()
{
  struct Cst_Parameter* param = new_cst_node(Cst_Parameter);
  param->direction = build_direction();
  if (token_is_typeRef(token)) {
    param->type = build_typeRef();
    if (token_is_name(token)) {
      param->name = build_name(false);
      if (token->klass == Token_Equal) {
        next_token();
        if (token_is_expression(token)) {
          param->init_expr = build_expression(1);
        } else error("at line %d: expression was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)param;
}

internal struct Cst*
build_parameterList()
{
  struct Cst* param_list = 0;
  if (token_is_parameter(token)) {
    struct Cst* prev_param = build_parameter();
    param_list = prev_param;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_param = build_parameter();
      list_append_item(&prev_param->link, &next_param->link);
      prev_param = next_param;
    }
  }
  return param_list;
}

internal struct Cst*
build_typeOrVoid(bool is_type)
{
  struct Cst* type = 0;
  if (token_is_typeOrVoid(token)) {
    if (token_is_typeRef(token)) {
      type = build_typeRef();
    } else if (token->klass == Token_Void) {
      struct Cst_TypeName* void_name = new_cst_node(Cst_TypeName);
      void_name->name = token->lexeme;
      type = (struct Cst*)void_name;
      next_token();
    } else if (token->klass == Token_Identifier) {
      struct Cst_NonTypeName* name = new_cst_node(Cst_NonTypeName);
      name->name = token->lexeme;
      type = (struct Cst*)name;
      if (is_type) {
        new_type(name->name, token->line_nr);
      }
      next_token();
    } else assert(0);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return type;
}

internal struct Cst*
build_functionPrototype()
{
  struct Cst_FunctionProto* proto = 0;
  if (token_is_typeOrVoid(token)) {
    proto = new_cst_node(Cst_FunctionProto);
    scope_push_level();
    proto->return_type = build_typeOrVoid(true);
    if (token_is_name(token)) {
      proto->name = build_name(false);
      proto->type_params = build_optTypeParameters();
      if (token->klass == Token_ParenthOpen) {
        next_token();
        proto->params = build_parameterList();
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
    scope_pop_level(scope_level-1);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)proto;
}

internal struct Cst*
build_methodPrototype()
{
  struct Cst* proto = 0;
  if (token_is_methodPrototype(token)) {
    if (token->klass == Token_TypeIdentifier && peek_token()->klass == Token_ParenthOpen) {
      /* constructor */
      struct Cst_MethodProto* method_proto = new_cst_node(Cst_MethodProto);
      proto = (struct Cst*)method_proto;
      next_token();
      if (token->klass == Token_ParenthOpen) {
        next_token();
        method_proto->params = build_parameterList();
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `(` as expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token_is_functionPrototype(token)) {
      proto = build_functionPrototype();
    } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return proto;
}

internal struct Cst*
build_methodPrototypes()
{
  struct Cst* protos = 0;
  if (token_is_methodPrototype(token)) {
    struct Cst* prev_proto = build_methodPrototype();
    protos = prev_proto;
    while (token_is_methodPrototype(token)) {
      struct Cst* next_proto = build_methodPrototype();
      list_append_item(&prev_proto->link, &next_proto->link);
      prev_proto = next_proto;
    }
  }
  return protos;
}

internal struct Cst*
build_externDeclaration()
{
  struct Cst* decl = 0;
  if (token->klass == Token_Extern) {
    struct Cst_ExternDecl* extern_decl = new_cst_node(Cst_ExternDecl);
    decl = (struct Cst*)extern_decl;
    next_token();
    if (token_is_nonTypeName(token)) {
      extern_decl->name = build_nonTypeName(true);
      extern_decl->type_params = build_optTypeParameters();
      if (token->klass == Token_BraceOpen) {
        scope_push_level();
        next_token();
        extern_decl->method_protos = build_methodPrototypes();
        if (token->klass == Token_BraceClose) {
          next_token();
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        scope_pop_level(scope_level-1);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token_is_functionPrototype(token)) {
      decl = build_functionPrototype();
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `extern` was expected, got `%s`.", token->line_nr, token->lexeme);
  return decl;
}

internal struct Cst*
build_integerTypeSize()
{
  struct Cst_IntTypeSize* type_size = new_cst_node(Cst_IntTypeSize);
  if (token->klass == Token_Integer) {
    struct Cst_Int* int_size = new_cst_node(Cst_Int);
    type_size->size = (struct Cst*)int_size;
    next_token();
  } else if (token->klass == Token_ParenthOpen) {
    type_size->size = build_expression(1);
  } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)type_size;
}

internal struct Cst*
build_baseType()
{
  struct Cst_BaseType* base_type = 0;
  if (token_is_baseType(token)) {
    base_type = new_cst_node(Cst_BaseType);
    if (token->klass == Token_Bool) {
      base_type->base_type = BASETYPE_BOOL;
      next_token();
    } else if (token->klass == Token_Error) {
      base_type->base_type = BASETYPE_ERROR;
      next_token();
    } else if (token->klass == Token_Int) {
      base_type->base_type = BASETYPE_INT;
      next_token();
      if (token->klass == Token_AngleOpen) {
        next_token();
        base_type->size = build_integerTypeSize();
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    } else if (token->klass == Token_Bit) {
      base_type->base_type = BASETYPE_BIT;
      next_token();
      if (token->klass == Token_AngleOpen) {
        next_token();
        base_type->size = build_integerTypeSize();
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    } else if (token->klass == Token_Varbit) {
      base_type->base_type = BASETYPE_VARBIT;
      next_token();
      if (token->klass == Token_AngleOpen) {
        next_token();
        base_type->size = build_integerTypeSize();
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    } else assert(false);
  } else error("at line %d: type as expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)base_type;
}

internal struct Cst*
build_typeArgumentList()
{
  struct Cst* arg_list = 0;
  if (token_is_typeArg(token)) {
    struct Cst* prev_arg = build_typeArg();
    arg_list = prev_arg;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_arg = build_typeArg();
      list_append_item(&prev_arg->link, &next_arg->link);
      prev_arg = next_arg;
    }
  }
  return arg_list;
}

internal struct Cst*
build_tupleType()
{
  struct Cst_Tuple* type = 0;
  if (token->klass == Token_Tuple) {
    type = new_cst_node(Cst_Tuple);
    next_token();
    if (token->klass == Token_AngleOpen) {
      next_token();
      type->type_args = build_typeArgumentList();
      if (token->klass == Token_AngleClose) {
        next_token();
      } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `<` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `tuple` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)type;
}

internal struct Cst*
build_headerStackType()
{
  struct Cst_HeaderStack* stack = 0;
  if (token->klass == Token_BracketOpen) {
    stack = new_cst_node(Cst_HeaderStack);
    next_token();
    if (token_is_expression(token)) {
      stack->expr = build_expression(1);
      if (token->klass == Token_BracketClose) {
        next_token();
      } else error("at line %d: `]` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: an expression expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `[` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)stack;
}

internal struct Cst*
build_specializedType()
{
  struct Cst_SpecdType* type = 0;
  if (token->klass == Token_AngleOpen) {
    type = new_cst_node(Cst_SpecdType);
    next_token();
    type->type_args = build_typeArgumentList();
    if (token->klass == Token_AngleClose) {
      next_token();
    } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `<` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)type;
}

internal struct Cst*
build_prefixedType()
{
  struct Cst_TypeName* name = 0;
  if (token->klass == Token_TypeIdentifier) {
    name = new_cst_node(Cst_TypeName);
    name->name = token->lexeme;
    next_token();
    if (token->klass == Token_Dotprefix && peek_token()->klass == Token_TypeIdentifier) {
      next_token();
      struct Cst_PrefixedType* pfx_type = new_cst_node(Cst_PrefixedType);
      pfx_type->first_name = name;
      pfx_type->second_name = new_cst_node(Cst_TypeName);
      pfx_type->second_name->name = token->lexeme;
      next_token();
    }
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)name;
}

internal struct Cst*
build_typeName()
{
  struct Cst* name = 0;
  if (token->klass == Token_TypeIdentifier) {
    name = build_prefixedType();
    if (token->klass == Token_AngleOpen) {
      name = build_specializedType();
    } if (token->klass == Token_BracketOpen) {
      name = build_headerStackType();
    }
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return name;
}

internal struct Cst*
build_typeRef()
{
  struct Cst* ref = 0;
  if (token_is_typeRef(token)) {
    if (token_is_baseType(token)) {
      ref = build_baseType();
    } else if (token->klass == Token_TypeIdentifier) {
      /* <typeName> | <specializedType> | <headerStackType> */
      ref = build_typeName();
    } else if (token->klass == Token_Tuple) {
      ref = build_tupleType();
    } else assert(false);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return ref;
}

internal bool
token_is_structField(struct Token* token)
{
  bool result = token_is_typeRef(token);
  return result;
}

internal struct Cst*
build_structField()
{
  struct Cst_StructField* field = new_cst_node(Cst_StructField);
  if (token_is_typeRef(token)) {
    field->type = build_typeRef();
    if (token_is_name(token)) {
      field->name = build_name(false);
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)field;
}

internal struct Cst*
build_structFieldList()
{
  struct Cst* field_list = 0;
  while (token_is_structField(token)) {
    struct Cst* prev_field = build_structField();
    field_list = prev_field;
    if (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_field = build_structField();
      list_append_item(&prev_field->link, &next_field->link);
      prev_field = next_field;
    }
  }
  return field_list;
}

internal struct Cst*
build_headerTypeDeclaration()
{
  struct Cst_HeaderDecl* decl = 0;
  if (token->klass == Token_Header) {
    decl = new_cst_node(Cst_HeaderDecl);
    next_token();
    if (token_is_name(token)) {
      decl->name = build_name(true);
      if (token->klass == Token_BraceOpen) {
        scope_push_level();
        next_token();
        decl->fields = build_structFieldList();
        if (token->klass == Token_BraceClose) {
          next_token(token);
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        scope_pop_level(scope_level-1);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `header` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_headerUnionDeclaration()
{
  struct Cst_HeaderUnionDecl* decl = 0;
  if (token->klass == Token_HeaderUnion) {
    decl = new_cst_node(Cst_HeaderUnionDecl);
    next_token();
    if (token_is_name(token)) {
      decl->name = build_name(true);
      if (token->klass == Token_BraceOpen) {
        next_token();
        decl->fields = build_structFieldList();
        if (token->klass == Token_BraceClose) {
          next_token();
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `header_union` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_structTypeDeclaration()
{
  struct Cst_StructDecl* decl = 0;
  if (token->klass == Token_Struct) {
    decl = new_cst_node(Cst_StructDecl);
    next_token();
    if (token_is_name(token)) {
      decl->name = build_name(true);
      if (token->klass == Token_BraceOpen) {
        next_token();
        decl->fields = build_structFieldList();
        if (token->klass == Token_BraceClose) {
          next_token();
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `struct` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal bool
token_is_specifiedIdentifier(struct Token* token)
{
  return token_is_name(token);
}

internal struct Cst*
build_initializer()
{
  return build_expression(1);
}

internal struct Cst*
build_optInitializer()
{
  struct Cst* init = 0;
  if (token->klass == Token_Equal) {
    next_token();
    init = build_initializer();
  }
  return init;
}

internal struct Cst*
build_specifiedIdentifier()
{
  struct Cst_SpecdId* id = 0;
  if (token_is_specifiedIdentifier(token)) {
    id = new_cst_node(Cst_SpecdId);
    id->name = build_name(false);
    if (token->klass == Token_Equal) {
      next_token();
      if (token_is_expression(token)) {
        id->init_expr = build_initializer();
      } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)id;
}

internal struct Cst*
build_specifiedIdentifierList()
{
  struct Cst* id_list = 0;
  if (token_is_specifiedIdentifier(token)) {
    struct Cst* prev_id = build_specifiedIdentifier();
    id_list = prev_id;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_id = build_specifiedIdentifier();
      list_append_item(&prev_id->link, &next_id->link);
      prev_id = next_id;
    }
  }
  return id_list;
}

internal struct Cst*
build_enumDeclaration()
{
  struct Cst_EnumDecl* decl = 0;
  if (token->klass == Token_Enum) {
    decl = new_cst_node(Cst_EnumDecl);
    next_token();
    if (token->klass == Token_Bit) {
      if (token->klass == Token_AngleOpen) {
        next_token();
        if (token->klass == Token_Integer) {
          struct Cst_Int* int_size = new_cst_node(Cst_Int);
          decl->type_size = (struct Cst*)int_size;
          next_token();
          if (token->klass == Token_AngleClose) {
            next_token();
          } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: an integer was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `<` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
    if (token_is_name) {
      decl->name = build_name(true);
      if (token->klass == Token_BraceOpen) {
        next_token();
        if (token_is_specifiedIdentifier(token)) {
          decl->id_list = build_specifiedIdentifierList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `enum` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_derivedTypeDeclaration()
{
  struct Cst* decl = 0;
  if (token_is_derivedTypeDeclaration(token)) {
    if (token->klass == Token_Header) {
      decl = build_headerTypeDeclaration();
    } else if (token->klass == Token_HeaderUnion) {
      decl = build_headerUnionDeclaration();
    } else if (token->klass == Token_Struct) {
      decl = build_structTypeDeclaration();
    } else if (token->klass == Token_Enum) {
      decl = build_enumDeclaration();
    } else assert(false);
  } else error("at line %d: structure declaration was expected, got `%s`.", token->line_nr, token->lexeme);
  return decl;
}

internal struct Cst*
build_parserTypeDeclaration()
{
  struct Cst_ParserType* type = 0;
  if (token->klass == Token_Parser) {
    type = new_cst_node(Cst_ParserType);
    next_token();
    if (token_is_name(token)) {
      type->name = build_name(true);
      type->type_params = build_optTypeParameters();
      if (token->klass == Token_ParenthOpen) {
        next_token();
        type->params = build_parameterList();
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `parser` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)type;
}

internal struct Cst*
build_optConstructorParameters()
{
  struct Cst* ctor_params = 0;
  if (token->klass == Token_ParenthOpen) {
    next_token();
    ctor_params = build_parameterList();
    if (token->klass == Token_ParenthClose) {
      next_token();
    } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
  }
  return ctor_params;
}

internal struct Cst*
build_constantDeclaration()
{
  struct Cst_ConstDecl* decl = 0;
  if (token->klass == Token_Const) {
    next_token();
    if (token_is_typeRef(token)) {
      decl->type = build_typeRef();
      if (token_is_name(token)) {
        decl->name = build_name(false);
        if (token->klass == Token_Equal) {
          next_token();
          if (token_is_expression(token)) {
            decl->expr = build_expression(1);
            if (token->klass == Token_Semicolon) {
              next_token();
            } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
          } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `const` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal bool
token_is_declaration(struct Token* token)
{
  bool result = token->klass == Token_Const || token->klass == Token_Extern || token->klass == Token_Action
    || token->klass == Token_Parser || token_is_typeDeclaration(token) || token->klass == Token_Control
    || token_is_typeRef(token) || token->klass == Token_Error || token->klass == Token_MatchKind
    || token_is_typeOrVoid(token);
  return result;
}

internal bool
token_is_lvalue(struct Token* token)
{
  bool result = token_is_nonTypeName(token) | token->klass == Token_Dotprefix;
  return result;
}

internal bool
token_is_assignmentOrMethodCallStatement(struct Token* token)
{
  bool result = token_is_lvalue(token) || token->klass == Token_ParenthOpen || token->klass == Token_AngleOpen
    || token->klass == Token_Equal;
  return result;
}

internal bool
token_is_statement(struct Token* token)
{
  bool result = token_is_assignmentOrMethodCallStatement(token) || token_is_typeName(token) || token->klass == Token_If
    || token->klass == Token_Semicolon || token->klass == Token_BraceOpen || token->klass == Token_Exit
    || token->klass == Token_Return || token->klass == Token_Switch;
  return result;
}

internal bool
token_is_statementOrDeclaration(struct Token* token)
{
  bool result = token_is_typeRef(token) || token->klass == Token_Const || token_is_statement(token);
  return result;
}

internal bool
token_is_argument(struct Token* token)
{
  bool result = token_is_expression(token) || token_is_name(token) || token->klass == Token_Dontcare;
  return result;
}

internal bool
token_is_parserLocalElement(struct Token* token)
{
  bool result = token->klass == Token_Const || token_is_typeRef(token);
  return result;
}

internal bool
token_is_parserStatement(struct Token* token)
{
  bool result = token_is_assignmentOrMethodCallStatement(token) || token_is_typeName(token)
    || token->klass == Token_BraceOpen || token->klass == Token_Const || token_is_typeRef(token)
    || token->klass == Token_Semicolon;
  return result;
}

internal bool
token_is_simpleKeysetExpression(struct Token* token) {
  bool result = token_is_expression(token) || token->klass == Token_Default || token->klass == Token_Dontcare;
  return result;
}

internal bool
token_is_keysetExpression(struct Token* token)
{
  bool result = token->klass == Token_Tuple || token_is_simpleKeysetExpression(token);
  return result;
}

internal bool
token_is_selectCase(struct Token* token)
{
  return token_is_keysetExpression(token);
}

internal bool
token_is_controlLocalDeclaration(struct Token* token)
{
  bool result = token->klass == Token_Const || token->klass == Token_Action
    || token->klass == Token_Table || token_is_typeRef(token) || token_is_typeRef(token);
  return result;
}

internal struct Cst*
build_argument()
{
  struct Cst* arg = 0;
  if (token_is_argument(token)) {
    if (token_is_expression(token)) {
      arg = build_expression(1);
    } else if (token_is_name(token)) {
      struct Cst_Argument* named_arg = new_cst_node(Cst_Argument);
      arg = (struct Cst*)named_arg;
      named_arg->name = build_name(false);
      if (token->klass == Token_Equal) {
        next_token();
        if (token_is_expression(token)) {
          named_arg->init_expr = build_expression(1);
        } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Dontcare) {
      struct Cst_Dontcare* dontcare_arg = new_cst_node(Cst_Dontcare);
      arg = (struct Cst*)dontcare_arg;
      next_token();
    } else assert(0);
  } else error("at line %d: an argument was expected, got `%s`.", token->line_nr, token->lexeme);
  return arg;
}

internal struct Cst*
build_argumentList()
{
  struct Cst* arg_list = 0;
  if (token_is_argument(token)) {
    struct Cst* prev_arg = build_argument();
    arg_list = prev_arg;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_arg = build_argument();
      list_append_item(&prev_arg->link, &next_arg->link);
      prev_arg = next_arg;
    }
  }
  return arg_list;
}

internal struct Cst*
build_variableDeclaration()
{
  struct Cst_VarDecl* decl = 0;
  if (token_is_typeRef(token)) {
    decl = new_cst_node(Cst_VarDecl);
    decl->type = build_typeRef();
    if (token_is_name(token)) {
      decl->name = build_name(false);
      decl->init_expr = build_optInitializer();
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_instantiation()
{
  struct Cst_Instantiation* inst = 0;
  if (token_is_typeRef(token)) {
    inst = new_cst_node(Cst_Instantiation);
    inst->type = build_typeRef();
    if (token->klass == Token_ParenthOpen) {
      next_token();
      inst->args = build_argumentList();
      if (token->klass == Token_ParenthClose) {
        next_token();
        if (token_is_name(token)) {
          inst->name = build_name(false);
          if (token->klass == Token_Semicolon) {
            next_token();
          } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)inst;
}

internal struct Cst*
build_parserLocalElement()
{
  struct Cst* elem = 0;
  if (token_is_parserLocalElement(token)) {
    if (token->klass == Token_Const) {
      elem = build_constantDeclaration();
    } else if (token_is_typeRef(token)) {
      if (peek_token()->klass == Token_ParenthOpen) {
        elem = build_instantiation();
      } else {
        elem = build_variableDeclaration();
      }
    } else assert(0);
  } else error("at line %d: local declaration was expected, got `%s`.", token->line_nr, token->lexeme);
  return elem;
}

internal struct Cst*
build_parserLocalElements()
{
  struct Cst* elem_list = 0;
  if (token_is_parserLocalElement(token)) {
    struct Cst* prev_elem = build_parserLocalElement();
    elem_list = prev_elem;
    while (token_is_parserLocalElement(token)) {
      struct Cst* next_elem = build_parserLocalElement();
      list_append_item(&prev_elem->link, &next_elem->link);
      prev_elem = next_elem;
    }
  }
  return elem_list;
}

internal struct Cst*
build_directApplication()
{
  struct Cst_DirectApplic* applic = 0;
  if (token_is_typeName(token)) {
    applic = new_cst_node(Cst_DirectApplic);
    applic->name = build_typeName();
    if (token->klass == Token_Dotprefix) {
      next_token();
      if (token->klass == Token_Apply) {
        next_token();
        if (token->klass == Token_ParenthOpen) {
          next_token();
          applic->args = build_argumentList();
          if (token->klass == Token_ParenthClose) {
            next_token();
            if (token->klass == Token_Semicolon) {
              next_token();
            } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
          } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `apply` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `.` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type name was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)applic;
}

internal struct Cst*
build_prefixedNonTypeName()
{
  struct Cst* name = 0;
  if (token->klass == Token_Dotprefix) {
    struct Cst_DotPrefix* dot_prefix = new_cst_node(Cst_DotPrefix);
    name = (struct Cst*)dot_prefix;
    next_token();
  }
  if (token_is_nonTypeName) {
    name = build_nonTypeName(false);
  } else error("at line %d: non-type name was expected, ", token->line_nr, token->lexeme);
  return name;
}

internal struct Cst*
build_arrayIndex()
{
  struct Cst* idx = 0;
  if (token_is_expression(token)) {
    idx = build_expression(1);
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  if (token->klass == Token_Colon) {
    struct Cst_SlicedIndex* idx_slice = new_cst_node(Cst_SlicedIndex);
    idx = (struct Cst*)idx_slice;
    idx_slice->idx_left = idx;
    next_token();
    if (token_is_expression(token)) {
      idx_slice->idx_right = build_expression(1);
    } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  }
  return (struct Cst*)idx;
}

internal struct Cst*
build_lvalue()
{
  struct Cst_Lvalue* lvalue = 0;
  if (token_is_lvalue(token)) {
    lvalue = new_cst_node(Cst_Lvalue);
    lvalue->name = build_prefixedNonTypeName();
    struct Cst* prev_name = lvalue->name;
    while (token->klass == Token_Dotprefix || token->klass == Token_BracketOpen) {
      if (token->klass == Token_Dotprefix) {
        next_token();
        struct Cst* next_name = build_name(false);
        list_append_item(&prev_name->link, &next_name->link);
        prev_name = next_name;
      }
      if (token->klass == Token_BracketOpen) {
        next_token();
        lvalue->array_index = build_arrayIndex();
        if (token->klass == Token_BracketClose) {
          next_token();
        } else error("at line %d: `]` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    }
  } else error("at line %d: an lvalue was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)lvalue;
}

internal struct Cst*
build_assignmentOrMethodCallStatement()
{
  if (token_is_lvalue(token)) {
    build_lvalue();
    if (token->klass == Token_AngleOpen) {
      next_token();
      build_typeArgumentList();
      if (token->klass == Token_AngleClose) {
        next_token();
      } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
    if (token->klass == Token_ParenthOpen) {
      next_token();
      build_argumentList();
      if (token->klass == Token_ParenthClose) {
        next_token();
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Equal) {
      next_token();
      build_expression(1);
    } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
  }
  return 0;
}

internal struct Cst*
build_parserStatements()
{
  while (token_is_parserStatement(token)) {
    build_parserStatement();
  }
  return 0;
}

internal struct Cst*
build_parserBlockStatements()
{
  if (token->klass == Token_BraceOpen) {
    next_token();
    build_parserStatements();
    if (token->klass == Token_BraceClose) {
      next_token();
    } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_parserStatement()
{
  if (token_is_assignmentOrMethodCallStatement(token)) {
    build_assignmentOrMethodCallStatement();
  } else if (token_is_typeName(token)) {
    build_directApplication();
  } else if (token->klass == Token_BraceOpen) {
    build_parserBlockStatements();
  } else if (token->klass == Token_Const) {
    build_constantDeclaration();
  } else if (token_is_typeRef(token)) {
    build_variableDeclaration();
  } else if (token->klass == Token_Semicolon) {
    ; // <emptyStatement>
  } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_expressionList()
{
  if (token_is_expression(token)) {
    build_expression(1);
    while (token->klass == Token_Comma) {
      next_token();
      build_expression(1);
    }
  }
  return 0;
}

internal struct Cst*
build_simpleKeysetExpression()
{
  struct Cst* expr = 0;
  if (token_is_expression(token)) {
    expr = build_expression(1);
  } else if (token->klass == Token_Default) {
    next_token();
  } else if (token->klass == Token_Dontcare) {
    next_token();
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

internal struct Cst*
build_tupleKeysetExpression()
{
  struct Cst* expr = 0;
  if (token->klass == Token_ParenthOpen) {
    next_token();
    build_simpleKeysetExpression();
    while (token->klass == Token_Comma) {
      next_token();
      build_simpleKeysetExpression();
    }
    if (token->klass == Token_ParenthClose) {
      next_token();
    } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

internal struct Cst*
build_keysetExpression()
{
  struct Cst* expr = 0;
  if (token->klass == Token_ParenthOpen) {
    build_tupleKeysetExpression();
  } else if (token_is_simpleKeysetExpression(token)) {
    build_simpleKeysetExpression();
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

internal struct Cst*
build_selectCase()
{
  if (token_is_keysetExpression(token)) {
    build_keysetExpression();
    if (token->klass == Token_Colon) {
      next_token();
      if (token_is_name(token)) {
        build_name(false);
        if (token->klass == Token_Semicolon) {
          next_token();
        } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_selectCaseList()
{
  while (token_is_selectCase(token)) {
    build_selectCase();
  }
  return 0;
}

internal struct Cst*
build_selectExpression()
{
  if (token->klass == Token_Select) {
    next_token();
    if (token->klass == Token_ParenthOpen) {
      next_token();
      build_expressionList();
      if (token->klass == Token_ParenthClose) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          scope_push_level();
          next_token();
          build_selectCaseList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
          scope_pop_level(scope_level-1);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `select` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_stateExpression()
{
  if (token_is_name(token)) {
    build_name(false);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else if (token->klass == Token_Select) {
    build_selectExpression();
  } else error("at line %d: state expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_transitionStatement()
{
  if (token->klass == Token_Transition) {
    next_token();
    build_stateExpression();
  } else error("at line %d: `transition` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_parserState()
{
  if (token->klass == Token_State) {
    next_token();
    build_name(false);
    if (token->klass == Token_BraceOpen) {
      scope_push_level();
      next_token();
      build_parserStatements();
      build_transitionStatement();
      if (token->klass == Token_BraceClose) {
        next_token();
      } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
      scope_pop_level(scope_level-1);
    } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `state` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_parserStates()
{
  if (token->klass == Token_State) {
    build_parserState();
    while (token->klass == Token_State) {
      build_parserState();
    }
  } else error("at line %d: `state` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_parserDeclaration()
{
  struct Cst_Parser* decl = 0;
  if (token->klass == Token_Parser) {
    decl = new_cst_node(Cst_Parser);
    decl->type_decl = build_parserTypeDeclaration();
    if (token->klass == Token_Semicolon) {
      next_token(); /* <parserTypeDeclaration> */
    } else {
      decl->ctor_params = build_optConstructorParameters();
      if (token->klass == Token_BraceOpen) {
        scope_push_level();
        next_token();
        decl->local_elements = build_parserLocalElements();
        decl->states = build_parserStates();
        if (token->klass == Token_BraceClose) {
          next_token();
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        scope_pop_level(scope_level-1);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
  } else error("at line %d: `parser` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_controlTypeDeclaration()
{
  if (token->klass == Token_Control) {
    next_token();
    if (token_is_name(token)) {
      build_name(true);
      build_optTypeParameters();
      if (token->klass == Token_ParenthOpen) {
        next_token();
        build_parameterList();
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `control` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_actionDeclaration()
{
  struct Cst_Action* decl = 0;
  if (token->klass == Token_Action) {
    decl = new_cst_node(Cst_Action);
    next_token();
    if (token_is_name(token)) {
      decl->name = build_name(false);
      if (token->klass == Token_ParenthOpen) {
        next_token();
        decl->params = build_parameterList();
        if (token->klass == Token_ParenthClose) {
          next_token();
          if (token->klass == Token_BraceOpen) {
            decl->stmt = build_blockStatement();
          } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `action` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_keyElement()
{
  if (token_is_expression(token)) {
    build_expression(1);
    if (token->klass == Token_Colon) {
      next_token();
      build_name(false);
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_keyElementList()
{
  while (token_is_expression(token)) {
    build_keyElement();
  }
  return 0;
}

internal struct Cst*
build_actionRef()
{
  if (token->klass == Token_Dotprefix || token_is_nonTypeName(token)) {
    build_prefixedNonTypeName();
    if (token->klass == Token_ParenthOpen) {
      next_token();
      build_argumentList();
      if (token->klass == Token_ParenthClose) {
        next_token();
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
  } else error("at line %d: non-type name was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_actionList()
{
  while (token_is_actionRef(token)) {
    build_actionRef();
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
  }
  return 0;
}

internal struct Cst*
build_entry()
{
  if (token_is_keysetExpression(token)) {
    build_keysetExpression();
    if (token->klass == Token_Colon) {
      next_token();
      build_actionRef();
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: keyset was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_entriesList()
{
  if (token_is_keysetExpression(token)) {
    build_entry();
    while (token_is_keysetExpression(token)) {
      build_entry();
    }
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_tableProperty()
{
  if (token_is_tableProperty(token)) {
    if (token->klass == Token_Const) {
      next_token();
    }
    if (token->klass == Token_Key) {
      next_token();
      if (token->klass == Token_Equal) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          build_keyElementList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Actions) {
      next_token();
      if (token->klass == Token_Equal) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          build_actionList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Entries) {
      next_token();
      if (token->klass == Token_Equal) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          build_entriesList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token_is_nonTableKwName(token)) {
      next_token();
      if (token->klass == Token_Equal) {
        next_token();
        build_initializer();
        if (token->klass == Token_Semicolon) {
          next_token();
        } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else assert(0);
  } else error("at line %d: table property was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_tablePropertyList()
{
  if (token_is_tableProperty(token)) {
    build_tableProperty();
    while (token_is_tableProperty(token)) {
      build_tableProperty();
    }
  } else error("at line %d: table property was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_tableDeclaration()
{
  if (token->klass == Token_Table) {
    next_token();
    build_name(false);
    if (token->klass == Token_BraceOpen) {
      next_token();
      build_tablePropertyList();
      if (token->klass == Token_BraceClose) {
        next_token();
      } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `table` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_controlLocalDeclaration()
{
  struct Cst* decl = 0;
  if (token->klass == Token_Const) {
    decl = build_constantDeclaration();
  } else if (token->klass == Token_Action) {
    decl = build_actionDeclaration();
  } else if (token->klass == Token_Table) {
    decl = build_tableDeclaration();
  } else if (token_is_typeRef(token)) {
    if (peek_token()->klass == Token_ParenthOpen) {
      decl = build_instantiation();
    } else {
      decl = build_variableDeclaration();
    }
  } else error("at line %d: local declaration was expected, got `%s`.", token->line_nr, token->lexeme);
  return decl;
}

internal struct Cst*
build_controlLocalDeclarations()
{
  while (token_is_controlLocalDeclaration(token)) {
    build_controlLocalDeclaration();
  }
  return 0;
}

internal struct Cst*
build_controlDeclaration()
{
  struct Cst_Control* decl = 0;
  if (token->klass == Token_Control) {
    decl = new_cst_node(Cst_Control);
    decl->type_decl = build_controlTypeDeclaration();
    if (token->klass == Token_Semicolon) {
      next_token(); /* <controlTypeDeclaration> */
    } else {
      decl->ctor_params = build_optConstructorParameters();
      if (token->klass == Token_BraceOpen) {
        next_token();
        decl->local_decls = build_controlLocalDeclarations();
        if (token->klass == Token_Apply) {
          next_token();
          decl->apply_stmt = build_blockStatement();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `apply` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
  } else error("at line %d: `control` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_packageTypeDeclaration()
{
  struct Cst_Package* decl = 0;
  if (token->klass == Token_Package) {
    decl = new_cst_node(Cst_Package);
    next_token();
    if (token_is_name(token)) {
      decl->name = build_name(true);
      decl->type_params = build_optTypeParameters();
      if (token->klass == Token_ParenthOpen) {
        next_token();
        decl->params = build_parameterList();
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `package` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_typedefDeclaration()
{
  struct Cst* decl = 0;
  if (token->klass == Token_Typedef || token->klass == Token_Type) {
    if (token->klass == Token_Typedef) {
      next_token();
    } else if (token->klass == Token_Type) {
      next_token();
    } else assert(false);

    if (token_is_typeRef(token) || token_is_derivedTypeDeclaration(token)) {
      if (token_is_typeRef(token)) {
        decl = build_typeRef();
      } else if (token_is_derivedTypeDeclaration(token)) {
        decl = build_derivedTypeDeclaration();
      } else assert(false);

      if (token_is_name(token)) {
        build_name(true);
        if (token->klass == Token_Semicolon) {
          next_token();
        } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type definition was expected, got `%s`.", token->line_nr, token->lexeme);
  return decl;
}

internal struct Cst*
build_typeDeclaration()
{
  struct Cst* decl = 0;
  if (token_is_typeDeclaration(token)) {
    if (token_is_derivedTypeDeclaration(token)) {
      decl = build_derivedTypeDeclaration();
    } else if (token->klass == Token_Typedef || token->klass == Token_Type) {
      decl = build_typedefDeclaration();
    } else if (token->klass == Token_Parser) {
      /* <parserTypeDeclaration> | <parserDeclaration> */
      decl = build_parserDeclaration();
    } else if (token->klass == Token_Control) {
      /* <controlTypeDeclaration> | <controlDeclaration> */
      decl = build_controlDeclaration();
    } else if (token->klass == Token_Package) {
      decl = build_packageTypeDeclaration();
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
    } else assert(0);
  } else error("at line %d: type declaration was expected, got `%s`.", token->line_nr, token->lexeme); 
  return decl;
}

internal struct Cst*
build_conditionalStatement()
{
  if (token->klass == Token_If) {
    next_token();
    if (token->klass == Token_ParenthOpen) {
      next_token();
      if (token_is_expression(token)) {
        build_expression(1);
        if (token->klass == Token_ParenthClose) {
          next_token();
          if (token_is_statement(token)) {
            build_statement();
            if (token->klass == Token_Else) {
              next_token();
              if (token_is_statement(token)) {
                build_statement();
              } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
            }
          } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `if` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_exitStatement()
{
  if (token->klass == Token_Exit) {
    next_token();
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `exit` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_returnStatement()
{
  if (token->klass == Token_Return) {
    next_token();
    if (token_is_expression(token))
      build_expression(1);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `return` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_switchLabel()
{
  if (token_is_name(token)) {
    build_name(false);
  } else if (token->klass == Token_Default) {
    next_token();
  } else error("at line %d: switch label was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_switchCase()
{
  if (token_is_switchLabel(token)) {
    build_switchLabel();
    if (token->klass == Token_Colon) {
      next_token();
      if (token->klass == Token_BraceOpen) {
        build_blockStatement();
      }
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: switch label was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_switchCases()
{
  while (token_is_switchLabel(token)) {
    build_switchCase();
  }
  return 0;
}

internal struct Cst*
build_switchStatement()
{
  if (token->klass == Token_Switch) {
    next_token();
    if (token->klass == Token_ParenthOpen) {
      next_token();
      build_expression(1);
      if (token->klass == Token_ParenthClose) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          build_switchCases();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `switch` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_statement()
{
  if (token_is_assignmentOrMethodCallStatement(token)) {
    build_assignmentOrMethodCallStatement();
  } else if (token_is_typeName(token)) {
    build_directApplication();
  } else if (token->klass == Token_If) {
    build_conditionalStatement();
  } else if (token->klass == Token_Semicolon) {
    next_token(); // empty statement
  } else if (token->klass == Token_BraceOpen) {
    build_blockStatement();
  } else if (token->klass == Token_Exit) {
    build_exitStatement();
  } else if (token->klass == Token_Return) {
    build_returnStatement();
  } else if (token->klass == Token_Switch) {
    build_switchStatement();
  } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_statementOrDeclList()
{
  while (token_is_statementOrDeclaration(token)) {
    if (token_is_typeRef(token)) {
      if (peek_token()->klass == Token_ParenthOpen) {
        build_instantiation();
      } else {
        build_variableDeclaration();
      }
    } else if (token_is_statement(token)) {
      build_statement();
    } else if (token->klass == Token_Const)
      build_constantDeclaration();
    else assert(false);
  }
  return 0;
}

internal struct Cst*
build_blockStatement()
{
  if (token->klass == Token_BraceOpen) {
    scope_push_level();
    next_token();
    build_statementOrDeclList();
    if (token->klass == Token_BraceClose) {
      next_token();
    } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
    scope_pop_level(scope_level-1);
  } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_identifierList()
{
  if (token_is_name(token)) {
    build_name(false);
    while (token->klass == Token_Comma) {
      next_token();
      build_name(false);
    }
  } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_errorDeclaration()
{
  struct Cst_Error* decl = 0;
  if (token->klass == Token_Error) {
    decl = new_cst_node(Cst_Error);
    next_token();
    if (token->klass == Token_BraceOpen) {
      next_token();
      if (token_is_name(token)) {
        decl->id_list = build_identifierList();
        if (token->klass == Token_BraceClose) {
          next_token();
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `error` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_matchKindDeclaration()
{
  struct Cst_MatchKind* decl = 0;
  if (token->klass == Token_MatchKind) {
    decl = new_cst_node(Cst_MatchKind);
    next_token();
    if (token->klass == Token_BraceOpen) {
      next_token();
      if (token_is_name(token)) {
        decl->fields = build_identifierList();
      } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `match_kind` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_functionDeclaration()
{
  struct Cst_FunctionDecl* decl = 0;
  if (token_is_typeOrVoid(token)) {
    decl->proto = build_functionPrototype();
    if (token->klass == Token_BraceOpen) {
      decl->stmt = build_blockStatement();
    } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_declaration()
{
  struct Cst* decl = 0;
  if (token_is_declaration(token)) {
    if (token->klass == Token_Const) {
      decl = build_constantDeclaration();
    } else if (token->klass == Token_Extern) {
      decl = build_externDeclaration();
    } else if (token->klass == Token_Action) {
      decl = build_actionDeclaration();
    } else if (token_is_typeDeclaration(token)) {
      /* <parserDeclaration> | <typeDeclaration> | <controlDeclaration> */
      decl = build_typeDeclaration();
    } else if (token_is_typeRef(token) && peek_token()->klass == Token_ParenthOpen) {
      decl = build_instantiation();
    } else if (token->klass == Token_Error) {
      decl = build_errorDeclaration();
    } else if (token->klass == Token_MatchKind) {
      decl = build_matchKindDeclaration();
    } else if (token_is_typeOrVoid(token)) {
      decl = build_functionDeclaration();
    } else assert(false);
  } else error("at line %d: top-level declaration as expected, got `%s`.", token->line_nr, token->lexeme);
  return decl;
}

internal struct Cst*
build_p4program()
{
  struct Cst_P4Program* prog = 0;
  while (token_is_declaration(token) || token->klass == Token_Semicolon) {
    if (token_is_declaration(token)) {
      struct Cst* decl = build_declaration();
    } else if (token->klass == Token_Semicolon) {
      next_token(); /* empty declaration */
    }
  }
  if (token->klass != Token_EndOfInput)
    error("at line %d: unexpected token `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)prog;
}

internal bool
token_is_realTypeArg(struct Token* token)
{
  bool result = token->klass == Token_Dontcare || token_is_typeRef(token);
  return result;
}

internal bool
token_is_binaryOperator(struct Token* token)
{
  bool result = token->klass == Token_Star || token->klass == Token_Slash
    || token->klass == Token_Plus || token->klass == Token_Minus
    || token->klass == Token_LessEqual || token->klass == Token_GreaterEqual
    || token->klass == Token_AngleOpen || token->klass == Token_AngleClose
    || token->klass == Token_LogicNotEqual || token->klass == Token_LogicEqual
    || token->klass == Token_LogicOr || token->klass == Token_LogicAnd
    || token->klass == Token_BitwiseOr || token->klass == Token_BitwiseAnd
    || token->klass == Token_BitwiseXor || token->klass == Token_BitshiftLeft
    || token->klass == Token_BitshiftRight;
  return result;
}

internal bool
token_is_exprOperator(struct Token* token)
{
  bool result = token_is_binaryOperator(token) || token->klass == Token_Dotprefix
    || token->klass == Token_BracketOpen || token->klass == Token_ParenthOpen
    || token->klass == Token_AngleOpen;
  return result;
}

internal struct Cst*
build_realTypeArg()
{
  if (token->klass == Token_Dontcare) {
    next_token();
  } else if (token_is_typeRef(token)) {
    build_typeRef();
  } else error("at line %d: type argument was expected, got `%s`.", token->line_nr, token->lexeme);
  return 0;
}

internal struct Cst*
build_realTypeArgumentList()
{
  if (token_is_realTypeArg(token)) {
    build_realTypeArg();
    while (token->klass == Token_Comma) {
      next_token();
      build_realTypeArg();
    }
  }
  return 0;
}

internal struct Cst*
build_expressionPrimary()
{
  struct Cst* primary = 0;
  if (token_is_expression(token)) {
    if (token->klass == Token_Integer) {
      next_token();
    } else if (token->klass == Token_True) {
      next_token();
    } else if (token->klass == Token_False) {
      next_token();
    } else if (token->klass == Token_StringLiteral) {
      next_token();
    } else if (token->klass == Token_Dotprefix) {
      next_token();
      build_nonTypeName(false);
    } else if (token_is_nonTypeName(token)) {
      primary = (struct Cst*)build_nonTypeName(false);
    } else if (token->klass == Token_BraceOpen) {
      next_token();
      build_expressionList();
      if (token->klass == Token_BraceClose) {
        next_token();
      } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_ParenthOpen) {
      next_token();
      if (token_is_typeRef(token)) {
        build_typeRef();
        if (token->klass == Token_ParenthClose) {
          next_token();
          build_expression(1);
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else if (token_is_expression(token)) {
        build_expression(1);
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_LogicNot) {
      next_token();
      build_expression(1);
    } else if (token->klass == Token_BitwiseNot) {
      next_token();
      build_expression(1);
    } else if (token->klass == Token_UnaryMinus) {
      next_token();
      build_expression(1);
    } else if (token_is_typeName(token)) {
      build_typeName();
    } else if (token->klass == Token_Error) {
      next_token();
    } else assert(false);
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return primary;
}

internal int
get_operator_priority(struct Token* token)
{
  int prio = 0;
  if (token->klass == Token_LogicAnd || token->klass == Token_LogicOr) {
    prio = 1;
  } else if (token->klass == Token_LogicEqual || token->klass == Token_LogicNotEqual
      || token->klass == Token_AngleOpen /* Less */ || token->klass == Token_AngleClose /* Greater */
      || token->klass == Token_LessEqual || token->klass == Token_GreaterEqual) {
    prio = 2;
  }
  else if (token->klass == Token_Plus || token->klass == Token_Minus
           || token->klass == Token_BitwiseAnd || token->klass == Token_BitwiseOr
           || token->klass == Token_BitwiseXor || token->klass == Token_BitshiftLeft
           || token->klass == Token_BitshiftRight) {
    prio = 3;
  }
  else if (token->klass == Token_Star || token->klass == Token_Slash) {
    prio = 4;
  }
  else assert(false);
  return prio;
}

internal struct Cst*
build_expression(int priority_threshold)
{
  struct Cst* expr = 0;
  if (token_is_expression(token)) {
    build_expressionPrimary();
    while (token_is_exprOperator(token)) {
      if (token->klass == Token_Dotprefix) {
        next_token();
        if (token_is_name(token)) {
          build_name(false);
        } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
      }
      else if (token->klass == Token_BracketOpen) {
        next_token();
        if (token_is_expression(token)) {
          build_expression(1);
          if (token->klass == Token_Colon) {
            next_token();
            if (token_is_expression(token)) {
              build_expression(1);
            } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
          }
          if (token->klass == Token_BracketClose) {
            next_token();
          } else error("at line %d: `]` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
      }
      else if (token->klass == Token_ParenthOpen) {
        next_token();
        build_argumentList();
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
      else if (token->klass == Token_AngleOpen && token_is_realTypeArg(peek_token())) {
        next_token();
        build_realTypeArgumentList();
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else if (token_is_binaryOperator(token)){
        int priority = get_operator_priority(token);
        if (priority >= priority_threshold) {
          next_token();
          build_expression(priority_threshold + 1);
        } else break;
      } else assert(0);
    }
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

void
build_cst()
{
  add_keyword("action", Token_Action);
  add_keyword("actions", Token_Actions);
  add_keyword("entries", Token_Entries);
  add_keyword("enum", Token_Enum);
  add_keyword("in", Token_In);
  add_keyword("package", Token_Package);
  add_keyword("select", Token_Select);
  add_keyword("switch", Token_Switch);
  add_keyword("tuple", Token_Tuple);
  add_keyword("control", Token_Control);
  add_keyword("error", Token_Error);
  add_keyword("header", Token_Header);
  add_keyword("inout", Token_InOut);
  add_keyword("parser", Token_Parser);
  add_keyword("state", Token_State);
  add_keyword("table", Token_Table);
  add_keyword("key", Token_Key);
  add_keyword("typedef", Token_Typedef);
  add_keyword("default", Token_Default);
  add_keyword("extern", Token_Extern);
  add_keyword("header_union", Token_HeaderUnion);
  add_keyword("out", Token_Out);
  add_keyword("transition", Token_Transition);
  add_keyword("else", Token_Else);
  add_keyword("exit", Token_Exit);
  add_keyword("if", Token_If);
  add_keyword("match_kind", Token_MatchKind);
  add_keyword("return", Token_Return);
  add_keyword("struct", Token_Struct);
  add_keyword("apply", Token_Apply);
  add_keyword("const", Token_Const);
  add_keyword("bool", Token_Bool);
  add_keyword("true", Token_True);
  add_keyword("false", Token_False);
  add_keyword("void", Token_Void);
  add_keyword("int", Token_Int);
  add_keyword("bit", Token_Bit);
  add_keyword("varbit", Token_Varbit);

  token = tokenized_input;
  next_token();
  build_p4program();
}
