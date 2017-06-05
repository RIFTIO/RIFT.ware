
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "rwvcs.h"

// Reference information for possible later use:
// http://bytes.com/topic/python/answers/30471-getting-output-embedded-python-program
// http://stackoverflow.com/questions/9541353/printing-a-variable-in-an-embedded-python-interpreter
// http://schneide.wordpress.com/2011/10/10/embedding-python-into-cpp/

rw_status_t
rwvcs_variable_list_evaluate(rwvcs_instance_ptr_t instance,
                             size_t n_variable,
                             char **variable)
{
  int i;
  char *expression;
  rw_status_t status = RW_STATUS_SUCCESS;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwvcs_instance_ptr_t);

  // Evaluate each variable in the list
  for (i = 0 ; i < n_variable ; i++) {
    expression = variable[i];
    RWTRACE_INFO(instance->rwvx->rwtrace,
                 RWTRACE_CATEGORY_RWVCS,
                 "Variable list evaluate - expression = %s\n",
                 expression);
    status = instance->rwpython_util.iface->python_run_string(
        instance->rwpython_util.cls,
        expression);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
  }

  // The variable list evaluation was successful
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwvcs_variable_evaluate_int(
    rwvcs_instance_ptr_t instance,
    char * variable,
    int * result)
{
  rw_status_t status;
  char expression[1024];
  char * match_string = "$python(";
  char copied_string[1024];
  int len;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwvcs_instance_ptr_t);
  RW_ASSERT(variable);

  if (variable[0] != '$') {
    *result = atoi(variable);
    return RW_STATUS_SUCCESS;
  }
  else {
    if (0 != strncmp(variable, match_string, 8) ) {
      variable++;
      sprintf(expression, "int(%s)", variable);
    } else {
      variable += 8;
      strcpy(copied_string,variable);
      len = strlen(copied_string);
      if (copied_string[len - 1] == ')') {
        copied_string[len - 1] = '\0';
        sprintf(expression, "int(%s)", copied_string);
      } else {
        RWTRACE_INFO(instance->rwvx->rwtrace,
                     RWTRACE_CATEGORY_RWVCS,
                    "Error evaluating int variable\n");
        return RW_STATUS_FAILURE;
      }
    }
  }

  status = instance->rwpython_util.iface->python_eval_integer(
      instance->rwpython_util.cls,
      expression,
      result);

  return status;
}

rw_status_t
rwvcs_variable_evaluate_str(
    struct rwvcs_instance_s *instance,
    const char * variable,
    char * result,
    int result_len)
{
  rw_status_t status;
  char expression[1024];
  char * match_string = "$python(";
  char copied_string[1024];
  int len;
  char * c_result = NULL;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwvcs_instance_ptr_t);
  RW_ASSERT(variable);

  if (variable[0] != '$') {
    strcpy(result, variable);
    return RW_STATUS_SUCCESS;
  }
  else {
    if (0 != strncmp(variable, match_string, 8) ) {
      variable++;
      sprintf(expression, "str(%s)", variable);
    } else {
      variable += 8;
      strcpy(copied_string,variable);
      len = strlen(copied_string);
      if (copied_string[len - 1] == ')') {
        copied_string[len - 1] = '\0';
        sprintf(expression, "str(%s)", copied_string);
      } else {
        RWTRACE_INFO(instance->rwvx->rwtrace,
                     RWTRACE_CATEGORY_RWVCS,
                     "Error evaluating string variable\n");
        return RW_STATUS_FAILURE;
      }
    }
  }

  // Evaluate a "result = variable" expression and extract the value of "result"
  RWTRACE_INFO(instance->rwvx->rwtrace,
               RWTRACE_CATEGORY_RWVCS,
               "Variable evaluate str - expression = %s\n",
               expression);
  status = instance->rwpython_util.iface->python_eval_string(
              instance->rwpython_util.cls,
              expression,
              &c_result);

  if (status != RW_STATUS_SUCCESS) {
    strncpy(result, "", result_len);
  } else {
    strncpy(result, c_result, result_len);
    result[result_len-1] = '\0';
  }

  if (c_result)
    free(c_result);

  return status;
}

rw_status_t
rwvcs_evaluate_python_loop_variables(rwvcs_instance_ptr_t instance,
          char *variable)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwvcs_instance_ptr_t);
  RW_ASSERT(variable);

  RWTRACE_INFO(instance->rwvx->rwtrace,
               RWTRACE_CATEGORY_RWVCS,
               "Variable evaluate python loop variable - expression = %s\n",
               variable);
  status = instance->rwpython_util.iface->python_run_string(
                instance->rwpython_util.cls,
                variable);

  return status;
}
