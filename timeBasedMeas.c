/*
 * Copyright (c) 2018, Shanshan Xie
 * All rights reserved.
 *
 */

/*
 *  ======== timeBaseMeas.c ========
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Timestamp.h>
#include <xdc/runtime/Types.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
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

/*MUX Array 8*8*/
#define     muxLeftLength   2
#define     muxTopLength    2

#define     muxAmount       2

#define     mux0_A          IOID_13
#define     mux0_B          IOID_14
#define     mux0_C          IOID_15
#define     mux0_En         IOID_7

#define     mux1_A          IOID_6
#define     mux1_B          IOID_20
#define     mux1_C          IOID_19
#define     mux1_En         IOID_18

/* Pin driver handles */
static PIN_Handle VcPinHandle;
static PIN_Handle switchandledPinHandle;

static PIN_Status switchStatus;

/* Global memory storage for a PIN_Config table */
static PIN_State buttonPinState;
static PIN_State ledPinState;

/* Switch Array */

static int charge[arrayLength] = {Board_DIO24_ANALOG, Board_DIO25_ANALOG};
static int whichRtoCharge = 0; //0 is Rfsr, 1 is Rref

//2 Mux Array
static uint8_t muxLeft[muxLeftLength] = {0,1};
static uint8_t muxTop[muxTopLength] = {0,1};

static int i = 0;
static int j = 0;

static int whichCaseIndex = 0;
static int whichMuxIndex = 0;

static uint32_t start = 0;
static uint32_t stop = 0;

static double result[arrayLength] = {0.0,0.0};
static double time_ratio = 0;

static double Rfsr;

static int measureCount = 0;

/*
 * Initial LED pin configuration table
 *   - LEDs Board_LED0 is on.
 *   - LEDs Board_LED1 is off.
 */
PIN_Config outputPinTable[] = {

    Board_DIO24_ANALOG | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX, //Rfsr
    Board_DIO25_ANALOG | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX, //Rref1
    Board_DIO26_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //discharge
    Board_DIO27_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //c2
    Board_DIO30_ANALOG | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX, //Rref2

    //Mux Pin
    //Left Mux:
    IOID_13 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //S1
    IOID_14 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //S2
    IOID_15 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //S3
    IOID_7 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //S1

    //Top Mux:
    IOID_6 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //A
    IOID_20 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //B
    IOID_19 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //C
    IOID_18 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, //En


//    Board_LED0 | PIN_GPIO_OUTPUT_DIS | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

//Inputs
PIN_Config VcPinTable[] = {
    Board_DIO23_ANALOG | PIN_INPUT_EN | PIN_IRQ_POSEDGE ,
    PIN_TERMINATE
};

static int isC2on = 0;
static int isRref2on = 0;
static int Rref = 9862;
static int isRref1on = 1;
static int ignoreInitialData = 0;

/*
 *  ======== functions ========
 */
//whichMux = 0 --> Left Mux; 1 --> Top Mux
void selectMuxPin (int whichCase, int whichMux){
    printf("CASE: %d, Mux: %d\n", whichCase, whichMux);
    switch(whichCase){
    case 0:
        if(whichMux == 0){
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_13, 1);
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_14, 0);

        }
        else{
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_15, 1);
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_7, 0);
        }
        break;
    case 1:
        if(whichMux == 0){
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_13, 0);
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_14, 1);

        }
        else{
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_15, 0);
            switchStatus = PIN_setOutputValue(switchandledPinHandle, IOID_7, 1);
        }
        break;
    }

}

