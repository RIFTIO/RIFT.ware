/*----------------------------------------------------------------------
 * watch.c -- watch to demo stw
 *
 * July 2009, Bo Berry
 *
 * Copyright (c) 2009 Bo Berry 
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *---------------------------------------------------------------------
 */ 

static char FileID[] = "@(#)"__FILE__" "__DATE__" "__TIME__" ";

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h> 
#include <sys/time.h>

#include "safe_types.h"
#include "time_units.h"

#include "stw_system_timer.h"



typedef enum {
    January = 1,
    Febuary,
    March,
    April,
    May, 
    June,
    July,
    August,
    September,
    October,
    November,
    December,
} months_e; 

#define NUM_MONTHS   ( 12 ) 


#define AM_INDEX     ( 0 ) 
#define PM_INDEX     ( 1 ) 


uint32_t hundreds = 0;
uint32_t seconds = 0;
uint32_t minutes = 0;
uint32_t hours_12 = 0;
uint32_t hours_24 = 0;
uint32_t am_pm_index = 0;   /* 0=AM, 1=PM */  
uint32_t day_of_week = 1;  
uint32_t day_of_month = 1;  
uint32_t day_of_year = 1;  
uint32_t week_of_year = 1;  
uint32_t month = 1;  
uint32_t year = 2000;  


int option = 0;
boolean_t military_time = FALSE;


char *am_pm_designation[2] =
     {"AM", "PM"}; 

/*
 * Indexed by non-leap and leap year
 */ 
uint32_t days_per_month_calendar[NUM_MONTHS+1][2] = 
      {{ 00, 00},
       { 31, 31 },    /* Jan */  
       { 28, 29 },    /* Feb - leap year */  
       { 31, 31 },  
       { 30, 30 },  
       { 31, 31 },  
       { 30, 30 },   /* June */  
       { 31, 31 },   /* July */  
       { 31, 31 },  
       { 30, 30 },  
       { 31, 31 },  
       { 30, 30 },  
       { 30, 31 }};   /* Dec */   


/*
 * A table providing the base day of the year by month. 
 */ 
uint32_t day_of_year_month_base[NUM_MONTHS+1] =
       {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};  


