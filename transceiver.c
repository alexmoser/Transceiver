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

/***** Includes *****/
#include <stdlib.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* Drivers */
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/PIN.h>
#include <driverlib/rf_data_entry.h>
#include <driverlib/rf_prop_mailbox.h>
#include <driverlib/rf_prop_cmd.h>

/* Board Header files */
#include "Board.h"

#include "smartrf_settings/smartrf_settings.h"

#define DEBUG   1
#define DELAY   0

/* Pin driver handle */
static PIN_Handle ledPinHandle;
static PIN_State ledPinState;

/*
 * Application LED pin configuration table:
 *   - All LEDs board LEDs are off.
 */
PIN_Config ledPinTable[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};


/***** Defines *****/
#define TX_TASK_STACK_SIZE      1024
#define TX_TASK_PRIORITY        2

#define FREQUENCY               2450    /* Frequency to use for tx/rx, in MHz */

/* Packet TX Configuration */
#define TX_PAYLOAD_LENGTH       4

/* Packet RX Configuration */
#define DATA_ENTRY_HEADER_SIZE  8    /* Constant header size of a Generic Data Entry */
#define RX_MAX_LENGTH           30   /* Max length byte the radio will accept */
#define NUM_DATA_ENTRIES        2    /* NOTE: Only two data entries supported at the moment */
#define NUM_APPENDED_BYTES      2    /* The Data Entries data field will contain:
                                     * 1 Header byte (RF_cmdPropRx.rxConf.bIncludeHdr = 0x1)
                                     * Max 30 payload bytes
                                     * 1 status byte (RF_cmdPropRx.rxConf.bAppendStatus = 0x1) */
#define RX_MIN_TIMEOUT          3    /* Minimum time to stay in receiving mode */
#define RX_MAX_TIMEOUT          7    /* Maximum time to stay in receiving mode */



/***** Prototypes *****/
static void txTaskFunction(UArg arg0, UArg arg1);



/***** Variable declarations *****/
static PIN_Handle pinHandle;

static Task_Params txTaskParams;
Task_Struct txTask;    /* not static so you can see in ROV */
static uint8_t txTaskStack[TX_TASK_STACK_SIZE];

static RF_Object rfObject;
static RF_Handle rfHandle;

/* TX variables */
static uint8_t txPacket[TX_PAYLOAD_LENGTH];
static uint16_t seqNumber;

/* RX variables */

/* Receive dataQueue for RF Core to fill in data */
static dataQueue_t dataQueue;
static rfc_dataEntryGeneral_t* currentDataEntry;
static uint8_t rxPacketLength;
static uint8_t* rxPacketDataPointer;

static uint8_t rxPacket[RX_MAX_LENGTH + NUM_APPENDED_BYTES - 1]; /* The length byte is stored in a separate variable */



/***** Function definitions *****/
void TxTask_init(PIN_Handle inPinHandle)
{
    pinHandle = inPinHandle;

    Task_Params_init(&txTaskParams);
    txTaskParams.stackSize = TX_TASK_STACK_SIZE;
    txTaskParams.priority = TX_TASK_PRIORITY;
    txTaskParams.stack = &txTaskStack;
    txTaskParams.arg0 = (UInt)1000000;

    Task_construct(&txTask, txTaskFunction, &txTaskParams, NULL);
}

