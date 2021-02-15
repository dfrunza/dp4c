#define DEBUG_ENABLED 0

#include "arena.h"
#include "lex.h"
#include "build_cst.h"

enum IdentKind
{
  Ident_None,
  Ident_Keyword,
  Ident_Type,
};

struct Ident {
  enum IdentKind ident_kind;
  char* name;
  int scope_level;
  struct Ident* next_in_scope;
};  

struct Ident_Keyword {
  struct Ident;
  enum TokenClass token_klass;
};

struct Symtable_Entry {
  char* name;
  struct Ident* ns_kw;
  struct Ident* ns_type;
  struct Symtable_Entry* next;
};

external Arena arena;

internal struct Token* tokens;
internal int token_count;
internal struct Token* token = 0;
internal struct Token* prev_token = 0;

internal struct Symtable_Entry** symtable;
internal int max_symtable_len = 997;  // table entry units
internal int scope_level = 0;

internal int node_id = 1;

internal struct Cst* build_expression();
internal struct Cst* build_typeRef();
internal struct Cst* build_blockStatement();
internal struct Cst* build_statement();
internal struct Cst* build_parserStatement();

#define new_cst_node(type, token) ({ \
  struct type* node = arena_push(&arena, sizeof(struct type)); \
  *node = (struct type){}; \
  node->kind = type; \
  node->id = node_id++; \
  node->line_nr = token->line_nr; \
  node; })

internal void
link_cst_nodes(struct Cst* node_a, struct Cst* node_b)
{
  assert(node_a->next_node == 0);
  assert(node_b->prev_node == 0);
  node_a->next_node = node_b;
  node_b->prev_node = node_a;
}

internal uint32_t
name_hash(char* name)
{
  uint32_t sum = 0;
  char* pc = name;
  while (*pc) {
    sum = (sum + (uint32_t)(*pc)*65599) % max_symtable_len;
    pc++;
  }
  return sum;
}

internal int
new_scope()
{
  int new_scope_level = ++scope_level;
  DEBUG("push scope %d\n", new_scope_level);
  return new_scope_level;
}

internal void
delete_scope()
{
  int prev_level = scope_level - 1;
  assert (prev_level >= 0);

  int i = 0;
  while (i < max_symtable_len) {
    struct Symtable_Entry* ns = symtable[i];
    while (ns) {
      struct Ident* ident = ns->ns_kw;
      if (ident && ident->scope_level > prev_level) {
        ns->ns_kw = ident->next_in_scope;
        if (ident->next_in_scope)
          assert (ident->next_in_scope->scope_level <= prev_level);
        ident->next_in_scope = 0;
      }
      ident = ns->ns_type;
      if (ident && ident->scope_level > prev_level) {
        ns->ns_type = ident->next_in_scope;
        if (ident->next_in_scope)
          assert (ident->next_in_scope->scope_level <= prev_level);
        ident->next_in_scope = 0;
      }
      ns = ns->next;
    }
    i++;
  }
  DEBUG("pop scope %d\n", prev_level);
  scope_level = prev_level;
}

internal bool
ident_is_declared(struct Ident* ident)
{
  bool is_declared = (ident && ident->scope_level >= scope_level);
  return is_declared;
}

internal struct Symtable_Entry*
get_symtable_entry(char* name)
{
  uint32_t h = name_hash(name);
  struct Symtable_Entry* entry = symtable[h];
  while (entry) {
    if (cstr_match(entry->name, name))
      break;
    entry = entry->next;
  }
  if (!entry) {
    entry = arena_push_struct(&arena, Symtable_Entry);
    entry->name = name;
    entry->next = symtable[h];
    symtable[h] = entry;
  }
  return entry;
}

struct Ident*
new_type(char* name, int line_nr)
{
  struct Symtable_Entry* ns = get_symtable_entry(name);
  struct Ident* ident = ns->ns_type;
  if (!ident) {
    ident = arena_push_struct(&arena, Ident);
    ident->name = name;
    ident->scope_level = scope_level;
    ident->ident_kind = Ident_Type;
    ident->next_in_scope = ns->ns_type;
    ns->ns_type = (struct Ident*)ident;
    DEBUG("new type `%s` at line %d.\n", ident->name, line_nr);
  }
  return ident;
}

internal struct Ident_Keyword*
add_keyword(char* name, enum TokenClass token_klass)
{
  struct Symtable_Entry* namespace = get_symtable_entry(name);
  assert (namespace->ns_kw == 0);
  struct Ident_Keyword* ident = arena_push_struct(&arena, Ident_Keyword);
  ident->name = name;
  ident->scope_level = scope_level;
  ident->token_klass = token_klass;
  ident->ident_kind = Ident_Keyword;
  namespace->ns_kw = (struct Ident*)ident;
  return ident;
}

internal struct Token*
next_token()
{
  assert(token < tokens + token_count);
  prev_token = token++;
  while (token->klass == Token_Comment) {
    token++;
  }
  if (token->klass == Token_Identifier) {
    struct Symtable_Entry* ns = get_symtable_entry(token->lexeme);
    if (ns->ns_kw) {
      struct Ident* ident = ns->ns_kw;
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
  bool result = token->klass == Token_DotPrefix || token_is_nonTypeName(token)
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
    || token->klass == Token_StringLiteral || token->klass == Token_DotPrefix || token_is_nonTypeName(token)
    || token->klass == Token_BraceOpen || token->klass == Token_ParenthOpen || token->klass == Token_Exclamation
    || token->klass == Token_Tilda || token->klass == Token_UnaryMinus || token_is_typeName(token)
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
    name = new_cst_node(Cst_NonTypeName, token);
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
  struct Cst* name = 0;
  if (token_is_name(token)) {
    if (token_is_nonTypeName(token)) {
      name = build_nonTypeName(is_type);
    } else if (token->klass == Token_TypeIdentifier) {
      struct Cst_TypeName* type_name = new_cst_node(Cst_TypeName, token);
      type_name->name = token->lexeme;
      name = (struct Cst*)type_name;
      next_token();
    } else assert(0);
  } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)name;
}

internal struct Cst*
build_typeParameterList()
{
  struct Cst* params = 0;
  if (token_is_typeParameterList(token)) {
    struct Cst* prev_param = build_name(true);
    params = prev_param;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_param = build_name(true);
      link_cst_nodes(prev_param, next_param);
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
      struct Cst_Dontcare* dontcare = new_cst_node(Cst_Dontcare, token);
      arg = (struct Cst*)dontcare;
      next_token();
    } else if (token_is_typeRef(token)) {
      arg = build_typeRef();
    } else if (token_is_nonTypeName(token)) {
      arg = build_nonTypeName(false);
    } else assert(0);
  } else error("at line %d: type argument was expected, got `%s`.", token->line_nr, token->lexeme);
  return arg;
}

