
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>

/* Board Header files */
#include "Board.h"
#include "wireless/address.h"

#include "wireless/comm_lib.h"
#include "sensors/mpu9250.h"

/* Task */
#define STACKSIZE 2048
Char labTaskStack[STACKSIZE];
Char commTaskStack[STACKSIZE];
Char menuTaskStack[STACKSIZE];


enum state { WAIT=1, READ_SENSOR, MENU };
enum state myState = MENU;

/* Display */
Display_Handle hDisplay;

// JTKJ: Painonappien konfiguraatio ja muuttujat
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE, // Hox! TAI-operaatio
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};

PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, 
   PIN_TERMINATE // Määritys lopetetaan aina tähän vakioon
};
static PIN_Handle buttonHandle;
static PIN_State buttonState;
    
static PIN_Handle ledHandle;
static PIN_State ledState;


//OFF NAPPI ALUSTUS
static PIN_Handle hButtonShut;
static PIN_State bStateShut;
PIN_Config buttonShut[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};
PIN_Config buttonWake[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};


// *******************************
//
// MPU GLOBAL VARIABLES
//
// *******************************
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};
// *******************************
//
// MPU9250 I2C CONFIG
//
// *******************************
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

char labTaskString[10];
char menuString[10];

char payload[16];

Void labTaskFxn(UArg arg0, UArg arg1) {

    I2C_Handle      i2c;
    I2C_Params      i2cParams;
    I2C_Handle      i2cMPU;
    I2C_Params      i2cMPUParams;
    
    float ax, ay, az, gx, gy, gz;
	double pres,temp;
	
	//printtausta varten:
	char str[80], axv[10], ayv[10], azv[10], gxv[10], gyv[10], gzv[10];
	
   
    /* Create I2C for sensors */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;
    
    
    i2cMPU = I2C_open(Board_I2C0, &i2cMPUParams);

    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2C\n");
    }
    else {
        System_printf("I2C Initialized!\n");
    }
    
    //MPU POWER ON
    
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);
    
     // WAIT 100MS FOR THE SENSOR TO POWER UP
	Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // JTKJ: Sensorin alustus t�ss� kirjastofunktiolla
    System_printf("MPU9250: Setup and calibration...\n");
	System_flush();
    
    mpu9250_setup(&i2cMPU);
    
    System_printf("MPU9250: Setup and calibration OK\n");
	System_flush();

    char ele_address[16];
    int i = 0;
    float ay_muisti[10];
    float az_muisti[10];
    float ay_keskiarvo;
    float az_keskiarvo;
    int move_muisti = 0;
    int five_muisti = 0;
    
    //READ_SENSOR
    while(1) {
        if(myState == READ_SENSOR) {
            
            System_printf("\n");
            System_flush();
            Task_sleep(10000 / Clock_tickPeriod);
            
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
            
            //Lasketaan keskiarvo ay ja az
            ay_muisti[i] = ay;
            az_muisti[i] = az;            
            if (i == 9) {
                ay_keskiarvo = (ay_muisti[0] + ay_muisti[1] + ay_muisti[2] + ay_muisti[3] + ay_muisti[4]+ ay_muisti[5] + ay_muisti[6]+ ay_muisti[7] + ay_muisti[8]+ ay_muisti[9]) / 10;
                az_keskiarvo = (az_muisti[0] + az_muisti[1] + az_muisti[2] + az_muisti[3] + az_muisti[4]+ az_muisti[5] + az_muisti[6]+ az_muisti[7] + az_muisti[8]+ az_muisti[9]) / 10;
                i = 0;
            } else {
                i = i + 1;
            }
  
            // HIGH FIVE 
            if(five_muisti > 0) {
                five_muisti = five_muisti - 1;
            }
            if(ay_keskiarvo > 0.5 && ay_keskiarvo < 1.2 && gx > 100) {
                five_muisti = 5;
            }
            if(ay_keskiarvo > 0.5 && ay_keskiarvo < 1.2 && ay > 0.6 && five_muisti > 0 && gx < -20) {
                System_printf("High Five!\n");
                System_flush();
                strcpy(labTaskString, "High Five!");
                sprintf(ele_address, "High Five!");
                Send6LoWPAN(IEEE80154_SERVER_ADDR, ele_address, 16);
                StartReceive6LoWPAN();
                Task_sleep(3000000 / Clock_tickPeriod);
                strcpy(labTaskString, "");
                five_muisti = 0;
            }

            // LETS MOVE
            if(move_muisti > 0) {
                move_muisti = move_muisti - 1;
            }
            if(az_keskiarvo < -0.7 && az_keskiarvo > -1.2 && gx < -50) {
                move_muisti = 5;
            }
            if(az_keskiarvo < -0.6 && az_keskiarvo > -1.2 && az < -0.6 && gx > 20 && move_muisti > 0) {
                System_printf("Let's Move\n");
                System_flush();
                strcpy(labTaskString, "Let's Move");
                sprintf(ele_address, "Let's Move");
                Send6LoWPAN(IEEE80154_SERVER_ADDR, ele_address, 16);
                StartReceive6LoWPAN();
                Task_sleep(3000000 / Clock_tickPeriod);
                strcpy(labTaskString, "");
                move_muisti = 0;
            }
        }
        Task_sleep(1000 / Clock_tickPeriod);
    }
    
}


