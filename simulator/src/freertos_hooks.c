#include <stdio.h>
#include <stdlib.h>

// FreeRTOS hook for malloc failures
void vApplicationMallocFailedHook(void)
{
    fprintf(stderr, "FreeRTOS: malloc failed!\n");
    abort();
}

// FreeRTOS stack overflow hook
void vApplicationStackOverflowHook(void *xTask, char *pcTaskName)
{
    fprintf(stderr, "FreeRTOS: stack overflow in task %s\n", pcTaskName);
    abort();
}
