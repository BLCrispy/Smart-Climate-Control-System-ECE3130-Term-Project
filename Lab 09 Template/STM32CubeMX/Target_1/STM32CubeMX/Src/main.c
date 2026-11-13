/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include "main.h"

// Sets up the system clock
void SystemClock_Config(void);

// Sets of the GPIO pins
static void MX_GPIO_Init(void);

// This lab will require a lot of programming, but most of what you'll need
//	is the function in the lecture slides! Slides 13-18 from Lecture 4 part 3
//	have all the code you need to work with the LCD, and slides 32-45 have
//	all the code you need to work with the keypad


// =============================================================================
// MACROS
// =============================================================================
#define ADC_CH_TEMP   1   // PC0 -> IN1
#define ADC_CH_LIGHT  2   // PC1 -> IN2
#define ADC_CH_POT    3   // PC2 -> IN3
#define VREF_MV 3300U   // assumes the Nucleo's 3.3 V supply is the ADC reference


// =============================================================================
// Function Prototypes
// =============================================================================

// LCD Functions
void LCD_Init();
void Write_SR_LCD(uint8_t temp);
void LCD_nibble_write(uint8_t temp, uint8_t s);
void Write_Char_LCD(uint8_t code);
void Write_Instr_LCD(uint8_t code);
void Write_String_LCD(char *temp);

// Keypad Functions
void Keypad_Init();
uint8_t Read_Keypad();

// Buzzer Functions
static void buzzer_init(uint32_t freq_hz);
static void buzzer_on(void)  { TIM3->CR1 |=  TIM_CR1_CEN; }
static void buzzer_off(void) { TIM3->CR1 &= ~TIM_CR1_CEN; }


// =============================================================================
// MAIN FUNCTION
// =============================================================================
// The main function, and the entry point of the program
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  LCD_Init();
  Keypad_Init();

  //buzzer initialization and test
  buzzer_init(1000);
  buzzer_on();  HAL_Delay(5000);
  buzzer_off(); HAL_Delay(5000);
  

  // TODO: Put any other initialization code here

  while (1)
  {
    // TODO: Put your main code here. It will run over and over again until the program is stopped
    
	}
}


