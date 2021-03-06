#include "arena.h"
#include "ast.h"


internal int tab_level = 0;
internal int tab_size = 2;


enum ValueType {
  Value_NONE_,
  Value_Integer,
  Value_String,
  Value_Id,
  Value_IdList,
};

internal void print_prop(char* name, enum ValueType type, ...);


internal char*
ast_kind_to_string(enum AstKind kind)
{
  switch (kind) {
    case Ast_Name:
      return "Ast_Name";
    case Ast_BaseType:
      return "Ast_BaseType";
    case Ast_ConstDecl:
      return "Ast_ConstDecl";
    case Ast_ExternDecl:
      return "Ast_ExternDecl";
    case Ast_FunctionProto:
      return "Ast_FunctionProto";
    case Ast_ActionDecl:
      return "Ast_ActionDecl";
    case Ast_HeaderDecl:
      return "Ast_HeaderDecl";
    case Ast_HeaderUnionDecl:
      return "Ast_HeaderUnionDecl";
    case Ast_StructDecl:
      return "Ast_StructDecl";
    case Ast_EnumDecl:
      return "Ast_EnumDecl";
    case Ast_Parser:
      return "Ast_Parser";
    case Ast_Control:
      return "Ast_Control";
    case Ast_Package:
      return "Ast_Package";
    case Ast_Instantiation:
      return "Ast_Instantiation";
    case Ast_Error:
      return "Ast_Error";
    case Ast_MatchKind:
      return "Ast_MatchKind";
    case Ast_FunctionDecl:
      return "Ast_FunctionDecl";
    case Ast_Dontcare:
      return "Ast_Dontcare";
    case Ast_IntTypeSize:
      return "Ast_IntTypeSize";
    case Ast_Int:
      return "Ast_Int";
    case Ast_Bool:
      return "Ast_Bool";
    case Ast_StringLiteral:
      return "Ast_StringLiteral";
    case Ast_Tuple:
      return "Ast_Tuple";
    case Ast_TupleKeyset:
      return "Ast_TupleKeyset";
    case Ast_HeaderStack:
      return "Ast_HeaderStack";
    case Ast_SpecdType:
      return "Ast_SpecdType";
    case Ast_StructField:
      return "Ast_StructField";
    case Ast_SpecdId:
      return "Ast_SpecdId";
    case Ast_ParserType:
      return "Ast_ParserType";
    case Ast_Argument:
      return "Ast_Argument";
    case Ast_VarDecl:
      return "Ast_VarDecl";
    case Ast_DirectApplic:
      return "Ast_DirectApplic";
    case Ast_ArrayIndex:
      return "Ast_ArrayIndex";
    case Ast_Parameter:
      return "Ast_Parameter";
    case Ast_Lvalue:
      return "Ast_Lvalue";
    case Ast_AssignmentStmt:
      return "Ast_AssignmentStmt";
    case Ast_MethodCallStmt:
      return "Ast_MethodCallStmt";
    case Ast_EmptyStmt:
      return "Ast_EmptyStmt";
    case Ast_Default:
      return "Ast_Default";
    case Ast_SelectExpr:
      return "Ast_SelectExpr";
    case Ast_SelectCase:
      return "Ast_SelectCase";
    case Ast_ParserState:
      return "Ast_ParserState";
    case Ast_ControlType:
      return "Ast_ControlType";
    case Ast_KeyElement:
      return "Ast_KeyElement";
    case Ast_ActionRef:
      return "Ast_ActionRef";
    case Ast_TableEntry:
      return "Ast_TableEntry";
    case Ast_TableProp_Key:
      return "Ast_TableProp_Key";
    case Ast_TableProp_Actions:
      return "Ast_TableProp_Actions";
    case Ast_TableProp_Entries:
      return "Ast_TableProp_Entries";
    case Ast_TableProp_SingleEntry:
      return "Ast_TableProp_SingleEntry";
    case Ast_TableDecl:
      return "Ast_TableDecl";
    case Ast_IfStmt:
      return "Ast_IfStmt";
    case Ast_ExitStmt:
      return "Ast_ExitStmt";
    case Ast_ReturnStmt:
      return "Ast_ReturnStmt";
    case Ast_SwitchLabel:
      return "Ast_SwitchLabel";
    case Ast_SwitchCase:
      return "Ast_SwitchCase";
    case Ast_SwitchStmt:
      return "Ast_SwitchStmt";
    case Ast_BlockStmt:
      return "Ast_BlockStmt";
    case Ast_ExpressionListExpr:
      return "Ast_ExpressionListExpr";
    case Ast_CastExpr:
      return "Ast_CastExpr";
    case Ast_UnaryExpr:
      return "Ast_UnaryExpr";
    case Ast_BinaryExpr:
      return "Ast_BinaryExpr";
    case Ast_KvPair:
      return "Ast_KvPair";
    case Ast_MemberSelectExpr:
      return "Ast_MemberSelectExpr";
    case Ast_IndexedArrayExpr:
      return "Ast_IndexedArrayExpr";
    case Ast_FunctionCallExpr:
      return "Ast_FunctionCallExpr";
    case Ast_TypeArgsExpr:
      return "Ast_TypeArgsExpr";
    case Ast_P4Program:
      return "Ast_P4Program";
    case Ast_TypeDecl:
      return "Ast_TypeDecl";
    default:
      assert(0);
  }
  return 0;
}