void dischargeCallbackFxn(PIN_Handle handle, PIN_Id pinId){

    //Record Stop Time
    stop = TimestampProvider_get32();

    if ( pinId == Board_DIO23_ANALOG && PIN_getInputValue(pinId)){
        //disable switch pin
        switchStatus = PIN_setOutputEnable(switchandledPinHandle, charge[whichRtoCharge], 0);
        if (switchStatus) {
            System_abort("Error disabling charging switch switch\n");
        }

        //Discharge Starts
        switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO26_ANALOG, 1);
        if (switchStatus) {
            System_abort("Error enabling discharge switch pin26\n");
        }

        //Calculate Time
        result[whichRtoCharge] = stop - start;
        Rfsr = time_ratio*Rref;

        //Ignore First xxx data
        if( ignoreInitialData < 5){
             ignoreInitialData++;
        }
        else {
            if (Rfsr < 2500 && isRref2on == 0){
                Rref = 989;
                charge[1] = Board_DIO30_ANALOG;
                switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO25_ANALOG, 0);
                printf("Rref 2 \n");
                whichRtoCharge = 1;
                isRref2on = 1;
                isRref1on = 0;
                if (switchStatus) {
                    System_abort("Error initially enabling pin24\n");
                }
            }
            if (Rfsr > 8000 && isRref1on == 0){
                Rref = 9862;
                charge[1] = Board_DIO25_ANALOG;  //Rref1 on
                switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO30_ANALOG, 0);
                printf("Rref 1 \n");
                whichRtoCharge = 1;
                isRref2on = 0;
                isRref1on = 1;
                if (switchStatus) {
                    System_abort("Error initially enabling pin24\n");
                }
            }
            if (Rfsr < 80000 && isC2on == 0){
                switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO27_ANALOG, 1);
                printf("C1+C2 \n");
                whichRtoCharge = 1;
                isC2on = 1;
                //                 start = Timestamp_get32();
                if (switchStatus) {
                    System_abort("Error initially enabling pin24\n");
                }
            }
            else if (Rfsr > 100000 && isC2on == 1){
                switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO27_ANALOG, 0);
                printf("C1 \n");
                whichRtoCharge = 1;
                isC2on = 0;
                if (switchStatus) {
                    System_abort("Error initially enabling pin24\n");
                }
            }
        }

        //int res = 817171;
        //int res = 297299;
        //int res = 99688;    //100k
        //int res = 80445;    //80k
        //int res = 81152;
        //int res = 29734;    //30k
        //int res = 19786;   //20k
        //int res = 9878;     //10kx
        //int res = 8081;     //8k
        int res = 11000;
        //int res = 2936;     //3k
        //int res = 1964;  //2k
        //int res = 989;      //1k
        //int res = 808;      //800

        if(whichRtoCharge > 0){
            time_ratio = result[0]/result[whichRtoCharge];
            if(measureCount < 10){
                measureCount++;
            }
            else{
                measureCount = 0;
            }


            //printf("time: %f\n", ((result[0]+result[index])*1000)/48000000);
            //printf("%f\n", result[0]*1000);
            //printf("Rref: %f\n", result[index]*1000);
            //printf("freq: %d\n", freq.hi);
            //printf("Error = %f\n", (((time_ratio*Rref)-res)/res)*100);
            printf("%f\n", time_ratio*Rref);
        }

        CPUdelay(10000*50);

        switchStatus = PIN_setOutputEnable(switchandledPinHandle, Board_DIO26_ANALOG, 0);
        if (switchStatus) {
            System_abort("Error disabling discharge switch pin26\n");
        }

        CPUdelay(800*50);

        whichRtoCharge = (whichRtoCharge+1) % arrayLength; //01010101 / 012012012

//        if (whichRtoCharge == 0){
//            if (whichCaseIndex == 1){
//                whichMuxIndex = (whichMuxIndex + 1) % muxAmount;
//                whichCaseIndex = 0;
//            }
//            else {
//                whichCaseIndex = whichCaseIndex +1;
//            }
//            selectMuxPin(whichCaseIndex, whichMuxIndex);
//        }

        if (whichRtoCharge == 0 && measureCount == 10){
            if (j == 1){
                if(i == 1){
                    printf("donedonedone");
                    i=0;
                    j=0;
                }
                else{
                    j = 0;
                    i = i+1;
                }
            }
            else {
                j = j+1;
            }
            selectMuxPin(muxLeft[i], 0);
            selectMuxPin(muxTop[j], 1);
        }

        //Start Charging
        switchStatus = PIN_setOutputEnable(switchandledPinHandle, charge[whichRtoCharge], 1);
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
    switchandledPinHandle = PIN_open(&ledPinState, outputPinTable);
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

    selectMuxPin(0, 0);
    selectMuxPin(0, 1);

    /* Start kernel. */
    BIOS_start();

    return (0);
}