static char *instructions[] = {
 " watch [options]  ",
 "  options: ",
 "    -v  Display version ",
 "    -d  Debug flags, hex ",
 "    -m  military time ",
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
 * INPUT PARAMETERS
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



/** 
 * NAME
 *    day_to_text 
 *
 * DESCRIPTION
 *    This function converts the numerical day to a
 *    string for display. 1==Sunday, ..., 7==Saturday
 *
 * INPUT PARAMETERS
 *    day_of_week - 1-Sunday, 7-Saturday
 *
 * RETURN VALUE
 *    pointer to the string 
 * 
 */
static char 
*day_to_text(uint32_t day_of_week)
{
    switch (day_of_week) {
    case 1:
       return ("Sunday");
       break;
    case 2:
       return ("Monday");
       break;
    case 3:
       return ("Tuesday");
       break;
    case 4:
       return ("Wednesday");
       break;
    case 5:
       return ("Thursday");
       break;
    case 6:
       return ("Friday");
       break;
    case 7:
       return ("Saturday");
       break;
    default:
       break;
    } 
    day_of_week = 1;
    return ("Crash");
}


/** 
 * NAME
 *    is_this_a_leap_year
 * 
 * DESCRIPTION 
 *    This function returns TRUE if the year is a leap year. 
 *
 * INPUT PARAMETERS:
 *    year to check  
 *
 * RETURN VALUE 
 *    TRUE - leap year
 *    FALSE - not a leap year 
 * 
 */ 
boolean_t 
is_this_a_leap_year (uint32_t year)
{
    if (!(year%400) ) {
        return (TRUE);
    } 
    if (!(year%100) ) {
        return (TRUE);
    } 
    if (!(year%4) ) {
        return (TRUE);
    } 
    return (FALSE);
}


/** 
 * NAME
 *    compute_days_per_month  
 * 
 * DESCRIPTION 
 *    This function returns the number of days in the month,
 *    accounting for leap year.
 *
 * INPUT PARAMETERS:
 *    month
 *    year  
 *
 * RETURN VALUE
 *    TRUE - leap year
 *    FALSE - not a leap year 
 * 
 */
static uint32_t 
compute_days_per_month (uint32_t month, uint32_t year)
{
    uint32_t dpm = 0;

    if (is_this_a_leap_year(year)) {
        dpm = days_per_month_calendar[month][1];
    } else {
        dpm = days_per_month_calendar[month][0];
    } 
    return (dpm); 
}


/** 
 * NAME
 *    compute_day_of_the_year 
 *  
 * DESCRIPTION 
 *    This function returns the numerical day of the year.
 *  
 * INPUT PARAMETERS:
 *    month
 *    day_of_month 
 *    year   
 *
 * RETURN VALUE
 *    numerical day of the year 
 * 
 */
static uint32_t
compute_day_of_the_year (uint32_t month, uint32_t day_of_month, uint32_t year)
{
    uint32_t day_of_the_year; 

    day_of_the_year = day_of_month + day_of_year_month_base[month]; 

    if (is_this_a_leap_year(year)) { 
        if (month > Febuary) {
            day_of_the_year++;
        } else if (month == Febuary && day_of_month > 28) {
            day_of_the_year++;
        } 
    } 
    return (day_of_the_year);
}


/*
 *
 * NAME
 *    parse_user_options
 *
 * DESCRIPTION
 *    Parses command line options.
 *
 * INPUT PARAMETERS:
 *    p2config - This is a pointer to the configuration data  
 * 
 *    argc - This is the number of arguments passed on the command line
 *
 *    argv - This is an array of pointers to the arguments
 *
 * RETURN VALUE
 *    index of the first filename in terms of argc
 *
 */
static int
parse_user_options (int argc, 
                         char *argv[])
{
    char *args;
    int i;

    for (i=1; i < argc ; i++)  {
       args =(char *)argv[i];
       if (args[0] == '-') {

           args++;     /* ptr to the option designator */
           switch (*args) {

           case 'v':
               printf("\n%s \n\n", &FileID[4]);
               exit (EXIT_SUCCESS);
               break;

           case 'm':
               military_time = TRUE; 
               break;

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


/** 
 * NAME
 *    watch_tick  
 *
 * DESCRIPTION
 *    This function drives the tick accumulation used to maintain
 *    time and date.
 *
 * INPUT PARAMETERS:
 *    tmr - The pointer to the timer structure
 *    prm - This is a persistent parameter set when the timer
 *          was started.  Not used. 
 *
 * RETURN VALUE
 *    none
 * 
 */
static void
watch_tick(stw_tmr_t *tmr, void *parm)
{
    /*
     * start with the hundredths and roll up 
     */ 
    hundreds++;

    if(hundreds == 99) { 
        hundreds = 0; 
        seconds++;
        if (seconds == 60) {
            seconds = 0; 
            minutes++;
            if (minutes == 60) {
                minutes = 0; 
                hours_12++;
                if (hours_12 == 12) {
                    hours_12 = 0; 
                    am_pm_index = am_pm_index ^ 1; 
                } 
                hours_24++;
                if (hours_24 == 24) {
                    hours_24 = 0; 
                    day_of_week++; 
                    if (day_of_week == 7) {
                        week_of_year++; 
                        day_of_week = 1;
                    }  
                    day_of_month++; 
                    if (day_of_month == compute_days_per_month(month, year)) {
                        day_of_month = 1;
                        month++;
                        if (month == 12) {
                           week_of_year = 1;  
                           month = 1;
                        } 
                    }  
                    day_of_year++; 
                    if (is_this_a_leap_year(year)) {
                        if (day_of_year == 366) {
                            day_of_year = 1;
                            year++;
                        } else if (day_of_year == 365) {
                            day_of_year = 1;
                            year++;
                        }  
                    }  
                }  
            }  
        }  
    }  
        
    if (military_time) {  
        printf("     %02u:%02u:%02u:%02u %s  %12s %02u/%02u/%04u  \r", 
               hours_12, minutes, seconds, hundreds, am_pm_designation[am_pm_index],  
               day_to_text(day_of_week), 
               month,  day_of_month, year); 
    } else {  
        printf("     %02u:%02u:%02u:%02u  %12s %02u/%02u/%04u  \r", 
               hours_24, minutes, seconds, hundreds, 
               day_to_text(day_of_week), 
               month,  day_of_month, year); 
    }
    return; 
}


/* 
 * This function is envoked as result of the interval timer expiration
 */    
static void timer_handler (int signum)
{
    stw_system_timer_tick();
}


int main (int argc, char **argv)
{
    /*
     * interval timer variables 
     */ 
    struct sigaction sa;
    struct itimerval timer;
    uint32_t microseconds;
    //uint32_t milliseconds;

    stw_tmr_t watch_tick_tmr;

    parse_user_options(argc, argv); 
 
   /*
    * create and configure the timer   
    */   
#define STW_NUMBER_BUCKETS     ( 512 ) 
#define STW_RESOLUTION         ( 100 ) 

    stw_system_timer_create(STW_NUMBER_BUCKETS, 
                          STW_RESOLUTION, 
                          "Digital Watch"); 

    /* 
     * Install the interval timer_handler as the signal handler 
     * for SIGALRM. 
     */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGALRM, &sa, NULL);

    /* 
     * Configure the initial and post delay 
     */
    microseconds = milli_to_microseconds(STW_RESOLUTION);
    microseconds = microseconds/10;  
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = microseconds;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = microseconds;

    /* 
     * Now start the interval timer. 
     */
    setitimer (ITIMER_REAL, &timer, NULL);

    stw_system_timer_prepare(&watch_tick_tmr); 
    stw_system_timer_start(&watch_tick_tmr,
                          1, 
                          1,  
                          watch_tick,
                          0);
 
#if 0 
    printf("\n");
    printf(" 2/2000 == %u \n", compute_days_per_month (2, 2000) ); 
    printf(" 2/2002 == %u \n", compute_days_per_month (2, 2002) ); 
    printf(" 2/2004 == %u \n", compute_days_per_month (2, 2004) ); 

    printf("\n");
    printf(" 3/1/2008 == %u \n", compute_day_of_the_year(3, 1, 2008));  
    printf(" 4/1/2008 == %u \n", compute_day_of_the_year(4, 1, 2008));  
    printf(" 3/10/2009 == %u \n", compute_day_of_the_year(3, 10, 2009));  
#endif 

    printf("\n");
    printf("\n");

    while (1) {
        //gets(input); 
        option = getchar(); 
        if (option == '0') {
            printf("option 0 \n"); 
        } else if (option == '1') {
            printf("option 1 \n"); 
        } else if (option == '2') {
            printf("option 2 \n"); 
        } else {  
        }
    }

    exit (0);
}