// =============================================================================
// GPIO Initialization
// =============================================================================
// Set up the GPIO pins
static void MX_GPIO_Init(void)
{
  // **** Pins Already Intialized ****
  // LCD Pins
  // PB5, PA10, PA5
  // ----
  // Keypad Pins
  // PB1, PB2, PB3, PB4, PB6, PB7, PB8, PB9
  // ----

  // Clock GPIOA,GPIOB,GPIOC enabled
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN; 
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;

  // **** LED Initialization ****

  // Set LED pins as outputs, PA1 = LED0, PA0 = LED1, PC7 = LED2, PC8 = LED3
  GPIOA->MODER &= ~(0x03 << (2*1)); // Clear mode bits for PA1
  GPIOA->MODER &= ~(0x03 << (2*0)); // Clear mode bits for PA0
  GPIOC->MODER &= ~(0x03 << (2*7)); // Clear mode bits for PC7
  GPIOC->MODER &= ~(0x03 << (2*8)); // Clear mode bits for PC8

  GPIOA->MODER |= (0x01 << (2*1)); // Set PA1 as output
  GPIOA->MODER |= (0x01 << (2*0)); // Set PA0 as output
  GPIOC->MODER |= (0x01 << (2*7)); // Set PC7 as output
  GPIOC->MODER |= (0x01 << (2*8)); // Set PC8 as output

  GPIOA->OTYPER &= ~(0x01 << 1); // Set PA1 as push-pull
  GPIOA->OTYPER &= ~(0x01 << 0); // Set PA0 as push-pull
  GPIOC->OTYPER &= ~(0x01 << 7); // Set PC7 as push-pull
  GPIOC->OTYPER &= ~(0x01 << 8); // Set PC8 as push-pull


  // **** ADC Initialization ****

  // Enable ADC clock
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  (void)RCC->AHB2ENR;   // dummy read for clock delay

  // ADC clock = HCLK/1 (synchronous)
  ADC123_COMMON->CCR = (ADC123_COMMON->CCR & ~ADC_CCR_CKMODE)
                     | (0x01 << ADC_CCR_CKMODE_Pos);

  // Set ADC pin as analog input, PC0 = Temp_Sensor, PC1 = Light_Sensor, PC2 = Potentiometer
  GPIOC->MODER |= (0x03 << (2*0)); // Set PC0 as analog input
  GPIOC->MODER |= (0x03 << (2*1)); // Set PC1 as analog input
  GPIOC->MODER |= (0x03 << (2*2)); // Set PC2 as analog input

  // Set ADC input pins as no pull
  GPIOC->PUPDR &= ~(0x03 << (2*0)); // Set PC0 as no pull
  GPIOC->PUPDR &= ~(0x03 << (2*1)); // Set PC1 as no pull
  GPIOC->PUPDR &= ~(0x03 << (2*2)); // Set PC2 as no pull

  // Enable analog mode for ADC input pins
  GPIOC->ASCR  |=  (0x01 << 0);
  GPIOC->ASCR  |=  (0x01 << 1);
  GPIOC->ASCR  |=  (0x01 << 2);

  // Wake ADC: exit deep power-down mode, enable regulator, delay 20us to account for ADC startup time
  ADC1->CR &= ~ADC_CR_DEEPPWD;  // Exit deep power-down mode
  ADC1->CR |= ADC_CR_ADVREGEN;  // Enable regulator
  for (volatile uint32_t delay = 0; delay < 20000; delay++); // Wait 20us for ADC startup
  
  // Calibrate ADC
  ADC1->CR &= ~ADC_CR_ADCALDIF;     // Sets single-ended calibration, not differential
  ADC1->CR |= ADC_CR_ADCAL;         // Starts calibration
  while (ADC1->CR & ADC_CR_ADCAL);  // Wait for calibration to complete

  // Configure ADC: 12-bit, right-aligned, single conversion
  ADC1->CFGR &= ~(ADC_CFGR_RES | ADC_CFGR_ALIGN | ADC_CFGR_CONT);

  // Sample time for channel (SMPR1 covers channels 0-9, 3 bits each)
  // 100 = 47.5 cycles for pot, temp, and light sensor
  ADC1->SMPR1 &= ~(0x07 << (3*0));
  ADC1->SMPR1 &= ~(0x07 << (3*1));
  ADC1->SMPR1 &= ~(0x07 << (3*2));
  ADC1->SMPR1 |=  (0x04 << (3*0));
  ADC1->SMPR1 |=  (0x04 << (3*1));
  ADC1->SMPR1 |=  (0x04 << (3*2));

  // Sequence of 3: temp (IN1), light (IN2), pot (IN3)
  ADC1->SQR1 = (0x02 << ADC_SQR1_L_Pos)      // L = 2 -> 3 conversions
             | (0x01 << ADC_SQR1_SQ1_Pos)    // 1st: IN1 (PC0, temp)
             | (0x02 << ADC_SQR1_SQ2_Pos)    // 2nd: IN2 (PC1, light)
             | (0x03 << ADC_SQR1_SQ3_Pos);   // 3rd: IN3 (PC2, pot)

  // Enable ADC
  ADC1->ISR |= ADC_ISR_ADRDY;              // clear flag (write 1)
  ADC1->CR  |= ADC_CR_ADEN;                // Power on ADC
  while (!(ADC1->ISR & ADC_ISR_ADRDY)) { } // Wait for ADC to be ready
}

// =============================================================================
// ADC Modules Functions
// =============================================================================
uint16_t adc_read_channel(uint32_t ch)
{
    // Single conversion (L = 0) of the requested channel
    ADC1->SQR1 = (ch << ADC_SQR1_SQ1_Pos);

    ADC1->CR |= ADC_CR_ADSTART;               // start conversion
    while (!(ADC1->ISR & ADC_ISR_EOC));       // wait for end of conversion

    return (uint16_t)ADC1->DR;                // reading DR clears EOC
}

uint32_t read_pot_percent(void) // Read potentiometer in percent (0 to 100%)
{
    return (adc_read_channel(ADC_CH_POT) * 100U) / 4095U;
}

uint32_t read_light_percent(void) // Read light in percent (0 to 100%)
{
    return (adc_read_channel(ADC_CH_LIGHT) * 100U) / 4095U;        
}

