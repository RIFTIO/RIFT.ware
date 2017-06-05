
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define ASSERT assert
#include <string.h>
#include <getopt.h>
#include <stdarg.h>
#include <ctype.h>

#define ZZZ 1


// Include protoc-c generated code to decode .dso files
#include "descriptor.pb-c.h"
#include "descriptor.pb-c.c"

static int indent_level = 0;
#define INDENT_PUSH() indent_level += 2
#define INDENT_POP() { ASSERT(indent_level >= 2); indent_level -= 2; }

FILE *hfp; // FILE stream for generated .h file
#define MAXFILENAME 8192
char h_genfilename[MAXFILENAME]; // Name of generated .h file
FILE *cfp; // FILE stream for generated .c file
char c_genfilename[MAXFILENAME]; // Name of generated .c file
int debug_c = 0; // generate debug info into .c file
int debug_h = 0; // generate debug info into .h file
int disable_c = 0; // disable generation of .c file
int disable_h = 0; // disable generation of .h file


#define MAX_SCOPE_DEPTH  128
#define MAX_SCOPE_STRLEN 4096
int current_scope_level = 0;
char current_scope_str[MAX_SCOPE_STRLEN+1] = {};
char *scope_stack[MAX_SCOPE_DEPTH+1] = {current_scope_str};
char package[MAX_SCOPE_STRLEN];
int warnings;

int scope_rule_flat_names = 0;
struct {
  int struct_level;      // # of scope levels to preserve for structs when flat
  int enum_level;        // # of scope levels to preserve for enums when flat
  int enum_item_level;   // # of scope levels to preserve for enum items when flat
} flat_limits = {1 ,1, 2};
int force_enum_items_uppercase = 1; // TRUE by default

int scope_package_pushed = 0;




typedef enum {
  SCOPE_PACKAGE,
  SCOPE_MESSAGE,
  SCOPE_ENUM,
  SCOPE_ENUM_ITEM,
} scope_t;
char *scope_t_string[] =
  {
    "SCOPE_PACKAGE",
    "SCOPE_MESSAGE",
    "SCOPE_ENUM",
    "SCOPE_ENUM_ITEM"
  };



/*
 * If enabled, send debug printf to one or more of the generated files
 */
__attribute__ ((format (printf, 1, 2)))
int
DEBUG_PRINTF(const char *format, ...)
{
  int rc = 0;
  va_list args;


  va_start(args, format);
  if (debug_c && !disable_c) {
    rc = vfprintf(cfp,format,args);
  }
  va_end(args);

  va_start(args, format);
  if (debug_h && !disable_h) {
    rc = vfprintf(hfp,format,args);
  }
  va_end(args);

  return rc;
}


/*
 * Prints WARNING message to stderr
 */
__attribute__ ((format (printf, 1, 2)))
int
WARNING_PRINTF(const char *format, ...)
{
  int rc = 0;
  va_list args;

  va_start(args, format);
  rc = vfprintf(stderr,format,args);
  va_end(args);

  return rc;
}



/*
 * If enabled, generate .c file output
 */
__attribute__  ((format (printf, 1, 2)))
int
C_PRINTF(const char *format, ...)
{
  int rc = 0;
  va_list args;
  va_start(args, format);

  if (!disable_c) {
    if (format[0] != '\n') {
      fprintf(cfp,"%*s",indent_level,"");
    }
    rc = vfprintf(cfp,format,args);
  }

  va_end(args);
  return rc;
}



/*
 * If enabled, generate .h file output
 */
__attribute__  ((format (printf, 1, 2)))
int
H_PRINTF(const char *format, ...)
{
  int rc = 0;
  va_list args;
  va_start(args, format);

  if (!disable_h) {
    if (format[0] != '\n') {
      fprintf(hfp,"%*s",indent_level,"");
    }
    rc = vfprintf(hfp,format,args);
  }

  va_end(args);
  return rc;
}



/*
 * Try to send to BOTH the .c and .h files
 */
__attribute__  ((format (printf, 1, 2)))
int
B_PRINTF(const char *format, ...)
{
  int rc = 0;
  va_list args;

  va_start(args, format);
  if (format[0] != '\n') {
    fprintf(cfp,"%*s",indent_level,"");
  }
  if (!disable_c) {
    rc = vfprintf(cfp,format,args);
  }
  va_end(args);

  va_start(args, format);
  if (!disable_h) {
    if (format[0] != '\n') {
      fprintf(hfp,"%*s",indent_level,"");
    }
    rc = vfprintf(hfp,format,args);
  }
  va_end(args);

  return rc;
}



/*
 * Add a new naming scope-tag to the scope-stack
 */
#define SCOPE_PUSH(scope_str,scope_type) scope_push((scope_str),(scope_type),__FILE__,__LINE__,__func__)
void
scope_push(char *str, scope_t tscope, char *file, int line, const char *function)
{
  ASSERT(current_scope_level < (MAX_SCOPE_DEPTH-1));

  // Save info to facilitate quick POP() of the new scope being added
  scope_stack[current_scope_level] = current_scope_str+strlen(current_scope_str);

  switch(tscope) {
  case SCOPE_PACKAGE:
    ASSERT(0 == scope_package_pushed);
    scope_package_pushed++;
    strcpy(package,str); // Save package name away
    break;
  case SCOPE_MESSAGE:
  case SCOPE_ENUM:
  case SCOPE_ENUM_ITEM:
    break;
  default:
    ASSERT(0);
  }

  current_scope_level++;

  strcat(current_scope_str,str);
  strcat(current_scope_str,".");

  ASSERT(strlen(current_scope_str) < MAX_SCOPE_STRLEN);  

  DEBUG_PRINTF("/* SCOPE_PUSH('%s',%s) %s:%d(%s) current_scope_level=%d current_scope_str='%s' */\n",
	       str,scope_t_string[tscope],
	       file,line,function,
	       current_scope_level,current_scope_str);
}



/*
 * Remove the last scope-tag added to the scope-stack
 * updating var current_scope_str
 */
#define SCOPE_POP(scope_type) scope_pop((scope_type),__FILE__,__LINE__,__func__)
void
scope_pop(scope_t tscope, char *file, int line, const char *function)
{
  ASSERT(current_scope_level > 0);

  switch(tscope) {
  case SCOPE_PACKAGE:
    ASSERT(scope_package_pushed == 1);
    scope_package_pushed--;
    package[0] = '\0';
    break;
  case SCOPE_MESSAGE:
  case SCOPE_ENUM:
  case SCOPE_ENUM_ITEM:
    break;
  default:
    ASSERT(0);
  }

  current_scope_level--;
  *scope_stack[current_scope_level]='\0'; // adjust_current_scope_str

  DEBUG_PRINTF("/* SCOPE_POP(%s) %s:%d(%s) current_scope_level=%d current_scope_str='%s' */\n",
	       scope_t_string[tscope],	       
	       file,line,function,
	       current_scope_level,current_scope_str);
}



/*
 * Allocate a static string result buffer
 * NOTE: there are RET_ARRAY_MAX different buffers
 */
static char *
get_ret_string(void)
{
#define RET_ARRAY_MAX 256
  static char ret_array[RET_ARRAY_MAX][MAX_SCOPE_STRLEN];
  static int ret_idx;
  char *ret;

  //Cycle through static return values
  ret_idx++;
  if (ret_idx >= RET_ARRAY_MAX) {
    ret_idx = 0;
  }
  ret = ret_array[ret_idx];
  *ret = '\0';
  return ret;
}

/*
 * Mangle a typestring according into a 'C' identifier name
 * Essentually just replace each "." with "__"
 */
char *
CMangle(char *s, char *prefix,scope_t scope_string_type)
{
  char *from, *to, *ret;
  int started_with_dot;
  int scope_levels_present;
  int min_flat_scope_levels_requested;
  int force_uppercase = 0;
  ASSERT(NULL != s);

  // If the input string began with '.' then the final string to process should also begin with '.'
  started_with_dot = ('.' == *s)?1:0;

#ifdef ZZZ
  if ((SCOPE_MESSAGE == scope_string_type) &&
      strstr(s,"._AU_ip_addressType")) {
    ret = get_ret_string();
    if (!strcmp(prefix,"struct __")) {
      prefix = "struct ";
    }
    else if (!strcmp(prefix,"struct") && started_with_dot) {
      prefix ="struct__";
    }
    sprintf(ret,"%s%s",prefix,"sockaddr_storage");
    return ret;
  }
#endif /* ZZZ */

  switch(scope_string_type) {
  case SCOPE_MESSAGE:
    min_flat_scope_levels_requested = flat_limits.struct_level;
    break;
  case SCOPE_ENUM:
    min_flat_scope_levels_requested = flat_limits.enum_level;
    break;
  case SCOPE_ENUM_ITEM:
    min_flat_scope_levels_requested = flat_limits.enum_item_level;
    if (force_enum_items_uppercase) {
      force_uppercase = 1;
    }
    break;
  case SCOPE_PACKAGE:
  default:
    ASSERT(0);
  }
  ASSERT(min_flat_scope_levels_requested >= 1);


  /*
   * Count the # of scope levels present in the input string
   * If the string starts with a '.', it doesn't change the count
   */
  scope_levels_present = 0;
  {
    char *p = s;
    int ondot = 1;
    while(*p) {
      switch (*p++) {
      case '.':
	ondot = 1;
	break;
      default:
	if (ondot) {
	  scope_levels_present++;
	}
	ondot = 0;
	break;
      }
    }
  }
  ASSERT(scope_levels_present > 0);

  // Prepare to process the input string
  ret = get_ret_string();
  to = ret;
  from = s;

  // Copy prefix into the string (EXACTLY)
  while (*prefix) {
    *to++ = *prefix++;
  }


  /*
   * Try to take care of the following flag: scope_rule_flat_names
   */
  if (scope_rule_flat_names) {
    char *p, *prev_p;
    int scope_levels_removed = 0;
    prev_p = p = from;
  
    if ('.' == *p) { p++; }
    while (NULL != p) {
      if ((scope_levels_present-scope_levels_removed) < min_flat_scope_levels_requested) {
	break;
      }
      prev_p = p;
      p = strchr(p+1,'.'); // Get next '.'
      if (NULL != p) {
	p++;
	scope_levels_removed++;
      }
    }
    from = prev_p;
  }

  // Adjust start of remaining string to match first char of input string 's'
  if ('.' == *from) {
    if (!started_with_dot) {
      from++;
    }
  }
  else {
    if (started_with_dot) {
      from--;
    }
  }
  ASSERT(*from);
  ASSERT(!started_with_dot || ('.' == *from));

  /*
   * Process the remaining string 'from' into 'ret' (using 'to')
   */
  while(*from) {
    if (*from == '.') {
      *to++ = '_';
      *to++ = '_';
    }
    else {
      *to++ = *from;
    }
    from++;
  }
  *to = '\0';


  // Make entire result uppercase (if requested)
  if (force_uppercase) {
    from = ret;
    while (*from) {
      if (islower(*from)) {
	*from = toupper(*from);
      }
      from++;
    }
  }

  DEBUG_PRINTF("/* >>>>>>>>>>>>>>>>>>>>>>>>>>>CMangle('%s') -> '%s'*/\n",s,ret);
  //printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>CMangle('%s') ----> '%s'\n",s,ret);
  
  return ret;
}



/*
 * Return Cname using current scope string with supplied name appended
 */
char *
CMangle_current_scope_str_with_name(char *prefix,char *name, scope_t scope_string_type)
{
  char *ret, *s;

  s = get_ret_string();
  strcpy(s,current_scope_str);
  strcat(s,name);
  ret = CMangle(s,prefix,scope_string_type);
  return ret;
}



// Return Cname for an enum using scope + enum_name
char *
enum_name_mangler(char *enum_name) {
  char *ret = CMangle_current_scope_str_with_name("__",enum_name,SCOPE_ENUM);
  return ret;
}



// C-name of an enum item using scope+name
char *
enum_item_mangler(char *item_name)
{
  char *ret = CMangle_current_scope_str_with_name("__",item_name,SCOPE_ENUM_ITEM);
  return ret;
}



