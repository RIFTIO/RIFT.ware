/*
 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: yangdump.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
01-nov-06    abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* #define MEMORY_DEBUG 1 */

#ifdef MEMORY_DEBUG
#include <mcheck.h>
#endif

#define _C_main 1

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_log
#include  "log.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_yang
#include  "yang.h"
#endif

#ifndef _H_yangdump
#include  "yangdump.h"
#endif

#ifndef _H_ydump
#include  "ydump.h"
#endif

#include "ncx.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                            *
*                                                                   *
*********************************************************************/

static yangdump_cvtparms_t   mycvtparms;


/********************************************************************
*                                                                   *
*                        FUNCTION main                              *
*                                                                   *
*********************************************************************/
int 
    main (int argc, 
          char *argv[])
{
    status_t      res;

#ifdef MEMORY_DEBUG
    mtrace();
#endif

    ncx_instance_t* instance = ncx_alloc();

    res = ydump_init(instance, argc, argv, TRUE, &mycvtparms);

    if (res == NO_ERR) {

	res = ydump_main(instance, &mycvtparms);

        /* if warnings+ are enabled, then res could be NO_ERR and still
         * have output to STDOUT
         */
        if (res == NO_ERR) {
            log_warn(instance, "\n");   /*** producing extra blank lines ***/
        }

        print_errors(instance);

        print_error_count(instance);
    } else {
        print_errors(instance);
    }

    // ATTN: print_error_count(instance);

    // ATTN: yang_final_memcheck(instance);

    ydump_cleanup(instance, &mycvtparms);

#ifdef MEMORY_DEBUG
    muntrace();
#endif

    return (res == NO_ERR) ? 0 : 1;

} /* main */


/* END yangdump.c */



