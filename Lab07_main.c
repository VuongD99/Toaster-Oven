/* Vuong Do
 * vuhdo
 * 5/21/2019
 *
 */

// Standard libraries
#include <stdio.h>

//CMPE13 Support Library
#include "BOARD.h"

// Microchip libraries
#include <xc.h>
#include <sys/attribs.h>

#include "Leds.h"
#include "Oled.h"
#include "OledDriver.h"
#include "Ascii.h"
#include "Adc.h"
#include "Buttons.h"


// **** Set any macros or preprocessor directives here ****
// Set a macro for resetting the timer, makes the code a little clearer.
#define TRUE 1
#define FALSE 0
#define defaultTemp 350
#define defaultTime 1
#define boilingTemp 500
#define MAX_LENGTH 100
#define firstPercentThreshold 87
#define nextPercentThreshold 13
// **** Set any local typedefs here ****
//enum for the diffrent oven states

typedef enum {
    SETUP, SELECTOR_CHANGE_PENDING, COOKING, RESET_PENDING
} OvenState;

//enum for the different cooking options 

typedef enum {
    BAKE, TOAST, BROIL
} CookingMode;

//enum for the different selector options

typedef enum {
    TIME, TEMPERATURE
} SelectorMode;

// **** Declare any datatypes here ****

typedef struct {
    OvenState state; //state of the oven
    int cookingTime; //current cooking time left
    int temperature; //temperature of the oven
    int16_t buttonPressTime; //time a button is pressed
    int16_t buttonReleaseTime; //time a button is released
    SelectorMode Selector; //selector (TIME OR TEMPERATURE)
    CookingMode cook_MODE; //cook mode (BAKE, TOAST, OR BROIL)
} OvenData;

// **** Define any module-level, global, or external variables here ****
static uint8_t buttonEvents; //store state of buttons
static uint8_t previous; //store state of buttons from previous interrupt
static int ADCevent = FALSE; //indicate whether the adc has moved
static int ThreeDown = FALSE; //indicate whether button 3 has been pressed
static int ThreeUp = FALSE; //indicate whether button 3 has been released
static int FourDown = FALSE; //indicate whether button 4 has been pressed
static int FourUp = FALSE; //indicate whether button 4 has been released
static int16_t freeRunningTimer = 0; //free running timer, increases every interrupt
static int oneSecondPassed = FALSE; //indicate when one second has passed
static int InitialTime = 0; //store the original cookingTime, before it goes down
static int minutes = 0; //store number of minutes left for cooking time
static int seconds = 0; //store number of seconds left for cooking time
static int percentageThreshold = firstPercentThreshold; //stores the percentage before the next LED change

static const char* COOKING_STRING[] = {//strings for different cooking modes
    "  Mode: BAKE\n", "  Mode: TOAST\n", "  Mode: BROIL\n"
};
static char outputOLED[MAX_LENGTH]; //max length of string for sprintf
static const char* topON = "|\x1\x1\x1\x1\x1|"; //top of oven, on
static const char* topOFF = "|\x2\x2\x2\x2\x2|"; //top of oven, off
static const char* botON = "|\x3\x3\x3\x3\x3|"; //bottom of oven, on
static const char* botOFF = "|\x4\x4\x4\x4\x4|"; //bottom of oven, off
static const char* side = "|     |"; //side of oven
static const char* dashSide = "|-----|"; //side of oven with dashes
static const char* selectedTime = " >Time: "; //time when selected
static const char* unselectedTime = "  Time: "; //time when not selected
static const char* selectedTemp = " >Temp: "; //temperature when selected
static const char* unselectedTemp = "  Temp: "; //temperature when not selected
static const char* temp = "F"; //stores F for printing purposes

OvenData OVEN = {SETUP, 1, defaultTemp, 0, 0, TIME, BAKE}; //initialize an instance of OvenData called OVEN

