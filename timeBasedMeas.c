/*
 * Copyright (c) 2015-2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== pinInterrupt.c ========
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Timestamp.h>
#include <xdc/runtime/Types.h>
#include <ti/sysbios/BIOS.h>

#include <ti/sysbios/knl/Task.h>

//#include <ti/sysbios/family/arm/lm4/Timer.h>

//#include <ti/sysbios/family/arm/cc26xx/TimestampProvider.h>
#include <ti/sysbios/family/arm/m3/TimestampProvider.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>

/* TI-RTOS Header files */
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

/* Example/Board Header files */
#include "Board.h"

/*constant Value*/
#define     arrayLength     2

/* Pin driver handles */
static PIN_Handle VcPinHandle;
static PIN_Handle switchandledPinHandle;

static PIN_Status switchStatus;

/* Global memory storage for a PIN_Config table */
static PIN_State buttonPinState;
static PIN_State ledPinState;

/* Switch Array */

static int charge[arrayLength] = {Board_DIO24_ANALOG, Board_DIO25_ANALOG};
static int index = 0;

static uint32_t start24 = 0;
static uint32_t start25 = 0;

static uint32_t start = 0;
static uint32_t stop = 0;

static double result[arrayLength] = {0.0,0.0};
static double time_ratio = 0;
static double time_to_take_measurement = 0;


static uint32_t pin24[100];
static uint32_t pin25[100];

static int index24 = 0;
static int index25 = 0;

static double Rfsr;

/*
 * Initial LED pin configuration table
 *   - LEDs Board_LED0 is on.
 *   - LEDs Board_LED1 is off.
 */
PIN_Config switchPinTable[] = {

    Board_DIO24_ANALOG | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_DIO25_ANALOG | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_DIO26_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_DIO27_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_DIO30_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX, //Rref2

//    Board_LED0 | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};



PIN_Config VcPinTable[] = {
    Board_DIO23_ANALOG | PIN_INPUT_EN | PIN_IRQ_POSEDGE ,
    PIN_TERMINATE
};

static int isC2on = 0;
static int isRref2on = 0;
static int Rref = 9862;
static int isRref1on = 1;
static int ignoreInitialData = 0;
void dischargeCallbackFxn(PIN_Handle handle, PIN_Id pinId){


    stop = TimestampProvider_get32();

//    stop = Timestamp_get32();


//    Types_FreqHz freq;
//    TimestampProvider_getFreq((&freq));

        if ( pinId == Board_DIO23_ANALOG && PIN_getInputValue(pinId)){

            //disable switch pin
            switchStatus = PIN_setOutputEnable(switchandledPinHandle, charge[index], 0);

            if (switchStatus) {
                System_abort("Error disabling charging switch switch\n");
            }
            switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO26_ANALOG, 1);
            if (switchStatus) {
                System_abort("Error enabling discharge switch pin26\n");
            }
//            if (index == 1){
                result[index] = stop - start;
//                printf("%f\n", result[index]*1000);
//            }

        Rfsr = time_ratio*Rref;
         if( ignoreInitialData < 5){
             ignoreInitialData++;
         }
         else {
//             if( result[0] < 60000 && isRref2on == 0){
             if (Rfsr < 1500 && isRref2on == 0){
               //turn on C2
//                  switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO30_ANALOG, 1);
               Rref = 989;
               charge[1] = Board_DIO30_ANALOG;
               switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO25_ANALOG, 0);
               printf("Rref 2 \n");
               index = 1;
               isRref2on = 1;
               isRref1on = 0;
               if (switchStatus) {
                 System_abort("Error initially enabling pin24\n");
               }
           }

//          if( result[0] > 105000 && isRref1on == 0){
             if (Rfsr > 8000 && isRref1on == 0){
                //turn on C2
//                   switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO25_ANALOG, 1);
                Rref = 9862;
                charge[1] = Board_DIO25_ANALOG;  //Rref1 on
                switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO30_ANALOG, 0);
                printf("Rref 1 \n");
                index = 1;
                isRref2on = 0;
                isRref1on = 1;
                if (switchStatus) {
                  System_abort("Error initially enabling pin24\n");
                }
            }
//
//             if( result[0] < 105000 && isC2on == 0){
             if (Rfsr < 8000 && isC2on == 0){
                 switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO27_ANALOG, 1);
                 printf("C1+C2 \n");
                 index = 1;
                 isC2on = 1;
//                 start = Timestamp_get32();
                 if (switchStatus) {
                   System_abort("Error initially enabling pin24\n");
                 }
             }
//             else if (result[0] > 500000 && isC2on == 1) {
             else if (Rfsr > 10000 && isC2on == 1){
                 switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO27_ANALOG, 0);
                 printf("C1 \n");
                 index = 1;
                 isC2on = 0;
//                     start = Timestamp_get32();
                 if (switchStatus) {
                 System_abort("Error initially enabling pin24\n");
                 }
             }
//
         }

//             int res = 817171;
//             int res = 297299;
//            int res = 99688;    //100k
            int res = 80445;    //80k
//            int res = 29734;    //30k
//         int res = 19786;   //20k
//            int res = 9878;     //10kx
//            int res = 8081;     //8k
//            int res = 5059;
//            int res = 2936;     //3k
//         int res = 1964;  //2k
//            int res = 989;      //1k
//         int res = 808;      //800

            if(index > 0){
            time_ratio = result[0]/result[index];
//            printf("%f\n", result[0]*1000);
//            printf("Rref: %f\n", result[index]*1000);
//            printf("freq: %d\n", freq.hi);
            printf("%f\n", (((time_ratio*Rref)-res)/res)*100);
//            printf("%f\n", time_ratio*Rref);

//            pin24[counter] = time_ratio;
            }
            CPUdelay(10000*50);

            switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO26_ANALOG, 0);
            if (switchStatus) {
                System_abort("Error disabling discharge switch pin26\n");
            }
            CPUdelay(800*50);

            index = (index+1) % arrayLength; //01010101 / 012012012

            //Start Charging
            switchStatus = PIN_setOutputEnable(switchandledPinHandle, charge[index], 1);
            start = TimestampProvider_get32();
             if (switchStatus) {
                    System_abort("Error enabling next charging switch switch\n");
                }

        }
}

/*
 *  ======== main ========
 */
int main(void)
{
    /* Call board init functions */
    Board_initGeneral();

    /* Open LED pins */
    switchandledPinHandle = PIN_open(&ledPinState, switchPinTable);
    if(!switchandledPinHandle) {
        System_abort("Error initializing board LED pins\n");
    }

    VcPinHandle = PIN_open(&buttonPinState, VcPinTable);
    if(!VcPinHandle) {
        System_abort("Error initializing button pins\n");
    }

    if (PIN_registerIntCb(VcPinHandle, &dischargeCallbackFxn) != 0) {
        System_abort("Error registering button callback function");
    }

    switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO24_ANALOG, 1);
//    start = Timestamp_get32();
    if (switchStatus) {
        System_abort("Error initially enabling pin24\n");
    }


    /* Start kernel. */
    BIOS_start();

    return (0);
}