internal bool
token_is_methodPrototype(struct Token* token)
{
  return token_is_typeOrVoid(token) | token->klass == Token_TypeIdentifier;
}

internal struct Cst*
build_direction()
{
  struct Cst_ParamDir* dir = 0;
  if (token_is_direction(token)) {
    dir = new_cst_node(Cst_ParamDir, token);
    if (token->klass == Token_In) {
      dir->dir_kind = AstDir_In;
    } else if (token->klass == Token_Out) {
      dir->dir_kind = AstDir_Out;
    } else if (token->klass == Token_InOut) {
      dir->dir_kind = AstDir_InOut;
    } else assert(0);
    next_token();
  }
  return (struct Cst*)dir;
}

internal struct Cst*
build_parameter()
{
  struct Cst_Parameter* param = new_cst_node(Cst_Parameter, token);
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
      link_cst_nodes(prev_param, next_param);
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
      struct Cst_TypeName* void_name = new_cst_node(Cst_TypeName, token);
      void_name->name = token->lexeme;
      type = (struct Cst*)void_name;
      next_token();
    } else if (token->klass == Token_Identifier) {
      struct Cst_NonTypeName* name = new_cst_node(Cst_NonTypeName, token);
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
    proto = new_cst_node(Cst_FunctionProto, token);
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
    } else error("at line %d: function name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: type was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)proto;
}

