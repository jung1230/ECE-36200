/**
  ******************************************************************************
  * @file    main.c
  * @author  Weili An
  * @version V1.0
  * @date    Oct 10, 2022
  * @brief   ECE 362 Lab 5 Solution
  ******************************************************************************
*/


#include "stm32f0xx.h"
#include <stdint.h>

void initb();
void initc();
void setn(int32_t pin_num, int32_t val);
int32_t readpin(int32_t pin_num);
void buttons(void);
void keypad(void);

void mysleep(void) {
    for(int n = 0; n < 1000; n++);
}

int main(void) {
    // Uncomment when most things are working
    autotest();
    
    initb();
    initc();

    // uncomment one of the loops, below, when ready
    // while(1) {
    //   buttons();
    // }

    // while(1) {
    //   keypad();
    // }

    for(;;);
}

/**
 * @brief Init GPIO port B
 *        Pin 0: input
 *        Pin 4: input
 *        Pin 8-11: output
 *
 */
void initb() {
    RCC -> AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB -> MODER &= ~0x00FF0303;
    GPIOB -> MODER |= 0x00550000;
}

/**
 * @brief Init GPIO port C
 *        Pin 0-3: inputs with internal pull down resistors
 *        Pin 4-7: outputs
 *
 */
void initc() {
    RCC -> AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC -> MODER &= ~0x0000FFFF;
    GPIOC -> MODER |= 0x00005500;
    GPIOC -> PUPDR &= ~0x000000FF;
    GPIOC -> PUPDR |= 0x000000AA;
}

/**
 * @brief Set GPIO port B pin to some value
 *
 * @param pin_num: Pin number in GPIO B
 * @param val    : Pin value, if 0 then the
 *                 pin is set low, else set high
 */
void setn(int32_t pin_num, int32_t val) {
    int32_t temp;
    if(val == 0){
        temp = 1 << 16 + (pin_num);
        GPIOB -> BSRR |= temp;
    }
    else{
        temp = 1 << pin_num;
        GPIOB -> BSRR |= temp;
    }
}

/**
 * @brief Read GPIO port B pin values
 *
 * @param pin_num   : Pin number in GPIO B to be read
 * @return int32_t  : 1: the pin is high; 0: the pin is low
 */
int32_t readpin(int32_t pin_num) {
    int32_t temp;
    temp = 1 << pin_num;
    temp &= GPIOB -> IDR ;
    temp = temp >> pin_num;
}

/**
 * @brief Control LEDs with buttons
 *        Use PB0 value for PB8
 *        Use PB4 value for PB9
 *
 */
void buttons(void) {
    int32_t temp;
    temp = readpin(0);
    setn(8,temp);
    temp = readpin(4);
    setn(9, temp);
}

/**
 * @brief Control LEDs with keypad
 *
 */

void keypad(void) {
    int column = 0, i = 0, temp = 0, rowinput, j;

    for(i = 0; i < 4; i++){
        temp = i + 4;
        column = 1 << temp;
        GPIOC -> ODR &= 0x00FF;
        GPIOC -> ODR |= column;
        mysleep();
        rowinput = GPIOC -> IDR & 0xF;
        for(j = 0; j < 4; j++){
            temp = 1 << j;
            temp = temp & rowinput;
            temp = temp >> j;
            setn(j+8,temp);
        }
    }



           //set ith column to be 1 using GPIOC->ODR

        mysleep();
           //read the row inputs PC0-PC3 using GPIOC->IDR & 0xF
           //check the ith row value and set this to ith LED output pin using `setn`

}
