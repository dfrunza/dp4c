#pragma once
#include "basic.h"
#include "arena.h"
#include <stdint.h>

enum TokenClass {
  Token_None,
  Token_Semicolon,
  Token_Identifier,
  Token_TypeIdentifier,
  Token_String,
  Token_Integer,
  Token_ParenthOpen,
  Token_ParenthClose,
  Token_AngleOpen,
  Token_AngleClose,
  Token_BraceOpen,
  Token_BraceClose,
  Token_BracketOpen,
  Token_BracketClose,
  Token_Dontcare,
  Token_Colon,
  Token_DotPrefix,
  Token_Comma,
  Token_Minus,
  Token_UnaryMinus,
  Token_Plus,
  Token_Star,
  Token_Slash,
  Token_Equal,
  Token_LogicEqual,
  Token_LogicNotEqual,
  Token_LogicNot,
  Token_LogicAnd,
  Token_LogicOr,
  Token_LessEqual,
  Token_GreaterEqual,
  Token_BitwiseNot,
  Token_BitwiseAnd,
  Token_BitwiseOr,
  Token_BitwiseXor,
  Token_BitshiftLeft,
  Token_BitshiftRight,
  Token_Comment,

  Token_Action,
  Token_Actions,
  Token_Enum,
  Token_In,
  Token_Package,
  Token_Select,
  Token_Switch,
  Token_Tuple,
  Token_Void,
  Token_Apply,
  Token_Control,
  Token_Error,
  Token_Header,
  Token_InOut,
  Token_Parser,
  Token_State,
  Token_Table,
  Token_Entries,
  Token_Key,
  Token_Typedef,
  Token_Type,
  Token_Bool,
  Token_True,
  Token_False,
  Token_Default,
  Token_Extern,
  Token_HeaderUnion,
  Token_Int,
  Token_Bit,
  Token_Varbit,
  Token_Out,
  Token_StringLiteral,
  Token_Transition,
  Token_Else,
  Token_Exit,
  Token_If,
  Token_MatchKind,
  Token_Return,
  Token_Struct,
  Token_Const,

  Token_Unknown,
  Token_StartOfInput,
  Token_EndOfInput,
};

enum IntegerFlags
{
  IntFlags_None,
  IntFlags_HasWidth,
  IntFlags_Signed,
};

struct Token {
  enum TokenClass klass;
  char* lexeme;
  int line_nr;

  union {
    struct {
      enum IntegerFlags flags;
      int width;
      int64_t value;
    } i;  /* integer */
    char* str;
  };
};

