/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 29 "scannerparser.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "sourcescanner.h"
#include "scannerparser.h"

extern FILE *yyin;
extern int lineno;
extern char linebuf[2000];
extern char *yytext;

extern int yylex (GISourceScanner *scanner);
static void yyerror (GISourceScanner *scanner, const char *str);

extern void ctype_free (GISourceType * type);

static int last_enum_value = -1;
static gboolean is_bitfield;
static GHashTable *const_table = NULL;

/**
 * parse_c_string_literal:
 * @str: A string containing a C string literal
 *
 * Based on g_strcompress(), but also handles
 * hexadecimal escapes.
 */
static char *
parse_c_string_literal (const char *str)
{
  const gchar *p = str, *num;
  gchar *dest = g_malloc (strlen (str) + 1);
  gchar *q = dest;

  while (*p)
    {
      if (*p == '\\')
        {
          p++;
          switch (*p)
            {
            case '\0':
              g_warning ("parse_c_string_literal: trailing \\");
              goto out;
            case '0':  case '1':  case '2':  case '3':  case '4':
            case '5':  case '6':  case '7':
              *q = 0;
              num = p;
              while ((p < num + 3) && (*p >= '0') && (*p <= '7'))
                {
                  *q = (*q * 8) + (*p - '0');
                  p++;
                }
              q++;
              p--;
              break;
	    case 'x':
	      *q = 0;
	      p++;
	      num = p;
	      while ((p < num + 2) && (g_ascii_isxdigit(*p)))
		{
		  *q = (*q * 16) + g_ascii_xdigit_value(*p);
		  p++;
		}
              q++;
              p--;
	      break;
            case 'b':
              *q++ = '\b';
              break;
            case 'f':
              *q++ = '\f';
              break;
            case 'n':
              *q++ = '\n';
              break;
            case 'r':
              *q++ = '\r';
              break;
            case 't':
              *q++ = '\t';
              break;
            default:            /* Also handles \" and \\ */
              *q++ = *p;
              break;
            }
        }
      else
        *q++ = *p;
      p++;
    }
out:
  *q = 0;

  return dest;
}



/* Line 268 of yacc.c  */
#line 176 "scannerparser.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


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

/* Line 293 of yacc.c  */
#line 134 "scannerparser.y"

  char *str;
  GList *list;
  GISourceSymbol *symbol;
  GISourceType *ctype;
  StorageClassSpecifier storage_class_specifier;
  TypeQualifier type_qualifier;
  FunctionSpecifier function_specifier;
  UnaryOperator unary_operator;