int32_t read_temp_c(void) // Read temperature in Celsius
{
    uint32_t mv = (adc_read_channel(ADC_CH_TEMP) * VREF_MV) / 4095U;
    return ((int32_t)mv - 500) / 10;                 // (- 500) accounts for 500mV offset
}

int32_t read_temp_f(void) // Read temperature in Fahrenheit
{
    int32_t mv = ((int32_t)adc_read_channel(ADC_CH_TEMP) * VREF_MV) / 4095;
    return ((mv - 500) * 9) / 50 + 32;      // (mV - 500)/10 * 9/5 + 32
}


// =============================================================================
// BUZZER Functions
// =============================================================================

static void buzzer_init(uint32_t freq_hz)
{
    // Clocks: GPIOC and TIM3
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;

    // PC9 -> alternate function mode (MODER bits 19:18 = 10)
    GPIOC->MODER &= ~(3U << (9 * 2));
    GPIOC->MODER |=  (2U << (9 * 2));

    // AF2 (TIM3_CH4) on PC9: AFR[1], pin 9 -> bits 7:4
    GPIOC->AFR[1] &= ~(0xFU << ((9 - 8) * 4));
    GPIOC->AFR[1] |=  (2U   << ((9 - 8) * 4));

    // Timer: 4 MHz / (PSC+1) = 1 MHz tick
    TIM3->PSC = 3;
    TIM3->ARR = (1000000U / freq_hz) - 1;
    TIM3->CCR4 = TIM3->ARR / 2;               // 50% duty

    // CH4: PWM mode 1 (OC4M = 110), preload enable
    TIM3->CCMR2 &= ~(TIM_CCMR2_OC4M | TIM_CCMR2_CC4S);
    TIM3->CCMR2 |= (6U << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;

    TIM3->CCER |= TIM_CCER_CC4E;              // enable output
    TIM3->CR1  |= TIM_CR1_ARPE;
    TIM3->EGR  |= TIM_EGR_UG;                 // load registers
}


// =============================================================================
// LCD Functions
// =============================================================================

void LCD_Init()
	{
		uint32_t temp;
		
        // GPIOA & GPIOB already enabled
        /*PB5 MOSI, PA10 /CS_7 latch , PA5 shift clock */

			 
        /*PA5 and PA10 are outputs*/ temp = GPIOA->MODER;
        temp &= ~(0x03<<(2*5)); temp|=(0x01<<(2*5)); 
        temp &= ~(0x03<<(2*10)); temp|=(0x01<<(2*10)); 
        GPIOA->MODER = temp;
			 
        temp=GPIOA->OTYPER;
        temp &=~(0x01<<5);
        temp &=~(0x01<<10); GPIOA->OTYPER=temp;

        temp=GPIOA->PUPDR;
        temp&=~(0x03<<(2*5));
        temp&=~(0x03<<(2*10)); GPIOA->PUPDR=temp;

        /*PB5 is output*/

        temp = GPIOB->MODER;
        temp &= ~(0x03<<(2*5)); 
        temp|=(0x01<<(2*5)); 
        GPIOB->MODER = temp;

			 
        temp=GPIOB->OTYPER;
        temp &=~(0x01<<5); 
        GPIOB->OTYPER=temp;

            
        temp=GPIOB->PUPDR;
        temp&=~(0x03<<(2*5)); 
        GPIOB->PUPDR=temp;

		
		
        /* LCD controller reset sequence */ 
        HAL_Delay(20);
        LCD_nibble_write(0x30,0); 
        HAL_Delay(5); 
        LCD_nibble_write(0x30,0); 
        HAL_Delay(1); 
        LCD_nibble_write(0x30,0);
        HAL_Delay(1); 
        LCD_nibble_write(0x20,0); 
        HAL_Delay(1);

        Write_Instr_LCD(0x28); /* set 4 bit data LCD - two line display - 5x8 font*/ 
        Write_Instr_LCD(0x0E); /* turn on display, turn on cursor , turn off blinking */ 
        Write_Instr_LCD(0x01); /* clear display screen and return to home position*/ 
        Write_Instr_LCD(0x06); /* move cursor to right (entry mode set instruction)*/


}


void Write_SR_LCD(uint8_t temp) {
  uint8_t mask = 0b10000000;
  for (int i = 0; i < 8; i++) {
    if ((temp & mask) == 0) /* clear data pin */
      GPIOB->ODR &= ~(1 << 5);
    else /* set data pin */
      GPIOB->ODR |= (1 << 5);
    /* pulse SCLK: low, then high — shifts this bit in */
    GPIOA->ODR &= ~(1 << 5);
    HAL_Delay(1);
    GPIOA->ODR |= (1 << 5);
    HAL_Delay(1);
    mask = mask >> 1;
  }
  /* Latch: copy the shifted byte onto the LCD's data pins */
  GPIOA->ODR |= (1 << 10);
  GPIOA->ODR &= ~(1 << 10);
}


void LCD_nibble_write(uint8_t temp, uint8_t s) {
  if (s == 0) { /* s=0 → writing a COMMAND */
    temp = temp & 0xF0;
    temp = temp | 0x02; /* RS=0, EN=high */
    Write_SR_LCD(temp);
    temp = temp & 0xFD; /* EN=low → LATCHES data */
    Write_SR_LCD(temp);
  } else if (s == 1) { /* s=1 → writing DATA (a character) */
    temp = temp & 0xF0;
    temp = temp | 0x03; /* RS=1, EN=high */
    Write_SR_LCD(temp);
    temp = temp & 0xFD; /* EN=low */
    Write_SR_LCD(temp);
  }
}


void Write_Char_LCD(uint8_t code) { /* displays a character */
  LCD_nibble_write(code & 0xF0, 1); /* high nibble, s=1 (data) */
  code = code << 4;
  LCD_nibble_write(code, 1); /* low nibble */
}


void Write_Instr_LCD(uint8_t code) { /* executes a command */
  LCD_nibble_write(code & 0xF0, 0);  /* s=0 (command) */
  code = code << 4;
  LCD_nibble_write(code, 0);
}


void Write_String_LCD(char *temp) { /* displays a whole string */
  int i = 0;
  while (temp[i] != 0) {
    Write_Char_LCD(temp[i]);
    i = i + 1;
  }
}


// =============================================================================
// Keypad Functions
// =============================================================================

void Keypad_Init()
	{
	uint32_t temp;
  // GPIOB already enabled

	/*configure input*/
	/* row0 to 3 are PB8, PB9, PB10, PB11*/
	temp = GPIOB->MODER;
	temp &= ~(0x03<<(2*11)); 
	temp &= ~(0x03<<(2*10)); temp &= ~(0x03<<(2*9)); 
	temp &= ~(0x03<<(2*8)); 
	GPIOB->MODER = temp;
	temp=GPIOB->OTYPER;
	temp &=~(0x01<<11); 
	temp &=~(0x01<<10); 
	temp &=~(0x01<<9); 
	temp &=~(0x01<<8); 
	GPIOB->OTYPER=temp;

	temp=GPIOB->PUPDR;
	temp&=~(0x03<<(2*11)); 
	temp&=~(0x03<<(2*10)); 
	temp&=~(0x03<<(2*9)); 
	temp&=~(0x03<<(2*8)); 
	GPIOB->PUPDR=temp;

	/* Col 0 to 3 are PB1, PB2, PB3, PB4*/
	/*configure output*/ 
	temp = GPIOB->MODER;
	temp &= ~(0x03<<(2*1)); 
	temp|=(0x01<<(2*1)); 
	temp &= ~(0x03<<(2*2)); 
	temp|=(0x01<<(2*2)); 
	temp &= ~(0x03<<(2*3)); 
	temp|=(0x01<<(2*3));
	temp &= ~(0x03<<(2*4)); 
	temp|=(0x01<<(2*4));
	GPIOB->MODER = temp;

	temp=GPIOB->OTYPER;
	temp &=~(0x01<<1); 
	temp &=~(0x01<<2); 
	temp &=~(0x01<<3); 
	temp &=~(0x01<<4); 
	GPIOB->OTYPER=temp;

	temp=GPIOB->PUPDR;
	temp&=~(0x03<<(2*1));
	temp&=~(0x03<<(2*2)); 
	temp&=~(0x03<<(2*3)); 
	temp&=~(0x03<<(2*4));
	GPIOB->PUPDR=temp;

}


uint8_t Read_Keypad() {

  /* All colomns are zeros col0  PB1  - col1  PB2 - col2 PB3   col3  PB4*/

  uint8_t a;
  /*set all columns high and wait until a putton is pressed*/
  GPIOB->ODR |= (1 << 1);
  GPIOB->ODR |= (1 << 2);
  GPIOB->ODR |= (1 << 3);
  GPIOB->ODR |= (1 << 4);

  while ((GPIOB->IDR & (0x1 << 8)) == 0 && (GPIOB->IDR & (0x1 << 9)) == 0 &&
         (GPIOB->IDR & (0x1 << 10)) == 0 && (GPIOB->IDR & (0x1 << 11)) == 0) {
  }

  HAL_Delay(25); /*debouncing*/
                 /*scanning */
  while (1) {
    GPIOB->ODR &= ~(1 << 1);
    GPIOB->ODR &= ~(1 << 2);
    GPIOB->ODR &= ~(1 << 3);
    GPIOB->ODR &= ~(1 << 4);

    /* Scan Col 0 – PB1 = high*/
    /* check rows */

    GPIOB->ODR |= (1 << 1);
    HAL_Delay(2);

    /* check rows*/

    if ((GPIOB->IDR & (0x1 << 8)) != 0) {
      a = 1;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 9)) != 0) {
      a = 4;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 10)) != 0) {
      a = 7;
      break;
    }

    if ((GPIOB->IDR & (0x1 << 11)) != 0) {
      a = 14;
      break;
    }

    /* Scan Col 1 */

    GPIOB->ODR &= ~(1 << 1);
    HAL_Delay(2);
    GPIOB->ODR |= (1 << 2);
    HAL_Delay(2);
    if ((GPIOB->IDR & (0x1 << 8)) != 0) {
      a = 2;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 9)) != 0) {
      a = 5;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 10)) != 0) {
      a = 8;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 11)) != 0) {
      a = 0;
      break;
    }

    /* Scan Col 2 */
    GPIOB->ODR &= ~(1 << 2);
    HAL_Delay(2);
    GPIOB->ODR |= (1 << 3);
    HAL_Delay(2);
    if ((GPIOB->IDR & (0x1 << 8)) != 0) {
      a = 3;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 9)) != 0) {
      a = 6;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 10)) != 0) {
      a = 9;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 11)) != 0) {
      a = 15;
      break;
    }

    /* Scan Col 3 */
    GPIOB->ODR &= ~(1 << 3);
    HAL_Delay(2);
    GPIOB->ODR |= (1 << 4);
    HAL_Delay(2);

    if ((GPIOB->IDR & (0x1 << 8)) != 0) {
      a = 10;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 9)) != 0) {
      a = 11;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 10)) != 0) {
      a = 12;
      break;
    }
    if ((GPIOB->IDR & (0x1 << 11)) != 0) {
      a = 13;
      break;
    }
  }
  /*wait until button is released*/
  GPIOB->ODR |= (1 << 1);
  HAL_Delay(2);
  GPIOB->ODR |= (1 << 2);
  HAL_Delay(2);
  GPIOB->ODR |= (1 << 3);
  HAL_Delay(2);
  GPIOB->ODR |= (1 << 4);
  HAL_Delay(2);

  while (!(
      ((GPIOB->IDR & (0x1 << 8)) == 0) && ((GPIOB->IDR & (0x1 << 9)) == 0) &&
      ((GPIOB->IDR & (0x1 << 10)) == 0) && ((GPIOB->IDR & (0x1 << 11)) == 0))) {
  }
  HAL_Delay(25);
  return (a);
}


// Set up the system clock
// ! IMPORTANT ! This function is generated by STM32CubeMX and should not be modified
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // Configure the main internal regulator output voltage
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  // Initializes the RCC Oscillators according to the specified parameters
  //  in the RCC_OscInitTypeDef structure.
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  // Initializes the CPU, AHB and APB buses clocks
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}



// This function is called whenever there is an error, such as illegal memory access
void Error_Handler(void)
{
  // We don't really need to do anything for errors, so just stay here forever
  __disable_irq();
  while (1)
  {
  }
}



// ! IMPORTANT ! All code below this line is generated by STM32CubeMX and should not be modified
#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
