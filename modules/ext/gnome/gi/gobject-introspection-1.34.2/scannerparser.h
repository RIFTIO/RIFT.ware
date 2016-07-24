/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     IDENTIFIER = 258,
     TYPEDEF_NAME = 259,
     INTEGER = 260,
     FLOATING = 261,
     CHARACTER = 262,
     STRING = 263,
     INTL_CONST = 264,
     INTUL_CONST = 265,
     ELLIPSIS = 266,
     ADDEQ = 267,
     SUBEQ = 268,
     MULEQ = 269,
     DIVEQ = 270,
     MODEQ = 271,
     XOREQ = 272,
     ANDEQ = 273,
     OREQ = 274,
     SL = 275,
     SR = 276,
     SLEQ = 277,
     SREQ = 278,
     EQ = 279,
     NOTEQ = 280,
     LTEQ = 281,
     GTEQ = 282,
     ANDAND = 283,
     OROR = 284,
     PLUSPLUS = 285,
     MINUSMINUS = 286,
     ARROW = 287,
     AUTO = 288,
     BOOL = 289,
     BREAK = 290,
     CASE = 291,
     CHAR = 292,
     CONST = 293,
     CONTINUE = 294,
     DEFAULT = 295,
     DO = 296,
     DOUBLE = 297,
     ELSE = 298,
     ENUM = 299,
     EXTENSION = 300,
     EXTERN = 301,
     FLOAT = 302,
     FOR = 303,
     GOTO = 304,
     IF = 305,
     INLINE = 306,
     INT = 307,
     LONG = 308,
     REGISTER = 309,
     RESTRICT = 310,
     RETURN = 311,
     SHORT = 312,
     SIGNED = 313,
     SIZEOF = 314,
     STATIC = 315,
     STRUCT = 316,
     SWITCH = 317,
     TYPEDEF = 318,
     UNION = 319,
     UNSIGNED = 320,
     VOID = 321,
     VOLATILE = 322,
     WHILE = 323,
     FUNCTION_MACRO = 324,
     OBJECT_MACRO = 325
   };
#endif
/* Tokens.  */
#define IDENTIFIER 258
#define TYPEDEF_NAME 259
#define INTEGER 260
#define FLOATING 261
#define CHARACTER 262
#define STRING 263
#define INTL_CONST 264
#define INTUL_CONST 265
#define ELLIPSIS 266
#define ADDEQ 267
#define SUBEQ 268
#define MULEQ 269
#define DIVEQ 270
#define MODEQ 271
#define XOREQ 272
#define ANDEQ 273
#define OREQ 274
#define SL 275
#define SR 276
#define SLEQ 277
#define SREQ 278
#define EQ 279
#define NOTEQ 280
#define LTEQ 281
#define GTEQ 282
#define ANDAND 283
#define OROR 284
#define PLUSPLUS 285
#define MINUSMINUS 286
#define ARROW 287
#define AUTO 288
#define BOOL 289
#define BREAK 290
#define CASE 291
#define CHAR 292
#define CONST 293
#define CONTINUE 294
#define DEFAULT 295
#define DO 296
#define DOUBLE 297
#define ELSE 298
#define ENUM 299
#define EXTENSION 300
#define EXTERN 301
#define FLOAT 302
#define FOR 303
#define GOTO 304
#define IF 305
#define INLINE 306
#define INT 307
#define LONG 308
#define REGISTER 309
#define RESTRICT 310
#define RETURN 311
#define SHORT 312
#define SIGNED 313
#define SIZEOF 314
#define STATIC 315
#define STRUCT 316
#define SWITCH 317
#define TYPEDEF 318
#define UNION 319
#define UNSIGNED 320
#define VOID 321
#define VOLATILE 322
#define WHILE 323
#define FUNCTION_MACRO 324
#define OBJECT_MACRO 325




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 134 "scannerparser.y"

  char *str;
  GList *list;
  GISourceSymbol *symbol;
  GISourceType *ctype;
  StorageClassSpecifier storage_class_specifier;
  TypeQualifier type_qualifier;
  FunctionSpecifier function_specifier;
  UnaryOperator unary_operator;



/* Line 2068 of yacc.c  */
#line 203 "scannerparser.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