/* Line 293 of yacc.c  */
#line 365 "scannerparser.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 377 "scannerparser.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  67
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2296

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  95
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  76
/* YYNRULES -- Number of rules.  */
#define YYNRULES  246
/* YYNRULES -- Number of states.  */
#define YYNSTATES  416

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   325

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    84,     2,     2,     2,    86,    79,     2,
      71,    72,    80,    81,    78,    82,    77,    85,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    92,    94,
      87,    93,    88,    91,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    75,     2,    76,    89,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    73,    90,    74,    83,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    17,    24,
      26,    29,    31,    33,    35,    37,    42,    47,    51,    55,
      59,    62,    65,    67,    71,    73,    76,    79,    82,    87,
      92,    95,   100,   102,   104,   106,   108,   110,   112,   114,
     119,   121,   125,   129,   133,   135,   139,   143,   145,   149,
     153,   155,   159,   163,   167,   171,   173,   177,   181,   183,
     187,   189,   193,   195,   199,   201,   205,   207,   211,   213,
     219,   221,   225,   227,   229,   231,   233,   235,   237,   239,
     241,   243,   245,   247,   249,   253,   256,   258,   262,   265,
     268,   270,   273,   275,   278,   280,   283,   285,   287,   291,
     293,   297,   299,   301,   303,   305,   307,   309,   311,   313,
     315,   317,   319,   321,   323,   325,   327,   329,   331,   333,
     339,   344,   347,   349,   351,   353,   356,   360,   363,   365,
     368,   370,   372,   376,   377,   379,   382,   386,   392,   397,
     404,   410,   413,   415,   416,   419,   423,   425,   429,   431,
     433,   435,   437,   439,   442,   444,   446,   450,   455,   459,
     464,   469,   473,   476,   478,   482,   485,   487,   490,   492,
     496,   499,   502,   504,   506,   508,   512,   514,   517,   519,
     521,   524,   528,   531,   535,   539,   544,   547,   551,   555,
     560,   562,   564,   568,   573,   575,   579,   581,   583,   585,
     587,   589,   591,   595,   600,   604,   607,   611,   613,   616,
     618,   620,   622,   625,   631,   639,   645,   651,   659,   666,
     674,   682,   691,   699,   708,   717,   727,   731,   734,   737,
     740,   744,   746,   749,   751,   753,   755,   760,   764,   766,
     769,   771,   773,   778,   781,   783,   785
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     162,     0,    -1,    98,    -1,     5,    -1,     7,    -1,     6,
      -1,    97,    -1,    71,   118,    72,    -1,    45,    71,    73,
     156,    74,    72,    -1,     8,    -1,    97,     8,    -1,     3,
      -1,    98,    -1,   150,    -1,    96,    -1,   100,    75,   118,
      76,    -1,   100,    71,   101,    72,    -1,   100,    71,    72,
      -1,   100,    77,    99,    -1,   100,    32,    99,    -1,   100,
      30,    -1,   100,    31,    -1,   116,    -1,   101,    78,   116,
      -1,   100,    -1,    30,   102,    -1,    31,   102,    -1,   103,
     104,    -1,     9,    71,   102,    72,    -1,    10,    71,   102,
      72,    -1,    59,   102,    -1,    59,    71,   147,    72,    -1,
      79,    -1,    80,    -1,    81,    -1,    82,    -1,    83,    -1,
      84,    -1,   102,    -1,    71,   147,    72,   104,    -1,   104,
      -1,   105,    80,   104,    -1,   105,    85,   104,    -1,   105,
      86,   104,    -1,   105,    -1,   106,    81,   105,    -1,   106,
      82,   105,    -1,   106,    -1,   107,    20,   106,    -1,   107,
      21,   106,    -1,   107,    -1,   108,    87,   107,    -1,   108,
      88,   107,    -1,   108,    26,   107,    -1,   108,    27,   107,
      -1,   108,    -1,   109,    24,   108,    -1,   109,    25,   108,
      -1,   109,    -1,   110,    79,   109,    -1,   110,    -1,   111,
      89,   110,    -1,   111,    -1,   112,    90,   111,    -1,   112,
      -1,   113,    28,   112,    -1,   113,    -1,   114,    29,   113,
      -1,   114,    -1,   114,    91,   118,    92,   118,    -1,   115,
      -1,   102,   117,   116,    -1,    93,    -1,    14,    -1,    15,
      -1,    16,    -1,    12,    -1,    13,    -1,    22,    -1,    23,
      -1,    18,    -1,    17,    -1,    19,    -1,   116,    -1,   118,
      78,   116,    -1,    45,   118,    -1,   115,    -1,   121,   122,
      94,    -1,   121,    94,    -1,   124,   121,    -1,   124,    -1,
     125,   121,    -1,   125,    -1,   138,   121,    -1,   138,    -1,
     139,   121,    -1,   139,    -1,   123,    -1,   122,    78,   123,
      -1,   140,    -1,   140,    93,   151,    -1,    63,    -1,    46,
      -1,    60,    -1,    33,    -1,    54,    -1,    66,    -1,    37,
      -1,    57,    -1,    52,    -1,    53,    -1,    47,    -1,    42,
      -1,    58,    -1,    65,    -1,    34,    -1,   126,    -1,   133,
      -1,   150,    -1,   127,    99,    73,   128,    74,    -1,   127,
      73,   128,    74,    -1,   127,    99,    -1,    61,    -1,    64,
      -1,   129,    -1,   128,   129,    -1,   130,   131,    94,    -1,
     125,   130,    -1,   125,    -1,   138,   130,    -1,   138,    -1,
     132,    -1,   131,    78,   132,    -1,    -1,   140,    -1,    92,
     119,    -1,   140,    92,   119,    -1,   134,    99,    73,   135,
      74,    -1,   134,    73,   135,    74,    -1,   134,    99,    73,
     135,    78,    74,    -1,   134,    73,   135,    78,    74,    -1,
     134,    99,    -1,    44,    -1,    -1,   136,   137,    -1,   135,
      78,   137,    -1,    98,    -1,    98,    93,   119,    -1,    38,
      -1,    55,    -1,    45,    -1,    67,    -1,    51,    -1,   142,
     141,    -1,   141,    -1,    98,    -1,    71,   140,    72,    -1,
     141,    75,   116,    76,    -1,   141,    75,    76,    -1,   141,
      71,   144,    72,    -1,   141,    71,   146,    72,    -1,   141,
      71,    72,    -1,    80,   143,    -1,    80,    -1,    80,   143,
     142,    -1,    80,   142,    -1,   138,    -1,   143,   138,    -1,
     145,    -1,   144,    78,   145,    -1,   121,   140,    -1,   121,
     148,    -1,   121,    -1,    11,    -1,    98,    -1,   146,    78,
      98,    -1,   130,    -1,   130,   148,    -1,   142,    -1,   149,
      -1,   142,   149,    -1,    71,   148,    72,    -1,    75,    76,
      -1,    75,   116,    76,    -1,   149,    75,    76,    -1,   149,
      75,   116,    76,    -1,    71,    72,    -1,    71,   144,    72,
      -1,   149,    71,    72,    -1,   149,    71,   144,    72,    -1,
       4,    -1,   116,    -1,    73,   152,    74,    -1,    73,   152,
      78,    74,    -1,   151,    -1,   152,    78,   151,    -1,   154,
      -1,   155,    -1,   158,    -1,   159,    -1,   160,    -1,   161,
      -1,    99,    92,   153,    -1,    36,   119,    92,   153,    -1,
      40,    92,   153,    -1,    73,    74,    -1,    73,   156,    74,
      -1,   157,    -1,   156,   157,    -1,   120,    -1,   153,    -1,
      94,    -1,   118,    94,    -1,    50,    71,   118,    72,   153,
      -1,    50,    71,   118,    72,   153,    43,   153,    -1,    62,
      71,   118,    72,   153,    -1,    68,    71,   118,    72,   153,
      -1,    41,   153,    68,    71,   118,    72,    94,    -1,    48,
      71,    94,    94,    72,   153,    -1,    48,    71,   118,    94,
      94,    72,   153,    -1,    48,    71,    94,   118,    94,    72,
     153,    -1,    48,    71,   118,    94,   118,    94,    72,   153,
      -1,    48,    71,    94,    94,   118,    72,   153,    -1,    48,
      71,   118,    94,    94,   118,    72,   153,    -1,    48,    71,
      94,   118,    94,   118,    72,   153,    -1,    48,    71,   118,
      94,   118,    94,   118,    72,   153,    -1,    49,    99,    94,
      -1,    39,    94,    -1,    35,    94,    -1,    56,    94,    -1,
      56,   118,    94,    -1,   163,    -1,   162,   163,    -1,   164,
      -1,   120,    -1,   170,    -1,   121,   140,   165,   155,    -1,
     121,   140,   155,    -1,   120,    -1,   165,   120,    -1,    69,
      -1,    70,    -1,   166,    71,   146,    72,    -1,   167,   119,
      -1,   168,    -1,   169,    -1,     1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   224,   224,   233,   249,   255,   262,   263,   267,   275,
     290,   304,   311,   312,   316,   317,   321,   325,   329,   333,
     337,   341,   348,   349,   353,   354,   358,   362,   385,   392,
     399,   403,   411,   415,   419,   423,   427,   431,   438,   439,
     451,   452,   458,   466,   477,   478,   484,   493,   494,   506,
     515,   516,   522,   528,   534,   543,   544,   550,   559,   560,
     569,   570,   579,   580,   589,   590,   601,   602,   613,   614,
     621,   622,   629,   630,   631,   632,   633,   634,   635,   636,
     637,   638,   639,   643,   644,   645,   652,   658,   676,   683,
     688,   693,   706,   707,   712,   717,   722,   730,   734,   741,
     742,   746,   750,   754,   758,   762,   769,   773,   777,   781,
     785,   789,   793,   797,   801,   805,   809,   810,   811,   819,
     838,   843,   851,   856,   864,   865,   872,   892,   897,   898,
     903,   911,   915,   923,   926,   927,   931,   942,   949,   956,
     963,   970,   977,   986,   986,   995,  1003,  1011,  1023,  1027,
    1031,  1035,  1042,  1049,  1054,  1058,  1063,  1067,  1072,  1077,
    1087,  1094,  1103,  1108,  1112,  1123,  1136,  1137,  1144,  1148,
    1155,  1160,  1165,  1170,  1177,  1183,  1192,  1193,  1197,  1202,
    1203,  1211,  1215,  1220,  1225,  1230,  1235,  1241,  1251,  1257,
    1270,  1277,  1278,  1279,  1283,  1284,  1290,  1291,  1292,  1293,
    1294,  1295,  1299,  1300,  1301,  1305,  1306,  1310,  1311,  1315,
    1316,  1320,  1321,  1325,  1326,  1327,  1331,  1332,  1333,  1334,
    1335,  1336,  1337,  1338,  1339,  1340,  1344,  1345,  1346,  1347,
    1348,  1354,  1355,  1359,  1360,  1361,  1365,  1366,  1370,  1371,
    1377,  1384,  1391,  1395,  1406,  1407,  1408
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "\"identifier\"", "\"typedef-name\"",
  "INTEGER", "FLOATING", "CHARACTER", "STRING", "INTL_CONST",
  "INTUL_CONST", "ELLIPSIS", "ADDEQ", "SUBEQ", "MULEQ", "DIVEQ", "MODEQ",
  "XOREQ", "ANDEQ", "OREQ", "SL", "SR", "SLEQ", "SREQ", "EQ", "NOTEQ",
  "LTEQ", "GTEQ", "ANDAND", "OROR", "PLUSPLUS", "MINUSMINUS", "ARROW",
  "AUTO", "BOOL", "BREAK", "CASE", "CHAR", "CONST", "CONTINUE", "DEFAULT",
  "DO", "DOUBLE", "ELSE", "ENUM", "EXTENSION", "EXTERN", "FLOAT", "FOR",
  "GOTO", "IF", "INLINE", "INT", "LONG", "REGISTER", "RESTRICT", "RETURN",
  "SHORT", "SIGNED", "SIZEOF", "STATIC", "STRUCT", "SWITCH", "TYPEDEF",
  "UNION", "UNSIGNED", "VOID", "VOLATILE", "WHILE", "FUNCTION_MACRO",
  "OBJECT_MACRO", "'('", "')'", "'{'", "'}'", "'['", "']'", "'.'", "','",
  "'&'", "'*'", "'+'", "'-'", "'~'", "'!'", "'/'", "'%'", "'<'", "'>'",
  "'^'", "'|'", "'?'", "':'", "'='", "';'", "$accept",
  "primary_expression", "strings", "identifier",
  "identifier_or_typedef_name", "postfix_expression",
  "argument_expression_list", "unary_expression", "unary_operator",
  "cast_expression", "multiplicative_expression", "additive_expression",
  "shift_expression", "relational_expression", "equality_expression",
  "and_expression", "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_or_expression",
  "conditional_expression", "assignment_expression", "assignment_operator",
  "expression", "constant_expression", "declaration",
  "declaration_specifiers", "init_declarator_list", "init_declarator",
  "storage_class_specifier", "type_specifier", "struct_or_union_specifier",
  "struct_or_union", "struct_declaration_list", "struct_declaration",
  "specifier_qualifier_list", "struct_declarator_list",
  "struct_declarator", "enum_specifier", "enum_keyword", "enumerator_list",
  "$@1", "enumerator", "type_qualifier", "function_specifier",
  "declarator", "direct_declarator", "pointer", "type_qualifier_list",
  "parameter_list", "parameter_declaration", "identifier_list",
  "type_name", "abstract_declarator", "direct_abstract_declarator",
  "typedef_name", "initializer", "initializer_list", "statement",
  "labeled_statement", "compound_statement", "block_item_list",
  "block_item", "expression_statement", "selection_statement",
  "iteration_statement", "jump_statement", "translation_unit",
  "external_declaration", "function_definition", "declaration_list",
  "function_macro", "object_macro", "function_macro_define",
  "object_macro_define", "macro", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,    40,    41,   123,   125,    91,    93,    46,    44,    38,
      42,    43,    45,   126,    33,    47,    37,    60,    62,    94,
     124,    63,    58,    61,    59
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    95,    96,    96,    96,    96,    96,    96,    96,    97,
      97,    98,    99,    99,   100,   100,   100,   100,   100,   100,
     100,   100,   101,   101,   102,   102,   102,   102,   102,   102,
     102,   102,   103,   103,   103,   103,   103,   103,   104,   104,
     105,   105,   105,   105,   106,   106,   106,   107,   107,   107,
     108,   108,   108,   108,   108,   109,   109,   109,   110,   110,
     111,   111,   112,   112,   113,   113,   114,   114,   115,   115,
     116,   116,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,   118,   118,   118,   119,   120,   120,   121,
     121,   121,   121,   121,   121,   121,   121,   122,   122,   123,
     123,   124,   124,   124,   124,   124,   125,   125,   125,   125,
     125,   125,   125,   125,   125,   125,   125,   125,   125,   126,
     126,   126,   127,   127,   128,   128,   129,   130,   130,   130,
     130,   131,   131,   132,   132,   132,   132,   133,   133,   133,
     133,   133,   134,   136,   135,   135,   137,   137,   138,   138,
     138,   138,   139,   140,   140,   141,   141,   141,   141,   141,
     141,   141,   142,   142,   142,   142,   143,   143,   144,   144,
     145,   145,   145,   145,   146,   146,   147,   147,   148,   148,
     148,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     150,   151,   151,   151,   152,   152,   153,   153,   153,   153,
     153,   153,   154,   154,   154,   155,   155,   156,   156,   157,
     157,   158,   158,   159,   159,   159,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   161,   161,   161,   161,
     161,   162,   162,   163,   163,   163,   164,   164,   165,   165,
     166,   167,   168,   169,   170,   170,   170
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     3,     6,     1,
       2,     1,     1,     1,     1,     4,     4,     3,     3,     3,
       2,     2,     1,     3,     1,     2,     2,     2,     4,     4,
       2,     4,     1,     1,     1,     1,     1,     1,     1,     4,
       1,     3,     3,     3,     1,     3,     3,     1,     3,     3,
       1,     3,     3,     3,     3,     1,     3,     3,     1,     3,
       1,     3,     1,     3,     1,     3,     1,     3,     1,     5,
       1,     3,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     3,     2,     1,     3,     2,     2,
       1,     2,     1,     2,     1,     2,     1,     1,     3,     1,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     5,
       4,     2,     1,     1,     1,     2,     3,     2,     1,     2,
       1,     1,     3,     0,     1,     2,     3,     5,     4,     6,
       5,     2,     1,     0,     2,     3,     1,     3,     1,     1,
       1,     1,     1,     2,     1,     1,     3,     4,     3,     4,
       4,     3,     2,     1,     3,     2,     1,     2,     1,     3,
       2,     2,     1,     1,     1,     3,     1,     2,     1,     1,
       2,     3,     2,     3,     3,     4,     2,     3,     3,     4,
       1,     1,     3,     4,     1,     3,     1,     1,     1,     1,
       1,     1,     3,     4,     3,     2,     3,     1,     2,     1,
       1,     1,     2,     5,     7,     5,     5,     7,     6,     7,
       7,     8,     7,     8,     8,     9,     3,     2,     2,     2,
       3,     1,     2,     1,     1,     1,     4,     3,     1,     2,
       1,     1,     4,     2,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,   246,   190,   104,   115,   107,   148,   112,   142,   150,
     102,   111,   152,   109,   110,   105,   149,   108,   113,   103,
     122,   101,   123,   114,   106,   151,   240,   241,   234,     0,
      90,    92,   116,     0,   117,     0,    94,    96,   118,     0,
     231,   233,     0,     0,   244,   245,   235,    11,     0,   163,
      88,   155,     0,    97,    99,   154,     0,    89,    91,     0,
      12,   121,    13,   143,   141,    93,    95,     1,   232,     0,
       3,     5,     4,     9,     0,     0,     0,     0,     0,     0,
       0,    32,    33,    34,    35,    36,    37,    14,     6,     2,
      24,    38,     0,    40,    44,    47,    50,    55,    58,    60,
      62,    64,    66,    68,    86,   243,     0,   166,   165,   162,
       0,    87,     0,     0,   238,     0,   237,     0,     0,     0,
     153,   128,     0,   124,   133,   130,     0,     0,     0,   143,
     174,     0,     0,     0,     0,    25,    26,     0,     0,    30,
     150,    38,    70,    83,     0,   176,     0,    10,    20,    21,
       0,     0,     0,     0,    27,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   156,   167,   164,    98,    99,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     205,   211,     2,     0,     0,   209,   118,   210,   196,   197,
       0,   207,   198,   199,   200,   201,     0,   191,   100,   239,
     236,   173,   161,   172,     0,   168,     0,   158,     0,   127,
     120,   125,     0,     0,   131,   134,   129,     0,   138,     0,
     146,   144,     0,   242,     0,     0,     0,     0,     0,     0,
       0,    85,    76,    77,    73,    74,    75,    81,    80,    82,
      78,    79,    72,     0,     7,     0,     0,     0,   178,   177,
     179,     0,    19,    17,     0,    22,     0,    18,    41,    42,
      43,    45,    46,    48,    49,    53,    54,    51,    52,    56,
      57,    59,    61,    63,    65,    67,     0,   228,     0,   227,
       0,     0,     0,     0,     0,   229,     0,     0,     0,     0,
     212,   206,   208,   194,     0,     0,   170,   178,   171,   159,
       0,   160,   157,   135,   133,   126,     0,   119,   140,   145,
       0,   137,     0,   175,    28,    29,     0,    31,    71,    84,
     186,     0,     0,   182,     0,   180,     0,     0,    39,    16,
       0,    15,     0,     0,   204,     0,     0,     0,   226,     0,
     230,     0,     0,   202,   192,     0,   169,   132,   136,   147,
     139,     0,   187,   181,   183,   188,     0,   184,     0,    23,
      69,   203,     0,     0,     0,     0,     0,     0,     0,   193,
     195,     8,   189,   185,     0,     0,     0,     0,     0,     0,
     213,   215,   216,     0,   218,     0,     0,     0,     0,     0,
       0,     0,   217,   222,   220,     0,   219,     0,     0,     0,
     214,   224,   223,   221,     0,   225
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    87,    88,    89,   193,    90,   264,   141,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     142,   143,   253,   194,   105,   195,   115,    52,    53,    30,
      31,    32,    33,   122,   123,   124,   223,   224,    34,    35,
     127,   128,   319,    36,    37,   106,    55,    56,   109,   331,
     215,   131,   146,   332,   260,    38,   208,   304,   197,   198,
     199,   200,   201,   202,   203,   204,   205,    39,    40,    41,
     117,    42,    43,    44,    45,    46
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -228
static const yytype_int16 yypact[] =
{
    2004,  -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,
    -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,
    -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,    45,
    2194,  2194,  -228,    11,  -228,    40,  2194,  2194,  -228,  1893,
    -228,  -228,   -35,  1606,  -228,  -228,  -228,  -228,    87,   144,
    -228,  -228,   -43,  -228,  1011,   202,    46,  -228,  -228,  2229,
    -228,     6,  -228,  -228,    23,  -228,  -228,  -228,  -228,    69,
    -228,  -228,  -228,  -228,    37,    39,  1622,  1622,    51,  1638,
    1164,  -228,  -228,  -228,  -228,  -228,  -228,  -228,   158,  -228,
     165,  -228,  1606,  -228,   132,    48,   186,   134,   203,    90,
      92,    93,   197,     0,  -228,  -228,   121,  -228,  -228,   144,
      87,  -228,   505,  1264,  -228,    45,  -228,  2085,  2049,  1280,
     202,  2229,  1930,  -228,    84,  2229,  2229,   209,    69,  -228,
    -228,   -25,  1622,  1622,  1705,  -228,  -228,   141,  1164,  -228,
    1721,    43,  -228,  -228,    -1,   133,   171,  -228,  -228,  -228,
     277,  1296,  1705,   277,  -228,  1606,  1606,  1606,  1606,  1606,
    1606,  1606,  1606,  1606,  1606,  1606,  1606,  1606,  1606,  1606,
    1606,  1606,  1606,  1705,  -228,  -228,  -228,  -228,   137,   143,
    1606,   161,   175,   843,   201,   277,   233,   859,   235,   239,
    -228,  -228,   182,   206,    29,  -228,   221,  -228,  -228,  -228,
     597,  -228,  -228,  -228,  -228,  -228,  1264,  -228,  -228,  -228,
    -228,  -228,  -228,    82,   120,  -228,   138,  -228,   244,  -228,
    -228,  -228,  1606,    50,  -228,   231,  -228,  1965,  -228,    19,
     232,  -228,   216,  -228,    69,   252,   258,  1721,   761,   259,
    1082,   254,  -228,  -228,  -228,  -228,  -228,  -228,  -228,  -228,
    -228,  -228,  -228,  1606,  -228,  1606,  1824,  1378,   220,  -228,
     228,  1606,  -228,  -228,   154,  -228,   -52,  -228,  -228,  -228,
    -228,   132,   132,    48,    48,   186,   186,   186,   186,   134,
     134,   203,    90,    92,    93,   197,   -14,  -228,   241,  -228,
     843,   266,   926,   242,  1705,  -228,    56,  1705,  1705,   843,
    -228,  -228,  -228,  -228,   227,  1779,  -228,    34,  -228,  -228,
    2159,  -228,  -228,  -228,    84,  -228,  1606,  -228,  -228,  -228,
    1606,  -228,    27,  -228,  -228,  -228,   679,  -228,  -228,  -228,
    -228,   157,   263,  -228,   261,   228,  2122,  1394,  -228,  -228,
    1606,  -228,  1705,   843,  -228,   268,   942,    85,  -228,   166,
    -228,   167,   169,  -228,  -228,  1182,  -228,  -228,  -228,  -228,
    -228,   271,  -228,  -228,  -228,  -228,   178,  -228,   264,  -228,
     254,  -228,  1705,  1410,   108,   958,   843,   843,   843,  -228,
    -228,  -228,  -228,  -228,   181,   843,   188,  1492,  1508,   109,
     301,  -228,  -228,   251,  -228,   843,   843,   190,   843,   191,
    1524,   843,  -228,  -228,  -228,   843,  -228,   843,   843,   193,
    -228,  -228,  -228,  -228,   843,  -228
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -228,  -228,  -228,   -29,   -10,  -228,  -228,   276,  -228,   -81,
     130,   148,   153,   155,   183,   177,   180,   187,   185,  -228,
     -34,  -106,  -228,   -46,  -168,    28,     2,  -228,   249,  -228,
      52,  -228,  -228,   234,  -101,   -75,  -228,    57,  -228,  -228,
     236,  -228,   246,    -7,  -228,   -12,   -53,   -41,  -228,  -117,
      65,   269,   250,   -76,  -227,   -15,  -196,  -228,    74,  -228,
      16,   139,  -184,  -228,  -228,  -228,  -228,  -228,   343,  -228,
    -228,  -228,  -228,  -228,  -228,  -228
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -14
static const yytype_int16 yytable[] =
{
      51,   214,    29,   120,    60,   145,    60,   207,   108,   104,
     303,   154,   288,   218,    47,     2,   302,    54,    62,    51,
      62,   221,    47,    61,   341,    64,   255,    51,    28,   172,
      47,   335,    57,    58,   144,   110,    69,    47,    65,    66,
     130,    29,   107,    47,     2,   265,   219,   233,    47,    47,
     226,   111,   125,   234,   313,   242,   243,   244,   245,   246,
     247,   248,   249,   145,   255,   250,   251,    28,   176,   259,
     116,   254,    47,   125,   268,   269,   270,   255,   342,   126,
     335,    51,   114,   192,    59,    47,    51,    47,   144,   130,
      47,   173,   144,   318,   241,    51,   129,   196,   178,   230,
     207,   360,   175,   178,   258,   305,   266,   255,   132,   257,
     133,   121,   225,    63,   125,   125,    48,    48,   125,   125,
     213,    60,   137,   300,    60,    49,   221,   286,   314,   158,
     159,   125,   121,   210,   255,    62,   252,   308,    62,    50,
     262,   296,   302,   267,   315,   209,   104,   328,   358,   329,
     350,   334,   359,   305,   192,    48,    60,   257,    48,   380,
     162,   163,    49,   255,    49,   145,   147,    49,    62,   168,
      62,   192,   307,   121,   121,   293,   222,   121,   121,   375,
     338,   169,     6,   170,    51,   196,   255,   255,   104,     9,
     121,   241,   309,   174,   144,   148,   149,   150,   310,    16,
     230,   306,   387,   400,   256,   323,   160,   161,   257,   192,
     311,    25,   155,    49,   238,   258,   234,   156,   157,   366,
     125,   164,   165,   196,    49,   171,   339,   166,   167,   362,
     113,   368,   340,   125,   369,   310,   151,   287,   376,   377,
     152,   378,   153,   261,   255,   255,   347,   255,   349,   207,
     382,   351,   352,   393,   120,   289,   310,   291,   213,   255,
     395,   192,   405,   407,   307,   414,   255,   290,   255,   255,
     192,   255,   292,   118,   -12,    62,    51,   119,    51,   121,
      47,     2,   104,   228,    62,    51,   104,   229,   271,   272,
     321,   256,   121,   230,   322,   257,   370,   192,   299,   336,
     374,   354,   225,   337,   294,   355,   297,   213,   273,   274,
     298,   196,   213,   -13,   192,   275,   276,   277,   278,    91,
     312,   279,   280,   316,   324,   320,   384,   386,    62,   389,
     325,   327,   255,   343,   345,   363,   348,   364,   213,   372,
     383,   397,   399,   381,   401,   402,   282,   192,   192,   192,
     283,   281,   135,   136,   409,   139,   192,   285,   284,   177,
     227,    62,    62,    62,   344,   232,   192,   192,    91,   192,
      62,   357,   192,   353,   231,   356,   192,   326,   192,   192,
      62,    62,    68,    62,     0,   192,    62,   216,   239,     0,
      62,     0,    62,    62,     0,     0,     0,     0,     0,    62,
       0,     0,     0,     0,     0,     0,     0,     0,   235,   236,
       0,     0,     0,     0,     0,     0,     0,   371,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    91,    91,    91,    91,    91,    91,    91,    91,    91,
      91,    91,    91,    91,    91,    91,    91,    91,    91,     0,
     390,   391,   392,     0,     0,     0,    91,     0,     0,   394,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   403,
     404,     0,   406,     0,     0,   410,     0,     0,     0,   411,
       0,   412,   413,     0,     0,     0,     0,     0,   415,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    91,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    47,     2,
      70,    71,    72,    73,    74,    75,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    76,    77,    91,     3,     4,
     179,   180,     5,     6,   181,   182,   183,     7,     0,     8,
     140,    10,    11,   184,   185,   186,    12,    13,    14,    15,
      16,   187,    17,    18,    79,    19,    20,   188,    21,    22,
      23,    24,    25,   189,     0,     0,    80,     0,   112,   190,
       0,     0,     0,     0,    81,    82,    83,    84,    85,    86,
       0,     0,    91,     0,     0,     0,    91,     0,     0,   191,
      47,     2,    70,    71,    72,    73,    74,    75,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    76,    77,     0,
       3,     4,   179,   180,     5,     6,   181,   182,   183,     7,
       0,     8,   140,    10,    11,   184,   185,   186,    12,    13,
      14,    15,    16,   187,    17,    18,    79,    19,    20,   188,
      21,    22,    23,    24,    25,   189,     0,     0,    80,     0,
     112,   301,     0,     0,     0,     0,    81,    82,    83,    84,
      85,    86,    47,     2,    70,    71,    72,    73,    74,    75,
       0,   191,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    76,
      77,     0,     3,     4,   179,   180,     5,     6,   181,   182,
     183,     7,     0,     8,   140,    10,    11,   184,   185,   186,
      12,    13,    14,    15,    16,   187,    17,    18,    79,    19,
      20,   188,    21,    22,    23,    24,    25,   189,     0,     0,
      80,     0,   112,   361,     0,     0,     0,     0,    81,    82,
      83,    84,    85,    86,    47,     2,    70,    71,    72,    73,
      74,    75,     0,   191,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    76,    77,     0,     3,     4,   179,   180,     5,     6,
     181,   182,   183,     7,     0,     8,   140,    10,    11,   184,
     185,   186,    12,    13,    14,    15,    16,   187,    17,    18,
      79,    19,    20,   188,    21,    22,    23,    24,    25,   189,
       0,     0,    80,     0,   112,     0,     0,     0,     0,     0,
      81,    82,    83,    84,    85,    86,    47,     2,    70,    71,
      72,    73,    74,    75,     0,   191,     0,     0,     0,     0,
       0,     0,    47,     0,    70,    71,    72,    73,    74,    75,
       0,     0,     0,    76,    77,     0,     0,     0,   179,   180,
       0,     0,   181,   182,   183,     0,     0,     0,   237,    76,
      77,   184,   185,   186,     0,     0,     0,     0,     0,   187,
       0,     0,    79,     0,   237,   188,     0,     0,     0,     0,
       0,   189,     0,     0,    80,     0,   112,     0,    79,     0,
       0,     0,    81,    82,    83,    84,    85,    86,     0,    47,
      80,    70,    71,    72,    73,    74,    75,   191,    81,    82,
      83,    84,    85,    86,     0,    47,     0,    70,    71,    72,
      73,    74,    75,   295,     0,     0,    76,    77,     0,     0,
       0,    47,     0,    70,    71,    72,    73,    74,    75,     0,
       0,   237,    76,    77,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    79,     0,   237,    76,    77,
       0,     0,     0,     0,     0,     0,     0,    80,     0,     0,
       0,    79,     0,   237,     0,    81,    82,    83,    84,    85,
      86,     0,     0,    80,     0,     2,     0,    79,     0,     0,
     346,    81,    82,    83,    84,    85,    86,     0,     0,    80,
       0,     0,     0,     0,     0,     0,   373,    81,    82,    83,
      84,    85,    86,     0,     3,     4,     0,     0,     5,     6,
       0,     0,   388,     7,     0,     8,     9,    10,    11,     0,
       0,     0,    12,    13,    14,    15,    16,     0,    17,    18,
       0,    19,    20,     0,    21,    22,    23,    24,    25,     0,
       0,     0,     0,     0,   112,    47,     2,    70,    71,    72,
      73,    74,    75,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   113,     0,     0,     0,     0,     0,
       0,     0,    76,    77,     0,     0,     4,     0,     0,     5,
       6,     0,     0,     0,     7,     0,     8,   140,     0,    11,
       0,     0,     0,     0,    13,    14,     0,    16,     0,    17,
      18,    79,     0,    20,     0,     0,    22,    23,    24,    25,
       0,     0,     0,    80,     0,   238,     0,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    47,     2,    70,
      71,    72,    73,    74,    75,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    47,     0,    70,    71,    72,
      73,    74,    75,     0,    76,    77,     0,     0,     4,     0,
       0,     5,     6,     0,     0,     0,     7,     0,     8,   140,
       0,    11,    76,    77,     0,     0,    13,    14,     0,    16,
       0,    17,    18,    79,     0,    20,     0,    78,    22,    23,
      24,    25,     0,     0,     0,    80,     0,     0,     0,     0,
       0,    79,     0,    81,    82,    83,    84,    85,    86,     0,
       0,     0,     0,    80,     0,   206,   379,     0,     0,     0,
       0,    81,    82,    83,    84,    85,    86,    47,     0,    70,
      71,    72,    73,    74,    75,     0,     0,     0,     0,     0,
       0,     0,     0,    47,     0,    70,    71,    72,    73,    74,
      75,     0,     0,     0,    76,    77,     0,     0,     0,    47,
       0,    70,    71,    72,    73,    74,    75,     0,     0,    78,
      76,    77,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    79,     0,    78,    76,    77,     0,     0,
       0,     0,     0,     0,     0,    80,     0,   206,     0,    79,
       0,    78,     0,    81,    82,    83,    84,    85,    86,     0,
       0,    80,     0,     0,     0,    79,   217,     0,     0,    81,
      82,    83,    84,    85,    86,     0,     0,    80,   263,     0,
       0,     0,     0,     0,     0,    81,    82,    83,    84,    85,
      86,    47,     0,    70,    71,    72,    73,    74,    75,     0,
       0,     0,     0,     0,     0,     0,     0,    47,     0,    70,
      71,    72,    73,    74,    75,     0,     0,     0,    76,    77,
       0,     0,     0,    47,     0,    70,    71,    72,    73,    74,
      75,     0,     0,    78,    76,    77,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    79,     0,    78,
      76,    77,     0,     0,     0,     0,     0,     0,     0,    80,
       0,     0,     0,    79,   333,   237,     0,    81,    82,    83,
      84,    85,    86,     0,     0,    80,     0,     0,     0,    79,
     367,     0,     0,    81,    82,    83,    84,    85,    86,     0,
       0,    80,   385,     0,     0,     0,     0,     0,     0,    81,
      82,    83,    84,    85,    86,    47,     0,    70,    71,    72,
      73,    74,    75,     0,     0,     0,     0,     0,     0,     0,
       0,    47,     0,    70,    71,    72,    73,    74,    75,     0,
       0,     0,    76,    77,     0,     0,     0,    47,     0,    70,
      71,    72,    73,    74,    75,     0,     0,   237,    76,    77,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    79,     0,   237,    76,    77,     0,     0,     0,     0,
       0,     0,     0,    80,   396,     0,     0,    79,     0,   237,
       0,    81,    82,    83,    84,    85,    86,     0,     0,    80,
     398,     0,     0,    79,     0,     0,     0,    81,    82,    83,
      84,    85,    86,     0,     0,    80,   408,     0,     0,     0,
       0,     0,     0,    81,    82,    83,    84,    85,    86,    47,
       0,    70,    71,    72,    73,    74,    75,     0,     0,     0,
       0,     0,     0,     0,     0,    47,     0,    70,    71,    72,
      73,    74,    75,     0,     0,     0,    76,    77,     0,     0,
       0,    47,     0,    70,    71,    72,    73,    74,    75,     0,
       0,    78,    76,    77,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    79,     0,    78,    76,    77,
       0,     0,     0,     0,     0,     0,     0,    80,     0,     0,
       0,    79,     0,    78,     0,    81,    82,    83,    84,    85,
      86,     0,     0,   134,     0,     0,     0,    79,     0,     0,
       0,    81,    82,    83,    84,    85,    86,     0,    47,   138,
      70,    71,    72,    73,    74,    75,     0,    81,    82,    83,
      84,    85,    86,     0,    47,     0,    70,    71,    72,    73,
      74,    75,     0,     0,     0,    76,    77,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     237,    76,    77,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    79,     0,   237,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    80,     0,     0,     0,
      79,     0,    47,     2,    81,    82,    83,    84,    85,    86,
     211,     0,   240,     0,     0,     0,     0,     0,     0,     0,
      81,    82,    83,    84,    85,    86,     0,     0,     0,     0,
       0,     0,     3,     4,     0,     0,     5,     6,     0,     0,
       0,     7,     0,     8,     9,    10,    11,     0,     2,     0,
      12,    13,    14,    15,    16,   211,    17,    18,     0,    19,
      20,     0,    21,    22,    23,    24,    25,     0,     0,     0,
     305,   330,     0,     0,   257,     0,     0,     3,     4,    49,
       0,     5,     6,     0,     0,     0,     7,     0,     8,     9,
      10,    11,     0,     0,     0,    12,    13,    14,    15,    16,
       0,    17,    18,     0,    19,    20,     0,    21,    22,    23,
      24,    25,     0,    67,     1,   256,   330,     2,     0,   257,
       0,     0,     0,     0,    49,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     3,     4,     0,     0,
       5,     6,     0,     0,     2,     7,     0,     8,     9,    10,
      11,     0,     0,     0,    12,    13,    14,    15,    16,     0,
      17,    18,     0,    19,    20,     0,    21,    22,    23,    24,
      25,     0,    26,    27,     4,     0,     0,     5,     6,     2,
       0,     0,     7,     0,     8,     9,     0,    11,     0,     0,
       0,     0,    13,    14,     0,    16,     0,    17,    18,     0,
       0,    20,     0,     0,    22,    23,    24,    25,     0,     4,
       0,     0,     5,     6,   220,     1,     0,     7,     2,     8,
       9,     0,    11,     0,     0,     0,     0,    13,    14,     0,
      16,     0,    17,    18,     0,     0,    20,     0,     0,    22,
      23,    24,    25,     0,     0,     0,     0,     3,     4,   317,
       0,     5,     6,     0,     0,     0,     7,     0,     8,     9,
      10,    11,    47,     2,     0,    12,    13,    14,    15,    16,
     211,    17,    18,     0,    19,    20,     0,    21,    22,    23,
      24,    25,     0,    26,    27,     0,     0,     0,     0,     0,
       0,     0,     3,     4,     0,     0,     5,     6,     0,     2,
       0,     7,     0,     8,     9,    10,    11,     0,     0,     0,
      12,    13,    14,    15,    16,     0,    17,    18,     0,    19,
      20,     0,    21,    22,    23,    24,    25,     0,     3,     4,
       0,   212,     5,     6,     0,     0,     2,     7,     0,     8,
       9,    10,    11,   211,     0,     0,    12,    13,    14,    15,
      16,     0,    17,    18,     0,    19,    20,     0,    21,    22,
      23,    24,    25,     0,     0,     3,     4,     0,   112,     5,
       6,     0,     0,     2,     7,     0,     8,     9,    10,    11,
     211,     0,     0,    12,    13,    14,    15,    16,     0,    17,
      18,     0,    19,    20,     0,    21,    22,    23,    24,    25,
       0,     0,     3,     4,   365,     0,     5,     6,     2,     0,
       0,     7,     0,     8,     9,    10,    11,     0,     0,     0,
      12,    13,    14,    15,    16,     0,    17,    18,     0,    19,
      20,     0,    21,    22,    23,    24,    25,     3,     4,     0,
       0,     5,     6,     2,     0,     0,     7,     0,     8,     9,
      10,    11,     0,     0,     0,    12,    13,    14,    15,    16,
       0,    17,    18,     0,    19,    20,     0,    21,    22,    23,
      24,    25,     0,     4,     0,     0,     5,     6,     0,     0,
       0,     7,     0,     8,     9,     0,    11,     0,     0,     0,
       0,    13,    14,     0,    16,     0,    17,    18,     0,     0,
      20,     0,     0,    22,    23,    24,    25
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-228))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
      29,   118,     0,    56,    33,    80,    35,   113,    49,    43,
     206,    92,   180,   119,     3,     4,   200,    29,    33,    48,
      35,   122,     3,    33,    76,    35,    78,    56,     0,    29,
       3,   258,    30,    31,    80,    78,    71,     3,    36,    37,
      69,    39,    49,     3,     4,   151,   121,    72,     3,     3,
     125,    94,    59,    78,   222,    12,    13,    14,    15,    16,
      17,    18,    19,   138,    78,    22,    23,    39,   109,   145,
      54,    72,     3,    80,   155,   156,   157,    78,    92,    73,
     307,   110,    54,   112,    73,     3,   115,     3,   134,   118,
       3,    91,   138,    74,   140,   124,    73,   112,   110,   128,
     206,    74,   109,   115,   145,    71,   152,    78,    71,    75,
      71,    59,   124,    73,   121,   122,    71,    71,   125,   126,
     118,   150,    71,    94,   153,    80,   227,   173,    78,    81,
      82,   138,    80,   117,    78,   150,    93,   213,   153,    94,
     150,   187,   326,   153,    94,   117,   180,   253,   316,   255,
      94,   257,   320,    71,   183,    71,   185,    75,    71,   355,
      26,    27,    80,    78,    80,   240,     8,    80,   183,    79,
     185,   200,   213,   121,   122,   185,    92,   125,   126,    94,
     261,    89,    38,    90,   213,   200,    78,    78,   222,    45,
     138,   237,    72,    72,   240,    30,    31,    32,    78,    55,
     229,   213,    94,    94,    71,   234,    20,    21,    75,   238,
      72,    67,    80,    80,    73,   256,    78,    85,    86,   336,
     227,    87,    88,   238,    80,    28,    72,    24,    25,    72,
      93,   337,    78,   240,   340,    78,    71,    94,    72,    72,
      75,    72,    77,    72,    78,    78,   292,    78,   294,   355,
      72,   297,   298,    72,   307,    94,    78,   183,   256,    78,
      72,   290,    72,    72,   305,    72,    78,    92,    78,    78,
     299,    78,    71,    71,    92,   290,   305,    75,   307,   227,
       3,     4,   316,    74,   299,   314,   320,    78,   158,   159,
      74,    71,   240,   322,    78,    75,   342,   326,    92,    71,
     346,    74,   314,    75,    71,    78,    71,   305,   160,   161,
      71,   326,   310,    92,   343,   162,   163,   164,   165,    43,
      76,   166,   167,    92,    72,    93,   372,   373,   343,   375,
      72,    72,    78,    92,    68,    72,    94,    76,   336,    71,
      76,   387,   388,    72,    43,    94,   169,   376,   377,   378,
     170,   168,    76,    77,   400,    79,   385,   172,   171,   110,
     126,   376,   377,   378,   290,   129,   395,   396,    92,   398,
     385,   314,   401,   299,   128,   310,   405,   238,   407,   408,
     395,   396,    39,   398,    -1,   414,   401,   118,   138,    -1,
     405,    -1,   407,   408,    -1,    -1,    -1,    -1,    -1,   414,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   132,   133,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   343,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,    -1,
     376,   377,   378,    -1,    -1,    -1,   180,    -1,    -1,   385,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   395,
     396,    -1,   398,    -1,    -1,   401,    -1,    -1,    -1,   405,
      -1,   407,   408,    -1,    -1,    -1,    -1,    -1,   414,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   222,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,     8,     9,    10,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    30,    31,   261,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    -1,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    -1,    -1,    71,    -1,    73,    74,
      -1,    -1,    -1,    -1,    79,    80,    81,    82,    83,    84,
      -1,    -1,   316,    -1,    -1,    -1,   320,    -1,    -1,    94,
       3,     4,     5,     6,     7,     8,     9,    10,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    30,    31,    -1,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      -1,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    -1,    -1,    71,    -1,
      73,    74,    -1,    -1,    -1,    -1,    79,    80,    81,    82,
      83,    84,     3,     4,     5,     6,     7,     8,     9,    10,
      -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    30,
      31,    -1,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    -1,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      71,    -1,    73,    74,    -1,    -1,    -1,    -1,    79,    80,
      81,    82,    83,    84,     3,     4,     5,     6,     7,     8,
       9,    10,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    30,    31,    -1,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      -1,    -1,    71,    -1,    73,    -1,    -1,    -1,    -1,    -1,
      79,    80,    81,    82,    83,    84,     3,     4,     5,     6,
       7,     8,     9,    10,    -1,    94,    -1,    -1,    -1,    -1,
      -1,    -1,     3,    -1,     5,     6,     7,     8,     9,    10,
      -1,    -1,    -1,    30,    31,    -1,    -1,    -1,    35,    36,
      -1,    -1,    39,    40,    41,    -1,    -1,    -1,    45,    30,
      31,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,    56,
      -1,    -1,    59,    -1,    45,    62,    -1,    -1,    -1,    -1,
      -1,    68,    -1,    -1,    71,    -1,    73,    -1,    59,    -1,
      -1,    -1,    79,    80,    81,    82,    83,    84,    -1,     3,
      71,     5,     6,     7,     8,     9,    10,    94,    79,    80,
      81,    82,    83,    84,    -1,     3,    -1,     5,     6,     7,
       8,     9,    10,    94,    -1,    -1,    30,    31,    -1,    -1,
      -1,     3,    -1,     5,     6,     7,     8,     9,    10,    -1,
      -1,    45,    30,    31,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    -1,    45,    30,    31,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    71,    -1,    -1,
      -1,    59,    -1,    45,    -1,    79,    80,    81,    82,    83,
      84,    -1,    -1,    71,    -1,     4,    -1,    59,    -1,    -1,
      94,    79,    80,    81,    82,    83,    84,    -1,    -1,    71,
      -1,    -1,    -1,    -1,    -1,    -1,    94,    79,    80,    81,
      82,    83,    84,    -1,    33,    34,    -1,    -1,    37,    38,
      -1,    -1,    94,    42,    -1,    44,    45,    46,    47,    -1,
      -1,    -1,    51,    52,    53,    54,    55,    -1,    57,    58,
      -1,    60,    61,    -1,    63,    64,    65,    66,    67,    -1,
      -1,    -1,    -1,    -1,    73,     3,     4,     5,     6,     7,
       8,     9,    10,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    30,    31,    -1,    -1,    34,    -1,    -1,    37,
      38,    -1,    -1,    -1,    42,    -1,    44,    45,    -1,    47,
      -1,    -1,    -1,    -1,    52,    53,    -1,    55,    -1,    57,
      58,    59,    -1,    61,    -1,    -1,    64,    65,    66,    67,
      -1,    -1,    -1,    71,    -1,    73,    -1,    -1,    -1,    -1,
      -1,    79,    80,    81,    82,    83,    84,     3,     4,     5,
       6,     7,     8,     9,    10,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,    -1,     5,     6,     7,
       8,     9,    10,    -1,    30,    31,    -1,    -1,    34,    -1,
      -1,    37,    38,    -1,    -1,    -1,    42,    -1,    44,    45,
      -1,    47,    30,    31,    -1,    -1,    52,    53,    -1,    55,
      -1,    57,    58,    59,    -1,    61,    -1,    45,    64,    65,
      66,    67,    -1,    -1,    -1,    71,    -1,    -1,    -1,    -1,
      -1,    59,    -1,    79,    80,    81,    82,    83,    84,    -1,
      -1,    -1,    -1,    71,    -1,    73,    74,    -1,    -1,    -1,
      -1,    79,    80,    81,    82,    83,    84,     3,    -1,     5,
       6,     7,     8,     9,    10,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,    -1,     5,     6,     7,     8,     9,
      10,    -1,    -1,    -1,    30,    31,    -1,    -1,    -1,     3,
      -1,     5,     6,     7,     8,     9,    10,    -1,    -1,    45,
      30,    31,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    59,    -1,    45,    30,    31,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    71,    -1,    73,    -1,    59,
      -1,    45,    -1,    79,    80,    81,    82,    83,    84,    -1,
      -1,    71,    -1,    -1,    -1,    59,    76,    -1,    -1,    79,
      80,    81,    82,    83,    84,    -1,    -1,    71,    72,    -1,
      -1,    -1,    -1,    -1,    -1,    79,    80,    81,    82,    83,
      84,     3,    -1,     5,     6,     7,     8,     9,    10,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,    -1,     5,
       6,     7,     8,     9,    10,    -1,    -1,    -1,    30,    31,
      -1,    -1,    -1,     3,    -1,     5,     6,     7,     8,     9,
      10,    -1,    -1,    45,    30,    31,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    -1,    45,
      30,    31,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    71,
      -1,    -1,    -1,    59,    76,    45,    -1,    79,    80,    81,
      82,    83,    84,    -1,    -1,    71,    -1,    -1,    -1,    59,
      76,    -1,    -1,    79,    80,    81,    82,    83,    84,    -1,
      -1,    71,    72,    -1,    -1,    -1,    -1,    -1,    -1,    79,
      80,    81,    82,    83,    84,     3,    -1,     5,     6,     7,
       8,     9,    10,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,    -1,     5,     6,     7,     8,     9,    10,    -1,
      -1,    -1,    30,    31,    -1,    -1,    -1,     3,    -1,     5,
       6,     7,     8,     9,    10,    -1,    -1,    45,    30,    31,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    59,    -1,    45,    30,    31,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    71,    72,    -1,    -1,    59,    -1,    45,
      -1,    79,    80,    81,    82,    83,    84,    -1,    -1,    71,
      72,    -1,    -1,    59,    -1,    -1,    -1,    79,    80,    81,
      82,    83,    84,    -1,    -1,    71,    72,    -1,    -1,    -1,
      -1,    -1,    -1,    79,    80,    81,    82,    83,    84,     3,
      -1,     5,     6,     7,     8,     9,    10,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,    -1,     5,     6,     7,
       8,     9,    10,    -1,    -1,    -1,    30,    31,    -1,    -1,
      -1,     3,    -1,     5,     6,     7,     8,     9,    10,    -1,
      -1,    45,    30,    31,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    -1,    45,    30,    31,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    71,    -1,    -1,
      -1,    59,    -1,    45,    -1,    79,    80,    81,    82,    83,
      84,    -1,    -1,    71,    -1,    -1,    -1,    59,    -1,    -1,
      -1,    79,    80,    81,    82,    83,    84,    -1,     3,    71,
       5,     6,     7,     8,     9,    10,    -1,    79,    80,    81,
      82,    83,    84,    -1,     3,    -1,     5,     6,     7,     8,
       9,    10,    -1,    -1,    -1,    30,    31,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    30,    31,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    59,    -1,    45,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    71,    -1,    -1,    -1,
      59,    -1,     3,     4,    79,    80,    81,    82,    83,    84,
      11,    -1,    71,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      79,    80,    81,    82,    83,    84,    -1,    -1,    -1,    -1,
      -1,    -1,    33,    34,    -1,    -1,    37,    38,    -1,    -1,
      -1,    42,    -1,    44,    45,    46,    47,    -1,     4,    -1,
      51,    52,    53,    54,    55,    11,    57,    58,    -1,    60,
      61,    -1,    63,    64,    65,    66,    67,    -1,    -1,    -1,
      71,    72,    -1,    -1,    75,    -1,    -1,    33,    34,    80,
      -1,    37,    38,    -1,    -1,    -1,    42,    -1,    44,    45,
      46,    47,    -1,    -1,    -1,    51,    52,    53,    54,    55,
      -1,    57,    58,    -1,    60,    61,    -1,    63,    64,    65,
      66,    67,    -1,     0,     1,    71,    72,     4,    -1,    75,
      -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    33,    34,    -1,    -1,
      37,    38,    -1,    -1,     4,    42,    -1,    44,    45,    46,
      47,    -1,    -1,    -1,    51,    52,    53,    54,    55,    -1,
      57,    58,    -1,    60,    61,    -1,    63,    64,    65,    66,
      67,    -1,    69,    70,    34,    -1,    -1,    37,    38,     4,
      -1,    -1,    42,    -1,    44,    45,    -1,    47,    -1,    -1,
      -1,    -1,    52,    53,    -1,    55,    -1,    57,    58,    -1,
      -1,    61,    -1,    -1,    64,    65,    66,    67,    -1,    34,
      -1,    -1,    37,    38,    74,     1,    -1,    42,     4,    44,
      45,    -1,    47,    -1,    -1,    -1,    -1,    52,    53,    -1,
      55,    -1,    57,    58,    -1,    -1,    61,    -1,    -1,    64,
      65,    66,    67,    -1,    -1,    -1,    -1,    33,    34,    74,
      -1,    37,    38,    -1,    -1,    -1,    42,    -1,    44,    45,
      46,    47,     3,     4,    -1,    51,    52,    53,    54,    55,
      11,    57,    58,    -1,    60,    61,    -1,    63,    64,    65,
      66,    67,    -1,    69,    70,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    33,    34,    -1,    -1,    37,    38,    -1,     4,
      -1,    42,    -1,    44,    45,    46,    47,    -1,    -1,    -1,
      51,    52,    53,    54,    55,    -1,    57,    58,    -1,    60,
      61,    -1,    63,    64,    65,    66,    67,    -1,    33,    34,
      -1,    72,    37,    38,    -1,    -1,     4,    42,    -1,    44,
      45,    46,    47,    11,    -1,    -1,    51,    52,    53,    54,
      55,    -1,    57,    58,    -1,    60,    61,    -1,    63,    64,
      65,    66,    67,    -1,    -1,    33,    34,    -1,    73,    37,
      38,    -1,    -1,     4,    42,    -1,    44,    45,    46,    47,
      11,    -1,    -1,    51,    52,    53,    54,    55,    -1,    57,
      58,    -1,    60,    61,    -1,    63,    64,    65,    66,    67,
      -1,    -1,    33,    34,    72,    -1,    37,    38,     4,    -1,
      -1,    42,    -1,    44,    45,    46,    47,    -1,    -1,    -1,
      51,    52,    53,    54,    55,    -1,    57,    58,    -1,    60,
      61,    -1,    63,    64,    65,    66,    67,    33,    34,    -1,
      -1,    37,    38,     4,    -1,    -1,    42,    -1,    44,    45,
      46,    47,    -1,    -1,    -1,    51,    52,    53,    54,    55,
      -1,    57,    58,    -1,    60,    61,    -1,    63,    64,    65,
      66,    67,    -1,    34,    -1,    -1,    37,    38,    -1,    -1,
      -1,    42,    -1,    44,    45,    -1,    47,    -1,    -1,    -1,
      -1,    52,    53,    -1,    55,    -1,    57,    58,    -1,    -1,
      61,    -1,    -1,    64,    65,    66,    67
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     4,    33,    34,    37,    38,    42,    44,    45,
      46,    47,    51,    52,    53,    54,    55,    57,    58,    60,
      61,    63,    64,    65,    66,    67,    69,    70,   120,   121,
     124,   125,   126,   127,   133,   134,   138,   139,   150,   162,
     163,   164,   166,   167,   168,   169,   170,     3,    71,    80,
      94,    98,   122,   123,   140,   141,   142,   121,   121,    73,
      98,    99,   150,    73,    99,   121,   121,     0,   163,    71,
       5,     6,     7,     8,     9,    10,    30,    31,    45,    59,
      71,    79,    80,    81,    82,    83,    84,    96,    97,    98,
     100,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   119,   140,   138,   142,   143,
      78,    94,    73,    93,   120,   121,   155,   165,    71,    75,
     141,   125,   128,   129,   130,   138,    73,   135,   136,    73,
      98,   146,    71,    71,    71,   102,   102,    71,    71,   102,
      45,   102,   115,   116,   118,   130,   147,     8,    30,    31,
      32,    71,    75,    77,   104,    80,    85,    86,    81,    82,
      20,    21,    26,    27,    87,    88,    24,    25,    79,    89,
      90,    28,    29,    91,    72,   138,   142,   123,   140,    35,
      36,    39,    40,    41,    48,    49,    50,    56,    62,    68,
      74,    94,    98,    99,   118,   120,   150,   153,   154,   155,
     156,   157,   158,   159,   160,   161,    73,   116,   151,   120,
     155,    11,    72,   121,   144,   145,   146,    76,   116,   130,
      74,   129,    92,   131,   132,   140,   130,   128,    74,    78,
      98,   137,   135,    72,    78,   102,   102,    45,    73,   147,
      71,   118,    12,    13,    14,    15,    16,    17,    18,    19,
      22,    23,    93,   117,    72,    78,    71,    75,   142,   148,
     149,    72,    99,    72,   101,   116,   118,    99,   104,   104,
     104,   105,   105,   106,   106,   107,   107,   107,   107,   108,
     108,   109,   110,   111,   112,   113,   118,    94,   119,    94,
      92,   153,    71,    99,    71,    94,   118,    71,    71,    92,
      94,    74,   157,   151,   152,    71,   140,   142,   148,    72,
      78,    72,    76,   119,    78,    94,    92,    74,    74,   137,
      93,    74,    78,    98,    72,    72,   156,    72,   116,   116,
      72,   144,   148,    76,   116,   149,    71,    75,   104,    72,
      78,    76,    92,    92,   153,    68,    94,   118,    94,   118,
      94,   118,   118,   153,    74,    78,   145,   132,   119,   119,
      74,    74,    72,    72,    76,    72,   144,    76,   116,   116,
     118,   153,    71,    94,   118,    94,    72,    72,    72,    74,
     151,    72,    72,    76,   118,    72,   118,    94,    94,   118,
     153,   153,   153,    72,   153,    72,    72,   118,    72,   118,
      94,    43,    94,   153,   153,    72,   153,    72,    72,   118,
     153,   153,   153,   153,    72,   153
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (scanner, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex (scanner)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, scanner); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, GISourceScanner* scanner)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    GISourceScanner* scanner;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (scanner);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, GISourceScanner* scanner)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    GISourceScanner* scanner;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, GISourceScanner* scanner)
#else
static void
yy_reduce_print (yyvsp, yyrule, scanner)
    YYSTYPE *yyvsp;
    int yyrule;
    GISourceScanner* scanner;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, scanner); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, GISourceScanner* scanner)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, scanner)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    GISourceScanner* scanner;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (scanner);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (GISourceScanner* scanner);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (GISourceScanner* scanner)