/* Communication Task */
Void commTaskFxn(UArg arg0, UArg arg1) {
    
    uint16_t senderAddr;
    // Radio to receive mode
	int32_t result = StartReceive6LoWPAN();
	if(result != true) {
		System_abort("Wireless receive mode failed");
	}

    while (true) {

        // jos true, viesti odottaa
        if (GetRXFlag()) {

           // Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
           memset(payload,0,16);
           // Luetaan viesti puskuriin payload
           Receive6LoWPAN(&senderAddr, payload, 16);
           // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
           System_printf(payload);
           System_flush();
      }
    }
}


/*MenuTask*/
Void menuTaskFxn(UArg arg0, UArg arg1) {

    /* Display */
    Display_Params displayParams;
	displayParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&displayParams);

    hDisplay = Display_open(Display_Type_LCD, &displayParams);
    if (hDisplay == NULL) {
        System_abort("Error initializing Display menutask\n");
    }
    
    Display_clear(hDisplay);
    
    //Menu-display
    char modeString[16];
    int loadtime = 0;
    while (1) {                                 
        if (myState == MENU) {
            strcpy(modeString, "Up: MEASURE");
        }
        else if(myState == READ_SENSOR) {
            strcpy(modeString, "Measuring...");
        }
        Display_print0(hDisplay, 2, 0, modeString);
        Display_print0(hDisplay, 4, 0, "Down: OFF");
        Display_print0(hDisplay, 6, 0, labTaskString);
        Display_print0(hDisplay, 8, 0, payload);
        Task_sleep(100000 / Clock_tickPeriod);
        Display_clear(hDisplay);
        Task_sleep(100000 / Clock_tickPeriod);
        
        //Näytetään saatua viestiä tietyn aikaa
        if(loadtime > 0) {
            loadtime = loadtime - 1;
        }
        if(loadtime == 1) {
            loadtime = loadtime - 1;
            memset(payload,0,16);
        }
        if(payload != "" && loadtime == 0) {
            loadtime = 20;
        }
    }
}

//Off-nappi
Void buttonShutFxn(PIN_Handle handle, PIN_Id pinId) {

   // Näyttö pois päältä
   Display_clear(hDisplay);
   Display_close(hDisplay);
   Task_sleep(100000 / Clock_tickPeriod);

   // Itse taikamenot
   PIN_close(hButtonShut);
   PINCC26XX_setWakeup(buttonWake);
   Power_shutdown(NULL,0);
}

//State vaihtaja nappi
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    
    if (myState == MENU) {
        System_printf("sensor-read\n");
        System_flush();
        myState = READ_SENSOR;
    }
    else if(myState == READ_SENSOR) {
        System_printf("menu\n");
        System_flush();
        myState = MENU;
    }
}


Int main(void) {
    // Task variables
	Task_Handle labTask;
	Task_Params labTaskParams;
	Task_Handle commTask;
	Task_Params commTaskParams;
	Task_Params menuTaskParams;
	Task_Handle menuTask;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();

    // JTKJ: Painonappi- ja ledipinnit k�ytt��n t�ss�

         hButtonShut = PIN_open(&bStateShut, buttonShut);
   if( !hButtonShut ) {
      System_abort("Error initializing button shut pins\n");
   }
   if (PIN_registerIntCb(hButtonShut, &buttonShutFxn) != 0) {
      System_abort("Error registering button callback function");
   }
    
    
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
      System_abort("Error initializing button pins\n");
    }
    ledHandle = PIN_open(&ledState, ledConfig);
    if(!ledHandle) {
       System_abort("Error initializing LED pins\n");
    }
    
    // JTKJ: Rekister�i painonapille keskeytyksen k�sittelij�funktio
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
      System_abort("Error registering button callback function");
    }
    
    
    //OPEN MPU POWER PIN
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    
    
    /* Task */
    Task_Params_init(&labTaskParams);
    labTaskParams.stackSize = STACKSIZE;
    labTaskParams.stack = &labTaskStack;
    labTaskParams.priority=2;

    labTask = Task_create(labTaskFxn, &labTaskParams, NULL);
    if (labTask == NULL) {
    	System_abort("Task craeate failed!");
    }

    /* Communication Task */
    Init6LoWPAN(); // This function call before use!

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=1;

    commTask = Task_create(commTaskFxn, &commTaskParams, NULL);
    if (commTask == NULL) {
    	System_abort("Task create failed!");
    }

    /* Menu Task */
    Task_Params_init(&menuTaskParams);
    menuTaskParams.stackSize = STACKSIZE;
    menuTaskParams.stack = &menuTaskStack;
    menuTaskParams.priority=2;

    menuTask = Task_create(menuTaskFxn, &menuTaskParams, NULL);
    if (menuTask == NULL) {
    	System_abort("Menu Task craeate failed!");
    }
    

    System_printf("Hello world!\n");
    System_flush();
    

    /* Start BIOS */
    BIOS_start();
    

    return (0);
}