internal struct Cst*
build_methodPrototype()
{
  struct Cst* proto = 0;
  if (token_is_methodPrototype(token)) {
    if (token->klass == Token_TypeIdentifier && peek_token()->klass == Token_ParenthOpen) {
      struct Cst_Constructor* ctor = new_cst_node(Cst_Constructor, token);
      proto = (struct Cst*)ctor;
      next_token();
      if (token->klass == Token_ParenthOpen) {
        next_token();
        ctor->params = build_parameterList();
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `(` as expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token_is_typeOrVoid(token)) {
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
      link_cst_nodes(prev_proto, next_proto);
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
    next_token();
    struct Cst_ExternDecl* extern_decl = new_cst_node(Cst_ExternDecl, token);
    decl = (struct Cst*)extern_decl;
    bool is_function_proto = false;
    if (token_is_typeOrVoid(token) && token_is_nonTypeName(token)) {
      is_function_proto = token_is_typeOrVoid(token) && token_is_name(peek_token());
    } else if (token_is_typeOrVoid(token)) {
      is_function_proto = true;
    } else if (token_is_nonTypeName(token)) {
      is_function_proto = false;
    } else error("at line %d: extern declaration was expected, got `%s`.", token->line_nr, token->lexeme);

    if (is_function_proto) {
      decl = build_functionPrototype();
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else {
      extern_decl->name = build_nonTypeName(true);
      extern_decl->type_params = build_optTypeParameters();
      if (token->klass == Token_BraceOpen) {
        next_token();
        extern_decl->method_protos = build_methodPrototypes();
        if (token->klass == Token_BraceClose) {
          next_token();
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
  }
  return decl;
}

internal struct Cst*
build_integer()
{
  struct Cst_Int* int_node = 0;
  if (token->klass == Token_Integer) {
    int_node = new_cst_node(Cst_Int, token);
    int_node->flags = token->i.flags;
    int_node->width = token->i.width;
    int_node->value = token->i.value;
    next_token();
  }
  return (struct Cst*)int_node;
}

internal struct Cst*
build_boolean()
{
  struct Cst_Bool* bool_node = 0;
  if (token->klass == Token_True || token->klass == Token_False) {
    bool_node = new_cst_node(Cst_Bool, token);
    if (token->klass == Token_True) {
      bool_node->value = 1;
    }
    next_token();
  }
  return (struct Cst*)bool_node;
}

internal struct Cst*
build_stringLiteral()
{
  struct Cst_StringLiteral* string = 0;
  if (token->klass == Token_StringLiteral) {
    string = new_cst_node(Cst_StringLiteral, token);
    // TODO: string->value = ...
    next_token();
  }
  return (struct Cst*)string;
}

internal struct Cst*
build_integerTypeSize()
{
  struct Cst_IntTypeSize* type_size = new_cst_node(Cst_IntTypeSize, token);
  if (token->klass == Token_Integer) {
    type_size->size = build_integer();
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
    base_type = new_cst_node(Cst_BaseType, token);
    if (token->klass == Token_Bool) {
      base_type->base_type = AstBaseType_Bool;
      next_token();
    } else if (token->klass == Token_Error) {
      base_type->base_type = AstBaseType_Error;
      next_token();
    } else if (token->klass == Token_Int) {
      base_type->base_type = AstBaseType_Int;
      next_token();
      if (token->klass == Token_AngleOpen) {
        next_token();
        base_type->size = build_integerTypeSize();
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    } else if (token->klass == Token_Bit) {
      base_type->base_type = AstBaseType_Bit;
      next_token();
      if (token->klass == Token_AngleOpen) {
        next_token();
        base_type->size = build_integerTypeSize();
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    } else if (token->klass == Token_Varbit) {
      base_type->base_type = AstBaseType_Varbit;
      next_token();
      if (token->klass == Token_AngleOpen) {
        next_token();
        base_type->size = build_integerTypeSize();
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
    } else assert(0);
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
      link_cst_nodes(prev_arg, next_arg);
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
    next_token();
    type = new_cst_node(Cst_Tuple, token);
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
    next_token();
    stack = new_cst_node(Cst_HeaderStack, token);
    if (token_is_expression(token)) {
      stack->stack_expr = build_expression(1);
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
    next_token();
    type = new_cst_node(Cst_SpecdType, token);
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
    name = new_cst_node(Cst_TypeName, token);
    name->name = token->lexeme;
    next_token();
    if (token->klass == Token_DotPrefix && peek_token()->klass == Token_TypeIdentifier) {
      next_token();
      struct Cst_PrefixedTypeName* pfx_type = new_cst_node(Cst_PrefixedTypeName, token);
      pfx_type->first_name = name;
      pfx_type->second_name = new_cst_node(Cst_TypeName, token);
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
      struct Cst_SpecdType* specd_type = (struct Cst_SpecdType*)build_specializedType();
      specd_type->name = name;
      name = (struct Cst*)specd_type;
    } if (token->klass == Token_BracketOpen) {
      struct Cst_HeaderStack* stack_type = (struct Cst_HeaderStack*)build_headerStackType();
      stack_type->name = name;
      name = (struct Cst*)stack_type;
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
    } else assert(0);
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
  struct Cst_StructField* field = new_cst_node(Cst_StructField, token);
  if (token_is_typeRef(token)) {
    field->type = build_typeRef();
    if (token_is_name(token)) {
      field->name = build_name(false);
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: struct field was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)field;
}

internal struct Cst*
build_structFieldList()
{
  struct Cst* field_list = 0;
  if (token_is_structField(token)) {
    struct Cst* prev_field = build_structField();
    field_list = prev_field;
    while (token_is_structField(token)) {
      struct Cst* next_field = build_structField();
      link_cst_nodes(prev_field, next_field);
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
    next_token();
    decl = new_cst_node(Cst_HeaderDecl, token);
    if (token_is_name(token)) {
      decl->name = build_name(true);
      if (token->klass == Token_BraceOpen) {
        next_token();
        decl->fields = build_structFieldList();
        if (token->klass == Token_BraceClose) {
          next_token(token);
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
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
    next_token();
    decl = new_cst_node(Cst_HeaderUnionDecl, token);
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
    next_token();
    decl = new_cst_node(Cst_StructDecl, token);
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
    id = new_cst_node(Cst_SpecdId, token);
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
      link_cst_nodes(prev_id, next_id);
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
    next_token();
    decl = new_cst_node(Cst_EnumDecl, token);
    if (token->klass == Token_Bit) {
      if (token->klass == Token_AngleOpen) {
        next_token();
        if (token->klass == Token_Integer) {
          struct Cst_Int* int_size = new_cst_node(Cst_Int, token);
          decl->type_size = (struct Cst*)int_size;
          next_token();
          if (token->klass == Token_AngleClose) {
            next_token();
          } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: an integer was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `<` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
    if (token_is_name(token)) {
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
    } else assert(0);
  } else error("at line %d: structure declaration was expected, got `%s`.", token->line_nr, token->lexeme);
  return decl;
}

internal struct Cst*
build_parserTypeDeclaration()
{
  struct Cst_ParserType* type = 0;
  if (token->klass == Token_Parser) {
    next_token();
    type = new_cst_node(Cst_ParserType, token);
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
    decl = new_cst_node(Cst_ConstDecl, token);
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
  bool result = token_is_nonTypeName(token) | token->klass == Token_DotPrefix;
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
      struct Cst_Argument* named_arg = new_cst_node(Cst_Argument, token);
      arg = (struct Cst*)named_arg;
      named_arg->name = build_name(false);
      if (token->klass == Token_Equal) {
        next_token();
        if (token_is_expression(token)) {
          named_arg->init_expr = build_expression(1);
        } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Dontcare) {
      struct Cst_Dontcare* dontcare_arg = new_cst_node(Cst_Dontcare, token);
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
      link_cst_nodes(prev_arg, next_arg);
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
    decl = new_cst_node(Cst_VarDecl, token);
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
    inst = new_cst_node(Cst_Instantiation, token);
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
        } else error("at line %d: instance name was expected, got `%s`.", token->line_nr, token->lexeme);
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
      link_cst_nodes(prev_elem, next_elem);
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
    applic = new_cst_node(Cst_DirectApplic, token);
    applic->name = build_typeName();
    if (token->klass == Token_DotPrefix) {
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
  bool is_dotprefixed = false;
  if (token->klass == Token_DotPrefix) {
    is_dotprefixed = true;
    next_token();
  }
  if (token_is_nonTypeName) {
    name = build_nonTypeName(false);
    if (is_dotprefixed) {
      struct Cst_DotPrefixedName* dot_name = new_cst_node(Cst_DotPrefixedName, token);
      dot_name->name = name;
      name = (struct Cst*)dot_name;
    }
  } else error("at line %d: non-type name was expected, ", token->line_nr, token->lexeme);
  return name;
}

internal struct Cst*
build_arrayIndex()
{
  struct Cst_ArrayIndex* index = new_cst_node(Cst_ArrayIndex, token);
  if (token_is_expression(token)) {
    index->index = build_expression(1);
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  if (token->klass == Token_Colon) {
    next_token();
    if (token_is_expression(token)) {
      index->colon_index = build_expression(1);
    } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  }
  return (struct Cst*)index;
}

internal struct Cst*
build_lvalueExpr()
{
  struct Cst* expr = 0;
  if (token->klass == Token_DotPrefix) {
    next_token();
    struct Cst_DotPrefixedName* dot_member = new_cst_node(Cst_DotPrefixedName, token);
    dot_member->name = build_name(false);
    expr = (struct Cst*)dot_member;
  } else if (token->klass == Token_BracketOpen) {
    next_token();
    expr = (struct Cst*)build_arrayIndex();
    if (token->klass == Token_BracketClose) {
      next_token();
    } else error("at line %d: `]` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: lvalue was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

internal struct Cst*
build_lvalue()
{
  struct Cst_Lvalue* lvalue = 0;
  if (token_is_lvalue(token)) {
    lvalue = new_cst_node(Cst_Lvalue, token);
    lvalue->name = build_prefixedNonTypeName();
    if (token->klass == Token_DotPrefix || token->klass == Token_BracketOpen) {
      struct Cst* prev_expr = build_lvalueExpr(); 
      lvalue->expr = prev_expr;
      while (token->klass == Token_DotPrefix || token->klass == Token_BracketOpen) {
        struct Cst* next_expr = build_lvalueExpr();
        link_cst_nodes(prev_expr, next_expr);
        prev_expr = next_expr;
      }
    }
  } else error("at line %d: lvalue was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)lvalue;
}

internal struct Cst*
build_assignmentOrMethodCallStatement()
{
  struct Cst* stmt = 0;
  if (token_is_lvalue(token)) {
    struct Cst* lvalue = build_lvalue();
    struct Cst* type_args = 0;
    stmt = (struct Cst*)lvalue;
    if (token->klass == Token_AngleOpen) {
      next_token();
      type_args = build_typeArgumentList();
      if (token->klass == Token_AngleClose) {
        next_token();
      } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
    if (token->klass == Token_ParenthOpen) {
      next_token();
      struct Cst_MethodCallStmt* call_stmt = new_cst_node(Cst_MethodCallStmt, token);
      call_stmt->lvalue = lvalue;
      call_stmt->type_args = type_args;
      call_stmt->args = build_argumentList();
      stmt = (struct Cst*)call_stmt;
      if (token->klass == Token_ParenthClose) {
        next_token();
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Equal) {
      next_token();
      struct Cst_AssignmentStmt* assgn_stmt = new_cst_node(Cst_AssignmentStmt, token);
      assgn_stmt->lvalue = lvalue;
      assgn_stmt->expr = build_expression(1);
      stmt = (struct Cst*)assgn_stmt;
    } else error("at line %d: assignment or function call was expected, got `%s`.", token->line_nr, token->lexeme);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: lvalue was expected, got `%s`.", token->line_nr, token->lexeme);
  return stmt;
}

internal struct Cst*
build_parserStatements()
{
  struct Cst* stmts = 0;
  if (token_is_parserStatement(token)) {
    struct Cst* prev_stmt = build_parserStatement();
    stmts = prev_stmt;
    while (token_is_parserStatement(token)) {
      struct Cst* next_stmt = build_parserStatement();
      link_cst_nodes(prev_stmt, next_stmt);
      prev_stmt = next_stmt;
    }
  }
  return stmts;
}

internal struct Cst*
build_parserBlockStatements()
{
  struct Cst* stmts = 0;
  if (token->klass == Token_BraceOpen) {
    next_token();
    stmts = build_parserStatements();
    if (token->klass == Token_BraceClose) {
      next_token();
    } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  return stmts;
}

internal struct Cst*
build_parserStatement()
{
  struct Cst* stmt = 0;
  if (token_is_assignmentOrMethodCallStatement(token)) {
    stmt = build_assignmentOrMethodCallStatement();
  } else if (token_is_typeName(token)) {
    stmt = build_directApplication();
  } else if (token->klass == Token_BraceOpen) {
    stmt = build_parserBlockStatements();
  } else if (token->klass == Token_Const) {
    stmt = build_constantDeclaration();
  } else if (token_is_typeRef(token)) {
    stmt = build_variableDeclaration();
  } else if (token->klass == Token_Semicolon) {
    stmt = (struct Cst*)new_cst_node(Cst_EmptyStmt, token);
  } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
  return stmt;
}

internal struct Cst*
build_expressionList()
{
  struct Cst* expr_list = 0;
  if (token_is_expression(token)) {
    struct Cst* prev_expr = build_expression(1);
    expr_list = prev_expr;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_expr = build_expression(1);
      link_cst_nodes(prev_expr, next_expr);
      prev_expr = next_expr;
    }
  }
  return expr_list;
}

internal struct Cst*
build_simpleKeysetExpression()
{
  struct Cst* expr = 0;
  if (token_is_expression(token)) {
    expr = build_expression(1);
  } else if (token->klass == Token_Default) {
    next_token();
    expr = (struct Cst*)new_cst_node(Cst_Default, token);
  } else if (token->klass == Token_Dontcare) {
    next_token();
    expr = (struct Cst*)new_cst_node(Cst_Dontcare, token);
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

internal struct Cst*
build_tupleKeysetExpression()
{
  struct Cst* tuple_expr = 0;
  if (token->klass == Token_ParenthOpen) {
    next_token();
    struct Cst* prev_keyset = build_simpleKeysetExpression();
    tuple_expr = prev_keyset;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_keyset = build_simpleKeysetExpression();
      link_cst_nodes(prev_keyset, next_keyset);
      prev_keyset = next_keyset;
    }
    if (token->klass == Token_ParenthClose) {
      next_token();
    } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  return tuple_expr;
}

internal struct Cst*
build_keysetExpression()
{
  struct Cst* expr = 0;
  if (token->klass == Token_ParenthOpen) {
    expr = build_tupleKeysetExpression();
  } else if (token_is_simpleKeysetExpression(token)) {
    expr = build_simpleKeysetExpression();
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

internal struct Cst*
build_selectCase()
{
  struct Cst_SelectCase* select_case = 0;
  if (token_is_keysetExpression(token)) {
    select_case = new_cst_node(Cst_SelectCase, token);
    select_case->keyset = build_keysetExpression();
    if (token->klass == Token_Colon) {
      next_token();
      if (token_is_name(token)) {
        select_case->name = build_name(false);
        if (token->klass == Token_Semicolon) {
          next_token();
        } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)select_case;
}

internal struct Cst*
build_selectCaseList()
{
  struct Cst* case_list = 0;
  if (token_is_selectCase(token)) {
    struct Cst* prev_case = build_selectCase();
    case_list = prev_case;
    while (token_is_selectCase(token)) {
      struct Cst* next_case = build_selectCase();
      link_cst_nodes(prev_case, next_case);
      prev_case = next_case;
    }
  }
  return case_list;
}

internal struct Cst*
build_selectExpression()
{
  struct Cst_SelectExpr* select_expr = 0;
  if (token->klass == Token_Select) {
    next_token();
    select_expr = new_cst_node(Cst_SelectExpr, token);
    if (token->klass == Token_ParenthOpen) {
      next_token();
      select_expr->expr_list = build_expressionList();
      if (token->klass == Token_ParenthClose) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          select_expr->case_list = build_selectCaseList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `select` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)select_expr;
}

internal struct Cst*
build_stateExpression()
{
  struct Cst* state_expr = 0;
  if (token_is_name(token)) {
    state_expr = build_name(false);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else if (token->klass == Token_Select) {
    state_expr = build_selectExpression();
  } else error("at line %d: state expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return state_expr;
}

internal struct Cst*
build_transitionStatement()
{
  struct Cst* stmt = 0;
  if (token->klass == Token_Transition) {
    next_token();
    stmt = build_stateExpression();
  } else error("at line %d: `transition` was expected, got `%s`.", token->line_nr, token->lexeme);
  return stmt;
}

internal struct Cst*
build_parserState()
{
  struct Cst_ParserState* state = 0;
  if (token->klass == Token_State) {
    next_token();
    state = new_cst_node(Cst_ParserState, token);
    state->name = build_name(false);
    if (token->klass == Token_BraceOpen) {
      next_token();
      state->stmts = build_parserStatements();
      state->trans_stmt = build_transitionStatement();
      if (token->klass == Token_BraceClose) {
        next_token();
      } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `state` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)state;
}

internal struct Cst*
build_parserStates()
{
  struct Cst* states = 0;
  if (token->klass == Token_State) {
    struct Cst* prev_state = build_parserState();
    states = prev_state;
    while (token->klass == Token_State) {
      struct Cst* next_state = build_parserState();
      link_cst_nodes(prev_state, next_state);
      prev_state = next_state;
    }
  } else error("at line %d: `state` was expected, got `%s`.", token->line_nr, token->lexeme);
  return states;
}

internal struct Cst*
build_parserDeclaration()
{
  struct Cst_Parser* decl = 0;
  if (token->klass == Token_Parser) {
    decl = new_cst_node(Cst_Parser, token);
    decl->type_decl = build_parserTypeDeclaration();
    if (token->klass == Token_Semicolon) {
      next_token(); /* <parserTypeDeclaration> */
    } else {
      decl->ctor_params = build_optConstructorParameters();
      if (token->klass == Token_BraceOpen) {
        next_token();
        decl->local_elements = build_parserLocalElements();
        decl->states = build_parserStates();
        if (token->klass == Token_BraceClose) {
          next_token();
        } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
  } else error("at line %d: `parser` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_controlTypeDeclaration()
{
  struct Cst_ControlType* decl = 0;
  if (token->klass == Token_Control) {
    next_token();
    decl = new_cst_node(Cst_ControlType, token);
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
  } else error("at line %d: `control` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)decl;
}

internal struct Cst*
build_actionDeclaration()
{
  struct Cst_ActionDecl* decl = 0;
  if (token->klass == Token_Action) {
    next_token();
    decl = new_cst_node(Cst_ActionDecl, token);
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
  struct Cst_KeyElement* key_elem = 0;
  if (token_is_expression(token)) {
    key_elem = new_cst_node(Cst_KeyElement, token);
    key_elem->expr = build_expression(1);
    if (token->klass == Token_Colon) {
      next_token();
      key_elem->name = build_name(false);
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)key_elem;
}

internal struct Cst*
build_keyElementList()
{
  struct Cst* elem_list = 0;
  if (token_is_expression(token)) {
    struct Cst* prev_elem = build_keyElement();
    elem_list = prev_elem;
    while (token_is_expression(token)) {
      struct Cst* next_elem = build_keyElement();
      link_cst_nodes(prev_elem, next_elem);
      prev_elem = next_elem;
    }
  }
  return elem_list;
}

internal struct Cst*
build_actionRef()
{
  struct Cst_ActionRef* ref = 0;
  if (token->klass == Token_DotPrefix || token_is_nonTypeName(token)) {
    ref = new_cst_node(Cst_ActionRef, token);
    ref->name = build_prefixedNonTypeName();
    if (token->klass == Token_ParenthOpen) {
      next_token();
      ref->args = build_argumentList();
      if (token->klass == Token_ParenthClose) {
        next_token();
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
  } else error("at line %d: non-type name was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)ref;
}

internal struct Cst*
build_actionList()
{
  struct Cst* action_list = 0;
  if (token_is_actionRef(token)) {
    struct Cst* prev_action = build_actionRef();
    action_list = prev_action;
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    while (token_is_actionRef(token)) {
      struct Cst* next_action = build_actionRef();
      link_cst_nodes(prev_action, next_action);
      prev_action = next_action;
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    }
  }
  return action_list;
}

internal struct Cst*
build_entry()
{
  struct Cst_TableEntry* entry = 0;
  if (token_is_keysetExpression(token)) {
    entry = new_cst_node(Cst_TableEntry, token);
    entry->keyset = build_keysetExpression();
    if (token->klass == Token_Colon) {
      next_token();
      entry->action = build_actionRef();
      if (token->klass == Token_Semicolon) {
        next_token();
      } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: keyset was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)entry;
}

internal struct Cst*
build_entriesList()
{
  struct Cst* entry_list = 0;
  if (token_is_keysetExpression(token)) {
    struct Cst* prev_entry = build_entry();
    entry_list = prev_entry;
    while (token_is_keysetExpression(token)) {
      struct Cst* next_entry = build_entry();
      link_cst_nodes(prev_entry, next_entry);
      prev_entry = next_entry;
    }
  } else error("at line %d: keyset expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return entry_list;
}

internal struct Cst*
build_tableProperty()
{
  struct Cst* prop = 0;
  if (token_is_tableProperty(token)) {
    bool is_const = false;
    if (token->klass == Token_Const) {
      next_token();
      is_const = true;
    }
    if (token->klass == Token_Key) {
      next_token();
      struct Cst_TableProp_Key* key_prop = new_cst_node(Cst_TableProp_Key, token);
      prop = (struct Cst*)key_prop;
      if (token->klass == Token_Equal) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          key_prop->keyelem_list = build_keyElementList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Actions) {
      next_token();
      struct Cst_TableProp_Actions* actions_prop = new_cst_node(Cst_TableProp_Actions, token);
      prop = (struct Cst*)actions_prop;
      if (token->klass == Token_Equal) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          actions_prop->action_list = build_actionList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Entries) {
      next_token();
      struct Cst_TableProp_Entries* entries_prop = new_cst_node(Cst_TableProp_Entries, token);
      entries_prop->is_const = is_const;
      prop = (struct Cst*)entries_prop;
      if (token->klass == Token_Equal) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          entries_prop->entries = build_entriesList();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token_is_nonTableKwName(token)) {
      struct Cst_TableProp_SingleEntry* entry_prop = new_cst_node(Cst_TableProp_SingleEntry, token);
      entry_prop->name = build_name(false);
      prop = (struct Cst*)entry_prop;
      if (token->klass == Token_Equal) {
        next_token();
        entry_prop->init_expr = build_initializer();
        if (token->klass == Token_Semicolon) {
          next_token();
        } else error("at line %d: `;` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `=` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else assert(0);
  } else error("at line %d: table property was expected, got `%s`.", token->line_nr, token->lexeme);
  return prop;
}

internal struct Cst*
build_tablePropertyList()
{
  struct Cst* prop_list = 0;
  if (token_is_tableProperty(token)) {
    struct Cst* prev_prop = build_tableProperty();
    prop_list = prev_prop;
    while (token_is_tableProperty(token)) {
      struct Cst* next_prop = build_tableProperty();
      link_cst_nodes(prev_prop, next_prop);
      prev_prop = next_prop;
    }
  } else error("at line %d: table property was expected, got `%s`.", token->line_nr, token->lexeme);
  return prop_list;
}

internal struct Cst*
build_tableDeclaration()
{
  struct Cst_TableDecl* table = 0;
  if (token->klass == Token_Table) {
    next_token();
    table = new_cst_node(Cst_TableDecl, token);
    table->name = build_name(false);
    if (token->klass == Token_BraceOpen) {
      next_token();
      table->prop_list = build_tablePropertyList();
      if (token->klass == Token_BraceClose) {
        next_token();
      } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `table` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)table;
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
  struct Cst* decls = 0;
  if (token_is_controlLocalDeclaration(token)) {
    struct Cst* prev_decl = build_controlLocalDeclaration();
    decls = prev_decl;
    while (token_is_controlLocalDeclaration(token)) {
      struct Cst* next_decl = build_controlLocalDeclaration();
      link_cst_nodes(prev_decl, next_decl);
      prev_decl = next_decl;
    }
  }
  return decls;
}

internal struct Cst*
build_controlDeclaration()
{
  struct Cst_Control* decl = 0;
  if (token->klass == Token_Control) {
    decl = new_cst_node(Cst_Control, token);
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
    next_token();
    decl = new_cst_node(Cst_Package, token);
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
    bool is_typedef = true;
    if (token->klass == Token_Typedef) {
      next_token();
    } else if (token->klass == Token_Type) {
      is_typedef = false;
      next_token();
    } else assert(0);

    if (token_is_typeRef(token) || token_is_derivedTypeDeclaration(token)) {
      struct Cst_TypeDecl* type_decl = new_cst_node(Cst_TypeDecl, token);
      type_decl->is_typedef = is_typedef;
      decl = (struct Cst*)type_decl;
      if (token_is_typeRef(token)) {
        type_decl->type_ref = build_typeRef();
      } else if (token_is_derivedTypeDeclaration(token)) {
        type_decl->type_ref = build_derivedTypeDeclaration();
      } else assert(0);

      if (token_is_name(token)) {
        type_decl->name = build_name(true);
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
  struct Cst_IfStmt* if_stmt = 0;
  if (token->klass == Token_If) {
    next_token();
    if_stmt = new_cst_node(Cst_IfStmt, token);
    if (token->klass == Token_ParenthOpen) {
      next_token();
      if (token_is_expression(token)) {
        if_stmt->cond_expr = build_expression(1);
        if (token->klass == Token_ParenthClose) {
          next_token();
          if (token_is_statement(token)) {
            if_stmt->stmt = build_statement();
            if (token->klass == Token_Else) {
              next_token();
              if (token_is_statement(token)) {
                if_stmt->else_stmt = build_statement();
              } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
            }
          } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `if` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)if_stmt;
}

internal struct Cst*
build_exitStatement()
{
  struct Cst_ExitStmt* exit_stmt = 0;
  if (token->klass == Token_Exit) {
    next_token();
    exit_stmt = new_cst_node(Cst_ExitStmt, token);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `exit` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)exit_stmt;
}

internal struct Cst*
build_returnStatement()
{
  struct Cst_ReturnStmt* ret_stmt = 0;
  if (token->klass == Token_Return) {
    next_token();
    ret_stmt = new_cst_node(Cst_ReturnStmt, token);
    if (token_is_expression(token))
      ret_stmt->expr = build_expression(1);
    if (token->klass == Token_Semicolon) {
      next_token();
    } else error("at line %d: `;` expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `return` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)ret_stmt;
}

internal struct Cst*
build_switchLabel()
{
  struct Cst* label = 0;
  if (token_is_name(token)) {
    struct Cst_SwitchLabel* name_label = new_cst_node(Cst_SwitchLabel, token);
    label = (struct Cst*)name_label;
    name_label->name = build_name(false);
  } else if (token->klass == Token_Default) {
    next_token();
    label = (struct Cst*)new_cst_node(Cst_Default, token);
  } else error("at line %d: switch label was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)label;
}

internal struct Cst*
build_switchCase()
{
  struct Cst_SwitchCase* switch_case = 0;
  if (token_is_switchLabel(token)) {
    switch_case = new_cst_node(Cst_SwitchCase, token);
    switch_case->label = build_switchLabel();
    if (token->klass == Token_Colon) {
      next_token();
      if (token->klass == Token_BraceOpen) {
        switch_case->stmt = build_blockStatement();
      }
    } else error("at line %d: `:` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: switch label was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)switch_case;
}

internal struct Cst*
build_switchCases()
{
  struct Cst* switch_cases = 0;
  if (token_is_switchLabel(token)) {
    struct Cst* prev_case = build_switchCase();
    switch_cases = prev_case;
    while (token_is_switchLabel(token)) {
      struct Cst* next_case = build_switchCase();
      link_cst_nodes(prev_case, next_case);
      prev_case = next_case;
    }
  }
  return switch_cases;
}

internal struct Cst*
build_switchStatement()
{
  struct Cst_SwitchStmt* stmt = 0;
  if (token->klass == Token_Switch) {
    next_token();
    stmt = new_cst_node(Cst_SwitchStmt, token);
    if (token->klass == Token_ParenthOpen) {
      next_token();
      stmt->expr = build_expression(1);
      if (token->klass == Token_ParenthClose) {
        next_token();
        if (token->klass == Token_BraceOpen) {
          next_token();
          stmt->switch_cases = build_switchCases();
          if (token->klass == Token_BraceClose) {
            next_token();
          } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
        } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else error("at line %d: `(` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `switch` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)stmt;
}

internal struct Cst*
build_statement()
{
  struct Cst* stmt = 0;
  if (token_is_assignmentOrMethodCallStatement(token)) {
    stmt = build_assignmentOrMethodCallStatement();
  } else if (token_is_typeName(token)) {
    stmt = build_directApplication();
  } else if (token->klass == Token_If) {
    stmt = build_conditionalStatement();
  } else if (token->klass == Token_Semicolon) {
    next_token();
    stmt = (struct Cst*)new_cst_node(Cst_EmptyStmt, token);
  } else if (token->klass == Token_BraceOpen) {
    stmt = build_blockStatement();
  } else if (token->klass == Token_Exit) {
    stmt = build_exitStatement();
  } else if (token->klass == Token_Return) {
    stmt = build_returnStatement();
  } else if (token->klass == Token_Switch) {
    stmt = build_switchStatement();
  } else error("at line %d: statement was expected, got `%s`.", token->line_nr, token->lexeme);
  return stmt;
}

internal struct Cst*
build_statementOrDecl()
{
  struct Cst* stmt = 0;
  if (token_is_statementOrDeclaration(token)) {
    if (token_is_typeRef(token)) {
      if (peek_token()->klass == Token_ParenthOpen) {
        stmt = build_instantiation();
      } else {
        stmt = build_variableDeclaration();
      }
    } else if (token_is_statement(token)) {
      stmt = build_statement();
    } else if (token->klass == Token_Const) {
      stmt = build_constantDeclaration();
    } else assert(0);
  }
  return stmt;
}

internal struct Cst*
build_statementOrDeclList()
{
  struct Cst* stmt_list = 0;
  if (token_is_statementOrDeclaration(token)) {
    struct Cst* prev_stmt = build_statementOrDecl();
    stmt_list = prev_stmt;
    while (token_is_statementOrDeclaration(token)) {
      struct Cst* next_stmt = build_statementOrDecl();
      link_cst_nodes(prev_stmt, next_stmt);
      prev_stmt = next_stmt;
    }
  }
  return stmt_list;
}

internal struct Cst*
build_blockStatement()
{
  struct Cst_BlockStmt* stmt = 0;
  if (token->klass == Token_BraceOpen) {
    next_token();
    stmt = new_cst_node(Cst_BlockStmt, token);
    stmt->stmt_list = build_statementOrDeclList();
    if (token->klass == Token_BraceClose) {
      next_token();
    } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
  } else error("at line %d: `{` was expected, got `%s`.", token->line_nr, token->lexeme);
  return (struct Cst*)stmt;
}

internal struct Cst*
build_identifierList()
{
  struct Cst* id_list = 0;
  if (token_is_name(token)) {
    struct Cst* prev_id = build_name(false);
    id_list = prev_id;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_id = build_name(false);
      link_cst_nodes(prev_id, next_id);
      prev_id = next_id;
    }
  } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
  return id_list;
}

internal struct Cst*
build_errorDeclaration()
{
  struct Cst_Error* decl = 0;
  if (token->klass == Token_Error) {
    next_token();
    decl = new_cst_node(Cst_Error, token);
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
    next_token();
    decl = new_cst_node(Cst_MatchKind, token);
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
    } else assert(0);
  } else error("at line %d: top-level declaration as expected, got `%s`.", token->line_nr, token->lexeme);
  return decl;
}

internal struct Cst*
build_p4program()
{
  struct Cst_P4Program* prog = new_cst_node(Cst_P4Program, token);
  struct Cst_EmptyStmt sentinel_decl = {};
  struct Cst* prev_decl = (struct Cst*)&sentinel_decl;
  while (token_is_declaration(token) || token->klass == Token_Semicolon) {
    if (token_is_declaration(token)) {
      struct Cst* next_decl = build_declaration();
      link_cst_nodes(prev_decl, next_decl);
      prev_decl = next_decl;
    } else if (token->klass == Token_Semicolon) {
      next_token(); /* empty declaration */
    }
  }
  struct Cst* first_decl = sentinel_decl.next_node;
  first_decl->prev_node = 0;
  prog->decl_list = first_decl;
  if (token->klass != Token_EndOfInput) {
    error("at line %d: unexpected token `%s`.", token->line_nr, token->lexeme);
  }
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
    || token->klass == Token_AngleOpenEqual || token->klass == Token_AngleCloseEqual
    || token->klass == Token_AngleOpen || token->klass == Token_AngleClose
    || token->klass == Token_ExclamationEqual || token->klass == Token_TwoEqual
    || token->klass == Token_TwoPipe || token->klass == Token_TwoAmpersand
    || token->klass == Token_Pipe || token->klass == Token_Ampersand
    || token->klass == Token_Circumflex || token->klass == Token_TwoAngleOpen
    || token->klass == Token_TwoAngleClose;
  return result;
}

internal bool
token_is_exprOperator(struct Token* token)
{
  bool result = token_is_binaryOperator(token) || token->klass == Token_DotPrefix
    || token->klass == Token_BracketOpen || token->klass == Token_ParenthOpen
    || token->klass == Token_AngleOpen;
  return result;
}

internal struct Cst*
build_realTypeArg()
{
  struct Cst* arg = 0;
  if (token->klass == Token_Dontcare) {
    next_token();
    arg = (struct Cst*)new_cst_node(Cst_Dontcare, token);
  } else if (token_is_typeRef(token)) {
    arg = build_typeRef();
  } else error("at line %d: type argument was expected, got `%s`.", token->line_nr, token->lexeme);
  return arg;
}

internal struct Cst*
build_realTypeArgumentList()
{
  struct Cst* args = 0;
  if (token_is_realTypeArg(token)) {
    struct Cst* prev_arg = build_realTypeArg();
    args = prev_arg;
    while (token->klass == Token_Comma) {
      next_token();
      struct Cst* next_arg = build_realTypeArg();
      link_cst_nodes(prev_arg, next_arg);
      prev_arg = next_arg;
    }
  }
  return args;
}

internal struct Cst*
build_expressionPrimary()
{
  struct Cst* primary = 0;
  if (token_is_expression(token)) {
    if (token->klass == Token_Integer) {
      primary = build_integer();
    } else if (token->klass == Token_True) {
      primary = build_boolean();
    } else if (token->klass == Token_False) {
      primary = build_boolean();
    } else if (token->klass == Token_StringLiteral) {
      primary = build_stringLiteral();
    } else if (token->klass == Token_DotPrefix) {
      next_token();
      struct Cst_DotPrefixedName* dot_name = new_cst_node(Cst_DotPrefixedName, token);
      dot_name->name = build_nonTypeName(false);
      primary = (struct Cst*)dot_name;
    } else if (token_is_nonTypeName(token)) {
      primary = (struct Cst*)build_nonTypeName(false);
    } else if (token->klass == Token_BraceOpen) {
      next_token();
      struct Cst_ExpressionListExpr* expr_list = new_cst_node(Cst_ExpressionListExpr, token);
      expr_list->expr_list = build_expressionList();
      primary = (struct Cst*)expr_list;
      if (token->klass == Token_BraceClose) {
        next_token();
      } else error("at line %d: `}` was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_ParenthOpen) {
      next_token();
      if (token_is_typeRef(token)) {
        struct Cst_CastExpr* cast = new_cst_node(Cst_CastExpr, token);
        cast->to_type = build_typeRef();
        primary = (struct Cst*)cast;
        if (token->klass == Token_ParenthClose) {
          next_token();
          cast->expr = build_expression(1);
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else if (token_is_expression(token)) {
        primary = build_expression(1);
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
    } else if (token->klass == Token_Exclamation) {
      next_token();
      struct Cst_UnaryExpr* unary_expr = new_cst_node(Cst_UnaryExpr, token);
      unary_expr->op = AstUnOp_LogNot;
      unary_expr->expr = build_expression(1);
      primary = (struct Cst*)unary_expr;
    } else if (token->klass == Token_Tilda) {
      next_token();
      struct Cst_UnaryExpr* unary_expr = new_cst_node(Cst_UnaryExpr, token);
      unary_expr->op = AstUnOp_BitNot;
      unary_expr->expr = build_expression(1);
      primary = (struct Cst*)unary_expr;
    } else if (token->klass == Token_UnaryMinus) {
      next_token();
      struct Cst_UnaryExpr* unary_expr = new_cst_node(Cst_UnaryExpr, token);
      unary_expr->op = AstUnOp_ArMinus;
      unary_expr->expr = build_expression(1);
      primary = (struct Cst*)unary_expr;
    } else if (token_is_typeName(token)) {
      primary = build_typeName();
    } else if (token->klass == Token_Error) {
      next_token();
      struct Cst_NonTypeName* name = new_cst_node(Cst_NonTypeName, token);
      name->name = token->lexeme;
      primary = (struct Cst*)name;
    } else assert(0);
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return primary;
}

internal int
get_operator_priority(struct Token* token)
{
  int prio = 0;
  if (token->klass == Token_TwoAmpersand || token->klass == Token_TwoPipe) {
    prio = 1;
  } else if (token->klass == Token_TwoEqual || token->klass == Token_ExclamationEqual
      || token->klass == Token_AngleOpen /* Less */ || token->klass == Token_AngleClose /* Greater */
      || token->klass == Token_AngleOpenEqual /* LessEqual */ || token->klass == Token_AngleCloseEqual /* GreaterEqual */) {
    prio = 2;
  }
  else if (token->klass == Token_Plus || token->klass == Token_Minus
           || token->klass == Token_Ampersand || token->klass == Token_Pipe
           || token->klass == Token_Circumflex || token->klass == Token_TwoAngleOpen /* BitshiftLeft */
           || token->klass == Token_TwoAngleClose /* BitshiftRight */) {
    prio = 3;
  }
  else if (token->klass == Token_Star || token->klass == Token_Slash) {
    prio = 4;
  }
  else assert(0);
  return prio;
}

internal enum AstExprOperator
token_to_binop(struct Token* token)
{
  switch (token->klass) {
    case Token_TwoAmpersand:
      return AstBinOp_LogAnd;
    case Token_TwoPipe:
      return AstBinOp_LogOr;
    case Token_TwoEqual:
      return AstBinOp_LogEqual;
    case Token_ExclamationEqual:
      return AstBinOp_LogNotEqual;
    case Token_AngleOpen:
      return AstBinOp_LogLess;
    case Token_AngleClose:
      return AstBinOp_LogGreater;
    case Token_AngleOpenEqual:
      return AstBinOp_LogLessEqual;
    case Token_AngleCloseEqual:
      return AstBinOp_LogGreaterEqual;
    case Token_Plus:
      return AstBinOp_ArAdd;
    case Token_Minus:
      return AstBinOp_ArSub;
    case Token_Star:
      return AstBinOp_ArMul;
    case Token_Slash:
      return AstBinOp_ArDiv;
    case Token_Ampersand:
      return AstBinOp_BitAnd;
    case Token_Pipe:
      return AstBinOp_BitOr;
    case Token_Circumflex:
      return AstBinOp_BitXor;
    case Token_TwoAngleOpen:
      return AstBinOp_BitShLeft;
    case Token_TwoAngleClose:
      return AstBinOp_BitShRight;
    default: return AstOp_None;
  }
}

internal struct Cst*
build_expression(int priority_threshold)
{
  struct Cst* expr = 0;
  if (token_is_expression(token)) {
    expr = build_expressionPrimary();
    while (token_is_exprOperator(token)) {
      if (token->klass == Token_DotPrefix) {
        next_token();
        struct Cst_MemberSelectExpr* select_expr = new_cst_node(Cst_MemberSelectExpr, token);
        select_expr->expr = expr;
        expr = (struct Cst*)select_expr;
        if (token_is_name(token)) {
          select_expr->member_name = build_name(false);
        } else error("at line %d: name was expected, got `%s`.", token->line_nr, token->lexeme);
      }
      else if (token->klass == Token_BracketOpen) {
        next_token();
        struct Cst_IndexedArrayExpr* index_expr = new_cst_node(Cst_IndexedArrayExpr, token);
        index_expr->expr = expr;
        index_expr->index_expr = build_arrayIndex();
        expr = (struct Cst*)index_expr;
        if (token->klass == Token_BracketClose) {
          next_token();
        } else error("at line %d: `]` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
      else if (token->klass == Token_ParenthOpen) {
        next_token();
        struct Cst_FunctionCallExpr* call_expr = new_cst_node(Cst_FunctionCallExpr, token);
        call_expr->expr = expr;
        call_expr->args = build_argumentList();
        expr = (struct Cst*)call_expr;
        if (token->klass == Token_ParenthClose) {
          next_token();
        } else error("at line %d: `)` was expected, got `%s`.", token->line_nr, token->lexeme);
      }
      else if (token->klass == Token_AngleOpen && token_is_realTypeArg(peek_token())) {
        next_token();
        struct Cst_TypeArgsExpr* args_expr = new_cst_node(Cst_TypeArgsExpr, token);
        args_expr->expr = expr;
        args_expr->type_args = build_realTypeArgumentList();
        expr = (struct Cst*)args_expr;
        if (token->klass == Token_AngleClose) {
          next_token();
        } else error("at line %d: `>` was expected, got `%s`.", token->line_nr, token->lexeme);
      } else if (token_is_binaryOperator(token)){
        int priority = get_operator_priority(token);
        if (priority >= priority_threshold) {
          struct Cst_BinaryExpr* bin_expr = new_cst_node(Cst_BinaryExpr, token);
          bin_expr->left_operand = expr;
          bin_expr->op = token_to_binop(token);
          next_token();
          bin_expr->right_operand = build_expression(priority + 1);
          expr = (struct Cst*)bin_expr;
        } else break;
      } else assert(0);
    }
  } else error("at line %d: an expression was expected, got `%s`.", token->line_nr, token->lexeme);
  return expr;
}

internal void
init_symtable()
{
  symtable = arena_push_array(&arena, struct Symtable_Entry*, max_symtable_len);
  int i = 0;
  while (i < max_symtable_len) {
    symtable[i++] = 0;
  }
}

struct Cst*
build_cst(struct Token* tokens_, int token_count_)
{
  tokens = tokens_;
  token_count = token_count_;
  init_symtable();
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

  token = tokens;
  next_token();
  return (struct Cst*)build_p4program();
}