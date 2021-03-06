#define DEBUG_ENABLED 1

#include "basic.h"
#include "arena.h"
#include "lex.h"
#include "build_ast.h"
#include <sys/stat.h>
#include <memory.h>  // memset


internal struct Arena main_storage = {};


struct CmdlineArg {
  char* name;
  char* value;
  struct CmdlineArg* next_arg;
};

internal void
read_source(char** text_, int* text_size_, struct Arena* text_storage, char* filename)
{
  FILE* f_stream = fopen(filename, "rb");
  fseek(f_stream, 0, SEEK_END);
  int text_size = ftell(f_stream);
  fseek(f_stream, 0, SEEK_SET);
  char* text = arena_push(text_storage, (text_size + 1)*sizeof(char));
  fread(text, sizeof(char), text_size, f_stream);
  text[text_size] = '\0';
  fclose(f_stream);
  *text_ = text;
  *text_size_ = text_size;
}

internal struct CmdlineArg*
find_unnamed_arg(struct CmdlineArg* args)
{
  struct CmdlineArg* unnamed_arg = 0;
  struct CmdlineArg* arg = args;
  while (arg) {
    if (!arg->name) {
      unnamed_arg = arg;
      break;
    }
    arg = arg->next_arg;
  }
  return unnamed_arg;
}

internal struct CmdlineArg*
find_named_arg(char* name, struct CmdlineArg* args)
{
  struct CmdlineArg* named_arg = 0;
  struct CmdlineArg* arg = args;
  while (arg) {
    if (arg->name && cstr_match(name, arg->name)) {
      named_arg = arg;
      break;
    }
    arg = arg->next_arg;
  }
  return named_arg;
}

internal struct CmdlineArg*
parse_cmdline_args(int arg_count, char* args[])
{
  struct CmdlineArg* arg_list = 0;
  if (arg_count <= 1) {
    return arg_list;
  }
  
  struct CmdlineArg sentinel_arg = {};
  struct CmdlineArg* prev_arg = &sentinel_arg;
  int i = 1;
  while (i < arg_count) {
    struct CmdlineArg* cmdline_arg = arena_push(&main_storage, sizeof(*cmdline_arg));
    memset(cmdline_arg, 0, sizeof(*cmdline_arg));
    if (cstr_start_with(args[i], "--")) {
      char* raw_arg = args[i] + 2;  /* skip the `--` prefix */
      cmdline_arg->name = raw_arg;
    } else {
      cmdline_arg->value = args[i];
    }
    prev_arg->next_arg = cmdline_arg;
    prev_arg = cmdline_arg;
    i += 1;
  }
  arg_list = sentinel_arg.next_arg;
  return arg_list;
}

int
main(int arg_count, char* args[])
{
  init_memory(400*KILOBYTE);

  struct CmdlineArg* cmdline_args = parse_cmdline_args(arg_count, args);
  struct CmdlineArg* filename_arg = find_unnamed_arg(cmdline_args);
  if (!filename_arg) {
    printf("<filename> is required.\n");
    exit(1);
  }
  struct Arena text_storage = {};
  char* text = 0;
  int text_size = 0;
  read_source(&text, &text_size, &text_storage, filename_arg->value);

  struct Arena tokens_storage = {};
  struct UnboundedArray tokens_array = {};
  lex_set_storage(&main_storage, &tokens_storage);
  lex_tokenize(text, text_size, &tokens_array);
  arena_delete(&text_storage);

  struct Arena symtable_storage = {};
  symtable_set_storage(&symtable_storage);
  symtable_init();

  struct Arena ast_storage = {};
  int ast_node_count = 0;
  struct Ast* ast_program = build_ast_program(&ast_program, &ast_node_count, &tokens_array, &ast_storage);
  assert(ast_program && ast_program->kind == Ast_P4Program);
  arena_delete(&tokens_storage);

  if (find_named_arg("print-ast", cmdline_args)) {
    print_ast(ast_program);
  }

  if (DEBUG_ENABLED) {
    printf("\n-- Build the symbol table --\n");
  }
  symtable_flush();
  build_symtable_program(ast_program);

  arena_delete(&ast_storage);
  arena_delete(&main_storage);
  return 0;
}