/*This function will update your OLED to reflect the state .*/
void updateOvenOLED(OvenData ovenData) {
    //clear oled
    OledClear(0);
    //find the number of minutes and seconds left for cooking time
    minutes = ovenData.cookingTime / 60;
    seconds = ovenData.cookingTime % 60;
    //if the oven is cooking....
    if (ovenData.state == COOKING) {
        //print these if the mode is BAKE
        if (ovenData.cook_MODE == BAKE) {
            sprintf(outputOLED, "%s%s%s%s%d:%d\n%s%s%d\xF8%s\n%s", topON,
                    COOKING_STRING[ovenData.cook_MODE], side, unselectedTime,
                    minutes, seconds, dashSide, unselectedTemp, ovenData.temperature,
                    temp, botON);
            OledDrawString(outputOLED);
            OledUpdate();
        //print these if the mode is TOAST    
        } else if (ovenData.cook_MODE == TOAST) {
            sprintf(outputOLED, "%s%s%s%s%d:%d\n%s\n%s", topOFF,
                    COOKING_STRING[ovenData.cook_MODE], side, unselectedTime, minutes,
                    seconds, dashSide, botON);
            OledDrawString(outputOLED);
            OledUpdate();
        //print these if the mode is BROIL            
        } else if (ovenData.cook_MODE == BROIL) {
            sprintf(outputOLED, "%s%s%s%s%d:%d\n%s%s%d\xF8%s\n%s", topON,
                    COOKING_STRING[ovenData.cook_MODE], side, unselectedTime, minutes,
                    seconds, dashSide, unselectedTemp, ovenData.temperature, temp, botOFF);
            OledDrawString(outputOLED);
            OledUpdate();
        }
    //if the oven is NOT cooking...
    } else {
        //display these if the mode is BAKE, and the selector is pointed to TIME
        if (ovenData.cook_MODE == BAKE && ovenData.Selector == TIME) {
            sprintf(outputOLED, "%s%s%s%s%d:%d\n%s%s%d\xF8%s\n%s", topOFF,
                    COOKING_STRING[ovenData.cook_MODE], side, selectedTime, minutes,
                    seconds, dashSide, unselectedTemp, ovenData.temperature, temp, botOFF);
            OledDrawString(outputOLED);
            OledUpdate();
        //display them if the mode is BAKE, and the selector is pointed to temperature    
        } else if (ovenData.cook_MODE == BAKE && ovenData.Selector == TEMPERATURE) {
            sprintf(outputOLED, "%s%s%s%s%d:%d\n%s%s%d\xF8%s\n%s", topOFF,
                    COOKING_STRING[ovenData.cook_MODE], side, unselectedTime, minutes,
                    seconds, dashSide, selectedTemp, ovenData.temperature, temp, botOFF);
            OledDrawString(outputOLED);
            OledUpdate();
        //display these if the mode is TOAST    
        } else if (ovenData.cook_MODE == TOAST) {
            sprintf(outputOLED, "%s%s%s%s%d:%d\n%s\n%s", topOFF,
                    COOKING_STRING[ovenData.cook_MODE], side, unselectedTime, minutes,
                    seconds, dashSide,botOFF);
            OledDrawString(outputOLED);
            OledUpdate();
        //display these if the mode is BROIL    
        } else if (ovenData.cook_MODE == BROIL) {
            sprintf(outputOLED, "%s%s%s%s%d:%d\n%s%s%d\xF8%s\n%s", topOFF,
                    COOKING_STRING[ovenData.cook_MODE], side, unselectedTime, minutes,
                    seconds, dashSide, unselectedTemp, ovenData.temperature, temp, botOFF);
            OledDrawString(outputOLED);
            OledUpdate();
        }
    }

}

/*This function will execute your state machine.  
 * It should ONLY run if an event flag has been set.*/