internal char*
expr_operator_to_string(enum AstExprOperator op)
{
  char* op_str = "Ast_Op_None";
  if (op == AstExprOp_Add) {
    op_str = "AstExprOp_Add";
  } else if (op == AstExprOp_Sub) {
    op_str = "AstExprOp_Sub";
  } else if (op == AstExprOp_Mul) {
    op_str = "AstExprOp_Mul";
  } else if (op == AstExprOp_Div) {
    op_str = "AstExprOp_Div";
  } else if (op == AstExprOp_And) {
    op_str = "AstExprOp_And";
  } else if (op == AstExprOp_Or) {
    op_str = "AstExprOp_Or";
  } else if (op == AstExprOp_Equal) {
    op_str = "AstExprOp_Equal";
  } else if (op == AstExprOp_NotEqual) {
    op_str = "AstExprOp_NotEqual";
  } else if (op == AstExprOp_Less) {
    op_str = "AstExprOp_Less";
  } else if (op == AstExprOp_Greater) {
    op_str = "AstExprOp_Greater";
  } else if (op == AstExprOp_LessEqual) {
    op_str = "AstExprOp_LessEqual";
  } else if (op == AstExprOp_GreaterEqual) {
    op_str = "AstExprOp_GreaterEqual";
  } else if (op == AstExprOp_BitAnd) {
    op_str = "AstExprOp_BitAnd";
  } else if (op == AstExprOp_BitOr) {
    op_str = "AstExprOp_BitOr";
  } else if (op == AstExprOp_BitXor) {
    op_str = "AstExprOp_BitXor";
  } else if (op == AstExprOp_BitShiftLeft) {
    op_str = "AstExprOp_BitShiftLeft";
  } else if (op == AstExprOp_BitShiftRight) {
    op_str = "AstExprOp_BitShiftRight";
  } else if (op == AstExprOp_LogNot) {
    op_str = "AstExprOp_LogNot";
  } else if (op == AstExprOp_BitNot) {
    op_str = "AstExprOp_BitNot";
  } else if (op == AstExprOp_Minus) {
    op_str = "AstExprOp_Minus";
  } else if (op == AstExprOp_Mask) {
    op_str = "AstExprOp_Mask";
  }
  else assert(op == AstExprOp_NONE_);
  return op_str;
}

internal void
indent_right()
{
  int i = 0;
  for (; i < tab_level*tab_size; i++) {
    printf(" ");
  }
}

internal void
ast_start()
{
  printf("{\n");
  tab_level++;
}

internal void
ast_end()
{
  tab_level--;
  printf("}\n");
}

internal void
list_open()
{
  printf("[");
}

internal void
list_close()
{
  printf("]");
}

internal void
print_nl()
{
  printf("\n");
}

internal void
print_value(enum ValueType type, va_list value)
{
  if (type == Value_Integer) {
    int i = va_arg(value, int);
    printf("%d", i);
  } else if (type == Value_String) {
    char* s = va_arg(value, char*);
    printf("%s", s);
  } else if (type == Value_Id) {
    int id = va_arg(value, int);
    printf("$%d", id);
  }
  else assert(0);
}

internal void
print_prop(char* name, enum ValueType type, ...)
{
  indent_right();
  printf("%s: ", name);
  if (type != Value_NONE_) {
    va_list value;
    va_start(value, type);
    if (type == Value_IdList) {
      struct AstList* list = va_arg(value, struct AstList*);
      list_open();
      if (list) {
        struct AstListLink* link = list->head->next;
        while (link) {
          printf("$%d", link->ast->id);
          if (link->next) {
            printf(", ");
          }
          link = link->next;
        }
      }
      list_close();
    } else {
      print_value(type, value);
    }
    print_nl();
    va_end(value);
  }
}

