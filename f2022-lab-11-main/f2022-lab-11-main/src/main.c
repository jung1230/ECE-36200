/**
  ******************************************************************************
  * @file    main.c
  * @author  Weili An
  * @version V1.0
  * @date    Nov 26, 2022
  * @brief   ECE 362 Lab 11 student template
  ******************************************************************************
*/


#include "stm32f0xx.h"
#include <stdint.h>

// Uncomment only one of the following to test each step
#define STEP41
//#define STEP42
//#define STEP43
//#define STEP44

void init_usart5();

void init_usart5() {
    RCC -> AHBENR |= RCC_AHBENR_GPIOCEN;
    RCC -> AHBENR |= RCC_AHBENR_GPIODEN;
    GPIOC -> MODER &= ~0X3000000;
    GPIOC -> MODER |= 0X2000000;
    GPIOD -> MODER &= ~0X30;
    GPIOD -> MODER |= 0X20;

    GPIOC->AFR[1] &= ~0XF0000;
    GPIOC->AFR[1] |= 0X20000;
    GPIOD->AFR[0] &= ~0XF00;
    GPIOD->AFR[0] |= 0X200;

    RCC -> APB1ENR|= RCC_APB1ENR_USART5EN;

    //(First, disable it by turning off its UE bit.)
    USART5 -> CR1 &= ~USART_CR1_UE;
    //Set a word size of 8 bits.
    USART5 -> CR1 &= ~0X10001000;
    //Set it for one stop bit.
    USART5 -> CR2 &= ~(11 << 12);//00
    //Set it for no parity.
    USART5 -> CR1 &= ~USART_CR1_PCE;
    //Use 16x oversampling.
    USART5 -> CR1 &= ~USART_CR1_OVER8;
    //Use a baud rate of 115200 (115.2 kbaud). Refer to table 96 of the Family Reference Manual, or simply divide the system clock rate by 115200.
    USART5 -> BRR =  0x1A1;
    //Enable the transmitter and the receiver by setting the TE and RE bits.
    USART5 -> CR1 |= USART_CR1_TE;
    USART5 -> CR1 |= USART_CR1_RE;
    //Enable the USART.
    USART5 -> CR1 |= USART_CR1_UE;
    //Finally, you should wait for the TE and RE bits to be acknowledged by checking that TEACK and REACK bits are both set in the ISR. This indicates that the USART is ready to transmit and receive.
    while( ( (USART5 -> ISR) & (USART_ISR_TEACK | USART_ISR_REACK) ) != 0X600000 );
}


#ifdef STEP41
int main(void){
    init_usart5();
    for(;;) {
        while (!(USART5->ISR & USART_ISR_RXNE)) { }
        char c = USART5->RDR;
        while(!(USART5->ISR & USART_ISR_TXE)) { }
        USART5->TDR = c;
    }
}
#endif

#ifdef STEP42
#include <stdio.h>

// TODO Resolve the echo and carriage-return problem

int __io_putchar(int c) {
    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    if(c == '\n'){
       while(!(USART5->ISR & USART_ISR_TXE));
       USART5 -> TDR = '\r';
    }
    return c;
}

int __io_getchar(void) {
    while (!(USART5->ISR & USART_ISR_RXNE));
    char c = USART5->RDR;
    if(c == '\r'){
           c = '\n';
        }
    __io_putchar(c);
    return c;
}

int main() {
    init_usart5();
    setbuf(stdin,0);
    setbuf(stdout,0);
    setbuf(stderr,0);
    printf("Enter your name: ");
    char name[80];
    fgets(name, 80, stdin);
    printf("Your name is %s", name);
    printf("Type any characters.\n");
    for(;;) {
        char c = getchar();
        putchar(c);
    }
}
#endif

#ifdef STEP43
#include <stdio.h>
#include "fifo.h"
#include "tty.h"
int __io_putchar(int c) {
    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    if(c == '\n'){
       while(!(USART5->ISR & USART_ISR_TXE));
       USART5 -> TDR = '\r';
    }
    return c;
}

int __io_getchar(void) {
    char c = line_buffer_getchar();
    return c;
}

int main() {
    init_usart5();
    setbuf(stdin,0);
    setbuf(stdout,0);
    setbuf(stderr,0);
    printf("Enter your name: ");
    char name[80];
    fgets(name, 80, stdin);
    printf("Your name is %s", name);
    printf("Type any characters.\n");
    for(;;) {
        char c = getchar();
        putchar(c);
    }
}
#endif

#ifdef STEP44

#include <stdio.h>
#include "fifo.h"
#include "tty.h"

