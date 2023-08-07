/**
  ******************************************************************************
  * @file    main.c
  * @author  Weili An
  * @version V1.0
  * @date    Nov 15, 2022
  * @brief   ECE 362 Lab 10 Student template
  ******************************************************************************
*/


#include "stm32f0xx.h"

// Be sure to change this to your login...
const char login[] = "xyz";

void set_char_msg(int, char);
void nano_wait(unsigned int);


//===========================================================================
// Configure GPIOC
//===========================================================================
void enable_ports(void) {
    // Only enable port C for the keypad
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~0xffff;
    GPIOC->MODER |= 0x55 << (4*2);
    GPIOC->OTYPER &= ~0xff;
    GPIOC->OTYPER |= 0xf0;
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;
}

uint8_t col; // the column being scanned

void drive_column(int);   // energize one of the column outputs
int  read_rows();         // read the four row inputs
void update_history(int col, int rows); // record the buttons of the driven column
char get_key_event(void); // wait for a button event (press or release)
char get_keypress(void);  // wait for only a button press event.
float getfloat(void);     // read a floating-point number from keypad
void show_keys(void);     // demonstrate get_key_event()

//===========================================================================
// Configure timer 7 to invoke the update interrupt at 1kHz
// Copy from lab 8 or 9.
//===========================================================================
void init_tim7(void) {
    RCC -> APB1ENR |= RCC_APB1ENR_TIM7EN;
    TIM7 -> DIER |= TIM_DIER_UIE;
    TIM7 -> PSC = 1000 - 1;
    TIM7 -> ARR = 48 - 1;
    NVIC -> ISER[0] |= 1 << TIM7_IRQn;
    TIM7 -> CR1 |= TIM_CR1_CEN;
}


//===========================================================================
// Copy the Timer 7 ISR from lab 9
//===========================================================================
// TODO To be copied
void TIM7_IRQHandler(){ /* TIM7 global interrupt*/
    // Remember to acknowledge the interrupt here!
    TIM7 -> SR &= ~TIM_SR_UIF;

    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);
}

//===========================================================================
// 4.1 Bit Bang SPI LED Array
//===========================================================================
int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];

//===========================================================================
// Configure PB12 (NSS), PB13 (SCK), and PB15 (MOSI) for outputs
//===========================================================================
void setup_bb(void) {
    RCC -> AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB -> MODER &= ~ 0xCF000000;
    GPIOB -> MODER |= 0x45000000;
    GPIOB -> ODR &= ~ 0xB000;
    GPIOB -> ODR |= 0x1000;
}

void small_delay(void) {
    nano_wait(500000);
}

//===========================================================================
// Set the MOSI bit, then set the clock high and low.
// Pause between doing these steps with small_delay().
//===========================================================================
void bb_write_bit(int val) {
    // NSS (PB12)
    // SCK (PB13)
    // MOSI (PB15)
    if(val == 0)
        GPIOB -> ODR &= ~0x8000;
    else
        GPIOB -> ODR |= 0x8000;
    small_delay();
    GPIOB -> ODR |= 0x2000;
    small_delay();
    GPIOB -> ODR &= ~0x2000;
}

//===========================================================================
// Set NSS (PB12) low,
// write 16 bits using bb_write_bit,
// then set NSS high.
//===========================================================================
void bb_write_halfword(int halfword) {
    GPIOB -> ODR &= ~0x1000;
    for(int i = 15; i >= 0; i--){
        int32_t temp = halfword & (1 << i);
        temp = temp >> i;
        bb_write_bit(temp);
    }
    GPIOB -> ODR |= 0x1000;
}

//===========================================================================
// Continually bitbang the msg[] array.
//===========================================================================
void drive_bb(void) {
    for(;;)
        for(int d=0; d<8; d++) {
            bb_write_halfword(msg[d]);
            nano_wait(1000000); // wait 1 ms between digits
        }
}