//C-name of struct using scope+name
char *
struct_name_mangler(char *struct_name) {
  char *ret = CMangle_current_scope_str_with_name("struct __",struct_name,SCOPE_MESSAGE);
  return ret;
}
//C-name of struct using scope+name (NO SPACES)
char *
struct_name_mangler_nosp(char *struct_name) {
  char *ret = CMangle_current_scope_str_with_name("struct__",struct_name,SCOPE_MESSAGE);
  return ret;
}



//C-name of an enum from dottype
char *
typename_mangler_enum(char *dotstring) {
  char *ret;

  ret = CMangle(dotstring,"",SCOPE_ENUM);
  return ret;
}



//C-name of enum item from dottype + name
char *
typename_mangler_enum_item(char *enum_typestr, char *enum_item)
{
  char *ret, *s;

  s = get_ret_string();
  sprintf(s,"%s.%s",enum_typestr,enum_item);
  ret = CMangle(s,"",SCOPE_ENUM_ITEM);
  return ret;
}



//C-name of a struct from dottype
char *
typename_mangler_struct(char *dotstring)
{
  char *ret = CMangle(dotstring,"struct ",SCOPE_MESSAGE);
  return ret;
}
//C-name of a struct from dottype
char *
typename_mangler_struct_nosp(char *dotstring)
{
  char *ret = CMangle(dotstring,"struct",SCOPE_MESSAGE);
  return ret;
}



/*
 * Produce the protoc-c typename of a message
 *
 * Form name as follows:
 *   Package name capitialized,
 *   First letter of struct_name capitialized,
 *   Remove and underscores in struct_name and 
 *   capitialize the letter after it
 */
char *
protoC_type2_cname(char *typename, char *name)
{
  char *from, *to, *ret;
  int needcap;
  ASSERT(typename);
  ASSERT(name);

  from = get_ret_string();
  strcpy(from,typename);
  strcat(from,name);
  to = ret = get_ret_string();

  needcap = 1;
  while(*from) {
    if (*from == '.') {
      needcap = 1;
      *to++ ='_';
      *to++ ='_';
    }
    else if (*from == '_') {
      needcap = 1;
    }
    else {
      if ((islower(*from)||isupper(*from)) && needcap) {
	*to++  = toupper(*from);
	needcap = 0;
      }
      else {
	*to++  = *from;
      }
    }
    from++;
  }

  *to = '\0';
  return ret;
}



/*
 * Return the proto-c generated typename for a protobuf message
 */
char *
protoC_pb_typename(char *struct_name)
{
  char *ret;
  ret = protoC_type2_cname(current_scope_str,struct_name);
  return ret;
}



// proto-c Msg type from dottype
char *
pb_name_from_typename(char *dotstring) {
  char *ret = protoC_type2_cname(dotstring+1,"");
  return ret;
}



/*
 * Use protoc-c name formation rules to create a string
 *
 * base_typestr should be a scope string of the form: <name>.<name>.<name>.
 *
 * return = PROTOC_TRANSFORM(<base_typestr><type_name>)<suffix>
 */
char *
protoC_init_string_transform(char *base_typestr, char *type_name, char *suffix)
{
  char *ret, *from, *to;
  ASSERT(base_typestr);
  ASSERT(type_name);
  ASSERT(suffix);

  from = get_ret_string();
  strcat(from,base_typestr); // If non-null, will have the form <str1>.<str2>.<str3>.
  ASSERT(!*base_typestr || (base_typestr[strlen(base_typestr)-1]=='.'));
  strcat(from,type_name);

  to = ret = get_ret_string();

  /*
   * For each word separated in '.' characters
   *   Transform word elimining uppercase by converting to '_' + lowercase
   *   while only adding the '_' if the previous character was uppercase
   * 
   *   Finally, convert the separating "." to "__"
   */
  while (*from) {
    while(*from && (*from != '.')) { // Iterate words
      int was_upper = 1;
      while(*from && (*from != '.')) { // Iterate letters in the word
	int is_upper = isupper(*from);
	if (is_upper) {
	  if (!was_upper) {
	    *to++ = '_';
	  }
	  *to++ = tolower(*from);
	}
	else {
	  *to++ = *from;
	}
	from++;
	was_upper = is_upper;
      }
    }
    if (*from == '.') {
      while(*from == '.') { from++; }
      *to++ = '_';
      *to++ = '_';
    }
  }

  *to = '\0';
  strcat(ret,suffix);

  DEBUG_PRINTF("/* protoC_init_string_transform(%s,%s,%s) -> %s() */\n",base_typestr,type_name,suffix,ret);

  return ret;
}


/*
 * Convert the current scope string + msg_name into
 * the protoc-c generated initializer function name for a struct
 */
char *
protoC_initializer_fn_name(char *struct_name)
{
  char *ret;
  ret = protoC_init_string_transform(current_scope_str,
				     struct_name,
				     "__init");
  return ret;
}



/*
 * Convert the supplied field-name string into the ProtoC form
 *
 * Looks like these are generated in all lower-case
 * and '_' characters are preserved
 *
 */
char *
protoC_field_name(char *field_name)
{
  char *ret, *from, *to;

  ret = get_ret_string();
  from = field_name;
  to = ret;
  while(*from) {
    *to++ = tolower(*from++);
  }
  *to++ = '\0';
  return ret;
}





/*  -------------------- END OF UTILITY ROUTINES -------------------- */



/*
 * Generate an enum declaration
 */
void
generate_enum_typedef(Google__Protobuf__EnumDescriptorProto *gp_ed)
{
  unsigned int i;

  H_PRINTF("typedef enum %s {\n",enum_name_mangler(gp_ed->name));

  INDENT_PUSH();
  SCOPE_PUSH(gp_ed->name, SCOPE_ENUM_ITEM);

  for(i=0;i<gp_ed->n_value;i++) {
    Google__Protobuf__EnumValueDescriptorProto *gp_evd = gp_ed->value[i];
    ASSERT(NULL == gp_evd->options);
    if (gp_evd->has_number) {
      H_PRINTF("%s = %d%s\n",
	       enum_item_mangler(gp_evd->name),
	       gp_evd->number,((i<(gp_ed->n_value-1))?",":""));
    }
    else {
      H_PRINTF("%s%s\n",
	       enum_item_mangler(gp_evd->name),
	       (i<(gp_ed->n_value-1))?",":"");
    }
  }
    
  SCOPE_POP(SCOPE_ENUM_ITEM);
  INDENT_POP();

  H_PRINTF("} %s;\n\n",enum_name_mangler(gp_ed->name));
}



/*
 * Generate the 'C' type name from the protcol buffer field's type enum
 */