void runOvenSM(void) {
    switch (OVEN.state) {
        //if the oven is currently in SETUP state
        case SETUP:
            //if the mode is BAKE and the selector is pointing to temperature and the adc
            //changes, change the temperature and display the new temperature
            if (OVEN.cook_MODE == BAKE && OVEN.Selector == TEMPERATURE) {
                OVEN.temperature = (unsigned int) ((AdcRead() >> 2) + 300);
            //if the mode is BAKE and the selector is pointing to time and the adc
            //changes, change the time and display the new time
            } else if (OVEN.cook_MODE == BAKE && OVEN.Selector == TIME) {
                OVEN.cookingTime = (unsigned int) ((AdcRead() >> 2) + 1);
            //if the mode is TOAST and the adc changes, change and display the new time    
            } else if (OVEN.cook_MODE == TOAST) {
                OVEN.cookingTime = (unsigned int) ((AdcRead() >> 2) + 1);
            //if the mode is BROIL and the adc changes, change and display the new time    
            } else if (OVEN.cook_MODE == BROIL) {
                OVEN.cookingTime = (unsigned int) ((AdcRead() >> 2) + 1);
            }
            updateOvenOLED(OVEN);
            //if button three is pressed, store the press time and transition to SEL_PENDING
            if (ThreeDown) {
                OVEN.buttonPressTime = freeRunningTimer;
                OVEN.state = SELECTOR_CHANGE_PENDING;
            }
            //if button four is pressed, store the press time and transition to COOKING
            if (FourDown) {
                OVEN.buttonPressTime = freeRunningTimer;
                //reset the freeRunningTimer to zero. Now if the timer value%5=0, the program
                //knows that one second has passed
                freeRunningTimer = 0;
                OVEN.state = COOKING;
                //store the initial cooking time. When cooking finishes, the oven can use
                //this value and return the original cooking time to the user
                InitialTime = OVEN.cookingTime;
                //set the percentage threshold needed for the LEDs to shift, the first is 87%
                percentageThreshold = firstPercentThreshold;
                updateOvenOLED(OVEN);
                //turn all LEDs on when the oven starts cooking
                LEDS_SET(0xFF);
            }
            break;
        //if the oven is in state SEL_PENDING
        case SELECTOR_CHANGE_PENDING:
            //if button three is released, store the release time. Subtract press time
            //from release time to find how long the button was held down
            if (ThreeUp) {
                OVEN.buttonReleaseTime = freeRunningTimer;
                //if button three was not pressed longer than one second...
                if (OVEN.buttonReleaseTime - OVEN.buttonPressTime <= 5) {
                    //change the cooking mode and return time and temperature
                    //to their default value
                    if (OVEN.cook_MODE == BAKE) {
                        OVEN.cook_MODE++;
                    } else if (OVEN.cook_MODE == TOAST) {
                        OVEN.temperature = boilingTemp;
                        OVEN.cook_MODE++;
                    } else {
                        OVEN.cook_MODE = 0;
                        OVEN.temperature = defaultTemp;
                        OVEN.cookingTime = defaultTime;
                    }
                //if button three was pressed longer than one second....
                } else if (ThreeUp) {
                    //if selector is pointing to time, point to temperature and vice versa
                    if (OVEN.Selector == TIME) {
                        OVEN.Selector = TEMPERATURE;
                    } else {
                        OVEN.Selector = TIME;
                    }
                }
                //also return oven to SETUP state
                OVEN.state = SETUP;
                updateOvenOLED(OVEN);
            }
            break;
        //if oven is in state COOKING    
        case COOKING:
            //if button four is pressed while oven is cooking, store press time and go to
            //next state (RESET PENDING)
            if (FourDown) {
                OVEN.buttonPressTime = freeRunningTimer;
                OVEN.state = RESET_PENDING;
            //otherwise...
            //if there is still cooking time left and one second has passed, subtract cooking
            //time by one. Also, check if the new cook time passes the percentage threshold. If so,
            //turn off rightmost LED by shifting LEDS_GET() to the left by one
            } else if (OVEN.cookingTime > 0 && oneSecondPassed) {
                OVEN.cookingTime--;
                if (OVEN.cookingTime * 100 / InitialTime < percentageThreshold) {
                    LEDS_SET(LEDS_GET() << 1);
                    //subtract 13% from the current percentage threshold. This will be the next 
                    //percentage threshold
                    percentageThreshold -= nextPercentThreshold;
                }
                updateOvenOLED(OVEN);
            //if cooking time runs out, set the cooking time to its original value
            //turn off all LEDS, and change state to SETUP
            } else if (OVEN.cookingTime <= 0 && oneSecondPassed) {
                OVEN.cookingTime = InitialTime;
                LEDS_SET(0x00);
                OVEN.state = SETUP;
                updateOvenOLED(OVEN);
            }
            break;
        //if oven is in state RESET_PENDING    
        case RESET_PENDING:
            //if button four is released, store the release time. Find how long it's been
            //pressed down. If the result is less than one second, return to COOKING state
            if (FourUp) {
                OVEN.buttonReleaseTime = freeRunningTimer;
                if (OVEN.buttonReleaseTime - OVEN.buttonPressTime < 5) {
                    OVEN.state = COOKING;
                }
                updateOvenOLED(OVEN);
            //otherwise, reset cooking time to its original value, turn off LEDS, and go to
            //SETUP state
            } else {
                if (freeRunningTimer - OVEN.buttonPressTime > 5) {
                    OVEN.cookingTime = InitialTime;
                    LEDS_SET(0x00);
                    OVEN.state = SETUP;
                }
            }
            break;
    }
}