#else
int
yyparse (scanner)
    GISourceScanner* scanner;
#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1806 of yacc.c  */
#line 225 "scannerparser.y"
    {
		(yyval.symbol) = g_hash_table_lookup (const_table, (yyvsp[(1) - (1)].str));
		if ((yyval.symbol) == NULL) {
			(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		} else {
			(yyval.symbol) = gi_source_symbol_ref ((yyval.symbol));
		}
	  }
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 234 "scannerparser.y"
    {
		char *rest;
		guint64 value;
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		if (g_str_has_prefix (yytext, "0x") && strlen (yytext) > 2) {
			value = g_ascii_strtoull (yytext + 2, &rest, 16);
		} else if (g_str_has_prefix (yytext, "0") && strlen (yytext) > 1) {
			value = g_ascii_strtoull (yytext + 1, &rest, 8);
		} else {
			value = g_ascii_strtoull (yytext, &rest, 10);
		}
		(yyval.symbol)->const_int = value;
		(yyval.symbol)->const_int_is_unsigned = (rest && (rest[0] == 'U'));
	  }
    break;

  case 4:

/* Line 1806 of yacc.c  */
#line 250 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = g_utf8_get_char(yytext + 1);
	  }
    break;

  case 5:

/* Line 1806 of yacc.c  */
#line 256 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_double_set = TRUE;
		(yyval.symbol)->const_double = 0.0;
        sscanf (yytext, "%lf", &((yyval.symbol)->const_double));
	  }
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 264 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(2) - (3)].symbol);
	  }
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 268 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 9:

/* Line 1806 of yacc.c  */
#line 276 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		yytext[strlen (yytext) - 1] = '\0';
		(yyval.symbol)->const_string = parse_c_string_literal (yytext + 1);
                if (!g_utf8_validate ((yyval.symbol)->const_string, -1, NULL))
                  {
#if 0
                    g_warning ("Ignoring non-UTF-8 constant string \"%s\"", yytext + 1);
#endif
                    g_free((yyval.symbol)->const_string);
                    (yyval.symbol)->const_string = NULL;
                  }

	  }
    break;

  case 10:

/* Line 1806 of yacc.c  */
#line 291 "scannerparser.y"
    {
		char *strings, *string2;
		(yyval.symbol) = (yyvsp[(1) - (2)].symbol);
		yytext[strlen (yytext) - 1] = '\0';
		string2 = parse_c_string_literal (yytext + 1);
		strings = g_strconcat ((yyval.symbol)->const_string, string2, NULL);
		g_free ((yyval.symbol)->const_string);
		g_free (string2);
		(yyval.symbol)->const_string = strings;
	  }
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 305 "scannerparser.y"
    {
		(yyval.str) = g_strdup (yytext);
	  }
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 318 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 322 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 17:

/* Line 1806 of yacc.c  */
#line 326 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 330 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 334 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 338 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 342 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 25:

/* Line 1806 of yacc.c  */
#line 355 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 359 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 27:

/* Line 1806 of yacc.c  */
#line 363 "scannerparser.y"
    {
		switch ((yyvsp[(1) - (2)].unary_operator)) {
		case UNARY_PLUS:
			(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
			break;
		case UNARY_MINUS:
			(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
			(yyval.symbol)->const_int = -(yyvsp[(2) - (2)].symbol)->const_int;
			break;
		case UNARY_BITWISE_COMPLEMENT:
			(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
			(yyval.symbol)->const_int = ~(yyvsp[(2) - (2)].symbol)->const_int;
			break;
		case UNARY_LOGICAL_NEGATION:
			(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
			(yyval.symbol)->const_int = !gi_source_symbol_get_const_boolean ((yyvsp[(2) - (2)].symbol));
			break;
		default:
			(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
			break;
		}
	  }
    break;

  case 28:

/* Line 1806 of yacc.c  */
#line 386 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(3) - (4)].symbol);
		if ((yyval.symbol)->const_int_set) {
			(yyval.symbol)->base_type = gi_source_basic_type_new ((yyval.symbol)->const_int_is_unsigned ? "guint64" : "gint64");
		}
	  }
    break;

  case 29:

/* Line 1806 of yacc.c  */
#line 393 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(3) - (4)].symbol);
		if ((yyval.symbol)->const_int_set) {
			(yyval.symbol)->base_type = gi_source_basic_type_new ("guint64");
		}
	  }
    break;

  case 30:

/* Line 1806 of yacc.c  */
#line 400 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 404 "scannerparser.y"
    {
		ctype_free ((yyvsp[(3) - (4)].ctype));
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 412 "scannerparser.y"
    {
		(yyval.unary_operator) = UNARY_ADDRESS_OF;
	  }
    break;

  case 33:

/* Line 1806 of yacc.c  */
#line 416 "scannerparser.y"
    {
		(yyval.unary_operator) = UNARY_POINTER_INDIRECTION;
	  }
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 420 "scannerparser.y"
    {
		(yyval.unary_operator) = UNARY_PLUS;
	  }
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 424 "scannerparser.y"
    {
		(yyval.unary_operator) = UNARY_MINUS;
	  }
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 428 "scannerparser.y"
    {
		(yyval.unary_operator) = UNARY_BITWISE_COMPLEMENT;
	  }
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 432 "scannerparser.y"
    {
		(yyval.unary_operator) = UNARY_LOGICAL_NEGATION;
	  }
    break;

  case 39:

/* Line 1806 of yacc.c  */
#line 440 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(4) - (4)].symbol);
		if ((yyval.symbol)->const_int_set || (yyval.symbol)->const_double_set || (yyval.symbol)->const_string != NULL) {
			(yyval.symbol)->base_type = (yyvsp[(2) - (4)].ctype);
		} else {
			ctype_free ((yyvsp[(2) - (4)].ctype));
		}
	  }
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 453 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int * (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 459 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		if ((yyvsp[(3) - (3)].symbol)->const_int != 0) {
			(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int / (yyvsp[(3) - (3)].symbol)->const_int;
		}
	  }
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 467 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		if ((yyvsp[(3) - (3)].symbol)->const_int != 0) {
			(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int % (yyvsp[(3) - (3)].symbol)->const_int;
		}
	  }
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 479 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int + (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 485 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int - (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 495 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int << (yyvsp[(3) - (3)].symbol)->const_int;

		/* assume this is a bitfield/flags declaration
		 * if a left shift operator is sued in an enum value
                 * This mimics the glib-mkenum behavior.
		 */
		is_bitfield = TRUE;
	  }
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 507 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int >> (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 517 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int < (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 523 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int > (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 529 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int <= (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 535 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int >= (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 545 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int == (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 551 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int != (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 561 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int & (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 571 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int ^ (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 581 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(1) - (3)].symbol)->const_int | (yyvsp[(3) - (3)].symbol)->const_int;
	  }
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 591 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int =
		  gi_source_symbol_get_const_boolean ((yyvsp[(1) - (3)].symbol)) &&
		  gi_source_symbol_get_const_boolean ((yyvsp[(3) - (3)].symbol));
	  }
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 603 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_CONST, scanner->current_filename, lineno);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int =
		  gi_source_symbol_get_const_boolean ((yyvsp[(1) - (3)].symbol)) ||
		  gi_source_symbol_get_const_boolean ((yyvsp[(3) - (3)].symbol));
	  }
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 615 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_get_const_boolean ((yyvsp[(1) - (5)].symbol)) ? (yyvsp[(3) - (5)].symbol) : (yyvsp[(5) - (5)].symbol);
	  }
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 623 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 85:

/* Line 1806 of yacc.c  */
#line 646 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 87:

/* Line 1806 of yacc.c  */
#line 659 "scannerparser.y"
    {
		GList *l;
		for (l = (yyvsp[(2) - (3)].list); l != NULL; l = l->next) {
			GISourceSymbol *sym = l->data;
			gi_source_symbol_merge_type (sym, gi_source_type_copy ((yyvsp[(1) - (3)].ctype)));
			if ((yyvsp[(1) - (3)].ctype)->storage_class_specifier & STORAGE_CLASS_TYPEDEF) {
				sym->type = CSYMBOL_TYPE_TYPEDEF;
			} else if (sym->base_type->type == CTYPE_FUNCTION) {
				sym->type = CSYMBOL_TYPE_FUNCTION;
			} else {
				sym->type = CSYMBOL_TYPE_OBJECT;
			}
			gi_source_scanner_add_symbol (scanner, sym);
			gi_source_symbol_unref (sym);
		}
		ctype_free ((yyvsp[(1) - (3)].ctype));
	  }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 677 "scannerparser.y"
    {
		ctype_free ((yyvsp[(1) - (2)].ctype));
	  }
    break;

  case 89:

/* Line 1806 of yacc.c  */
#line 684 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(2) - (2)].ctype);
		(yyval.ctype)->storage_class_specifier |= (yyvsp[(1) - (2)].storage_class_specifier);
	  }
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 689 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_type_new (CTYPE_INVALID);
		(yyval.ctype)->storage_class_specifier |= (yyvsp[(1) - (1)].storage_class_specifier);
	  }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 694 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(1) - (2)].ctype);
		/* combine basic types like unsigned int and long long */
		if ((yyval.ctype)->type == CTYPE_BASIC_TYPE && (yyvsp[(2) - (2)].ctype)->type == CTYPE_BASIC_TYPE) {
			char *name = g_strdup_printf ("%s %s", (yyval.ctype)->name, (yyvsp[(2) - (2)].ctype)->name);
			g_free ((yyval.ctype)->name);
			(yyval.ctype)->name = name;
			ctype_free ((yyvsp[(2) - (2)].ctype));
		} else {
			(yyval.ctype)->base_type = (yyvsp[(2) - (2)].ctype);
		}
	  }
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 708 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(2) - (2)].ctype);
		(yyval.ctype)->type_qualifier |= (yyvsp[(1) - (2)].type_qualifier);
	  }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 713 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_type_new (CTYPE_INVALID);
		(yyval.ctype)->type_qualifier |= (yyvsp[(1) - (1)].type_qualifier);
	  }
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 718 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(2) - (2)].ctype);
		(yyval.ctype)->function_specifier |= (yyvsp[(1) - (2)].function_specifier);
	  }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 723 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_type_new (CTYPE_INVALID);
		(yyval.ctype)->function_specifier |= (yyvsp[(1) - (1)].function_specifier);
	  }
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 731 "scannerparser.y"
    {
		(yyval.list) = g_list_append (NULL, (yyvsp[(1) - (1)].symbol));
	  }
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 735 "scannerparser.y"
    {
		(yyval.list) = g_list_append ((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].symbol));
	  }
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 747 "scannerparser.y"
    {
		(yyval.storage_class_specifier) = STORAGE_CLASS_TYPEDEF;
	  }
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 751 "scannerparser.y"
    {
		(yyval.storage_class_specifier) = STORAGE_CLASS_EXTERN;
	  }
    break;

  case 103:

/* Line 1806 of yacc.c  */
#line 755 "scannerparser.y"
    {
		(yyval.storage_class_specifier) = STORAGE_CLASS_STATIC;
	  }
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 759 "scannerparser.y"
    {
		(yyval.storage_class_specifier) = STORAGE_CLASS_AUTO;
	  }
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 763 "scannerparser.y"
    {
		(yyval.storage_class_specifier) = STORAGE_CLASS_REGISTER;
	  }
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 770 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_type_new (CTYPE_VOID);
	  }
    break;

  case 107:

/* Line 1806 of yacc.c  */
#line 774 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("char");
	  }
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 778 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("short");
	  }
    break;

  case 109:

/* Line 1806 of yacc.c  */
#line 782 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("int");
	  }
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 786 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("long");
	  }
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 790 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("float");
	  }
    break;

  case 112:

/* Line 1806 of yacc.c  */
#line 794 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("double");
	  }
    break;

  case 113:

/* Line 1806 of yacc.c  */
#line 798 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("signed");
	  }
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 802 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("unsigned");
	  }
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 806 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_basic_type_new ("bool");
	  }
    break;

  case 118:

/* Line 1806 of yacc.c  */
#line 812 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_typedef_new ((yyvsp[(1) - (1)].str));
		g_free ((yyvsp[(1) - (1)].str));
	  }
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 820 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(1) - (5)].ctype);
		(yyval.ctype)->name = (yyvsp[(2) - (5)].str);
		(yyval.ctype)->child_list = (yyvsp[(4) - (5)].list);

		GISourceSymbol *sym = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		if ((yyval.ctype)->type == CTYPE_STRUCT) {
			sym->type = CSYMBOL_TYPE_STRUCT;
		} else if ((yyval.ctype)->type == CTYPE_UNION) {
			sym->type = CSYMBOL_TYPE_UNION;
		} else {
			g_assert_not_reached ();
		}
		sym->ident = g_strdup ((yyval.ctype)->name);
		sym->base_type = gi_source_type_copy ((yyval.ctype));
		gi_source_scanner_add_symbol (scanner, sym);
		gi_source_symbol_unref (sym);
	  }
    break;

  case 120:

/* Line 1806 of yacc.c  */
#line 839 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(1) - (4)].ctype);
		(yyval.ctype)->child_list = (yyvsp[(3) - (4)].list);
	  }
    break;

  case 121:

/* Line 1806 of yacc.c  */
#line 844 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(1) - (2)].ctype);
		(yyval.ctype)->name = (yyvsp[(2) - (2)].str);
	  }
    break;

  case 122:

/* Line 1806 of yacc.c  */
#line 852 "scannerparser.y"
    {
                scanner->private = FALSE;
		(yyval.ctype) = gi_source_struct_new (NULL);
	  }
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 857 "scannerparser.y"
    {
                scanner->private = FALSE;
		(yyval.ctype) = gi_source_union_new (NULL);
	  }
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 866 "scannerparser.y"
    {
		(yyval.list) = g_list_concat ((yyvsp[(1) - (2)].list), (yyvsp[(2) - (2)].list));
	  }
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 873 "scannerparser.y"
    {
	    GList *l;
	    (yyval.list) = NULL;
	    for (l = (yyvsp[(2) - (3)].list); l != NULL; l = l->next)
	      {
		GISourceSymbol *sym = l->data;
		if ((yyvsp[(1) - (3)].ctype)->storage_class_specifier & STORAGE_CLASS_TYPEDEF)
		    sym->type = CSYMBOL_TYPE_TYPEDEF;
		else
		    sym->type = CSYMBOL_TYPE_MEMBER;
		gi_source_symbol_merge_type (sym, gi_source_type_copy ((yyvsp[(1) - (3)].ctype)));
                sym->private = scanner->private;
                (yyval.list) = g_list_append ((yyval.list), sym);
	      }
	    ctype_free ((yyvsp[(1) - (3)].ctype));
	  }
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 893 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(1) - (2)].ctype);
		(yyval.ctype)->base_type = (yyvsp[(2) - (2)].ctype);
	  }
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 899 "scannerparser.y"
    {
		(yyval.ctype) = (yyvsp[(2) - (2)].ctype);
		(yyval.ctype)->type_qualifier |= (yyvsp[(1) - (2)].type_qualifier);
	  }
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 904 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_type_new (CTYPE_INVALID);
		(yyval.ctype)->type_qualifier |= (yyvsp[(1) - (1)].type_qualifier);
	  }
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 912 "scannerparser.y"
    {
		(yyval.list) = g_list_append (NULL, (yyvsp[(1) - (1)].symbol));
	  }
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 916 "scannerparser.y"
    {
		(yyval.list) = g_list_append ((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].symbol));
	  }
    break;

  case 133:

/* Line 1806 of yacc.c  */
#line 923 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 135:

/* Line 1806 of yacc.c  */
#line 928 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
	  }
    break;

  case 136:

/* Line 1806 of yacc.c  */
#line 932 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(1) - (3)].symbol);
		if ((yyvsp[(3) - (3)].symbol)->const_int_set) {
		  (yyval.symbol)->const_int_set = TRUE;
		  (yyval.symbol)->const_int = (yyvsp[(3) - (3)].symbol)->const_int;
		}
	  }
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 943 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_enum_new ((yyvsp[(2) - (5)].str));
		(yyval.ctype)->child_list = (yyvsp[(4) - (5)].list);
		(yyval.ctype)->is_bitfield = is_bitfield || scanner->flags;
		last_enum_value = -1;
	  }
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 950 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_enum_new (NULL);
		(yyval.ctype)->child_list = (yyvsp[(3) - (4)].list);
		(yyval.ctype)->is_bitfield = is_bitfield || scanner->flags;
		last_enum_value = -1;
	  }
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 957 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_enum_new ((yyvsp[(2) - (6)].str));
		(yyval.ctype)->child_list = (yyvsp[(4) - (6)].list);
		(yyval.ctype)->is_bitfield = is_bitfield || scanner->flags;
		last_enum_value = -1;
	  }
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 964 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_enum_new (NULL);
		(yyval.ctype)->child_list = (yyvsp[(3) - (5)].list);
		(yyval.ctype)->is_bitfield = is_bitfield || scanner->flags;
		last_enum_value = -1;
	  }
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 971 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_enum_new ((yyvsp[(2) - (2)].str));
	  }
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 978 "scannerparser.y"
    {
                scanner->flags = FALSE;
                scanner->private = FALSE;
          }
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 986 "scannerparser.y"
    {
		/* reset flag before the first enum value */
		is_bitfield = FALSE;
	  }
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 991 "scannerparser.y"
    {
            (yyvsp[(2) - (2)].symbol)->private = scanner->private;
            (yyval.list) = g_list_append (NULL, (yyvsp[(2) - (2)].symbol));
	  }
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 996 "scannerparser.y"
    {
            (yyvsp[(3) - (3)].symbol)->private = scanner->private;
            (yyval.list) = g_list_append ((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].symbol));
	  }
    break;

  case 146:

/* Line 1806 of yacc.c  */
#line 1004 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_OBJECT, scanner->current_filename, lineno);
		(yyval.symbol)->ident = (yyvsp[(1) - (1)].str);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = ++last_enum_value;
		g_hash_table_insert (const_table, g_strdup ((yyval.symbol)->ident), gi_source_symbol_ref ((yyval.symbol)));
	  }
    break;

  case 147:

/* Line 1806 of yacc.c  */
#line 1012 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_OBJECT, scanner->current_filename, lineno);
		(yyval.symbol)->ident = (yyvsp[(1) - (3)].str);
		(yyval.symbol)->const_int_set = TRUE;
		(yyval.symbol)->const_int = (yyvsp[(3) - (3)].symbol)->const_int;
		last_enum_value = (yyval.symbol)->const_int;
		g_hash_table_insert (const_table, g_strdup ((yyval.symbol)->ident), gi_source_symbol_ref ((yyval.symbol)));
	  }
    break;

  case 148:

/* Line 1806 of yacc.c  */
#line 1024 "scannerparser.y"
    {
		(yyval.type_qualifier) = TYPE_QUALIFIER_CONST;
	  }
    break;

  case 149:

/* Line 1806 of yacc.c  */
#line 1028 "scannerparser.y"
    {
		(yyval.type_qualifier) = TYPE_QUALIFIER_RESTRICT;
	  }
    break;

  case 150:

/* Line 1806 of yacc.c  */
#line 1032 "scannerparser.y"
    {
		(yyval.type_qualifier) = TYPE_QUALIFIER_EXTENSION;
	  }
    break;

  case 151:

/* Line 1806 of yacc.c  */
#line 1036 "scannerparser.y"
    {
		(yyval.type_qualifier) = TYPE_QUALIFIER_VOLATILE;
	  }
    break;

  case 152:

/* Line 1806 of yacc.c  */
#line 1043 "scannerparser.y"
    {
		(yyval.function_specifier) = FUNCTION_INLINE;
	  }
    break;

  case 153:

/* Line 1806 of yacc.c  */
#line 1050 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), (yyvsp[(1) - (2)].ctype));
	  }
    break;

  case 155:

/* Line 1806 of yacc.c  */
#line 1059 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		(yyval.symbol)->ident = (yyvsp[(1) - (1)].str);
	  }
    break;

  case 156:

/* Line 1806 of yacc.c  */
#line 1064 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(2) - (3)].symbol);
	  }
    break;

  case 157:

/* Line 1806 of yacc.c  */
#line 1068 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(1) - (4)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), gi_source_array_new ((yyvsp[(3) - (4)].symbol)));
	  }
    break;

  case 158:

/* Line 1806 of yacc.c  */
#line 1073 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(1) - (3)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), gi_source_array_new (NULL));
	  }
    break;

  case 159:

/* Line 1806 of yacc.c  */
#line 1078 "scannerparser.y"
    {
		GISourceType *func = gi_source_function_new ();
		// ignore (void) parameter list
		if ((yyvsp[(3) - (4)].list) != NULL && ((yyvsp[(3) - (4)].list)->next != NULL || ((GISourceSymbol *) (yyvsp[(3) - (4)].list)->data)->base_type->type != CTYPE_VOID)) {
			func->child_list = (yyvsp[(3) - (4)].list);
		}
		(yyval.symbol) = (yyvsp[(1) - (4)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), func);
	  }
    break;

  case 160:

/* Line 1806 of yacc.c  */
#line 1088 "scannerparser.y"
    {
		GISourceType *func = gi_source_function_new ();
		func->child_list = (yyvsp[(3) - (4)].list);
		(yyval.symbol) = (yyvsp[(1) - (4)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), func);
	  }
    break;

  case 161:

/* Line 1806 of yacc.c  */
#line 1095 "scannerparser.y"
    {
		GISourceType *func = gi_source_function_new ();
		(yyval.symbol) = (yyvsp[(1) - (3)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), func);
	  }
    break;

  case 162:

/* Line 1806 of yacc.c  */
#line 1104 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_pointer_new (NULL);
		(yyval.ctype)->type_qualifier = (yyvsp[(2) - (2)].type_qualifier);
	  }
    break;

  case 163:

/* Line 1806 of yacc.c  */
#line 1109 "scannerparser.y"
    {
		(yyval.ctype) = gi_source_pointer_new (NULL);
	  }
    break;

  case 164:

/* Line 1806 of yacc.c  */
#line 1113 "scannerparser.y"
    {
		GISourceType **base = &((yyvsp[(3) - (3)].ctype)->base_type);

		while (*base != NULL) {
			base = &((*base)->base_type);
		}
		*base = gi_source_pointer_new (NULL);
		(*base)->type_qualifier = (yyvsp[(2) - (3)].type_qualifier);
		(yyval.ctype) = (yyvsp[(3) - (3)].ctype);
	  }
    break;

  case 165:

/* Line 1806 of yacc.c  */
#line 1124 "scannerparser.y"
    {
		GISourceType **base = &((yyvsp[(2) - (2)].ctype)->base_type);

		while (*base != NULL) {
			base = &((*base)->base_type);
		}
		*base = gi_source_pointer_new (NULL);
		(yyval.ctype) = (yyvsp[(2) - (2)].ctype);
	  }
    break;

  case 167:

/* Line 1806 of yacc.c  */
#line 1138 "scannerparser.y"
    {
		(yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier) | (yyvsp[(2) - (2)].type_qualifier);
	  }
    break;

  case 168:

/* Line 1806 of yacc.c  */
#line 1145 "scannerparser.y"
    {
		(yyval.list) = g_list_append (NULL, (yyvsp[(1) - (1)].symbol));
	  }
    break;

  case 169:

/* Line 1806 of yacc.c  */
#line 1149 "scannerparser.y"
    {
		(yyval.list) = g_list_append ((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].symbol));
	  }
    break;

  case 170:

/* Line 1806 of yacc.c  */
#line 1156 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), (yyvsp[(1) - (2)].ctype));
	  }
    break;

  case 171:

/* Line 1806 of yacc.c  */
#line 1161 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), (yyvsp[(1) - (2)].ctype));
	  }
    break;

  case 172:

/* Line 1806 of yacc.c  */
#line 1166 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		(yyval.symbol)->base_type = (yyvsp[(1) - (1)].ctype);
	  }
    break;

  case 173:

/* Line 1806 of yacc.c  */
#line 1171 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_ELLIPSIS, scanner->current_filename, lineno);
	  }
    break;

  case 174:

/* Line 1806 of yacc.c  */
#line 1178 "scannerparser.y"
    {
		GISourceSymbol *sym = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		sym->ident = (yyvsp[(1) - (1)].str);
		(yyval.list) = g_list_append (NULL, sym);
	  }
    break;

  case 175:

/* Line 1806 of yacc.c  */
#line 1184 "scannerparser.y"
    {
		GISourceSymbol *sym = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		sym->ident = (yyvsp[(3) - (3)].str);
		(yyval.list) = g_list_append ((yyvsp[(1) - (3)].list), sym);
	  }
    break;

  case 178:

/* Line 1806 of yacc.c  */
#line 1198 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		gi_source_symbol_merge_type ((yyval.symbol), (yyvsp[(1) - (1)].ctype));
	  }
    break;

  case 180:

/* Line 1806 of yacc.c  */
#line 1204 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(2) - (2)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), (yyvsp[(1) - (2)].ctype));
	  }
    break;

  case 181:

/* Line 1806 of yacc.c  */
#line 1212 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(2) - (3)].symbol);
	  }
    break;

  case 182:

/* Line 1806 of yacc.c  */
#line 1216 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		gi_source_symbol_merge_type ((yyval.symbol), gi_source_array_new (NULL));
	  }
    break;

  case 183:

/* Line 1806 of yacc.c  */
#line 1221 "scannerparser.y"
    {
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		gi_source_symbol_merge_type ((yyval.symbol), gi_source_array_new ((yyvsp[(2) - (3)].symbol)));
	  }
    break;

  case 184:

/* Line 1806 of yacc.c  */
#line 1226 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(1) - (3)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), gi_source_array_new (NULL));
	  }
    break;

  case 185:

/* Line 1806 of yacc.c  */
#line 1231 "scannerparser.y"
    {
		(yyval.symbol) = (yyvsp[(1) - (4)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), gi_source_array_new ((yyvsp[(3) - (4)].symbol)));
	  }
    break;

  case 186:

/* Line 1806 of yacc.c  */
#line 1236 "scannerparser.y"
    {
		GISourceType *func = gi_source_function_new ();
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		gi_source_symbol_merge_type ((yyval.symbol), func);
	  }
    break;

  case 187:

/* Line 1806 of yacc.c  */
#line 1242 "scannerparser.y"
    {
		GISourceType *func = gi_source_function_new ();
		// ignore (void) parameter list
		if ((yyvsp[(2) - (3)].list) != NULL && ((yyvsp[(2) - (3)].list)->next != NULL || ((GISourceSymbol *) (yyvsp[(2) - (3)].list)->data)->base_type->type != CTYPE_VOID)) {
			func->child_list = (yyvsp[(2) - (3)].list);
		}
		(yyval.symbol) = gi_source_symbol_new (CSYMBOL_TYPE_INVALID, scanner->current_filename, lineno);
		gi_source_symbol_merge_type ((yyval.symbol), func);
	  }
    break;

  case 188:

/* Line 1806 of yacc.c  */
#line 1252 "scannerparser.y"
    {
		GISourceType *func = gi_source_function_new ();
		(yyval.symbol) = (yyvsp[(1) - (3)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), func);
	  }
    break;

  case 189:

/* Line 1806 of yacc.c  */
#line 1258 "scannerparser.y"
    {
		GISourceType *func = gi_source_function_new ();
		// ignore (void) parameter list
		if ((yyvsp[(3) - (4)].list) != NULL && ((yyvsp[(3) - (4)].list)->next != NULL || ((GISourceSymbol *) (yyvsp[(3) - (4)].list)->data)->base_type->type != CTYPE_VOID)) {
			func->child_list = (yyvsp[(3) - (4)].list);
		}
		(yyval.symbol) = (yyvsp[(1) - (4)].symbol);
		gi_source_symbol_merge_type ((yyval.symbol), func);
	  }
    break;

  case 190:

/* Line 1806 of yacc.c  */
#line 1271 "scannerparser.y"
    {
		(yyval.str) = g_strdup (yytext);
	  }
    break;

  case 240:

/* Line 1806 of yacc.c  */
#line 1378 "scannerparser.y"
    {
		(yyval.str) = g_strdup (yytext + strlen ("#define "));
	  }
    break;

  case 241:

/* Line 1806 of yacc.c  */
#line 1385 "scannerparser.y"
    {
		(yyval.str) = g_strdup (yytext + strlen ("#define "));
	  }
    break;

  case 243:

/* Line 1806 of yacc.c  */
#line 1396 "scannerparser.y"
    {
		if ((yyvsp[(2) - (2)].symbol)->const_int_set || (yyvsp[(2) - (2)].symbol)->const_double_set || (yyvsp[(2) - (2)].symbol)->const_string != NULL) {
			(yyvsp[(2) - (2)].symbol)->ident = (yyvsp[(1) - (2)].str);
			gi_source_scanner_add_symbol (scanner, (yyvsp[(2) - (2)].symbol));
			gi_source_symbol_unref ((yyvsp[(2) - (2)].symbol));
		}
	  }
    break;



/* Line 1806 of yacc.c  */
#line 3995 "scannerparser.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, scanner);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, scanner);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 2067 of yacc.c  */
#line 1411 "scannerparser.y"

static void
yyerror (GISourceScanner *scanner, const char *s)
{
  /* ignore errors while doing a macro scan as not all object macros
   * have valid expressions */
  if (!scanner->macro_scan)
    {
      fprintf(stderr, "%s:%d: %s in '%s' at '%s'\n",
	      scanner->current_filename, lineno, s, linebuf, yytext);
    }
}

static int
eat_hspace (FILE * f)
{
  int c;
  do
    {
      c = fgetc (f);
    }
  while (c == ' ' || c == '\t');
  return c;
}

static int
eat_line (FILE * f, int c)
{
  while (c != EOF && c != '\n')
    {
      c = fgetc (f);
    }
  if (c == '\n')
    {
      c = fgetc (f);
      if (c == ' ' || c == '\t')
        {
          c = eat_hspace (f);
        }
    }
  return c;
}

static int
read_identifier (FILE * f, int c, char **identifier)
{
  GString *id = g_string_new ("");
  while (g_ascii_isalnum (c) || c == '_')
    {
      g_string_append_c (id, c);
      c = fgetc (f);
    }
  *identifier = g_string_free (id, FALSE);
  return c;
}

void
gi_source_scanner_parse_macros (GISourceScanner *scanner, GList *filenames)
{
  GError *error = NULL;
  char *tmp_name = NULL;
  FILE *fmacros =
    fdopen (g_file_open_tmp ("gen-introspect-XXXXXX.h", &tmp_name, &error),
            "w+");
  g_unlink (tmp_name);

  GList *l;
  for (l = filenames; l != NULL; l = l->next)
    {
      FILE *f = fopen (l->data, "r");
      int line = 1;

      GString *define_line;
      char *str;
      gboolean error_line = FALSE;
      int c = eat_hspace (f);
      while (c != EOF)
        {
          if (c != '#')
            {
              /* ignore line */
              c = eat_line (f, c);
              line++;
              continue;
            }

          /* print current location */
          str = g_strescape (l->data, "");
          fprintf (fmacros, "# %d \"%s\"\n", line, str);
          g_free (str);

          c = eat_hspace (f);
          c = read_identifier (f, c, &str);
          if (strcmp (str, "define") != 0 || (c != ' ' && c != '\t'))
            {
              g_free (str);
              /* ignore line */
              c = eat_line (f, c);
              line++;
              continue;
            }
          g_free (str);
          c = eat_hspace (f);
          c = read_identifier (f, c, &str);
          if (strlen (str) == 0 || (c != ' ' && c != '\t' && c != '('))
            {
              g_free (str);
              /* ignore line */
              c = eat_line (f, c);
              line++;
              continue;
            }
          define_line = g_string_new ("#define ");
          g_string_append (define_line, str);
          g_free (str);
          if (c == '(')
            {
              while (c != ')')
                {
                  g_string_append_c (define_line, c);
                  c = fgetc (f);
                  if (c == EOF || c == '\n')
                    {
                      error_line = TRUE;
                      break;
                    }
                }
              if (error_line)
                {
                  g_string_free (define_line, TRUE);
                  /* ignore line */
                  c = eat_line (f, c);
                  line++;
                  continue;
                }

              g_assert (c == ')');
              g_string_append_c (define_line, c);
              c = fgetc (f);

              /* found function-like macro */
              fprintf (fmacros, "%s\n", define_line->str);

              g_string_free (define_line, TRUE);
              /* ignore rest of line */
              c = eat_line (f, c);
              line++;
              continue;
            }
          if (c != ' ' && c != '\t')
            {
              g_string_free (define_line, TRUE);
              /* ignore line */
              c = eat_line (f, c);
              line++;
              continue;
            }
          while (c != EOF && c != '\n')
            {
              g_string_append_c (define_line, c);
              c = fgetc (f);
              if (c == '\\')
                {
                  c = fgetc (f);
                  if (c == '\n')
                    {
                      /* fold lines when seeing backslash new-line sequence */
                      c = fgetc (f);
                    }
                  else
                    {
                      g_string_append_c (define_line, '\\');
                    }
                }
            }

          /* found object-like macro */
          fprintf (fmacros, "%s\n", define_line->str);

          c = eat_line (f, c);
          line++;
        }

      fclose (f);
    }

  rewind (fmacros);
  gi_source_scanner_parse_file (scanner, fmacros);
}

gboolean
gi_source_scanner_parse_file (GISourceScanner *scanner, FILE *file)
{
  g_return_val_if_fail (file != NULL, FALSE);

  const_table = g_hash_table_new_full (g_str_hash, g_str_equal,
				       g_free, (GDestroyNotify)gi_source_symbol_unref);

  lineno = 1;
  yyin = file;
  yyparse (scanner);

  g_hash_table_destroy (const_table);
  const_table = NULL;

  yyin = NULL;

  return TRUE;
}

gboolean
gi_source_scanner_lex_filename (GISourceScanner *scanner, const gchar *filename)
{
  lineno = 1;
  yyin = fopen (filename, "r");

  while (yylex (scanner) != YYEOF)
    ;

  fclose (yyin);

  return TRUE;
}