//============================================================================
// setup_dma()
// Copy this from lab 8 or lab 9.
// Write to SPI2->DR instead of GPIOB->ODR.
//============================================================================
void setup_dma(void) {
    RCC -> AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel5 -> CCR &= ~DMA_CCR_EN;
    DMA1_Channel5 -> CPAR = &(SPI2 -> DR); //change
    DMA1_Channel5 -> CMAR = msg;
    DMA1_Channel5 -> CNDTR = 8;
    DMA1_Channel5 -> CCR |= DMA_CCR_DIR;
    DMA1_Channel5 -> CCR |= DMA_CCR_MINC;
    DMA1_Channel5 -> CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel5 -> CCR &= ~DMA_CCR_PSIZE;

    DMA1_Channel5 -> CCR |= 0x520; //mem size | peri size | circ mode
    enable_dma();
}
//============================================================================
// enable_dma()
// Copy this from lab 8 or lab 9.
//============================================================================
void enable_dma(void) {
    DMA1_Channel5 -> CCR |= DMA_CCR_EN;
}
//============================================================================
// Configure Timer 15 for an update rate of 1 kHz.
// Trigger the DMA channel on each update.
// Copy this from lab 8 or lab 9.
//============================================================================
void init_tim15(void) {
    RCC -> APB2ENR |= RCC_APB2ENR_TIM15EN;
    TIM15 -> DIER |= TIM_DIER_UDE;
    TIM15 -> PSC = 1000 - 1;
    TIM15 -> ARR = 48 - 1;
    TIM15 -> CR1 |= TIM_CR1_CEN;
}

//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
void init_spi2(void) {
    RCC -> AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC -> APB1ENR |= RCC_APB1ENR_SPI2EN;

    GPIOB -> MODER &= ~0xCF000000;
    GPIOB -> MODER |= 0x8A000000; //AF
    //Ensure that the CR1_SPE bit is clear. Many of the bits set in the control registers require that the SPI channel is not enabled.
    SPI2 -> CR1 &= ~SPI_CR1_SPE;
    //Set the baud rate as low as possible (maximum divisor for BR).
    SPI2 -> CR1 |= 111 << 3;
    //Configure the interface for a 16-bit word size.
    //Set the SS Output enable bit and enable NSSP.
    //Set the TXDMAEN bit to enable DMA transfers on transmit buffer empty
    SPI2 -> CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0 | SPI_CR2_SSOE | SPI_CR2_NSSP | SPI_CR2_TXDMAEN;
    //Configure the SPI channel to be in “master mode”.
    SPI2 -> CR1 |= SPI_CR1_MSTR;
    //Enable the SPI channel.
    SPI2 -> CR1 |= SPI_CR1_SPE;
}

//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.
//===========================================================================
void spi2_setup_dma(void) {
    setup_dma();
    SPI2->CR2 |= SPI_CR2_TXDMAEN; // Transfer register empty DMA enable
}

//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void) {
    enable_dma();
}

//===========================================================================
// 4.4 SPI OLED Display
//===========================================================================
void init_spi1() {
    RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC -> APB2ENR |= RCC_APB2ENR_SPI1EN;
    // PA5  SPI1_SCK
    // PA6  SPI1_MISO
    // PA7  SPI1_MOSI
    // PA15 SPI1_NSS
    GPIOA -> MODER &= ~0xC000FC00;
    GPIOA -> MODER |= 0x8000A800;

    //Ensure that the CR1_SPE bit is clear. Many of the bits set in the control registers require that the SPI channel is not enabled.
    SPI1 -> CR1 &= ~SPI_CR1_SPE;
    //Set the baud rate as low as possible (maximum divisor for BR).
    SPI1 -> CR1 |= 111 << 3;
    //Configure the interface for a 16-bit word size.
    //Set the SS Output enable bit and enable NSSP.
    //Set the TXDMAEN bit to enable DMA transfers on transmit buffer empty
    SPI1 -> CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_0 | SPI_CR2_SSOE | SPI_CR2_NSSP | SPI_CR2_TXDMAEN;
    //Configure the SPI channel to be in “master mode”.
    SPI1 -> CR1 |= SPI_CR1_MSTR;
    //Enable the SPI channel.
    SPI1 -> CR1 |= SPI_CR1_SPE;
}

void spi_cmd(unsigned int data) {
    while(!(SPI1->SR & SPI_SR_TXE)) {}
    SPI1->DR = data;
}
void spi_data(unsigned int data) {
    spi_cmd(data | 0x200);
}
void spi1_init_oled() {
    nano_wait(1000000);
    spi_cmd(0x38);
    spi_cmd(0x08);
    spi_cmd(0x01);
    nano_wait(2000000);
    spi_cmd(0x06);
    spi_cmd(0x02);
    spi_cmd(0x0c);
}
void spi1_display1(const char *string) {
    spi_cmd(0x02);
    while(*string != '\0') {
        spi_data(*string);
        string++;
    }
}
void spi1_display2(const char *string) {
    spi_cmd(0xc0);
    while(*string != '\0') {
        spi_data(*string);
        string++;
    }
}

