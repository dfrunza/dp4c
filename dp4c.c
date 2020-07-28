#include "dp4c.h"
#include "lex.h"
#include "syntax.h"

Arena arena = {};
char* input_text = 0;
int input_size = 0;
Token* tokenized_input = 0;
int tokenized_input_len = 0;
int max_tokenized_input_len = 1000;
int max_symtable_len = 997;
SymbolTable_Entry** symtable = 0;
int scope_level = 0;
TypeTable_Entry* typetable = 0;
TypeTable_Entry* typetable_free = 0;
int max_typetable_len = 1000;
Ast_P4Program* p4program = 0;

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
  arena_print_usage(&arena, "Memory (read_input): ");
}

int
main(int arg_count, char* args[])
{
  arena_new(&arena, 192*KILOBYTE);

  read_input("test.p4");
  tokenized_input = arena_push_array(&arena, Token, max_tokenized_input_len);
  lex_input_init(input_text);
  lex_tokenize_input();
  symtable = arena_push_array(&arena, SymbolTable_Entry*, max_symtable_len);
  int i = 0;
  while (i < max_symtable_len)
    symtable[i++] = 0;
  syn_parse();
  typetable = arena_push_array(&arena, TypeTable_Entry, max_typetable_len);
  typetable_free = typetable;
  build_typetable();
  arena_print_usage(&arena, "Memory (@exit): ");
  return 0;
}

