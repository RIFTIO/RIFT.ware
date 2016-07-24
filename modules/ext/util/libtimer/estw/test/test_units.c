

#include <stdio.h>

#include "units.h"


main()
{
    uint32_t result;
    uint32_t input;


    input = 5000;  
    result = nano_to_microseconds(input); 
    if (result != 5) {
        printf("res=%u \n"); 
    } 

    input = 5000000;  
    result = nano_to_milliseconds(input); 
    if (result != 5) {
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    input = UINT32_MAX; 
    result = nano_to_seconds(input); 
    if (result != (UINT32_MAX/1000/1000/1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 



    input = 5;  
    result = milli_to_nanoseconds(input);  
    if (result != (5 * 1000 * 1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    /* overflow case */ 
    input = UINT32_MAX;  
    result = milli_to_nanoseconds(input);  
    if (result != 0) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    input = 5;
    result = milli_to_microseconds(input);
    if (result != (5 * 1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    /* overflow case */ 
    input = UINT32_MAX;  
    result = milli_to_microseconds(input);
    if (result != 0) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    input = 5000;  
    result = milli_to_seconds(input);
    if (result != (5) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    input = UINT32_MAX; 
    result = milli_to_seconds(input); 
    if (result != (UINT32_MAX/1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 


    input = 2; 
    result = secs_to_nanoseconds(input);  
    if (result != (2 * 1000 * 1000 * 1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

#if 0 
    /* overflow */ 
    input = 4; 
    result = secs_to_nanoseconds(input);  
    if (result != (4 * 1000 * 1000 * 1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 
#endif

    /* overflow */ 
    input = UINT32_MAX; 
    result = secs_to_nanoseconds(input);  
    if (result != 0) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    input = (UINT32_MAX / 1000 / 1000 / 1000) - 1; 
    result = secs_to_nanoseconds(input);  
    if (result != (((UINT32_MAX / 1000 / 1000 / 1000) - 1 ) * 1000 * 1000 * 1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    input = (UINT32_MAX / 1000 / 1000) - 1; 
    result = secs_to_microseconds(input); 
    if (result != (((UINT32_MAX / 1000 / 1000) - 1) * 1000 * 1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 

    input = (UINT32_MAX / 1000) - 1; 
    result = secs_to_milliseconds(input);  
    if (result != (((UINT32_MAX / 1000) - 1) * 1000) ) {   
        printf("%s-%u res=%u \n", __FUNCTION__, __LINE__, result); 
    } 




    printf("Done \n"); 
    return; 
}

