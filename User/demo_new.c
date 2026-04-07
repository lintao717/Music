/**
 * Demo entry point - calls hardware test
 */

#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "hardware_test.h"
#include "demo.h"

void demo_run(void)
{
    /* Run hardware test program */
    hardware_test_run();
}