//===========================================================================
// This is the 34-entry buffer to be copied into SPI1.
// Each element is a 16-bit value that is either character data or a command.
// Element 0 is the command to set the cursor to the first position of line 1.
// The next 16 elements are 16 characters.
// Element 17 is the command to set the cursor to the first position of line 2.
//===========================================================================
uint16_t display[34] = {
        0x002, // Command to set the cursor at the first position line 1
        0x200+'E', 0x200+'C', 0x200+'E', 0x200+'3', 0x200+'6', + 0x200+'2', 0x200+' ', 0x200+'i',
        0x200+'s', 0x200+' ', 0x200+'t', 0x200+'h', + 0x200+'e', 0x200+' ', 0x200+' ', 0x200+' ',
        0x0c0, // Command to set the cursor at the first position line 2
        0x200+'c', 0x200+'l', 0x200+'a', 0x200+'s', 0x200+'s', + 0x200+' ', 0x200+'f', 0x200+'o',
        0x200+'r', 0x200+' ', 0x200+'y', 0x200+'o', + 0x200+'u', 0x200+'!', 0x200+' ', 0x200+' ',
};

//===========================================================================
// Configure the proper DMA channel to be triggered by SPI1_TX.
// Set the SPI1 peripheral to trigger a DMA when the transmitter is empty.
//===========================================================================
void spi1_setup_dma(void) {
    RCC -> AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel3 -> CCR &= ~DMA_CCR_EN;
    DMA1_Channel3 -> CPAR = &(SPI1 -> DR); //change
    DMA1_Channel3 -> CMAR = display; //change
    DMA1_Channel3 -> CNDTR = 34; //34?
    DMA1_Channel3 -> CCR |= DMA_CCR_DIR;
    DMA1_Channel3 -> CCR |= DMA_CCR_MINC;
    DMA1_Channel3 -> CCR &= ~DMA_CCR_MSIZE;
    DMA1_Channel3 -> CCR &= ~DMA_CCR_PSIZE;

    DMA1 -> RMPCR &= 0000 << 3; //change
    DMA1_Channel3 -> CCR |= 0x520; //mem size | peri size | circ mode
    spi1_enable_dma();
//It should circularly copy the 16-bit entries from the display[] array to the SPI1->DR address.

//You should also configure the SPI1 device to trigger a DMA operation when the transmitter is empty.
}

//===========================================================================
// Enable the DMA channel triggered by SPI1_TX.
//===========================================================================
void spi1_enable_dma(void) {
    DMA1_Channel3 -> CCR |= DMA_CCR_EN;
}

//===========================================================================
// Main function
//===========================================================================

int main(void) {
    msg[0] |= font['E'];
    msg[1] |= font['C'];
    msg[2] |= font['E'];
    msg[3] |= font[' '];
    msg[4] |= font['3'];
    msg[5] |= font['6'];
    msg[6] |= font['2'];
    msg[7] |= font[' '];

    // This time, autotest always runs as an invisible aid to you.
    autotest();

    // GPIO enable
    enable_ports();
    // setup keyboard
    init_tim7();

    // LED array Bit Bang
//#define BIT_BANG
#if defined(BIT_BANG)
    setup_bb();
    drive_bb();
#endif

    // Direct SPI peripheral to drive LED display
//#define SPI_LEDS
#if defined(SPI_LEDS)
    init_spi2();
    setup_dma();
    enable_dma();
    init_tim15();
    show_keys();
#endif

    // LED array SPI
//#define SPI_LEDS_DMA
#if defined(SPI_LEDS_DMA)
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    show_keys();
#endif

    // SPI OLED direct drive
//#define SPI_OLED
#if defined(SPI_OLED)
    init_spi1();
    spi1_init_oled();
    spi1_display1("Hello again,");
    spi1_display2(login);
#endif

    // SPI
//#define SPI_OLED_DMA
#if defined(SPI_OLED_DMA)
    init_spi1();
    spi1_init_oled();
    spi1_setup_dma();
    spi1_enable_dma();
#endif

    // Game on!  The goal is to score 100 points.
    game();
}