static void txTaskFunction(UArg arg0, UArg arg1)
{
    uint8_t i;
    RF_Params rfParams;
    RF_Params_init(&rfParams);

    /* Init TX parameters */
    RF_cmdPropTx.pktLen = TX_PAYLOAD_LENGTH;
    RF_cmdPropTx.pPkt = txPacket;
    RF_cmdPropTx.startTrigger.triggerType = TRIG_NOW;   // send immediately
    RF_cmdPropTx.startTrigger.pastTrig = 1;
    RF_cmdPropTx.startTime = 0;

    /* Init RX parameters */
    RF_cmdPropRx.pQueue = &dataQueue;               /* Set the Data Entity queue for received data */
//    RF_cmdPropRx.rxConf.bAutoFlushIgnored = 1;        /* Discard ignored packets from Rx queue */
//    RF_cmdPropRx.rxConf.bAutoFlushCrcErr = 1;         /* Discard packets with CRC error from Rx queue */
    RF_cmdPropRx.maxPktLen = RX_MAX_LENGTH;         /* Implement packet length filtering to avoid PROP_ERROR_RXBUF */
//    RF_cmdPropRx.pktConf.bRepeatOk = 1;
//    RF_cmdPropRx.pktConf.bRepeatNok = 1;
    RF_cmdPropRx.startTrigger.triggerType = TRIG_NOW;
    RF_cmdPropRx.startTrigger.pastTrig = 1;
    RF_cmdPropRx.startTime = 0;

    /* Request access to the radio */
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);

    /* Set the frequency */
    RF_cmdFs.frequency = FREQUENCY;
    RF_postCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
    if (DEBUG) {
        System_printf("Frequency set to %d MHz\n", FREQUENCY);
        System_flush();
    }

    while(1)
    {
        /****** Go into TX mode ******************************/
        /* Toggle LED0 to indicate begin of TX */
        PIN_setOutputValue(pinHandle, Board_LED1,!PIN_getOutputValue(Board_LED1));

        /* Create txPacket with incrementing sequence number */
        txPacket[0] = (uint8_t)(seqNumber >> 8);
        txPacket[1] = (uint8_t)(seqNumber++);
        txPacket[2] = 'a';
        txPacket[3] = 'a';

        if (DEBUG) {
            System_printf("Sending data: ");
            for (i=0; i<TX_PAYLOAD_LENGTH; i++) {
                System_printf("%u", txPacket[i]);
            }
            System_printf("\n");
            System_flush();
        }

        /* Send txPacket */
        RF_CmdHandle tx_cmd = RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropTx, RF_PriorityNormal, NULL, 0);

        /* Wait for posted command to complete */
        RF_EventMask result = RF_pendCmd(rfHandle, tx_cmd, (RF_EventLastCmdDone | RF_EventCmdError));

        if (!(result & RF_EventLastCmdDone))
        {
            if (DEBUG) {
                System_printf("Error while sending!\n");
                System_flush();
            }
            /* Error */
            while(1);
        }

        /* introduce delay */
        if (DELAY)
            Task_sleep(1000000 / Clock_tickPeriod);

        /* Toggle LED0 to indicate end of TX */
        PIN_setOutputValue(pinHandle, Board_LED1,!PIN_getOutputValue(Board_LED1));

        /****** Go into RX mode ******************************/
        /* Toggle LED1 to indicate begin of RX */
        PIN_setOutputValue(pinHandle, Board_LED0,!PIN_getOutputValue(Board_LED0));

        currentDataEntry = (rfc_dataEntryGeneral_t *) rxPacket;
        currentDataEntry->length = 1 + RX_MAX_LENGTH + NUM_APPENDED_BYTES;
        currentDataEntry->status = 0;
        dataQueue.pCurrEntry = (uint8_t *) currentDataEntry;
        dataQueue.pLastEntry = NULL;

        /* Modify CMD_PROP_RX command for application needs */
        uint8_t timeout = RX_MIN_TIMEOUT + (rand() % (RX_MAX_TIMEOUT-RX_MIN_TIMEOUT));
        RF_cmdPropRx.endTrigger.triggerType = TRIG_ABSTIME;
        RF_cmdPropRx.endTime = RF_getCurrentTime() + (uint32_t)(timeout*8000000*0.5f);
//        if (DEBUG) System_printf("timeout set to: %d s\n", timeout);

        /* Start receiving */
        RF_CmdHandle rx_cmd = RF_postCmd(rfHandle, (RF_Op*)&RF_cmdPropRx, RF_PriorityNormal, NULL, 0);

        /* Wait for posted command to complete */
        result = RF_pendCmd(rfHandle, rx_cmd, (RF_EventLastCmdDone | RF_EventCmdError));

        if (result & RF_EventLastCmdDone) {
            //Check command status
            if (RF_cmdPropRx.status == PROP_DONE_OK) {
                //Check that data entry status indicates it is finished with
                if (currentDataEntry->status == DATA_ENTRY_FINISHED) {
                    /* Make LED blink when data is received */
                    PIN_setOutputValue(pinHandle, Board_LED0,!PIN_getOutputValue(Board_LED0));

                    /* Handle the packet data, located at &currentDataEntry->data:
                     * - Length is the first byte with the current configuration
                     * - Data starts from the second byte */
                    // TODO might add address
                    rxPacketLength      = *(uint8_t *)(&currentDataEntry->data);
                    rxPacketDataPointer = (uint8_t *)(&currentDataEntry->data + 1);

                    /* Copy the payload + the status byte to the packet variable */
                    memcpy(rxPacket, rxPacketDataPointer, (rxPacketLength + 1));

                    /* Print received data on the console */
                    System_printf("Data received: ", rxPacketLength);
                    for (i=0; i<rxPacketLength; i++) {
                        System_printf("%u", rxPacketDataPointer[i]);
                    }
                    System_printf(" (len = %u)\n", rxPacketLength);
                    System_flush();

                    PIN_setOutputValue(pinHandle, Board_LED0,!PIN_getOutputValue(Board_LED0));
                }
            }
            else if (RF_cmdPropRx.status == PROP_DONE_RXTIMEOUT) {
                if(DEBUG) {
                    System_printf("Timeout expired (%d s)\n", timeout);
                    System_flush();
                }
            }
            else {
                if(DEBUG) {
                    System_printf("Other reason (code 0x%04x)\n", RF_cmdPropRx.status);
                    System_flush();
                }
            }
        }
        else {
            if (DEBUG) {
                System_printf("Error while sending!\n");
                System_flush();
            }
            /* Error */
            while(1);
        }

        /* Toggle LED1 to indicate end of RX */
        PIN_setOutputValue(pinHandle, Board_LED0,!PIN_getOutputValue(Board_LED0));
    }
}

/*
 *  ======== main ========
 */
int main(void)
{
    /* Call board init functions. */
    Board_initGeneral();

    System_printf("Transceiver 1\n");
    System_flush();

    /* Open LED pins */
    ledPinHandle = PIN_open(&ledPinState, ledPinTable);
    if(!ledPinHandle)
    {
        System_abort("Error initializing board LED pins\n");
    }

    /* Initialize task */
    TxTask_init(ledPinHandle);

    /* Start BIOS */
    BIOS_start();

    return (0);
}