void
make_type_str(Google__Protobuf__FieldDescriptorProto *gp_fdp,
	      char *type_str)
{
  ASSERT(gp_fdp->has_type);
  ASSERT(type_str);

  /*
   * ZZZ-1 -- I DON'T THINK ANYTHING IS REQUIRED HERE BECAUSE OF #define
   * Perform replace type check here for named fields of a message
   * This could be used to do stuff like replace a string by a struct sockaddr_in6
   */

  switch(gp_fdp->type) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_DOUBLE:    
    strcpy(type_str,"double"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FLOAT:
    strcpy(type_str,"float"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT64:
    strcpy(type_str,"int64_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT64:
    strcpy(type_str,"uint64_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT32:
    strcpy(type_str,"uint32_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED64:
    strcpy(type_str,"uint64_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED32:
    strcpy(type_str,"uint32_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BOOL:
    strcpy(type_str,"uint32_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_STRING:
    strcpy(type_str,"char *"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_GROUP:
    ASSERT(0);
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_MESSAGE:
    {
      char *optional_ptr = "";
      ASSERT(gp_fdp->type_name && ('.' == gp_fdp->type_name[0]));
      // if optional, use ptr to the struct
      if (gp_fdp->label == GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_OPTIONAL) {
	optional_ptr = " *";
      }
      sprintf(type_str,"%s%s",typename_mangler_struct(gp_fdp->type_name),optional_ptr);
    }
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BYTES:
    strcpy(type_str,"__ByteVec_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT32:
    strcpy(type_str,"uint32_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_ENUM:
    ASSERT(gp_fdp->type_name && ('.' == gp_fdp->type_name[0]));
    sprintf(type_str,"%s",typename_mangler_enum(gp_fdp->type_name));
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED32:
    strcpy(type_str,"int32_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED64:
    strcpy(type_str,"int64_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT32:
    strcpy(type_str,"int32_t"); break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT64:
    strcpy(type_str,"int64_t"); break;
  default:
    ASSERT(0);
  }
}



/*
 * This routine is called to create the 'C' declaration for a field in a struct (message)
 */
void
generate_struct_field_decl(Google__Protobuf__FieldDescriptorProto *gp_fdp)
{
  char type_str[8192];
  char comment[8192];
  char *pb_field_name, *c_field_name;
  ASSERT(gp_fdp);
  ASSERT(gp_fdp->name);

  type_str[0] = '\0';
  make_type_str(gp_fdp,type_str);

  pb_field_name = protoC_field_name(gp_fdp->name);
  c_field_name = gp_fdp->name;

  comment[0]='\0';

  switch(gp_fdp->label) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_OPTIONAL:
    if ((gp_fdp->type == GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_MESSAGE) ||
	(gp_fdp->type == GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BYTES)) {
      sprintf(comment," /* No DEFAULT VALUE required */");
    } else if (NULL == gp_fdp->default_value) {
      strcpy(comment," /* <<WARNING>> - THIS OPTIONAL FIELD IS MISSING A DEFAULT VALUE */");
      WARNING_PRINTF("WARNING: OPTIONAL field '%s'(pbuf->%s) missing a DEFAULT value\n",c_field_name,pb_field_name);
      warnings++;
    }
    else {
      sprintf(comment," /* OPTIONAL field default-value='%s' */",gp_fdp->default_value);
    }
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REQUIRED:
    sprintf(comment," /* REQUIRED field */");
    /* ignore */
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REPEATED:
    H_PRINTF("struct { size_t vec_len; %s %svec; } %s;\n",
	     type_str,
	     ((gp_fdp->type == GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_MESSAGE)?"**":"*"), /* use double pointers if inner type is message */
	     c_field_name);
    sprintf(comment," /* REPEATED field */");
    return;
  default:
    ASSERT(0);
  }

  H_PRINTF("%s %s;%s\n",type_str,c_field_name,comment);
}



/*
 * This routine is called to generate a 'C' enum declaration
 */
void
generate_enum_decls(Google__Protobuf__DescriptorProto *gp_dp)
{
  unsigned int i;
  ASSERT(gp_dp);

  SCOPE_PUSH(gp_dp->name,SCOPE_ENUM);

  for(i=0;i<gp_dp->n_enum_type;i++) {
    Google__Protobuf__EnumDescriptorProto *gp_ed = gp_dp->enum_type[i];
    ASSERT(gp_ed);
    generate_enum_typedef(gp_ed);
  }

  // Handle an nesteded enum declarations
  if (gp_dp->n_nested_type > 0) {
    ASSERT(NULL != gp_dp->nested_type);
    for(i=0;i<gp_dp->n_nested_type;i++) {
      struct Google__Protobuf__DescriptorProto *inner_gp_dp = gp_dp->nested_type[i];
      ASSERT(inner_gp_dp);
      //INDENT_PUSH();
      SCOPE_PUSH(gp_dp->name,SCOPE_ENUM);
      generate_enum_decls(inner_gp_dp);
      SCOPE_POP(SCOPE_ENUM);
      //INDENT_POP();
    }
  }

  SCOPE_POP(SCOPE_ENUM);
}



/*
 * This function is called to generate a 'C' struct forward declaration for each
 * struct (message) type in the file to place at the top of the generated 'C' defs
 */
void
generate_struct_fwd_decls(Google__Protobuf__DescriptorProto *gp_dp)
{
  unsigned int i;
  ASSERT(gp_dp);
  ASSERT(gp_dp->name);

  H_PRINTF("%s;\n",struct_name_mangler(gp_dp->name));

  /*
   * Recursively generate a struct fwd references for all nested messages
   */
  if (gp_dp->n_nested_type > 0) {
    ASSERT(NULL != gp_dp->nested_type);
    for(i=0;i<gp_dp->n_nested_type;i++) {
      struct Google__Protobuf__DescriptorProto *inner_gp_dp = gp_dp->nested_type[i];
      ASSERT(inner_gp_dp);
      SCOPE_PUSH(gp_dp->name,SCOPE_MESSAGE);
      generate_struct_fwd_decls(inner_gp_dp);
      SCOPE_POP(SCOPE_MESSAGE);
    }
  }
}



/*
 * This routine generate the actual 'C' struct definitions with the corresponding fields
 */
void
generate_struct_decls(Google__Protobuf__DescriptorProto *gp_dp)
{
  unsigned int i;
  ASSERT(gp_dp);
  ASSERT(gp_dp->name);

  /*
   * Verify no unsupported Protobuf features are being requested
   */
  ASSERT(0 == gp_dp->n_extension);
  ASSERT(NULL == gp_dp->extension);
  ASSERT(0 == gp_dp->n_extension_range);
  ASSERT(NULL == gp_dp->extension_range);
  //no, we like these  ASSERT(NULL == gp_dp->options);


  /*
   * Recursively generate types for nested messages
   */
  if (gp_dp->n_nested_type > 0) {
    ASSERT(NULL != gp_dp->nested_type);
    SCOPE_PUSH(gp_dp->name,SCOPE_MESSAGE);
    for(i=0;i<gp_dp->n_nested_type;i++) {
      struct Google__Protobuf__DescriptorProto *inner_gp_dp = gp_dp->nested_type[i];
      ASSERT(inner_gp_dp);
      generate_struct_decls(inner_gp_dp);
    }
    SCOPE_POP(SCOPE_MESSAGE);
  }

  /*
   * ZZZ-2
   * Perform type check and potentially replace ALL field declarations
   * with a replacement type
   * e.g. message ipaddr { string foo; } -> struct sockaddr_storage
   */
#ifdef ZZZ
  if (!strcmp(gp_dp->name,"_AU_ip_addressType")) {
    H_PRINTF("/* ---(((------------ Skipping  _AU_ip_addressType ---------------)))--- */\n\n");
    return;
  }
#endif /* ZZZ */


  H_PRINTF("%s { /* Corresponding Protobuf Type: '%s' */\n",
	   struct_name_mangler(gp_dp->name),
	   protoC_pb_typename(gp_dp->name));

  INDENT_PUSH();
  for(i=0;i<gp_dp->n_field;i++) {
    Google__Protobuf__FieldDescriptorProto *gp_fdp = gp_dp->field[i];
    ASSERT(gp_fdp);
    generate_struct_field_decl(gp_fdp);
  }
  INDENT_POP();

  H_PRINTF("};/* %s */\n",struct_name_mangler(gp_dp->name));
  H_PRINTF("\n");

}


void
generate_field_assignment_C_TO_PB(Google__Protobuf__FieldDescriptorProto *gp_fdp,
				  char *c_name,
				  char *pb_name)
{
  int optional = 0;
  int repeated = 0;
  char *pb_field_name, *c_field_name;
  ASSERT(gp_fdp);
  ASSERT(c_name);
  ASSERT(pb_name);

  //ZZZ-3 -- I DON'T THINK ANYTHING IS REQUIRED HERE

  pb_field_name = protoC_field_name(gp_fdp->name);
  c_field_name = gp_fdp->name;

  switch(gp_fdp->label) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REQUIRED:
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_OPTIONAL:
    optional = 1;

    // Generate has_<field_name> = 1 for types that require it
    switch(gp_fdp->type) {
    case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_MESSAGE:
    case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_STRING:
    case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BYTES:
      break; // Pointer-based -> struct doesn;t have a "has" field
    default:
      /*
       * NOTE:
       * It might be possible to eliminate setting this when an optional field has the default value
       */
      C_PRINTF("%s->has_%s = 1;\n",pb_name,pb_field_name);
      break;
    }
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REPEATED:
    repeated = 1;
    break;
  default:
    ASSERT(0);
  }

  switch(gp_fdp->type) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_DOUBLE:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FLOAT:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BOOL:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_ENUM:  // MIGHT WANT TO ADD RANGE CHECKING HERE????
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT64:
    if (repeated) {
      C_PRINTF("{\n");
      INDENT_PUSH();
      C_PRINTF("void *p = NULL;\n");
      C_PRINTF("size_t alloc_size = %s->%s.vec_len * sizeof(%s->%s.vec[0]);\n",
	       c_name,c_field_name,c_name,c_field_name);
      C_PRINTF("if (%s->%s.vec_len > 0) {\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("PB2C_ASSERT(NULL != %s->%s.vec);\n",c_name,c_field_name);
      C_PRINTF("p = PB2C_PB_MALLOC(alloc_size);\n");
      C_PRINTF("memcpy(p,%s->%s.vec,alloc_size);\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("%s->n_%s = %s->%s.vec_len;\n",
	       pb_name,pb_field_name,
	       c_name,c_field_name);
      C_PRINTF("%s->%s = p;\n",pb_name,pb_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
    }
    else {
      C_PRINTF("%s->%s = %s->%s;\n",
	       pb_name,pb_field_name,
	       c_name,c_field_name);      
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_STRING:
    if (repeated) {
      C_PRINTF("%s->%s = NULL;\n",pb_name,pb_field_name);
      C_PRINTF("%s->n_%s = 0;\n",pb_name,pb_field_name);
      C_PRINTF("if (%s->%s.vec_len > 0) {\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("uint32_t i, alloc_size, n;\n");
      C_PRINTF("char **v;\n");
      C_PRINTF("PB2C_ASSERT(NULL != %s->%s.vec);\n",c_name,c_field_name);
      C_PRINTF("n = %s->%s.vec_len;\n",c_name,c_field_name);
      C_PRINTF("alloc_size = sizeof(*v) * n;\n");
      C_PRINTF("v = PB2C_PB_MALLOC(alloc_size);\n");
      C_PRINTF("%s->%s = v;\n",pb_name,pb_field_name);
      C_PRINTF("%s->n_%s = n;\n",pb_name,pb_field_name);
      C_PRINTF("for(i=0;i<n;i++) {\n");
      INDENT_PUSH();
      C_PRINTF("PB2C_ASSERT(NULL != %s->%s.vec[i]);\n",c_name,c_field_name);
      C_PRINTF("alloc_size = 1 + strlen(%s->%s.vec[i]);\n",c_name,c_field_name);
      C_PRINTF("%s->%s[i] = PB2C_PB_MALLOC(alloc_size);\n",
	       pb_name,pb_field_name);
      C_PRINTF("memcpy(%s->%s[i],%s->%s.vec[i],alloc_size);\n",
	       pb_name,pb_field_name,
	       c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("%s->n_%s = %s->%s.vec_len;\n",
	       pb_name,pb_field_name,
	       c_name,c_field_name);
    }
    else {
      C_PRINTF("{ /* Handle %s string %s->%s */\n",(optional?"optional":"required"),pb_name,pb_field_name);
      INDENT_PUSH();
      C_PRINTF("char *p = %s->%s;\n",c_name,c_field_name);
      C_PRINTF("uint32_t len = ((NULL==p)?0:strlen(p));\n");
      if (optional) {
	C_PRINTF("if (len > 0) { /* NOTE: this string is optional */\n");
	INDENT_PUSH();
      }
      C_PRINTF("%s->%s = PB2C_PB_MALLOC(len+1);\n",
	       pb_name,pb_field_name);
      C_PRINTF("strcpy(%s->%s,((NULL!=p)?p:\"\"));\n",pb_name,pb_field_name);
      if (optional) {
	INDENT_POP();
	C_PRINTF("}\n");
      }
      INDENT_POP();
      C_PRINTF("}\n");
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_MESSAGE:
    {
      ASSERT(gp_fdp->type_name && gp_fdp->type_name[0]);
      if (optional) {
	C_PRINTF("if (NULL != %s->%s) {\n",c_name,c_field_name);		 
	INDENT_PUSH();
	C_PRINTF("%s->%s = PB2CM_%s_TO_%s(%s->%s);\n",
		 pb_name,pb_field_name,
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 pb_name_from_typename(gp_fdp->type_name),
		 c_name,c_field_name);
	INDENT_POP();
	C_PRINTF("}\n");
      }
      else if (repeated) {
	C_PRINTF("if (%s->%s.vec_len > 0) {\n",c_name,c_field_name);
	INDENT_PUSH();
	C_PRINTF("uint32_t i, alloc_size, n;\n");
	C_PRINTF("%s **v;\n",pb_name_from_typename(gp_fdp->type_name));
	C_PRINTF("n = %s->%s.vec_len;\n",c_name,c_field_name);
	C_PRINTF("alloc_size = sizeof(*v) * n;\n");
	C_PRINTF("v = (%s **) PB2C_PB_MALLOC(alloc_size);\n",
		 pb_name_from_typename(gp_fdp->type_name));
	C_PRINTF("%s->%s = v;\n",pb_name,pb_field_name);
	C_PRINTF("%s->n_%s = n;\n",pb_name,pb_field_name);
	C_PRINTF("for(i=0;i<n;i++) {\n");
	INDENT_PUSH();
	C_PRINTF("%s->%s[i] = PB2CM_%s_TO_%s(%s->%s.vec[i]);\n",
		 pb_name,pb_field_name,
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 pb_name_from_typename(gp_fdp->type_name),
		 c_name,c_field_name);
	INDENT_POP();
	C_PRINTF("}\n");
	INDENT_POP();
	C_PRINTF("}\n");
      }
      else {
	C_PRINTF("%s->%s = PB2CM_%s_TO_%s(&%s->%s);\n",
		 pb_name,pb_field_name,
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 pb_name_from_typename(gp_fdp->type_name),
		 c_name,c_field_name);
      }
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BYTES:
    if (repeated) {
      C_PRINTF("if (%s->%s.vec_len > 0) { /* Repeated bytes field */\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("uint32_t i, alloc_size, n;\n");
      C_PRINTF("n = %s->%s.vec_len;\n",c_name,c_field_name);
      C_PRINTF("%s->n_%s = n;\n",pb_name,pb_field_name);
      C_PRINTF("alloc_size = n * sizeof(%s->%s[0]);\n",pb_name,pb_field_name);
      C_PRINTF("%s->%s = (ProtobufCBinaryData *) PB2C_PB_MALLOC(alloc_size);\n",
	       pb_name,pb_field_name);
      C_PRINTF("for(i=0;i<n;i++) {\n");
      INDENT_PUSH();
      C_PRINTF("alloc_size = %s->%s.vec[i].len;\n",c_name,c_field_name);
      C_PRINTF("%s->%s[i].len = alloc_size;\n",pb_name,pb_field_name);
      C_PRINTF("%s->%s[i].data = (uint8_t *) PB2C_PB_MALLOC(alloc_size);\n",
	       pb_name,pb_field_name);
      C_PRINTF("memcpy(%s->%s[i].data,%s->%s.vec[i].data,alloc_size);\n",
	       pb_name,pb_field_name,
	       c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      INDENT_POP();
      C_PRINTF("}\n");
    }
    else {
      C_PRINTF("if (%s->%s.len > 0) { /* bytes field */\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("size_t alloc_size = %s->%s.len;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.len = alloc_size;\n",pb_name,pb_field_name);
      C_PRINTF("%s->%s.data = (uint8_t *)PB2C_PB_MALLOC(alloc_size);\n",
	       pb_name,pb_field_name);
      C_PRINTF("memcpy(%s->%s.data,%s->%s.data,alloc_size);\n",
	       pb_name,pb_field_name,
	       c_name,c_field_name);
      if (optional) {
	C_PRINTF("%s->has_%s = 1;\n",pb_name,pb_field_name);
      }
      INDENT_POP();
      C_PRINTF("}\n");
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_GROUP:
  default:
    ASSERT(0);
  }
}

void
generate_field_assignments_C_TO_PB(Google__Protobuf__DescriptorProto *gp_dp,
				   char *c_name,
				   char *pb_name)
{
  uint32_t i;

  C_PRINTF("%s *%s;\n\n",
	   protoC_pb_typename(gp_dp->name),
	   pb_name);
  C_PRINTF("%s = (%s *)PB2C_PB_MALLOC(sizeof(*%s));\n",
	   pb_name,
	   protoC_pb_typename(gp_dp->name),
	   pb_name);
  // Call the appropriate auto-generated protobuf initializer generated by protoc-c
  C_PRINTF("%s(%s);\n\n",
	   protoC_initializer_fn_name(gp_dp->name),pb_name);

  //ZZZ-4
#ifdef ZZZ
  if (!strcmp(gp_dp->name,"_AU_ip_addressType")) {
    C_PRINTF("/* ZZZ PERFORM sockaddr_in6->PB assignments here ZZZ */\n");
    C_PRINTF("{ /* Handle sockaddr_in6 */\n");
    {
      INDENT_PUSH();
      C_PRINTF("char __tmpstr[INET6_ADDRSTRLEN];\n");
      C_PRINTF("if (AF_INET6 == %s->ss_family) {\n",c_name);
      {
	INDENT_PUSH();
	C_PRINTF("const char *__rc;\n");
	C_PRINTF("struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)%s;\n",c_name);
	C_PRINTF("%s->type = 1;\n",pb_name);
	C_PRINTF("__rc = inet_ntop(AF_INET6,(void *)&(s6->sin6_addr),__tmpstr,sizeof(__tmpstr));\n");
	C_PRINTF("PB2C_ASSERT(__tmpstr == __rc);\n");
	C_PRINTF("%s->f1 =  PB2C_PB_MALLOC(sizeof(Foo__Ipv6AddressType));\n",pb_name);
	C_PRINTF("PB2C_ASSERT(%s->f1);\n",pb_name);
	C_PRINTF("foo__ipv6_address_type__init(%s->f1);\n",pb_name);
	C_PRINTF("%s->f1->ipv6_address = PB2C_PB_STRDUP(__tmpstr);\n",pb_name);
	C_PRINTF("PB2C_ASSERT(%s->f1->ipv6_address);\n",pb_name);
	INDENT_POP();
      }
      C_PRINTF("}\n");
      C_PRINTF("else { /* IPv4? */\n");
      {
	INDENT_PUSH();
	C_PRINTF("const char *__rc;\n");
	C_PRINTF("struct sockaddr_in *s4 = (struct sockaddr_in *)%s;\n",c_name);
	C_PRINTF("%s->type = 0;\n",pb_name);
	C_PRINTF("__rc = inet_ntop(AF_INET,(void *)&(s4->sin_addr),__tmpstr,sizeof(__tmpstr));\n");
	C_PRINTF("PB2C_ASSERT(__tmpstr == __rc);\n");
	C_PRINTF("%s->f0 =  PB2C_PB_MALLOC(sizeof(Foo__Ipv4AddressType));\n",pb_name);
	C_PRINTF("PB2C_ASSERT(%s->f0);\n",pb_name);
	C_PRINTF("foo__ipv4_address_type__init(%s->f0);\n",pb_name);
	C_PRINTF("%s->f0->ipv4_address = PB2C_PB_STRDUP(__tmpstr);\n",pb_name);
	C_PRINTF("PB2C_ASSERT(%s->f0->ipv4_address);\n",pb_name);
	INDENT_POP();
      }
      C_PRINTF("}\n");
      INDENT_POP();
    }
    C_PRINTF("} /* Handle sockaddr */\n");
    return;
  }
#endif /* ZZZ */

  SCOPE_PUSH(gp_dp->name,SCOPE_MESSAGE);
  for(i=0;i<gp_dp->n_field;i++) {
    Google__Protobuf__FieldDescriptorProto *fp = gp_dp->field[i];
    generate_field_assignment_C_TO_PB(fp,c_name,pb_name);
  }
  SCOPE_POP(SCOPE_MESSAGE);
}


void
generate_field_assignment_PB_TO_C(Google__Protobuf__FieldDescriptorProto *gp_fdp,
				  char *pb_name,
				  char *c_name)
{
  int optional = 0;
  int repeated = 0;
  char *pb_field_name, *c_field_name;
  ASSERT(gp_fdp);
  ASSERT(pb_name);
  ASSERT(c_name);

  //ZZZ-5 -- I DON'T THINK ANYTHING IS REQUIRED HERE

  pb_field_name = protoC_field_name(gp_fdp->name);
  c_field_name = gp_fdp->name;

  switch(gp_fdp->label) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REQUIRED:
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_OPTIONAL:
    optional = 1;
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REPEATED:
    repeated = 1;
    break;
  default:
    ASSERT(0);
  }

  switch(gp_fdp->type) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_DOUBLE:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FLOAT:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BOOL:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_ENUM:  // MIGHT WANT TO ADD RANGE CHECKING HERE????
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT64:
    if (repeated) {
      C_PRINTF("{ /* Handle repeated atom %s->%s */\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("void *p = NULL;\n");
      C_PRINTF("size_t alloc_size = %s->n_%s * sizeof(%s->%s[0]);\n",
	       pb_name,pb_field_name,
	       pb_name,pb_field_name);
      C_PRINTF("if (alloc_size > 0) {\n");
      INDENT_PUSH();
      C_PRINTF("PB2C_ASSERT(NULL != %s->%s);\n",pb_name,pb_field_name);
      C_PRINTF("p = PB2C_CSTRUCT_MALLOC(alloc_size);\n");
      C_PRINTF("memcpy(p,%s->%s,alloc_size);\n",pb_name,pb_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("%s->%s.vec_len = %s->n_%s;\n",
	       c_name,c_field_name,
	       pb_name,pb_field_name);
      C_PRINTF("%s->%s.vec = p;\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
    }
    else {
      if (optional) {
	char *def_val = gp_fdp->default_value;
	if (def_val) {
	  // A few types require massaging of the default value
	  switch(gp_fdp->type) {
	  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BOOL:
	    def_val = (!strcmp(def_val,"true"))?"1":"0";
	    break;
	  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_ENUM:
	    def_val = typename_mangler_enum_item(gp_fdp->type_name,gp_fdp->default_value);
	    DEBUG_PRINTF("/* Generating enum value for type='%s' item='%s' result='%s' */\n",
			 gp_fdp->type_name,gp_fdp->default_value,def_val);
	    break;
	  default:
	    break;
	  }
	}
	C_PRINTF("if (%s->has_%s) {\n",pb_name,pb_field_name);
	INDENT_PUSH();
	C_PRINTF("%s->%s = %s->%s;\n",
		 c_name,c_field_name,pb_name,pb_field_name);
	INDENT_POP();
	C_PRINTF("}\n");
	C_PRINTF("else {\n");
	INDENT_PUSH();
	if (def_val) {
	  C_PRINTF("%s->%s = %s;\n",
		   c_name,c_field_name,def_val);
	}
	else {
	  C_PRINTF("/* %s->%s = <UNKNOWN_DEFAULT_VALUE>; } <<WARNING>> NO DEFAULT VALUE - Using 0 */\n",
		   c_name,c_field_name);
	  //WARNING_PRINTF("WARNING: Optional Field '%s' (%s) missing a DEFAULT value\n",c_field_name,pb_field_name);
	  //warnings++;	  
	}
	INDENT_POP();
	C_PRINTF("}\n");
      }
      else {
	C_PRINTF("%s->%s = %s->%s; /* required atom */\n",
		 c_name,c_field_name,
		 pb_name,pb_field_name);
      }
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_STRING:
    if (repeated) {
      C_PRINTF("/* %s->%s.vec = NULL; */\n",c_name,c_field_name);
      C_PRINTF("/* %s->%s.vec_len = 0; */\n",c_name,c_field_name);
      C_PRINTF("if (%s->n_%s > 0) {\n",pb_name,pb_field_name);
      INDENT_PUSH();
      C_PRINTF("uint32_t i, alloc_size, n;\n");
      C_PRINTF("char **v;\n");
      C_PRINTF("n = %s->n_%s;\n",pb_name,pb_field_name);
      C_PRINTF("alloc_size = sizeof(*v) * n;\n");
      C_PRINTF("v = PB2C_CSTRUCT_MALLOC(alloc_size);\n");
      C_PRINTF("/*memset(v,0,alloc_size);*/\n");
      C_PRINTF("%s->%s.vec = v;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec_len = n;\n",c_name,c_field_name);
      C_PRINTF("for(i=0;i<n;i++) {\n");
      INDENT_PUSH();
      C_PRINTF("%s->%s.vec[i] = ((NULL==%s->%s[i])?PB2C_CSTRUCT_STRDUP(\"\"):PB2C_CSTRUCT_STRDUP(%s->%s[i]));\n",
	       c_name,c_field_name,
	       pb_name,pb_field_name,
	       pb_name,pb_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("%s->n_%s = %s->%s.vec_len;\n",
	       pb_name,pb_field_name,
	       c_name,c_field_name);
    }
    else if (optional) {      
      char *def_val = gp_fdp->default_value;
      char *def_val_quoted = get_ret_string();
      if (NULL == def_val) {
	C_PRINTF("/* <<WARNING>> Optional string field '%s' (%s) MISSING a DEFAULT VALUE - using \"\" */\n",c_field_name,pb_field_name);
	def_val = ""; // Use empty string
	//WARNING_PRINTF("WARNING: Optional string field '%s' (%s) missing a DEFAULT VALUE - using \"\"\n",c_field_name,pb_field_name);
	//warnings++;
      }
      sprintf(def_val_quoted,"\"%s\"",def_val);
      C_PRINTF("%s->%s = ((NULL == %s->%s)?PB2C_CSTRUCT_STRDUP(%s):PB2C_CSTRUCT_STRDUP(%s->%s));\n",
	       c_name,c_field_name,
	       pb_name,pb_field_name,
	       def_val_quoted,
	       pb_name,pb_field_name);
    }
    else {
      C_PRINTF("%s->%s = ((NULL==%s->%s)?PB2C_CSTRUCT_STRDUP(\"\"):PB2C_CSTRUCT_STRDUP(%s->%s));\n",
	       c_name,c_field_name,
	       pb_name,pb_field_name,
	       pb_name,pb_field_name);
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_MESSAGE:
    {
      ASSERT(gp_fdp->type_name && gp_fdp->type_name[0]);
      if (optional) {
	C_PRINTF("/* Handling optional msg field '%s' (%s) */\n",c_field_name,pb_field_name);
	C_PRINTF("%s->%s = NULL;\n",c_name,c_field_name);
	C_PRINTF("if (NULL != %s->%s) {\n",pb_name,pb_field_name);		 
	INDENT_PUSH();
	C_PRINTF("%s->%s = PB2CM_%s_TO_%s(%s->%s,NULL);\n",
		 c_name,c_field_name,
		 pb_name_from_typename(gp_fdp->type_name),
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 pb_name,pb_field_name);
	INDENT_POP();
	C_PRINTF("}\n");
      }
      else if (repeated) {
	C_PRINTF("/* Handling repeated msg field '%s' (%s) */\n",c_field_name, pb_field_name);
	C_PRINTF("/* %s->%s.vec = NULL; */\n",c_name,c_field_name);
	C_PRINTF("/* %s->%s.vec_len = 0; */\n",c_name,c_field_name);
	C_PRINTF("if (%s->n_%s > 0) {\n",pb_name,pb_field_name);
	INDENT_PUSH();
	C_PRINTF("uint32_t i, alloc_size, n;\n");
	C_PRINTF("%s **v;\n",typename_mangler_struct(gp_fdp->type_name));
	C_PRINTF("n = %s->n_%s;\n",pb_name,pb_field_name);
	C_PRINTF("alloc_size = sizeof(*v) * n;\n");
	C_PRINTF("v = PB2C_CSTRUCT_MALLOC(alloc_size);\n");
	C_PRINTF("%s->%s.vec = v;\n",c_name,c_field_name);
	C_PRINTF("%s->%s.vec_len = n;\n",c_name,c_field_name);
	C_PRINTF("for(i=0;i<n;i++) {\n");
	INDENT_PUSH();
	C_PRINTF("%s->%s.vec[i] = PB2CM_%s_TO_%s(%s->%s[i],NULL);\n",
		 c_name,c_field_name,
		 pb_name_from_typename(gp_fdp->type_name),
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 pb_name,pb_field_name);
	INDENT_POP();
	C_PRINTF("}\n");
	INDENT_POP();
	C_PRINTF("}\n");
      }
      else {
	C_PRINTF("/* Handling required msg field '%s' (%s) */\n",c_field_name,pb_field_name);
	C_PRINTF("PB2C_ASSERT(NULL != %s->%s);\n",pb_name,pb_field_name);
	C_PRINTF("PB2CM_%s_TO_%s(%s->%s,&%s->%s);\n",
		 pb_name_from_typename(gp_fdp->type_name),
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 pb_name,pb_field_name,
		 c_name,c_field_name);
      }
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BYTES:
    if (repeated) {
      C_PRINTF("if (%s->n_%s > 0) { /* Repeated bytes field */\n",pb_name,pb_field_name);
      INDENT_PUSH();
      C_PRINTF("uint32_t i, alloc_size, n;\n");
      C_PRINTF("n = %s->n_%s;\n",pb_name,pb_field_name);
      C_PRINTF("%s->%s.vec_len = n;\n",c_name,c_field_name);
      C_PRINTF("alloc_size = n * sizeof(%s->%s.vec[0]);\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec = (__ByteVec_t *)PB2C_CSTRUCT_MALLOC(alloc_size);\n",c_name,c_field_name);
      C_PRINTF("for(i=0;i<n;i++) {\n");
      INDENT_PUSH();
      C_PRINTF("alloc_size = %s->%s[i].len;\n",pb_name,pb_field_name);
      C_PRINTF("%s->%s.vec[i].len = alloc_size;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec[i].data = PB2C_CSTRUCT_MALLOC(alloc_size);\n",c_name,c_field_name);
      C_PRINTF("memcpy(%s->%s.vec[i].data,%s->%s[i].data,alloc_size);\n",
	       c_name,c_field_name,
	       pb_name,pb_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      INDENT_POP();
      C_PRINTF("}\n");
    }
    else {
      C_PRINTF("if (%s->%s.len > 0) { /* bytes field */\n",pb_name,pb_field_name);
      INDENT_PUSH();
      C_PRINTF("size_t alloc_size = %s->%s.len;\n",pb_name,pb_field_name);
      C_PRINTF("%s->%s.len = alloc_size;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.data = PB2C_CSTRUCT_MALLOC(alloc_size);\n",c_name,c_field_name);
      C_PRINTF("memcpy(%s->%s.data,%s->%s.data,alloc_size);\n",
	       c_name,c_field_name,
	       pb_name,pb_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_GROUP:
  default:
    ASSERT(0);
  }
}



/*
 * Generate code to copy each field from a protobuf to C struct
 */
void
generate_field_assignments_PB_TO_C(Google__Protobuf__DescriptorProto *gp_dp,
				   char *pb_name,
				   char *c_name)
{
  uint32_t i;

  C_PRINTF("%s *%s;\n\n",
	   struct_name_mangler(gp_dp->name),
	   c_name);
  C_PRINTF("%s = (NULL != __memp)?__memp:((%s *)PB2C_CSTRUCT_MALLOC(sizeof(*%s)));\n",
	   c_name,
	   struct_name_mangler(gp_dp->name),
	   c_name);
  C_PRINTF("memset(%s,0,sizeof(*%s));\n\n",c_name,c_name);

  //ZZZ-6
#ifdef ZZZ
  if (!strcmp(gp_dp->name,"_AU_ip_addressType")) {
    C_PRINTF("{\n");
    {
      INDENT_PUSH();
      C_PRINTF("/* ZZZ PERFORM PB->sockaddr assignments here ZZZZ */\n");
      C_PRINTF("int __rc;\n");
      C_PRINTF("struct sockaddr_in *s4 = (struct sockaddr_in *)%s;\n",c_name);
      C_PRINTF("struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)%s;\n",c_name);
      C_PRINTF("const char *__p;\n");
      C_PRINTF("switch (%s->type) {\n",pb_name);
      C_PRINTF("case 0: /* IPv4 */\n");
      C_PRINTF("__p = ((NULL != %s->f0) && (NULL != %s->f0->ipv4_address))?%s->f0->ipv4_address:\"0.0.0.0\";\n",pb_name,pb_name,pb_name);
      C_PRINTF("%s->ss_family = AF_INET;\n",c_name);
      C_PRINTF("__rc = inet_pton(AF_INET,__p,&s4->sin_addr);\n");
      C_PRINTF("PB2C_ASSERT(1 == __rc);\n");
      C_PRINTF("break;\n");
      C_PRINTF("case 1: /* IPv6 */\n");
      C_PRINTF("__p = ((NULL != %s->f1) && (NULL != %s->f1->ipv6_address))?%s->f1->ipv6_address:\"::0\";\n",pb_name,pb_name,pb_name);
      C_PRINTF("%s->ss_family = AF_INET6;\n",c_name);
      C_PRINTF("__rc = inet_pton(AF_INET6,__p,&s6->sin6_addr);\n");
      C_PRINTF("PB2C_ASSERT(1 == __rc);\n");
      C_PRINTF("break;\n");
      C_PRINTF("default:\n");
      C_PRINTF("%s->ss_family = AF_INET;\n",c_name);
      C_PRINTF("break;\n");
      C_PRINTF("}\n");
      INDENT_POP();
    }
    C_PRINTF("}\n");
    return;
  }
#endif /* ZZZ */

  SCOPE_PUSH(gp_dp->name,SCOPE_MESSAGE);
  for(i=0;i<gp_dp->n_field;i++) {
    Google__Protobuf__FieldDescriptorProto *fp = gp_dp->field[i];
    generate_field_assignment_PB_TO_C(fp,pb_name,c_name);
  }
  SCOPE_POP(SCOPE_MESSAGE);
}


/*
 * Create an #ifdef wrapper name (outstr) from an input name (instr)
 */
static void
make_wrapper_name(char *instr, char *outstr)
{
  char *out = outstr;
  char *in = instr;

  *out++ = '_';
  *out++ = 'x';
  *out++ = 'x';
  *out++ = '_';
  while(*in) {
    *out ++ = toupper(*in++);
  }
  *out++ = '_';
  *out++ = 'x';
  *out++ = 'x';
  *out++ = '_';
  *out = '\0';
}


/*
 * Generate marshalling functions for each message type
 * NOTE: Generates just function prototypes when "prototype_only"!=0
 */
void
generate_marshalling_fn(Google__Protobuf__DescriptorProto *gp_dp,
			int prototype_only,
			int recursion_depth)
{
  char funcname[MAX_SCOPE_STRLEN*2], wrappername[MAX_SCOPE_STRLEN*2];

  ASSERT(gp_dp);
  ASSERT(gp_dp->name);
  
  if (prototype_only) {
    H_PRINTF("%s *PB2CM_%s_TO_%s(%s *__pin);\n",
	     protoC_pb_typename(gp_dp->name), // Return type
	     struct_name_mangler_nosp(gp_dp->name),
	     protoC_pb_typename(gp_dp->name),
	     struct_name_mangler(gp_dp->name));
    
    H_PRINTF("%s *PB2CM_%s_TO_%s(%s *__pin, %s *__memp);\n",
	     struct_name_mangler(gp_dp->name),
	     protoC_pb_typename(gp_dp->name),
	     struct_name_mangler_nosp(gp_dp->name),
	     protoC_pb_typename(gp_dp->name),
	     struct_name_mangler(gp_dp->name));

    if (recursion_depth == 0) {
      H_PRINTF("%s *PB2CM_unpack_%s(%s *__optional_cstruct_ptr, uint8_t *__buf, size_t __buf_len);\n",
	       struct_name_mangler(gp_dp->name),
	       struct_name_mangler_nosp(gp_dp->name),
	       struct_name_mangler(gp_dp->name));

      H_PRINTF("int32_t PB2CM_pack_%s(%s *__pin, uint8_t *__buf,  size_t __buf_len_max);\n",
	       struct_name_mangler_nosp(gp_dp->name),
	       struct_name_mangler(gp_dp->name));

      H_PRINTF("void PB2CM_pack_to_buffer_%s(%s *__pin, ProtobufCBufferSimple *__sbuffer);\n",
	       struct_name_mangler_nosp(gp_dp->name),
	       struct_name_mangler(gp_dp->name));
    }
    H_PRINTF("\n");
  }
  else {
    sprintf(funcname,"PB2CM_%s_TO_%s",
	    struct_name_mangler_nosp(gp_dp->name),
	    protoC_pb_typename(gp_dp->name));
    make_wrapper_name(funcname,wrappername);
    C_PRINTF("#if !defined(%s)\n",wrappername);
    C_PRINTF("#define %s\n",wrappername);
    C_PRINTF("%s *\n",protoC_pb_typename(gp_dp->name)); // Return type
    C_PRINTF("%s(%s *__pin)\n",
	     funcname,
	     struct_name_mangler(gp_dp->name));
    C_PRINTF("\n");
    C_PRINTF("{\n");
    INDENT_PUSH();
    generate_field_assignments_C_TO_PB(gp_dp,"__pin","__pout");
    C_PRINTF("\n");
    C_PRINTF("return __pout;\n");
    INDENT_POP();
    C_PRINTF("}\n");
    C_PRINTF("#endif /* !defined(%s) */\n",wrappername);
    C_PRINTF("\n\n\n");

    sprintf(funcname,"PB2CM_%s_TO_%s",
	    protoC_pb_typename(gp_dp->name),
	    struct_name_mangler_nosp(gp_dp->name));
    make_wrapper_name(funcname,wrappername);
    C_PRINTF("#if !defined(%s)\n",wrappername);
    C_PRINTF("#define %s\n",wrappername);
    C_PRINTF("%s *\n",struct_name_mangler(gp_dp->name)); // Return type
    C_PRINTF("%s(%s *__pin, %s *__memp)\n",
	     funcname,
	     protoC_pb_typename(gp_dp->name),
	     struct_name_mangler(gp_dp->name));
    C_PRINTF("\n");
    C_PRINTF("{\n");
    INDENT_PUSH();
    generate_field_assignments_PB_TO_C(gp_dp,"__pin","__pout");
    C_PRINTF("\n");

    C_PRINTF("return __pout;\n");
    INDENT_POP();
    C_PRINTF("}\n");
    C_PRINTF("#endif /* !defined(%s) */\n",wrappername);
    C_PRINTF("\n\n\n");

    if (recursion_depth == 0) {
      sprintf(funcname,"PB2CM_unpack_%s",struct_name_mangler_nosp(gp_dp->name));
      make_wrapper_name(funcname,wrappername);
      C_PRINTF("#if !defined(%s)\n",wrappername);
      C_PRINTF("#define %s\n",wrappername);
      C_PRINTF("%s *\n",struct_name_mangler(gp_dp->name)); // Return type
      C_PRINTF("%s(%s *__optional_cstruct_ptr, uint8_t *__buf, size_t __buf_len)\n",
	       funcname, struct_name_mangler(gp_dp->name));
      C_PRINTF("{\n");
      INDENT_PUSH();
      C_PRINTF("%s *__ret;\n",struct_name_mangler(gp_dp->name));
      C_PRINTF("%s *__msg;\n\n",protoC_pb_typename(gp_dp->name));
      C_PRINTF("/* PACK-> C-Protobuf */\n");
      C_PRINTF("__msg = %s(&protobuf_c_default_instance,__buf_len,__buf);\n",
	       protoC_init_string_transform(current_scope_str,gp_dp->name,"__unpack"));
      C_PRINTF("PB2C_ASSERT(NULL != __msg);\n");
      C_PRINTF("__ret = PB2CM_%s_TO_%s(__msg,__optional_cstruct_ptr);\n",
	       protoC_pb_typename(gp_dp->name),
	       struct_name_mangler_nosp(gp_dp->name));
      C_PRINTF("PB2C_ASSERT(NULL != __ret);\n");
      C_PRINTF("%s(&protobuf_c_default_instance,__msg);\n",
	       protoC_init_string_transform(current_scope_str,gp_dp->name,"__free_unpacked"));
      C_PRINTF("return __ret;\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("#endif /* !defined(%s) */\n",wrappername);
      C_PRINTF("\n\n\n");


      sprintf(funcname,"PB2CM_pack_%s",struct_name_mangler_nosp(gp_dp->name));
      make_wrapper_name(funcname,wrappername);
      C_PRINTF("#if !defined(%s)\n",wrappername);
      C_PRINTF("#define %s\n",wrappername);
      C_PRINTF("int32_t\n"); // Return type
      C_PRINTF("%s(%s *__pin, uint8_t *__buf, size_t __buf_max_len)\n",
	       funcname,
	       struct_name_mangler(gp_dp->name));
      C_PRINTF("{\n");
      INDENT_PUSH();
      C_PRINTF("size_t __sz1, __ret;\n");
      C_PRINTF("%s *__msg;\n\n",protoC_pb_typename(gp_dp->name));
      C_PRINTF("__msg = PB2CM_%s_TO_%s(__pin);\n",
	       struct_name_mangler_nosp(gp_dp->name),
	       protoC_pb_typename(gp_dp->name));
      C_PRINTF("PB2C_ASSERT(NULL != __msg);\n");
      C_PRINTF("__sz1 = %s(&protobuf_c_default_instance, __msg);\n",
	       protoC_init_string_transform(current_scope_str,gp_dp->name,"__get_packed_size"));
      C_PRINTF("if (__sz1 > __buf_max_len) {\n");
      INDENT_PUSH();
      C_PRINTF("__ret = - __sz1; /* set error code */\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("else {\n");
      INDENT_PUSH();
      C_PRINTF("__ret = %s(&protobuf_c_default_instance, __msg, __buf);\n",
	       protoC_init_string_transform(current_scope_str,gp_dp->name,"__pack"));
      C_PRINTF("PB2C_ASSERT(__sz1 == __ret);\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("%s(&protobuf_c_default_instance,__msg);\n",
	       protoC_init_string_transform(current_scope_str,gp_dp->name,"__free_unpacked"));
      C_PRINTF("return __ret;\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("#endif /* !defined(%s) */\n",wrappername);
      C_PRINTF("\n\n\n");


      sprintf(funcname,"PB2CM_pack_to_buffer_%s",struct_name_mangler_nosp(gp_dp->name));
      make_wrapper_name(funcname,wrappername);
      C_PRINTF("#if !defined(%s)\n",wrappername);
      C_PRINTF("#define %s\n",wrappername);
      C_PRINTF("void\n");
      C_PRINTF("%s(%s *__pin, ProtobufCBufferSimple *__sbuffer)\n",
	       funcname,
	       struct_name_mangler(gp_dp->name));
      C_PRINTF("{\n");
      INDENT_PUSH();
      C_PRINTF("%s *__msg;\n\n",protoC_pb_typename(gp_dp->name));
      C_PRINTF("PB2C_ASSERT(NULL != __pin);\n");
      C_PRINTF("PB2C_ASSERT((NULL != __sbuffer) && (NULL != __sbuffer->data));\n\n");
      C_PRINTF("/* Convert C-struct to Proto-C form */\n");
      C_PRINTF("__msg = PB2CM_%s_TO_%s(__pin);\n",
	       struct_name_mangler_nosp(gp_dp->name),
	       protoC_pb_typename(gp_dp->name));
      C_PRINTF("PB2C_ASSERT(NULL != __msg);\n\n");
      C_PRINTF("/* Pack the Proto-C form into resizable ProtobufCBufferSimple * __sbuffer */\n");
      C_PRINTF("%s(__msg, &__sbuffer->base);\n",
	       protoC_init_string_transform(current_scope_str,gp_dp->name,"__pack_to_buffer"));
      C_PRINTF("\n");
      C_PRINTF("/* Free __msg (the freshly allocated Proto-C form of __pin) */\n");
      C_PRINTF("%s(&protobuf_c_default_instance,__msg);\n",
	       protoC_init_string_transform(current_scope_str,gp_dp->name,"__free_unpacked"));      
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("#endif /* !defined(%s) */\n",wrappername);
      C_PRINTF("\n\n\n");

    } /* if (recursion_depth == 0) { */
  }
}



/*
 * Recursivley generate all marshalling functions
 */
void
generate_marshalling_functions(Google__Protobuf__DescriptorProto *gp_dp,
			       int prototype_only,
			       int recursion_depth)
{
  unsigned int i;
  ASSERT(gp_dp);
  ASSERT(gp_dp->name);

  // Recursively generate a marshalling function for any nested message types
  if (gp_dp->n_nested_type > 0) {
    ASSERT(NULL != gp_dp->nested_type);
    for(i=0;i<gp_dp->n_nested_type;i++) {
      struct Google__Protobuf__DescriptorProto *inner_gp_dp = gp_dp->nested_type[i];
      ASSERT(inner_gp_dp);
      SCOPE_PUSH(gp_dp->name,SCOPE_MESSAGE);
      generate_marshalling_functions(inner_gp_dp,prototype_only,recursion_depth+1);
      SCOPE_POP(SCOPE_MESSAGE);
    }
  }

  // Generate marshalling function for this message
  generate_marshalling_fn(gp_dp,prototype_only,recursion_depth);
}



/*
 * This code generates any code required to free ONE field of a generated C structure
 */
void
generate_field_assignments_C_struct_field_free(Google__Protobuf__FieldDescriptorProto *gp_fdp,
					       char *c_name)
{
  int optional = 0;
  int repeated = 0;
  char *c_field_name;
  //char *pb_field_name;
  ASSERT(gp_fdp);
  ASSERT(c_name);

  //ZZZ-7


  //pb_field_name = protoC_field_name(gp_fdp->name);
  c_field_name = gp_fdp->name;

  switch(gp_fdp->label) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REQUIRED:
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_OPTIONAL:
    optional = 1;
    break;
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__LABEL__LABEL_REPEATED:
    repeated = 1;
    break;
  default:
    ASSERT(0);
  }

  switch(gp_fdp->type) {
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_DOUBLE:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FLOAT:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_INT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_FIXED32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BOOL:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_UINT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_ENUM:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SFIXED64:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT32:
  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_SINT64:
    if (repeated) {
      C_PRINTF("if (NULL != %s->%s.vec) { /* repeated atom field %s */\n",
	       c_name,c_field_name,
	       c_field_name);
      INDENT_PUSH();
      C_PRINTF("PB2C_ASSERT(%s->%s.vec_len > 0);\n",c_name,c_field_name);
      C_PRINTF("PB2C_CSTRUCT_FREE(%s->%s.vec);\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec = NULL;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec_len = 0;\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("PB2C_ASSERT(%s->%s.vec_len == 0);\n",c_name,c_field_name);
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_STRING:
    if (repeated) {
      C_PRINTF("if (NULL != %s->%s.vec) { /* repeated string field %s */\n",
	       c_name,c_field_name,
	       c_field_name);
      INDENT_PUSH();
      C_PRINTF("uint32_t i;\n");
      C_PRINTF("for(i=0;i<%s->%s.vec_len;i++) {\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("if (NULL != %s->%s.vec[i]) {\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("PB2C_CSTRUCT_FREE(%s->%s.vec[i]);\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec[i] = NULL;\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("%s->%s.vec = NULL;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec_len = 0;\n",c_name,c_field_name);
      C_PRINTF("}\n");
      INDENT_POP();
      C_PRINTF("PB2C_ASSERT(%s->%s.vec_len == 0);\n",c_name,c_field_name);
    }
    else {
      C_PRINTF("if (NULL != %s->%s) { /* string field %s */\n",
	       c_name,c_field_name,
	       c_field_name);
      INDENT_PUSH();
      C_PRINTF("PB2C_CSTRUCT_FREE(%s->%s);\n",c_name,c_field_name);
      C_PRINTF("%s->%s = NULL;\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_MESSAGE:
    {
      ASSERT(gp_fdp->type_name && gp_fdp->type_name[0]);
      if (optional) {
	C_PRINTF("/* Handling optional msg field '%s' */\n",c_field_name);
	C_PRINTF("if (NULL != %s->%s) {\n",c_name,c_field_name);		 
	INDENT_PUSH();
	C_PRINTF("PB2C_DEEP_FREE_%s(%s->%s,1/*Free top*/);\n",
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 c_name,c_field_name);
	C_PRINTF("%s->%s = NULL;\n",c_name,c_field_name);		 
	INDENT_POP();
	C_PRINTF("}\n");
      }
      else if (repeated) {
	C_PRINTF("/* Handling repeated msg field '%s' */\n",c_field_name);
	C_PRINTF("if (%s->%s.vec_len > 0) {\n",c_name,c_field_name);
	INDENT_PUSH();
	C_PRINTF("uint32_t i;\n");
	C_PRINTF("PB2C_ASSERT(NULL != %s->%s.vec);\n",c_name,c_field_name);
	C_PRINTF("for(i=0;i<%s->%s.vec_len;i++) {\n",c_name,c_field_name);
	INDENT_PUSH();

	C_PRINTF("if (NULL != %s->%s.vec[i]) {\n",c_name,c_field_name);
	INDENT_PUSH();
	C_PRINTF("PB2C_DEEP_FREE_%s(%s->%s.vec[i],1/*Free top*/);\n",
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 c_name,c_field_name);
	C_PRINTF("%s->%s.vec[i] = NULL;\n",c_name,c_field_name);
	INDENT_POP();
	C_PRINTF("}\n");
	INDENT_POP();
	C_PRINTF("}\n");
	C_PRINTF("PB2C_CSTRUCT_FREE(%s->%s.vec);\n",c_name,c_field_name);
	C_PRINTF("%s->%s.vec = NULL;\n",c_name,c_field_name);
	C_PRINTF("%s->%s.vec_len = 0;\n",c_name,c_field_name);
	INDENT_POP();
	C_PRINTF("}\n");
	C_PRINTF("PB2C_ASSERT(NULL == %s->%s.vec);\n",c_name,c_field_name);
      }
      else {
	C_PRINTF("/* Handling required msg field '%s' */\n",c_field_name);
	C_PRINTF("PB2C_DEEP_FREE_%s(&%s->%s,0/*DO NOT FREE THIS*/);\n",
		 typename_mangler_struct_nosp(gp_fdp->type_name),
		 c_name,c_field_name);
      }
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_BYTES:
    if (repeated) {
      C_PRINTF("if (%s->%s.vec_len > 0) { /* repeated bytes field */\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("uint32_t i;\n");
      C_PRINTF("PB2C_ASSERT(NULL != %s->%s.vec);\n",c_name,c_field_name);
      C_PRINTF("for(i=0;i<%s->%s.vec_len;i++) {\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("if (NULL != %s->%s.vec[i].data) {\n",c_name,c_field_name);
      INDENT_PUSH();
	
      C_PRINTF("PB2C_CSTRUCT_FREE(%s->%s.vec[i].data);\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec[i].data = NULL;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec[i].len = 0;\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("PB2C_CSTRUCT_FREE(%s->%s.vec);\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec = NULL;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.vec_len = 0;\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("PB2C_ASSERT(NULL == %s->%s.vec);\n",c_name,c_field_name);
    }
    else {
      C_PRINTF("if (%s->%s.len > 0) { /* bytes field */\n",c_name,c_field_name);
      INDENT_PUSH();
      C_PRINTF("PB2C_ASSERT(NULL != %s->%s.data);\n",c_name,c_field_name);
      C_PRINTF("PB2C_CSTRUCT_FREE(%s->%s.data);\n",c_name,c_field_name);
      C_PRINTF("%s->%s.data = NULL;\n",c_name,c_field_name);
      C_PRINTF("%s->%s.len = 0;\n",c_name,c_field_name);
      INDENT_POP();
      C_PRINTF("}\n");
      C_PRINTF("PB2C_ASSERT(NULL == %s->%s.data);\n",c_name,c_field_name);
    }
    break;

  case GOOGLE__PROTOBUF__FIELD_DESCRIPTOR_PROTO__TYPE__TYPE_GROUP:
  default:
    ASSERT(0);
  }
}



/*
 * Generate code to free each field from a protobuf to C struct
 */
void
generate_field_assignments_C_struct_free(Google__Protobuf__DescriptorProto *gp_dp,
					 char *c_name)
{
  uint32_t i;

  //ZZZ-8
#ifdef ZZZ
  if (!strcmp(gp_dp->name,"_AU_ip_addressType")) {
    C_PRINTF("/* ZZZ I DON'T THINK ANYTHING IS REQUIRED HERE ... ZZZZ */\n");
    return;
  }
#endif /* ZZZ */




  SCOPE_PUSH(gp_dp->name,SCOPE_MESSAGE);

  for(i=0;i<gp_dp->n_field;i++) {
    Google__Protobuf__FieldDescriptorProto *fp = gp_dp->field[i];
    generate_field_assignments_C_struct_field_free(fp,c_name);
  }

  SCOPE_POP(SCOPE_MESSAGE);
}



/*
 * Generate C utility functions for each message type
 * NOTE: Generates just function prototypes when "prototype_only"!=0
 */
void
generate_c_utility_fn(Google__Protobuf__DescriptorProto *gp_dp,
		      int prototype_only)
{
  char *struct_typename, *struct_typename_nosp;
  char funcname[MAX_SCOPE_STRLEN*2], wrappername[MAX_SCOPE_STRLEN*2];

  ASSERT(gp_dp);
  ASSERT(gp_dp->name);

  struct_typename = struct_name_mangler(gp_dp->name);
  struct_typename_nosp = struct_name_mangler_nosp(gp_dp->name);
  
  if (prototype_only) {
    H_PRINTF("void PB2C_DEEP_FREE_%s(%s *__pin, int __free_pin);\n",
	     struct_typename_nosp,
	     struct_typename);
  }
  else {
    sprintf(funcname,"PB2C_DEEP_FREE_%s",struct_typename_nosp);
    make_wrapper_name(funcname,wrappername);
    C_PRINTF("#if !defined(%s)\n",wrappername);
    C_PRINTF("#define %s\n",wrappername);
    C_PRINTF("/*\n");
    C_PRINTF(" * This function performs a deep-free operation on the\n");
    C_PRINTF(" * type 'struct %s *'\n",struct_typename);
    C_PRINTF(" *\n");
    C_PRINTF(" * __pin: Object upon which to perform deep-free operation\n");
    C_PRINTF(" *        The type of __pin is: 'struct %s *'\n",struct_typename);
    C_PRINTF(" *\n");
    C_PRINTF(" * __free_pin: TRUE when PB2C_CSTRUCT_FREE(__pin) should also be called\n");
    C_PRINTF(" */\n");
    C_PRINTF("void\n");
    C_PRINTF("%s(%s *__pin, int __free_pin)\n",
	     funcname,
	     struct_typename);
    C_PRINTF("\n");
    C_PRINTF("{\n");
    INDENT_PUSH();
    C_PRINTF("PB2C_ASSERT(NULL != __pin);\n\n");
    generate_field_assignments_C_struct_free(gp_dp,"__pin");
    C_PRINTF("\n");
    C_PRINTF("if (__free_pin) { PB2C_CSTRUCT_FREE(__pin); }\n");
    INDENT_POP();
    C_PRINTF("}\n");
    C_PRINTF("#endif /* !defined(%s) */\n",wrappername);
    C_PRINTF("\n\n\n");
  }
}



/*
 * Recursively generate all C utility functions
 */
void
generate_c_utility_functions(Google__Protobuf__DescriptorProto *gp_dp,
			     int prototype_only)
{
  unsigned int i;
  ASSERT(gp_dp);
  ASSERT(gp_dp->name);

  // Recursively generate c utility functions for any nested messages types
  if (gp_dp->n_nested_type > 0) {
    ASSERT(NULL != gp_dp->nested_type);
    for(i=0;i<gp_dp->n_nested_type;i++) {
      struct Google__Protobuf__DescriptorProto *inner_gp_dp = gp_dp->nested_type[i];
      ASSERT(inner_gp_dp);
      SCOPE_PUSH(gp_dp->name,SCOPE_MESSAGE);
      generate_c_utility_functions(inner_gp_dp,prototype_only);
      SCOPE_POP(SCOPE_MESSAGE);
    }
  }

  // Generate c utility functions for this message type
  generate_c_utility_fn(gp_dp,prototype_only);
}



/*
 * This function inserts some helper functions into the generated C file
 */
void
generate_marshalling_helper_functions(void)
{
  static int once = 0;

  // Output helper functions at most once
  if (once++ > 0) {
    return;
  }

  C_PRINTF("/* ------ msg marshalling helper functions ------ */\n\n");
  C_PRINTF("\n\n");
  C_PRINTF("#if !defined(PB2C_PB_MALLOC)\n");
  C_PRINTF("/* Define how to dynamically allocate memory for a generated protobuf */\n");
  C_PRINTF("#define PB2C_PB_MALLOC(alloc_sz) \\\n");
  C_PRINTF("        protobuf_c_default_instance.alloc(&protobuf_c_default_instance,(alloc_sz));\n");
  //  C_PRINTF("__attribute__((always_inline)) static char *\n");
  C_PRINTF("#define PB2C_PB_STRDUP(in) ({\\\n");
  {
    INDENT_PUSH();
    C_PRINTF("char *out = NULL;\\\n");
    C_PRINTF("if (NULL == (in)) { out = NULL; }\\\n");
    C_PRINTF("else {\\\n");
    {
      INDENT_PUSH();
      C_PRINTF("out = PB2C_PB_MALLOC(strlen(in)+1);\\\n");
      C_PRINTF("if (NULL != out) { strcpy(out,(in)); }\\\n");
      INDENT_POP();
    }
    C_PRINTF("}\\\n");
    C_PRINTF("  out;\\\n");
    INDENT_POP();
  }
  C_PRINTF("})\n");

  C_PRINTF("#endif /* !defined(PB2C_PB_MALLOC) */\n\n\n");

  C_PRINTF("#if !defined(PB2C_CSTRUCT_MALLOC)\n");
  C_PRINTF("/* Define how to dynamically alloc/free memory for a generated struct & fields */\n");
  C_PRINTF("#define PB2C_CSTRUCT_MALLOC(__alloc_sz__) malloc((__alloc_sz__))\n");
  C_PRINTF("#define PB2C_CSTRUCT_FREE(__tofree__)     free((__tofree__))\n");
  C_PRINTF("#define PB2C_CSTRUCT_STRDUP(__dupstr__)   strdup((__dupstr__))\n");
  C_PRINTF("#endif /* !defined(PB2C_CSTRUCT_MALLOC) */\n\n\n");

  C_PRINTF("#if !defined(PB2C_ASSERT)\n");
  C_PRINTF("#include <assert.h>\n");
  C_PRINTF("#define PB2C_ASSERT assert\n");
  C_PRINTF("#endif /* !defined(PB2C_ASSERT) */\n\n\n");
}



char *
make_ifdef_safe(char *name)
{
  char *ret, *to, *from;
  ASSERT(name);
  
  ret = to = get_ret_string();
  from = name;

  while(*from) {
    if (isalnum(*from)) {
      *to++ = toupper(*from);
    }
    else {
      *to++ = '_';
    }
    from++;
  }
  *to = '\0';
  return ret;
}



void
generate_xml_helper_decl(Google__Protobuf__DescriptorProto *gp_dp)
{
  ASSERT(gp_dp);

  H_PRINTF("{ &%s, (void *(*)(void*, void*))PB2CM_%s_TO_%s, (void (*)(void *, void *))%s },\\\n",
	   protoC_init_string_transform(current_scope_str,gp_dp->name,"__descriptor"),
	   protoC_pb_typename(gp_dp->name),
	   struct_name_mangler_nosp(gp_dp->name),
	   protoC_init_string_transform(current_scope_str,gp_dp->name,"__free_unpacked"));
}

void
generate_xml_helper_decls(Google__Protobuf__FileDescriptorProto *gp_fdp)
{
  unsigned int i;
  ASSERT(gp_fdp);


  H_PRINTF("#define PB2C_XML2CSTRUCT_DEF%s_%s \\\n",
	   (scope_rule_flat_names?"_FLAT":""),
	   make_ifdef_safe(gp_fdp->name));
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_xml_helper_decl(gp_dp);
  }
  H_PRINTF("\n");
}
    


/*
 * This function is the top-level generator for 'C' types corresponding the
 * the protocol buffer definitions
 */
void
generate_code(Google__Protobuf__FileDescriptorProto *gp_fdp)
{
  unsigned int i;

  B_PRINTF("\n/* BEGIN FILE %s */\n",gp_fdp->name);

  if (scope_rule_flat_names) {
    B_PRINTF("/* SCOPE: FLATNAMES enabled: (flat-msg-lvl=%d, flat-enum-lvl=%d, flat-enum-item-lvl=%d)*/\n",
	     flat_limits.struct_level,
	     flat_limits.enum_level,
	     flat_limits.enum_item_level);
  }

  if (NULL != gp_fdp->package) {
    B_PRINTF("\n\n");
    B_PRINTF("/* PACKAGE: '%s' */\n",gp_fdp->package);
    SCOPE_PUSH(gp_fdp->package,SCOPE_PACKAGE);
  }
  for(i=0;i<gp_fdp->n_dependency;i++) {
    B_PRINTF("/* IMPORT DEPENDENCY: '%s' */\n",gp_fdp->dependency[i]);
  }

  /*
   * Make note of unsupported protocol buffer definitions
   */
  if (gp_fdp->n_service != 0) {
    B_PRINTF("/* <<WARNING>>: PROTOBUF SERVICE DEFINITIONS UNSUPPORTED -- IGNORED\n*/");
    WARNING_PRINTF("WARNING: PROTOBUF SERVICE DEFINITIONS UNSUPPORTED -- IGNORED\n");
    warnings++;
  }
  if (gp_fdp->n_extension != 0) {
    B_PRINTF("/* <<WARNING>>: PROTOBUF EXTENSION DEFINITIONS UNSUPPORTED -- IGNORED*/\n");
    WARNING_PRINTF("WARNING: PROTOBUF EXTENSION DEFINITIONS UNSUPPORTED -- IGNORED\n");  
    warnings++;
  }


  B_PRINTF("\n\n");
  H_PRINTF("#include <stdint.h>\n");
  H_PRINTF("#include <stddef.h>\n");
  H_PRINTF("#include <string.h>\n");
  H_PRINTF("#include <unistd.h>\n");
  H_PRINTF("#include <stdlib.h>\n");
  H_PRINTF("#include <protobuf-c/rift-protobuf-c.h>\n");
  H_PRINTF("#include \"%.*s.pb-c.h\"\n",(int)(strlen(gp_fdp->name)-sizeof(".proto")+1),gp_fdp->name);
  H_PRINTF("\n\n");
  H_PRINTF("#if !defined(__BYTEVEC_T__)\n");
  H_PRINTF("#define __BYTEVEC_T__\n");
  H_PRINTF("/* This type is used to hold variable-length byte vectors */\n");
#if 1
  H_PRINTF("struct __ByteVec_t__\n");
  H_PRINTF("{\n");
  INDENT_PUSH();
  H_PRINTF("size_t  len;    /* Length of dynamically allocated uint8_t 'data'       */\n");
  H_PRINTF("uint8_t *data; /* NOTE: If non-NULL, free(data) called  even if len==0 */\n");
  INDENT_POP();
  H_PRINTF("};\n");
  H_PRINTF("typedef struct __ByteVec_t__ __ByteVec_t ;\n");
#else
  H_PRINTF("typedef ProtobufCBinaryData __ByteVec_t ;\n");
#endif
  H_PRINTF("#endif /* !defined(__BYTEVEC_T__) */\n");
  H_PRINTF("\n\n");

  C_PRINTF("#if !defined(__PB2C_MARSHALLING_FUNCTIONS__)\n");
  C_PRINTF("#define __PB2C_MARSHALLING_FUNCTIONS__ 1\n");
  C_PRINTF("#endif /* !defined(__PB2C_MARSHALLING_FUNCTIONS__)*/\n");
  C_PRINTF("\n\n");

  H_PRINTF("\n__BEGIN_DECLS\n");
  if (h_genfilename[0]) {
    C_PRINTF("#include \"%s\"\n",h_genfilename);
    C_PRINTF("\n\n");
  }
  H_PRINTF("\n\n");


  H_PRINTF("/* ------ enums ------ */\n\n");

  // First at the top level
  for(i=0;i<gp_fdp->n_enum_type;i++) {
    Google__Protobuf__EnumDescriptorProto *gp_edp = gp_fdp->enum_type[i];
    generate_enum_typedef(gp_edp);
  }
  // Now recursively
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_enum_decls(gp_dp);
  }


  H_PRINTF("/* ------ messages ------ */\n\n");
    
  /*
   * Generate struct forward references by recusively walking all messages
   */
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_struct_fwd_decls(gp_dp);
  }
  H_PRINTF("\n");
  
  /*
   * Generate the actual C struct definitions by recusively walking all messages
   */
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_struct_decls(gp_dp);
  }


  H_PRINTF("\n\n");
  H_PRINTF("/* ------ C-struct utility function prototypes ------ */\n\n");
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_c_utility_functions(gp_dp,1/*Generate Fn() Prototypes Only*/);
  }
  H_PRINTF("\n\n");


  H_PRINTF("/* ------ Protobuf Msg <-> C-struct marshalling function prototypes ------ */\n\n");
  H_PRINTF("#if defined(__PB2C_MARSHALLING_FUNCTIONS__)\n\n");

  /* Generate function prototypes by recusively walking all messages */
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_marshalling_functions(gp_dp,1/*Generate Fn() Prototypes Only*/,0/*Top*/);
  }
  H_PRINTF("\n\n");
  H_PRINTF("#endif /* __PB2C_MARSHALLING_FUNCTIONS__ */\n");
  H_PRINTF("\n\n");

  H_PRINTF("/* ------ XML->CStruct helper decls ------ */\n\n");
  generate_xml_helper_decls(gp_fdp);
  H_PRINTF("\n\n");


  H_PRINTF("\n\n");
  H_PRINTF("__END_DECLS\n\n");


  /*
   * Generate helper functions
   */
  generate_marshalling_helper_functions();


  /*
   * Generate function's code by recusively walking all messages
   */
  C_PRINTF("/* ------ msg marshalling functions ------ */\n\n");
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_marshalling_functions(gp_dp,0/*Generate Fn() Bodies*/,0/*Top*/);
  }

  C_PRINTF("/* ------ C-struct utility functions ------ */\n\n");
  for(i=0;i<gp_fdp->n_message_type;i++) {
    Google__Protobuf__DescriptorProto *gp_dp = gp_fdp->message_type[i];
    generate_c_utility_functions(gp_dp,0/*Generate Fn() Bodies*/);
  }
  H_PRINTF("\n\n");

  B_PRINTF("\n\n");
  B_PRINTF("/* END FILE %s */\n",gp_fdp->name);

  if (NULL != gp_fdp->package) {
    SCOPE_POP(SCOPE_PACKAGE);
  }
}



/*
 * This program converts a Google Protobuf definition into a set
 * of corresponding 'C' data structures using the .dso file
 * generated by the protobuf compiler 'protoc'
 *
 * The format of the .dso file (itself a protobuf) is:
 * /usr/include/google/protobuf/descriptor.proto gen
 */
int
main(int argc, char** argv) {
  Google__Protobuf__FileDescriptorSet *fds;
  FILE *fp = NULL;
  char *fname = NULL;
  char *progname = argv[0];
  const uint8_t *buf;
  unsigned long fsize;
  int rc;
  unsigned int fn;
  size_t sz;
  char *genfilebase = NULL;
  int opt_idx, opt;
  char ifdef_genname[sizeof(h_genfilename)]; // Generated #ifdef wrapper name

  static struct option lgopts[] = {
    { "help",                       0, 0, 0 },
    { "dsofile",                    1, 0, 0 },
    { "genfilebase",                1, 0, 0 },
    { "scope-no-package",           0, 0, 0 },
    { "scope-flatnames",            0, 0, 0 },
    { "flat-enum-item-lvl",         1, 0, 0 },
    { "flat-enum-lvl",              1, 0, 0 },
    { "flat-msg-lvl",               1, 0, 0 },
    { "force-enum-items-uppercase", 1, 0, 0 },
    { "debug-c",                    0, 0, 0 },
    { "debug-h",                    0, 0, 0 },
    { "disable-c",                  0, 0, 0 },
    { "disable-h",                  0, 0, 0 },
    { 0, 0, 0, 0 },
  };
  

  //argvopt = argv;
  while ((opt = getopt_long(argc,argv,"hd:fg:",lgopts, &opt_idx)) != EOF) {
    switch(opt) {
    case 'h': // help
      goto usage;
    case 'd': // .dso filename
      fname = optarg;
      break;
    case 'f':
      scope_rule_flat_names = 1;
      break;
    case 'g': // generated file basename i.e. <optarg>.[ch]
      genfilebase = optarg;
      break;
    default:
      goto usage;
    case 0: // longopts
      if (!strcmp(lgopts[opt_idx].name, "help")) {
	goto usage;
      }
      if (!strcmp(lgopts[opt_idx].name, "dsofile")) {
	fname = optarg;
      }
      if (!strcmp(lgopts[opt_idx].name, "scope-flatnames")) {
	scope_rule_flat_names = 1;
      }
      if (!strcmp(lgopts[opt_idx].name, "flat-msg-lvl")) {
	rc = atoi(optarg);
	if (rc < 1) {
	  printf("invalid flat-msg-lvl");
	  exit(-1);
	}
	flat_limits.struct_level = rc;
      }
      if (!strcmp(lgopts[opt_idx].name, "flat-enum-lvl")) {
	rc = atoi(optarg);
	if (rc < 1) {
	  printf("invalid flat-enum-lvl");
	  exit(-1);
	}
	flat_limits.enum_level = rc;
      }
      if (!strcmp(lgopts[opt_idx].name, "flat-enum-item-lvl")) {
	rc = atoi(optarg);
	if (rc < 1) {
	  printf("invalid flat-enum-item-lvl");
	  exit(-1);
	}
	flat_limits.enum_item_level = rc;
      }
      if (!strcmp(lgopts[opt_idx].name, "force-enum-items-uppercase")) {
	rc = atoi(optarg);
	if ((rc < 0) || (rc > 1)) {
	  printf("--invalid force-enum-item-uppercase [0|1]\n");
	  exit(-1);
	}
	force_enum_items_uppercase = rc;
      }
      if (!strcmp(lgopts[opt_idx].name, "genfilebase")) {
	genfilebase = optarg;
      }
      if (!strcmp(lgopts[opt_idx].name, "disable-c")) {
	disable_c = 1;
      }
      if (!strcmp(lgopts[opt_idx].name, "disable-h")) {
	disable_h = 1;
      }
      if (!strcmp(lgopts[opt_idx].name, "debug-c")) {
	debug_c = 1;
      }
      if (!strcmp(lgopts[opt_idx].name, "debug-h")) {
	debug_h = 1;
      }
    }
  }

  if (NULL == fname) {
    fprintf(stderr,"missing .dso filename\n");

  usage:
    fprintf(stderr,"Usage: %s",progname);
    fprintf(stderr,
	    "    --dsofile=<dsofilename.dso> | -d <dsofilename.dso>\n"
	    "    [--genfilebase=<genFilenameBase.[ch]>] [-g <genFilenameBase.[ch]]\n"
	    "    [--scope-flatnames] [-f]\n"
	    "    [--flat-msg-lvl <1..n> (default %d)\n"
	    "    [--flat-enum-lvl <1..n> (default %d)\n"
	    "    [--flat-enum-item-lvl <1..n> (default %d)\n"
	    "    [--force-enum-items-uppercase [0:1]  (default %d)\n"
	    "    [--debug-c] [--debug-h]\n"
	    "    [--disable-c] [--disable-h]\n"
	    "    [--help]\n"
	    "\n",
	    flat_limits.struct_level,
	    flat_limits.enum_level,
	    flat_limits.enum_item_level,
	    force_enum_items_uppercase
	    );
    exit(-1);
  }


  /*
   * Read the .dso file into a memory buffer
   */

  // open file
  fp = fopen(fname,"r");
  if (NULL == fp) {
    fprintf(stderr,"failed to open file: %s\n",fname);
    exit(-1);
  }

  // determine size of buffer to hold the file
  rc = fseek(fp,0,SEEK_END);
  ASSERT(rc == 0);
  fsize = ftell(fp);
  ASSERT(fsize > 0);
  rewind(fp);

  // put .dso file into buffer
  buf = malloc(fsize);
  ASSERT(buf);
  sz = fread((void *)buf,1,fsize,fp);
  ASSERT(sz == fsize);
  fclose(fp);

  /*
   * Unpack the DSO treee so it can be walked and code generated
   */
  fds = google__protobuf__file_descriptor_set__unpack(&protobuf_c_default_instance,
						      sz,
						      buf);
  ASSERT(fds);
  if ((NULL == fds) || strcmp(fds->base.descriptor->short_name,"FileDescriptorSet")) {
    fprintf(stderr,"File doesn't contain FileDescriptorSet\n");
    exit(-1);
  }

  if (genfilebase) {
    sprintf(c_genfilename,"%s.c",genfilebase);
    cfp = fopen(c_genfilename,"w");
    if (NULL == cfp) {
      fprintf(stderr,"failed to open file: %s\n",c_genfilename);
      exit(-1);
    }
    sprintf(h_genfilename,"%s.h",genfilebase);
    hfp = fopen(h_genfilename,"w");
    if (NULL == hfp) {
      fprintf(stderr,"failed to open file: %s\n",h_genfilename);
      exit(-1);
    }
  }
  else {
    // Generate output to stdout
    hfp = stdout;
    cfp = stdout;
    // If generating to stdout, only generate one file at a time
    if (!disable_c && !disable_h) {
      disable_h = 1;
    }
  }

  /*
   * Generate the 'C' code & declarations from the .dso file
   * that has been decoded into memory
   */
  ifdef_genname[0] = '\0';
  if (h_genfilename[0]) {
    char *from = h_genfilename;
    char *to = ifdef_genname;

    // Create a "safe" #ifdef name
    while(*from) {
      if (isalpha(*from)) {
	*to++ = toupper(*from++);
      }
      else if (isdigit(*from) && (from != h_genfilename)) {
	*to++ = *from++;
      }
      else {
	*to++ = '_';
	from++;
      }
    }
    *to++ = '\0';
  }


  B_PRINTF("/* DO NOT EDIT - CODE AUTO-GENERATED BY %s */\n\n",progname);

  // OPENING #ifdef on the .H file
  if (ifdef_genname[0]) {
    H_PRINTF("#if !defined(__%s__)\n",ifdef_genname);
    H_PRINTF("#define __%s__\n",ifdef_genname);
  }

  // Generate everything requested
  ASSERT(fds->n_file == 1);
  for(fn = 0;fn < fds->n_file; fn++) {
    Google__Protobuf__FileDescriptorProto *gp_fdp = fds->file[fn];
    generate_code(gp_fdp);
  }

  // CLOSING #endif on the .H file
  if (ifdef_genname[0]) {
    H_PRINTF("\n#endif /* defined(__%s__) */\n",ifdef_genname);
  }

  B_PRINTF("\n/* DO NOT EDIT - CODE AUTO-GENERATED BY %s */\n",progname);

  
  /*
   * If not generating to stdout, close the files
   */
  if (genfilebase) {
    fclose(hfp);
    fclose(cfp);
  }

  if (warnings > 0) {
    fprintf(stderr,"Total Warnings: %d\n",warnings);
  }

  exit(0);
}