// TODO DMA data structures
#define FIFOSIZE 16
char serfifo[FIFOSIZE];
int seroffset = 0;

void enable_tty_interrupt(void) {
    // TODO
    USART5 -> CR1 |= USART_CR1_RXNEIE;
    USART5 -> CR3 |= USART_CR3_DMAR;
    NVIC -> ISER[0] |= 0x20000000;


    RCC->AHBENR |= RCC_AHBENR_DMA2EN;
    DMA2->RMPCR |= DMA_RMPCR2_CH2_USART5_RX;
    DMA2_Channel2->CCR &= ~DMA_CCR_EN;  // First make sure DMA is turned off
    //CMAR should be set to the address of serfifo.
    DMA2_Channel2 -> CMAR = serfifo;

    //CPAR should be set to the address of the USART5->RDR.
    DMA2_Channel2 -> CPAR = &(USART5->RDR);

    //CNDTR should be set to FIFOSIZE.
    DMA2_Channel2 -> CNDTR = FIFOSIZE;

    //The DIRection of copying should be from peripheral to memory.
    DMA2_Channel2 -> CCR &= ~DMA_CCR_DIR;

    //Neither the total-completion nor the half-transfer interrupt should be enabled.
    DMA2_Channel2 -> CCR &= ~DMA_CCR_TCIE;
    DMA2_Channel2 -> CCR &= ~DMA_CCR_HTIE;

    //Both the MSIZE and the PSIZE should be set for 8 bits.
    DMA2_Channel2 -> CCR &= ~DMA_CCR_MSIZE;
    DMA2_Channel2 -> CCR &= ~DMA_CCR_PSIZE;

    //MINC should be set to increment the CMAR.
    DMA2_Channel2 -> CCR |= DMA_CCR_MINC;

    //PINC should not be set so that CPAR always points at the USART5->RDR.
    DMA2_Channel2 -> CCR &= ~DMA_CCR_PINC;

    //Enable CIRCular transfers.
    DMA2_Channel2 -> CCR |= DMA_CCR_CIRC;

    //Do not enable MEM2MEM transfers.
    DMA2_Channel2 -> CCR &= ~DMA_CCR_MEM2MEM;

    //Set the Priority Level to highest.
    DMA2_Channel2 -> CCR |= DMA_CCR_PL;

    //Finally, make sure that the channel is enabled for operation.
    DMA2_Channel2->CCR |= DMA_CCR_EN;
}

// Works like line_buffer_getchar(), but does not check or clear ORE nor wait on new characters in USART
char interrupt_getchar() {
    // TODO
    USART_TypeDef *u = USART5;
    // If we missed reading some characters, clear the overrun flag.
    if (u->ISR & USART_ISR_ORE)
        u->ICR |= USART_ICR_ORECF;
    // Wait for a newline to complete the buffer.

    while(fifo_newline(&input_fifo) == 0) {
    asm volatile ("wfi"); // wait for an interrupt

        while (!(u->ISR & USART_ISR_RXNE))
            ;
        insert_echo_char(u->RDR);
    }
    // Return a character from the line buffer.
    char ch = fifo_remove(&input_fifo);
    return ch;
}



int __io_putchar(int c) {
    // TODO Copy from step 42
    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    if(c == '\n'){
       while(!(USART5->ISR & USART_ISR_TXE));
       USART5 -> TDR = '\r';
    }
    return c;
}

int __io_getchar(void) {
    // TODO Use interrupt_getchar() instead of line_buffer_getchar()
    interrupt_getchar();
}

// TODO Copy the content for the USART5 ISR here
// TODO Remember to look up for the proper name of the ISR function
void USART3_4_5_6_7_8_IRQHandler(void) {
    while(DMA2_Channel2->CNDTR != sizeof serfifo - seroffset) {
        if (!fifo_full(&input_fifo))
            insert_echo_char(serfifo[seroffset]);
        seroffset = (seroffset + 1) % sizeof serfifo;
    }
}

int main() {
    init_usart5();
    enable_tty_interrupt();

    setbuf(stdin,0); // These turn off buffering; more efficient, but makes it hard to explain why first 1023 characters not dispalyed
    setbuf(stdout,0);
    setbuf(stderr,0);
    printf("Enter your name: "); // Types name but shouldn't echo the characters; USE CTRL-J to finish
    char name[80];
    fgets(name, 80, stdin);
    printf("Your name is %s", name);
    printf("Type any characters.\n"); // After, will type TWO instead of ONE
    for(;;) {
        char c = getchar();
        putchar(c);
    }
}
#endif