int main() {
    BOARD_Init();

    //initalize timers and timer ISRs:
    // <editor-fold defaultstate="collapsed" desc="TIMER SETUP">

    // Configure Timer 2 using PBCLK as input. We configure it using a 1:16 prescalar, so each timer
    // tick is actually at F_PB / 16 Hz, so setting PR2 to F_PB / 16 / 100 yields a .01s timer.

    T2CON = 0; // everything should be off
    T2CONbits.TCKPS = 0b100; // 1:16 prescaler
    PR2 = BOARD_GetPBClock() / 16 / 100; // interrupt at .5s intervals
    T2CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T2IF = 0; //clear the interrupt flag before configuring
    IPC2bits.T2IP = 4; // priority of  4
    IPC2bits.T2IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T2IE = 1; // turn the interrupt on

    // Configure Timer 3 using PBCLK as input. We configure it using a 1:256 prescaler, so each timer
    // tick is actually at F_PB / 256 Hz, so setting PR3 to F_PB / 256 / 5 yields a .2s timer.

    T3CON = 0; // everything should be off
    T3CONbits.TCKPS = 0b111; // 1:256 prescaler
    PR3 = BOARD_GetPBClock() / 256 / 5; // interrupt at .5s intervals
    T3CONbits.ON = 1; // turn the timer on

    // Set up the timer interrupt with a priority of 4.
    IFS0bits.T3IF = 0; //clear the interrupt flag before configuring
    IPC3bits.T3IP = 4; // priority of  4
    IPC3bits.T3IS = 0; // subpriority of 0 arbitrarily 
    IEC0bits.T3IE = 1; // turn the interrupt on;

    // </editor-fold>

    printf("Welcome to vuhdo's Lab07 (Toaster Oven).  Compiled on %s %s.\n", __TIME__, __DATE__);

    //initialize state machine (and anything else you need to init) here
    LEDS_INIT();
    OledInit();
    AdcInit();
    ButtonsInit();
    updateOvenOLED(OVEN);
    while (1) {
        //if detects adc changes, call state machine and change OLED accordingly
        if (ADCevent) {
            runOvenSM();
            ADCevent = FALSE;
        }
        //if detects button changes, call state machine
        if (buttonEvents) {
            if (buttonEvents & BUTTON_EVENT_3UP && buttonEvents != previous) {
                ThreeUp = TRUE;
                runOvenSM();
                ThreeUp = FALSE;
            } else if (buttonEvents & BUTTON_EVENT_3DOWN && buttonEvents != previous) {
                ThreeDown = TRUE;
                runOvenSM();
                ThreeDown = FALSE;
            } else if (buttonEvents & BUTTON_EVENT_4UP && buttonEvents != previous) {
                FourUp = TRUE;
                runOvenSM();
                FourUp = FALSE;
            } else if (buttonEvents & BUTTON_EVENT_4DOWN && buttonEvents != previous) {
                FourDown = TRUE;
                runOvenSM();
                FourDown = FALSE;
            }
            previous = buttonEvents;
            buttonEvents = FALSE;
        }
        //every time a second goes by, call the state machine
        if (oneSecondPassed) {
            runOvenSM();
            oneSecondPassed = FALSE;
        }
    }
}

/*The 5hz timer is used to update the free-running timer and to generate freeRunningTimer events*/
void __ISR(_TIMER_3_VECTOR, ipl4auto) TimerInterrupt5Hz(void) {
    // Clear the interrupt flag.
    IFS0CLR = 1 << 12;
    //for every interrupt, increment the timer value by one
    freeRunningTimer++;
    //since the timer increments once every .2 seconds, 5 ticks means one second has gone by,
    //if so, return event flag TRUE to main
    if (freeRunningTimer % 5 == 0) {
        oneSecondPassed = TRUE;
    }

}

/*The 100hz timer is used to check for button and ADC events*/
void __ISR(_TIMER_2_VECTOR, ipl4auto) TimerInterrupt100Hz(void) {
    // Clear the interrupt flag.
    IFS0CLR = 1 << 8;

    //for every interrupt, store the state of the buttons
    buttonEvents = ButtonsCheckEvents();
    //if the changes, set adc flag to TRUE and return to main
    if (AdcChanged()) {
        ADCevent = TRUE;
    }
}