void
print_parameter(struct Ast* param)
{
  assert(param->kind == Ast_Parameter);
  ast_start();
  char* dir_str = "AstDir_NONE_";
  if (*(enum AstParamDirection*)ast_getattr(param, "direction") == AstParamDir_In) {
    dir_str = "AstParamDir_In";
  } else if (*(enum AstParamDirection*)ast_getattr(param, "direction") == AstParamDir_Out) {
    dir_str = "AstParamDir_Out";
  } else if (*(enum AstParamDirection*)ast_getattr(param, "direction") == AstParamDir_InOut) {
    dir_str = "AstParamDir_InOut";
  } else assert(*(enum AstParamDirection*)ast_getattr(param, "direction") == AstParamDir_NONE_);
  print_prop("direction", Value_String, dir_str);
  print_prop("type", Value_Id, ((struct Ast*)ast_getattr(param, "type"))->id);
  print_prop("name", Value_Id, ((struct Ast*)ast_getattr(param, "name"))->id);
  print_prop("init_expr", Value_Id, ((struct Ast*)ast_getattr(param, "init_expr"))->id);
  ast_end();
  print_ast(ast_getattr(param, "type"));
  print_ast(ast_getattr(param, "name"));
  print_ast(ast_getattr(param, "init_expr"));
}

internal void
print_int(struct Ast* node)
{
  assert(node->kind == Ast_Int);
  ast_start();
  char flags_str[256];
  char* str = flags_str + sprintf(flags_str, "AstInteger_NONE_");
  if ((*(int*)ast_getattr(node, "flags") & AstInteger_HasWidth) != 0) {
    str += sprintf(str, "|%s", "AstInteger_HasWidth");
  }
  if ((*(int*)ast_getattr(node, "flags") & AstInteger_IsSigned) != 0) {
    str += sprintf(str, "|%s", "AstInteger_IsSigned");
  }
  print_prop("flags", Value_String, flags_str);
  print_prop("width", Value_Integer, *(int*)ast_getattr(node, "width"));
  print_prop("value", Value_Integer, *(int*)ast_getattr(node, "value"));
  ast_end();
}

internal void
print_base_type(struct Ast* type)
{
  assert(type->kind == Ast_BaseType);
  ast_start();
  char* type_str = "AstBaseType_NONE_";
  if (*(enum AstBaseTypeKind*)ast_getattr(type, "base_type") == AstBaseType_Bool) {
    type_str = "AstBaseType_Bool";
  } else if (*(enum AstBaseTypeKind*)ast_getattr(type, "base_type") == AstBaseType_Error) {
    type_str = "AstBaseType_Error";
  } else if (*(enum AstBaseTypeKind*)ast_getattr(type, "base_type") == AstBaseType_Int) {
    type_str = "AstBaseType_Int";
  } else if (*(enum AstBaseTypeKind*)ast_getattr(type, "base_type") == AstBaseType_Bit) {
    type_str = "AstBaseType_Bit";
  } else if (*(enum AstBaseTypeKind*)ast_getattr(type, "base_type") == AstBaseType_Varbit) {
    type_str = "AstBaseType_Varbit";
  } else if (*(enum AstBaseTypeKind*)ast_getattr(type, "base_type") == AstBaseType_String) {
    type_str = "AstBaseType_String";
  }
  else assert(*(enum AstBaseTypeKind*)ast_getattr(type, "base_type") == AstBaseType_NONE_);
  print_prop("base_type", Value_String, type_str);
  print_prop("size", Value_Id, ((struct Ast*)ast_getattr(type, "size"))->id);
  ast_end();
  print_ast(ast_getattr(type, "size"));
}

void
print_ast(struct Ast* ast)
{
  if (!ast) { return; }
  ast_start();
  print_prop("id", Value_Id, ast->id);
  print_prop("kind", Value_String, ast_kind_to_string(ast->kind));
  print_prop("line_nr", Value_Integer, ast->line_nr);
  struct AstAttributeIterator attr_iter = {};
  struct AstAttribute* attr;
  for (attr = ast_attriter_init(&attr_iter, ast); attr; attr = ast_attriter_get_next(&attr_iter)) {
    if (attr->type == AstAttr_Integer) {
      print_prop(attr->name, Value_Integer, *(int*)attr->value);
    } else if (attr->type == AstAttr_String) {
      print_prop(attr->name, Value_String, attr->value);
    } else if (attr->type == AstAttr_Ast) {
      if (attr->value) {
        print_prop(attr->name, Value_Id, ((struct Ast*)attr->value)->id);
      }
    } else if (attr->type == AstAttr_AstList) {
      print_prop(attr->name, Value_IdList, attr->value);
    } else if (attr->type == AstAttr_ExprOperator) {
      print_prop(attr->name, Value_String, expr_operator_to_string(*(enum AstExprOperator*)attr->value));
    }
  }
  ast_end();
  for (attr = ast_attriter_init(&attr_iter, ast); attr; attr = ast_attriter_get_next(&attr_iter)) {
    if (attr->type == AstAttr_Ast) {
      print_ast(attr->value);
    } else if (attr->type == AstAttr_AstList) {
      struct AstList* list = attr->value;
      if (list) {
        struct AstListLink* link = list->head->next;
        while (link) {
          print_ast(link->ast);
          link = link->next;
        }
      }
    }
  }
}

