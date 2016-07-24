/*----------------------------------------------------------------------
 * demo_stw.c 
 *
 * January 2009, Bo Berry
 *
 *=
 *---------------------------------------------------------------------
 */ 


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <signal.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "stw_system_timer.h"


typedef struct {
    uint32_t timer_a; 
    uint32_t timer_b;
} some_data_t;  


static stw_tmr_t demo_a_tmr; 
static stw_tmr_t demo_b_tmr; 

some_data_t global_data; 




static char *instructions[] = {
 " radio [options]  ",
 "  options: ",
 "    -h  This help file  ",
 " ",
 };



/** 
 * NAME
 *    show_instructions
 *
 * DESCRIPTION
 *    Displays the help info to stdout.
 *
 * Input INPUT PARAMETERS
 *    None.
 *
 * Output INPUT PARAMETERS
 *    None.
 *
 * RETURN VALUE
 *    None.
 * 
 */
static void
show_instructions( )
{
    int i;

    for (i=0; i<sizeof(instructions)/sizeof(char *); i++) {
        printf("%s\n", instructions[i] );
    }
    return;
}


/*
 *
 * NAME
 *    parse_user_options
 *
 * DESCRIPTION
 *    Parses command line options.
 *
 * Input INPUT PARAMETERS
 *    argc      number of arguments passed on the command line
 *
 *    argv      array of pointers to the arguments
 *
 * Output INPUT PARAMETERS
 *    None. 
 *
 * RETURN VALUE
 *    index of the first filename in terms of argc
 *
 */
static int
parse_user_options (int argc, char *argv[])
{

    char *args;
    int i;

    for (i=1; i < argc ; i++)  {

       args =(char *)argv[i];

       if (args[0] == '-') {

           args++;     /* ptr to the option designator */
           switch (*args) {

           case 'h':
               show_instructions();
               exit (EXIT_SUCCESS);
               break;

           default:
               fprintf(stderr, "\nUnknown option: %s\n", args);
               exit (EXIT_SUCCESS);
               break;
           }

        } else {
           return (i);
        }
    }

    return (argc);
}


/*
 * b timer   
 */
static void
b_timer (stw_tmr_t *tmr, void *parm)
{
    some_data_t *p2gdata;

    p2gdata = (some_data_t *)parm;

    p2gdata->timer_b++;

    printf("%s timer b count=%u \n",
           __FUNCTION__,
           p2gdata->timer_b);

    return;
}



/*
 * a repeating timer 
 */   
static void 
a_repeating_timer (stw_tmr_t *tmr, void *parm) 
{
    some_data_t *p2gdata;

    p2gdata = (some_data_t *)parm; 

    p2gdata->timer_a++;
    printf("%s timer a count=%u \n", 
           __FUNCTION__, 
           p2gdata->timer_a);

    return;
} 


/* 
 * This function is envoked as result of the sigalrm
 * to drive the timer wheel.
 */    
static void timer_handler (int signum)
{
    stw_system_timer_tick();
}



/*
 * main line!
 */ 
int main (int argc, char **argv)
{
    /*
     * interval timer variables 
     */ 
    struct sigaction sa;
    struct itimerval timer;
    uint32_t microseconds;
    uint32_t milliseconds;

    parse_user_options(argc, argv);  



   /*
    * create and configure nodal KPA timer   
    */   
#define STW_NUMBER_BUCKETS     ( 512 ) 
#define STW_RESOLUTION         ( 100 ) 

    stw_system_timer_create(STW_NUMBER_BUCKETS, 
                            STW_RESOLUTION, 
                            "Demo Timer Wheel"); 


    stw_system_timer_destroy();





    stw_system_timer_create(STW_NUMBER_BUCKETS, 
                            STW_RESOLUTION, 
                            "Demo Timer Wheel"); 

    milliseconds = 100;
    stw_timer_prepare(&demo_a_tmr);  
    stw_system_timer_start(&demo_a_tmr, 
                            milliseconds,
                            milliseconds,
                            a_repeating_timer, 
                            &global_data);

    milliseconds = 500;
    stw_timer_prepare(&demo_b_tmr);  
    stw_system_timer_start(&demo_b_tmr,
                            milliseconds,
                            milliseconds,
                            b_timer,
                            &global_data);
 
    /* 
     * Install the interval timer_handler as the signal handler 
     * for SIGALRM. 
     */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGALRM, &sa, NULL);

    /* 
     * Configure the initial delay for 500 msec 
     * and then every 100 msec after
     */
    microseconds = (STW_RESOLUTION * 1000); 
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = microseconds;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = microseconds;

    /* 
     * Now start the interval timer. 
     */
    setitimer (ITIMER_REAL, &timer, NULL);


    global_data.timer_a = 0;
    global_data.timer_b = 0;
    while (1) {
        if (global_data.timer_a > 19) {  
            global_data.timer_a = 0;
            stw_system_timer_stats();   
        } 
    }

    exit (0);
}


