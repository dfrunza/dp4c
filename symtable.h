#pragma once
#include "dp4c.h"

Namespace_Entry* sym_get_namespace(char* name);
Ident* sym_get_var(char* name);
Ident* sym_new_var(char* name, Ast* ast);
Ident* sym_get_type(char* name);
Ident* sym_new_type(char* name, Ast* ast);
Ident* sym_new_typevar(char* name, Ast* ast);
void sym_import_var(Ident* var_ident);
void sym_import_type(Ident* type_ident);
Ident* sym_get_error_type();
void sym_unimport_var(Ident* var_ident);
void sym_unimport_type(Ident* type_ident);
int scope_push_level();
int scope_pop_level(int to_level);
bool sym_ident_is_declared(Ident* ident);
Ident_Keyword* add_keyword(char* name, enum TokenClass token_klass);
