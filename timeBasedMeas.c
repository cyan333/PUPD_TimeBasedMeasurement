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

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>

/* TI-RTOS Header files */
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

/* Example/Board Header files */
#include "Board.h"

/* Pin driver handles */
static PIN_Handle VcPinHandle;
static PIN_Handle switchandledPinHandle;

static PIN_Status switchStatus;

/* Global memory storage for a PIN_Config table */
static PIN_State buttonPinState;
static PIN_State ledPinState;

/* Switch Array */
static int arrayLength = 2;
static int charge[2] = {Board_DIO24_ANALOG, Board_DIO25_ANALOG};
static int index = 0;

/*
 * Initial LED pin configuration table
 *   - LEDs Board_LED0 is on.
 *   - LEDs Board_LED1 is off.
 */
PIN_Config switchPinTable[] = {

    Board_DIO24_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_DIO25_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_DIO26_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
//    Board_LED0 | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};



PIN_Config VcPinTable[] = {
    Board_DIO23_ANALOG | PIN_INPUT_EN | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};


/*
 * Application button pin configuration table:
 *   - Buttons interrupts are configured to trigger on falling edge.
 */
PIN_Config buttonPinTable[] = {
    Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    Board_BUTTON1  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

/*
 *  ======== buttonCallbackFxn ========
 *  Pin interrupt Callback function board buttons configured in the pinTable.
 *  If Board_LED3 and Board_LED4 are defined, then we'll add them to the PIN
 *  callback function.
 */
//void buttonCallbackFxn(PIN_Handle handle, PIN_Id pinId) {
//    uint32_t currVal = 0;
//
//    /* Debounce logic, only toggle if the button is still pushed (low) */
//    CPUdelay(8000*50);
//    if (!PIN_getInputValue(pinId)) {
//        /* Toggle LED based on the button pressed */
//        switch (pinId) {
//            case Board_DIO23_ANALOG:
//                currVal =  PIN_getOutputValue(Board_LED0);
//                PIN_setOutputValue(switchandledPinHandle, Board_LED0, !currVal);
//                break;
//            default:
//                /* Do nothing */
//                break;
//        }
//    }
//}

void dischargeCallbackFxn(PIN_Handle handle, PIN_Id pinId){

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

        CPUdelay(8000*50);

        switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO26_ANALOG, 0);
        if (switchStatus) {
            System_abort("Error disabling discharge switch pin26\n");
        }

        index = (index+1) % arrayLength; //01010101 / 012012012

        //Start Charging
        switchStatus = PIN_setOutputEnable(switchandledPinHandle, charge[index], 1);
//        CPUdelay(80000*50);
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
    if (switchStatus) {
        System_abort("Error initially enabling pin24\n");
    }

//    CPUdelay(800000*50);

//    if (PIN_getInputValue(Board_DIO23_ANALOG) == 1){
//        switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO24_ANALOG, 0);
//        if (switchStatus) {
//            System_abort("Error disabling charging switch switch\n");
//        }
//        switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO26_ANALOG, 1);
//        if (switchStatus) {
//            System_abort("Error enabling discharge switch pin26\n");
//        }
//    }

//    switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO24_ANALOG, 1);
//
//    if (switchStatus == 0){
//        whatevertime = PIN_getInputValue(Board_DIO23_ANALOG);
//        if (whatevertime == 1){
//            PIN_setOutputEnable(switchandledPinHandle, Board_LED0, 1);
//        }
//
//    }
//    else{
//        System_abort("Error enabling switch\n");
//    }
//    if (!enableSwitchStatus) {
//        System_abort("Error enabling switch\n");
//    }
//
//    if (PIN_getOutputValue(PIN_getOutputValue(Board_DIO23_ANALOG)) == 1){
//        PIN_setOutputEnable(switchandledPinHandle, Board_LED0, 1);
//    }


//    /* Setup callback for button pins */
//    PIN_setOutputEnable(switchandledPinHandle, Board_LED0, 1);
//
//    if (PIN_registerIntCb(VcPinHandle, &buttonCallbackFxn) != 0) {
//            System_abort("Error registering button callback function");
//        }

//    Types_FreqHz freq;

//    t1 = Timestamp_get32();
//
//    xdc_Bits32 time_period = t1 - t0;
//    System_printf("hi: %d", t1);
//    System_flush();

//    BIOS_getCpuFreq(&freq);
//        System_printf("hi: %d, lo: %d", freq.hi, freq.lo);
//        System_flush();

    /* Start kernel. */
    BIOS_start();

    return (0);
}