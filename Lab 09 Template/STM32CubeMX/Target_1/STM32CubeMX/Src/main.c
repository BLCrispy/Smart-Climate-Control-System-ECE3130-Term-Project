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
//	is the function in the lecture slides! Slides 13-18 from Lecture 4 part
// 3 	have all the code you need to work with the LCD, and slides 32-45 have
// all the code you need to work with the keypad

// =============================================================================
// MACROS
// =============================================================================
#define ADC_CH_TEMP 1  // PC0 -> IN1 -> LM45 temperature sensor
#define ADC_CH_LIGHT 2 // PC1 -> IN2 -> Light sensor
#define ADC_CH_POT 3   // PC2 -> IN3 -> Potentiometer
#define VREF_MV 3300   // assumes the Nucleo's 3.3 V supply is the ADC reference
#define TEMP_OFFSET_MV 0 // LM45 temperature sensor offset in mV
#define PWM_ARR 160      // 4 MHz / 400 = 10 kHz

// PA1 = LED0, PA0 = LED1, PC7 = LED2, PC8 = LED3
#define LED0_PIN 1
#define LED1_PIN 0
#define LED2_PIN 7
#define LED3_PIN 8

#define BAUD_RATE 115200U

// Define the main modes for the system
#define MODE_AUTO 0
#define MODE_MANUAL 1
#define MODE_OFF 2

// Defines for 7seg display segments
#define SEG_BLANK 10
#define SEG_F 11

// Defines for keypad keys
#define KEY_NONE 0xFF // 0 is a real key, so "no key" can't be 0
#define KEY_A 10
#define KEY_B 11
#define KEY_C 12
#define KEY_D 13
#define KEY_STAR '*'
#define KEY_HASH '#'

// =============================================================================
// GLOBAL VARIABLES (for interrupts)
// =============================================================================
volatile uint8_t mute = 0;
volatile uint8_t mode = MODE_AUTO; // Default mode is auto

volatile uint8_t seg_digits[4] = {SEG_BLANK, SEG_BLANK, 0, 0};
volatile uint8_t seg_dp = 0; // bit i set = DP on for digit i

// keypad mapping: columns are outputs, rows are inputs
static const uint8_t keymap[4][4] = {{'1', '2', '3', 'A'},
                                     {'4', '5', '6', 'B'},
                                     {'7', '8', '9', 'C'},
                                     {'*', '0', '#', 'D'}};

// =============================================================================
// Function Prototypes
// =============================================================================

// *** NOTE: Function Prototypes sections are organized in same order as
// functions are implemented below main function to make it easier to find them
// ***

// LED Functions
void GPIO_LED_On(uint8_t pin);
void GPIO_LED_Off(uint8_t pin);
void GPIO_LED_Toggle(uint8_t pin);

// ADC Functions
uint16_t ADC_read_channel(uint32_t ch); // Read a single ADC channel (0-18)
uint32_t Read_pot_percent(void);   // Read potentiometer in percent (0 to 100%)
uint32_t Read_light_percent(void); // Read light in percent (0 to 100%)
int32_t Read_temp_f_x10(void); // Read temperature in Fahrenheit, x10 (gives a
                               // decimal value, e.g., 723 = 72.3 F)

// Fan Functions
static void Fan_Init(void);
static void Fan_set_speed(uint8_t speed_percent);

// Buzzer Functions
static void Buzzer_Init(uint32_t freq_hz);
static void Buzzer_on(void);
static void Buzzer_off(void);

// Push Button Functions
void PushButtons_Init(void);
void EXTI15_10_IRQHandler(void);

// 7-segment Display Functions
void Seg7_Init(void);
void Write_SR_7S(uint8_t enable, uint8_t digit);
void Write_7Seg(uint8_t enable, uint8_t digit, uint8_t decimal);
void Seg7_SetTempF1(int32_t t_x10); // Show a temperature in tenths of a degree
                                    // F as e.g. "72.3F" on 7seg display
static void
Seg7_Refresh(void); // Refresh the 7-segment display (one digit per call)

// UART Functions
void UART2_Init(void);
void UART2_SendChar(char c);
void UART2_SendString(const char *s);
void UART2_SendUInt(uint32_t value);
void UART2_SendInt(int32_t value);
void send_sensor_report(void);

// LCD Functions
void LCD_Init();
void Write_SR_LCD(uint8_t temp);
void LCD_nibble_write(uint8_t temp, uint8_t s);
void Write_Char_LCD(uint8_t code);
void Write_Instr_LCD(uint8_t code);
void Write_String_LCD(char *temp);

// Keypad Functions
void Keypad_Init();
uint8_t Keypad_Scan(void);

// =============================================================================
// MAIN FUNCTION
// =============================================================================
// The main function, and the entry point of the program
int main(void) {
  // Initialize all necessary peripherals and modules
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  LCD_Init();
  Keypad_Init();
  Fan_Init();
  PushButtons_Init();
  Seg7_Init();
  UART2_Init();
  Buzzer_Init(1000);

  // Variable Initialization
  uint32_t fan_speed_percent = 0; // Fan speed in percent (0 to 100)
  uint32_t light_percent = 0;     // Light level in percent (0 to 100)
  uint32_t pot_percent = 0;       // Potentiometer level in percent (0 to 100)
  int32_t temp_f_x10 = 0; // Temperature in Fahrenheit, x10 (e.g., 723 = 72.3 F)

  // Variables to keep track of the last values read from sensors and displayed
  uint32_t last_seg = 0;
  uint32_t last_sensor = 0;
  uint32_t last_report = 0;

  uint8_t new_temp_setpoint_count =
      0; // Flag to indicate if a new temperature setpoint has being entered
  uint8_t new_temp_setpoint[2] = {
      0}; // Array to store the new temperature setpoint digits
  uint8_t temp_setpoint =
      75; // Variable to store the current temperature setpoint
  uint8_t entering_setpoint = 0; // 1 while a new setpoint is being typed
  uint8_t buzzer_muted = 0;

  int32_t delta_x10 = 0; // Variable to store the difference between the current
                         // temperature and the setpoint in tenths of a degree F
  uint8_t overtemp_flag = 0; // Flag to indicate if the temperature is above 10
                             // degrees F of the setpoint
  // LCD variables
  uint8_t lcd_mode = 0xFF;
  uint8_t lcd_set = 0xFF;
  uint8_t lcd_over = 0xFF; // 0xFF forces the first draw
  int32_t lcd_temp = 0xFF;
  uint8_t lcd_fan = 0xFF;

  Write_Instr_LCD(0x28);
  HAL_Delay(1); // 4-bit, 2 lines, 5x8 font
  Write_Instr_LCD(0x0C);
  HAL_Delay(1); // display on, cursor off
  Write_Instr_LCD(0x01);
  HAL_Delay(2); // clear display
  Write_Instr_LCD(0x06);
  HAL_Delay(1); // entry mode: cursor moves right

  // Main Program Loop
  while (1) {
    // TODO: Put your main code here. It will run over and over again until the
    // program is stopped
    uint32_t now = HAL_GetTick();

    // Keep the 7seg display alive & scan keypad: one digit every 2 ms
    if ((now - last_seg) >= 2) {
      last_seg = now;
      Seg7_Refresh();

      uint8_t key = Keypad_Scan();
      if (key != KEY_NONE) {
        if (key == KEY_STAR) { // start (or restart) entry
          Write_Instr_LCD(0xC0);
          Write_String_LCD("                ");
          Write_Instr_LCD(0xC0);
          Write_String_LCD("New Set: ");
          entering_setpoint = 1;
          new_temp_setpoint_count = 0;
        } else if (entering_setpoint && key >= '0' && key <= '9') {
          if (new_temp_setpoint_count < 2) { // ignore a 3rd digit
            Write_Char_LCD(key);
            new_temp_setpoint[new_temp_setpoint_count++] = key - '0';
          }
        } else if (entering_setpoint && key == KEY_HASH) {
          if (new_temp_setpoint_count == 2) { // need both digits to save
            temp_setpoint = new_temp_setpoint[0] * 10 + new_temp_setpoint[1];
          }
          entering_setpoint = 0; // done (or cancelled if incomplete)
          new_temp_setpoint_count = 0;
          Write_Instr_LCD(0xC0); // wipe the bottom row
          Write_String_LCD("                ");
          Write_Instr_LCD(0xC0); // Put back at front of row
        }
      }
    }

    // Read sensors every 100 ms and update the 7-segment display with the
    // current temperature in Fahrenheit
    if ((now - last_sensor) >= 100) { // Read sensors every 100 ms
      last_sensor = now;

      // Read the potentiometer and light sensor values in percent
      light_percent = Read_light_percent();
      pot_percent = Read_pot_percent();

      // Update the 7-segment display with the current temperature in
      // Fahrenheit
      temp_f_x10 = Read_temp_f_x10();
      Seg7_SetTempF1(temp_f_x10);
    }

    // Send a sensor report over UART every 1 second
    if ((now - last_report) >= 1000) { // Send sensor report every 1 second
      last_report = now;
      send_sensor_report();
      GPIO_LED_Toggle(LED3_PIN); // Toggle LED3 to indicate system heartbeat
    }

    // If the mute flag is set, turn off the buzzer
    if (mute) {
      mute = 0;
      buzzer_muted = 1;
    }
    if (!overtemp_flag)
      buzzer_muted = 0; // re-arm once temperature recovers

    // Check the current mode and perform actions based on the mode
    switch (mode) {
    case MODE_AUTO:
      // Mode Auto functionality

      // For temp_f_x10 in tenths of a degree, and temp_setpoint in whole
      // degrees:
      delta_x10 = temp_f_x10 - (temp_setpoint * 10);

      if (delta_x10 <= 0) {
        fan_speed_percent = 0; // Turn fan off at or below setpoint
        overtemp_flag = 0;     // Clear the overtemp flag when temperature is at
                               // or below setpoint
      } else if (delta_x10 >= 100) { // If the temperature is 10°F or more above
                                     // the setpoint, set fan speed to 100%
        fan_speed_percent =
            100; // Cap at 100% when 10°F (100 tenths) or more above setpoint

        overtemp_flag = 1; // Set the overtemp flag to indicate that the
                           // temperature is above 10°F of the setpoint
      } else {
        fan_speed_percent = 50 + (delta_x10 * 50) / 100;
        overtemp_flag = 0; // Clear the overtemp flag when temperature is
                           // below 10°F of the setpoint
      }
      break;
    case MODE_MANUAL:
      // Mode Manual functionality

      // Set the fan speed based on the potentiometer value
      fan_speed_percent = pot_percent; // Set fan speed to potentiometer value

      delta_x10 = temp_f_x10 - (temp_setpoint * 10);

      if (delta_x10 >= 100) { // If the temperature is 10°F or more above the
                              // setpoint, turn on the buzzer
        overtemp_flag = 1;    // Set the overtemp flag to indicate that the
                              // temperature is above 10°F of the setpoint
      } else {
        overtemp_flag = 0; // Clear the overtemp flag when temperature is
                           // below 10°F of the setpoint
      }
      break;
    case MODE_OFF:
      // Mode Off functionality
      fan_speed_percent = 0;
      overtemp_flag = 0; // Clear the overtemp flag when the system is off
      break;
    }

    // Set fan LED if fan is on (fan speed > 0%)
    if (fan_speed_percent > 0) {
      GPIO_LED_On(LED2_PIN); // Turn on LED2 to indicate fan is on
    } else {
      GPIO_LED_Off(LED2_PIN);
    }

    // If the light level is 0%, set the fan speed to 40%
    if ((light_percent == 0) && (fan_speed_percent > 40)) {
      fan_speed_percent = 40; // Set fan speed to 40% if light level is 0%
      GPIO_LED_On(LED0_PIN);  // Turn on LED0 to indicate light level is 0%
    } else {
      GPIO_LED_Off(LED0_PIN); // Turn off LED0 if light level is not 0%
    }

    Fan_set_speed(fan_speed_percent); // Set fan speed

    // Determines LCD display and alarm function based on overtemp_flag status
    // (if temperature is above 10°F of the setpoint)
    if (overtemp_flag) {
      if (buzzer_muted)
        Buzzer_off();
      else
        Buzzer_on();
      GPIO_LED_On(LED1_PIN);
    } else {
      Buzzer_off();
      GPIO_LED_Off(LED1_PIN);
    }

    // Top row: redraw only when something it shows has changed
    uint8_t m = mode;
    if (m != lcd_mode || temp_setpoint != lcd_set ||
        overtemp_flag != lcd_over) {
      lcd_mode = m;
      lcd_set = temp_setpoint;
      lcd_over = overtemp_flag;

      Write_Instr_LCD(0x80);
      if (overtemp_flag) {
        Write_String_LCD("!! OVER TEMP !! ");
      } else {
        if (m == MODE_AUTO)
          Write_String_LCD("AUTO  SET:");
        else if (m == MODE_MANUAL)
          Write_String_LCD("MAN   SET:");
        else
          Write_String_LCD("OFF   SET:");
        Write_Char_LCD('0' + temp_setpoint / 10);
        Write_Char_LCD('0' + temp_setpoint % 10);
        Write_String_LCD(
            "F   "); // spaces cover leftovers from the warning text
      }
    }

    // Bottom row: current temp and fan speed (skipped while typing a setpoint)
    if (!entering_setpoint &&
        (temp_f_x10 != lcd_temp || fan_speed_percent != lcd_fan)) {
      lcd_temp = temp_f_x10;
      lcd_fan  = fan_speed_percent;

      int32_t t = temp_f_x10;
      if (t < 0)   t = 0;              // same limits as the 7-seg
      if (t > 999) t = 999;

      Write_Instr_LCD(0xC0);           // bottom row, column 0
      Write_String_LCD("T:");
      Write_Char_LCD(t >= 100 ? '0' + (t / 100) % 10 : ' ');   // tens digit, blank if under 10
      Write_Char_LCD('0' + (t / 10) % 10);                     // ones digit
      Write_Char_LCD('.');
      Write_Char_LCD('0' + (t % 10));                          // tenths digit
      Write_String_LCD("F FAN:");

      Write_Char_LCD(fan_speed_percent >= 100 ? '1' : ' ');            // hundreds
      Write_Char_LCD(fan_speed_percent >= 10 ? '0' + (fan_speed_percent / 10) % 10 : ' ');
      Write_Char_LCD('0' + fan_speed_percent % 10);                    // ones
      Write_Char_LCD('%');
    }
  }
}

// =============================================================================
// GPIO Initialization
// =============================================================================
// Set up the GPIO pins
static void MX_GPIO_Init(void) {
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
  GPIOA->MODER &= ~(0x03 << (2 * 1)); // Clear mode bits for PA1
  GPIOA->MODER &= ~(0x03 << (2 * 0)); // Clear mode bits for PA0
  GPIOC->MODER &= ~(0x03 << (2 * 7)); // Clear mode bits for PC7
  GPIOC->MODER &= ~(0x03 << (2 * 8)); // Clear mode bits for PC8

  GPIOA->MODER |= (0x01 << (2 * 1)); // Set PA1 as output
  GPIOA->MODER |= (0x01 << (2 * 0)); // Set PA0 as output
  GPIOC->MODER |= (0x01 << (2 * 7)); // Set PC7 as output
  GPIOC->MODER |= (0x01 << (2 * 8)); // Set PC8 as output

  GPIOA->OTYPER &= ~(0x01 << 1); // Set PA1 as push-pull
  GPIOA->OTYPER &= ~(0x01 << 0); // Set PA0 as push-pull
  GPIOC->OTYPER &= ~(0x01 << 7); // Set PC7 as push-pull
  GPIOC->OTYPER &= ~(0x01 << 8); // Set PC8 as push-pull

  // **** ADC Initialization ****

  // Enable ADC clock
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  (void)RCC->AHB2ENR; // dummy read for clock delay

  // ADC clock = HCLK/1 (synchronous)
  ADC123_COMMON->CCR =
      (ADC123_COMMON->CCR & ~ADC_CCR_CKMODE) | (0x01 << ADC_CCR_CKMODE_Pos);

  // Set ADC pin as analog input, PC0 = Temp_Sensor, PC1 = Light_Sensor, PC2 =
  // Potentiometer
  GPIOC->MODER |= (0x03 << (2 * 0)); // Set PC0 as analog input
  GPIOC->MODER |= (0x03 << (2 * 1)); // Set PC1 as analog input
  GPIOC->MODER |= (0x03 << (2 * 2)); // Set PC2 as analog input

  // Set ADC input pins as no pull
  GPIOC->PUPDR &= ~(0x03 << (2 * 0)); // Set PC0 as no pull
  GPIOC->PUPDR &= ~(0x03 << (2 * 1)); // Set PC1 as no pull
  GPIOC->PUPDR &= ~(0x03 << (2 * 2)); // Set PC2 as no pull

  // Enable analog mode for ADC input pins
  GPIOC->ASCR |= (0x01 << 0);
  GPIOC->ASCR |= (0x01 << 1);
  GPIOC->ASCR |= (0x01 << 2);

  // Wake ADC: exit deep power-down mode, enable regulator, delay 20us to
  // account for ADC startup time
  ADC1->CR &= ~ADC_CR_DEEPPWD; // Exit deep power-down mode
  ADC1->CR |= ADC_CR_ADVREGEN; // Enable regulator
  for (volatile uint32_t delay = 0; delay < 20000; delay++)
    ; // Wait 20us for ADC startup

  // Calibrate ADC
  ADC1->CR &=
      ~ADC_CR_ADCALDIF;     // Sets single-ended calibration, not differential
  ADC1->CR |= ADC_CR_ADCAL; // Starts calibration
  while (ADC1->CR & ADC_CR_ADCAL)
    ; // Wait for calibration to complete

  // Configure ADC: 12-bit, right-aligned, single conversion
  ADC1->CFGR &= ~(ADC_CFGR_RES | ADC_CFGR_ALIGN | ADC_CFGR_CONT);

  // Sample time for channel (SMPR1 covers channels 0-9, 3 bits each)
  // 100 = 47.5 cycles for pot, temp, and light sensor
  ADC1->SMPR1 &= ~(0x07 << (3 * 0));
  ADC1->SMPR1 &= ~(0x07 << (3 * 1));
  ADC1->SMPR1 &= ~(0x07 << (3 * 2));
  ADC1->SMPR1 |= (0x04 << (3 * 0));
  ADC1->SMPR1 |= (0x04 << (3 * 1));
  ADC1->SMPR1 |= (0x04 << (3 * 2));

  // Sequence of 3: temp (IN1), light (IN2), pot (IN3)
  ADC1->SQR1 = (0x02 << ADC_SQR1_L_Pos)      // L = 2 -> 3 conversions
               | (0x01 << ADC_SQR1_SQ1_Pos)  // 1st: IN1 (PC0, temp)
               | (0x02 << ADC_SQR1_SQ2_Pos)  // 2nd: IN2 (PC1, light)
               | (0x03 << ADC_SQR1_SQ3_Pos); // 3rd: IN3 (PC2, pot)

  // Enable ADC
  ADC1->ISR |= ADC_ISR_ADRDY; // clear flag (write 1)
  ADC1->CR |= ADC_CR_ADEN;    // Power on ADC
  while (!(ADC1->ISR & ADC_ISR_ADRDY)) {
  } // Wait for ADC to be ready
}

void GPIO_LED_On(uint8_t pin) {
  // Finds pin chosen and turns that LED on. PA1 = LED0, PA0 = LED1, PC7 =
  // LED2, PC8 = LED3
  if (pin == LED0_PIN) {
    GPIOA->ODR |= (1 << LED0_PIN);
  } else if (pin == LED1_PIN) {
    GPIOA->ODR |= (1 << LED1_PIN);
  } else if (pin == LED2_PIN) {
    GPIOC->ODR |= (1 << LED2_PIN);
  } else if (pin == LED3_PIN) {
    GPIOC->ODR |= (1 << LED3_PIN);
  }
}

void GPIO_LED_Off(uint8_t pin) {
  // Finds pin chosen and turns that LED off. PA1 = LED0, PA0 = LED1, PC7 =
  // LED2, PC8 = LED3
  if (pin == LED0_PIN) {
    GPIOA->ODR &= ~(1 << LED0_PIN);
  } else if (pin == LED1_PIN) {
    GPIOA->ODR &= ~(1 << LED1_PIN);
  } else if (pin == LED2_PIN) {
    GPIOC->ODR &= ~(1 << LED2_PIN);
  } else if (pin == LED3_PIN) {
    GPIOC->ODR &= ~(1 << LED3_PIN);
  }
}

void GPIO_LED_Toggle(uint8_t pin) {
  // Finds pin chosen and toggles that LED. PA1 = LED0, PA0 = LED1, PC7 =
  // LED2, PC8 = LED3
  if (pin == LED0_PIN) {
    GPIOA->ODR ^= (1 << LED0_PIN);
  } else if (pin == LED1_PIN) {
    GPIOA->ODR ^= (1 << LED1_PIN);
  } else if (pin == LED2_PIN) {
    GPIOC->ODR ^= (1 << LED2_PIN);
  } else if (pin == LED3_PIN) {
    GPIOC->ODR ^= (1 << LED3_PIN);
  }
}

// =============================================================================
// ADC Modules Functions
// =============================================================================
uint16_t ADC_read_channel(uint32_t ch) { // Read a single ADC channel (0-18)
  // Single conversion (L = 0) of the requested channel
  ADC1->SQR1 = (ch << ADC_SQR1_SQ1_Pos);

  ADC1->CR |= ADC_CR_ADSTART; // start conversion
  while (!(ADC1->ISR & ADC_ISR_EOC))
    ; // wait for end of conversion

  return (uint16_t)ADC1->DR; // reading DR clears EOC
}

uint32_t Read_pot_percent(void) // Read potentiometer in percent (0 to 100%)
{
  return (ADC_read_channel(ADC_CH_POT) * 100) / 4095;
}

uint32_t Read_light_percent(void) // Read light in percent (0 to 100%)
{
  return (ADC_read_channel(ADC_CH_LIGHT) * 100) / 4095;
}

int32_t Read_temp_f_x10(void) { // Read temperature in Fahrenheit, x10 (gives
                                // a decimal value, e.g., 723 = 72.3 F)
  // Convert the raw count to millivolts, rounded to the nearest mV
  int32_t mv =
      (int32_t)(((uint32_t)ADC_read_channel(ADC_CH_TEMP) * VREF_MV + 2047) /
                4095);

  // Tenths of a degree C: 1 mV per 0.1 C, minus the LM45's 500 mV offset
  int32_t c_x10 = mv - TEMP_OFFSET_MV;

  // F = C * 9/5 + 32, in tenths (the +/-2 rounds the divide by 5)
  return (c_x10 * 9 + (c_x10 >= 0 ? 2 : -2)) / 5 + 320;
}

// =============================================================================
// FAN Functions
// =============================================================================

static void Fan_Init(void) { // Initialize TIM1 for PWM output on PA8
                             // (TIM1_CH1) for fan speed control
  // Enable TIM1 clock
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  (void)RCC->APB2ENR; // dummy read for clock delay

  // Pin PA8: alternate function, AF1 (TIM1_CH1)
  GPIOA->MODER &= ~(3U << (8 * 2));
  GPIOA->MODER |= (2U << (8 * 2)); // 10 = alternate function
  GPIOA->AFR[1] &= ~(0xFU << ((8 - 8) * 4));
  GPIOA->AFR[1] |= (1U << ((8 - 8) * 4)); // AF1

  // --- TIM1 time base ---
  TIM1->PSC = 0;       // timer clock = 4 MHz
  TIM1->ARR = PWM_ARR; // period = 400 ticks = 10 kHz
  TIM1->CCR1 = 0;      // start with fan off

  // CH1: PWM mode 1, preload enabled
  TIM1->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S);
  TIM1->CCMR1 |= (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;

  TIM1->CCER |= TIM_CCER_CC1E; // enable CH1 output
  TIM1->CR1 |= TIM_CR1_ARPE;   // preload the period
  TIM1->BDTR |= TIM_BDTR_MOE;  // main output enable (required on TIM1)

  TIM1->EGR |= TIM_EGR_UG;  // load PSC, ARR, CCR1
  TIM1->CR1 |= TIM_CR1_CEN; // start the timer
}

static void
Fan_set_speed(uint8_t speed_percent) // Set fan speed in percent (0 to 100%)
{
  if (speed_percent > 100)
    speed_percent = 100;
  TIM1->CCR1 = (PWM_ARR * speed_percent) / 100U; // set duty cycle
}

// =============================================================================
// BUZZER Functions
// =============================================================================

static void Buzzer_Init(uint32_t freq_hz) {
  // TIM3 clock = 4 MHz, so period = 4 MHz / freq_hz
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;

  // PC9 -> alternate function mode (MODER bits 19:18 = 10)
  GPIOC->MODER &= ~(3U << (9 * 2));
  GPIOC->MODER |= (2U << (9 * 2));

  // AF2 (TIM3_CH4) on PC9: AFR[1], pin 9 -> bits 7:4
  GPIOC->AFR[1] &= ~(0xFU << ((9 - 8) * 4));
  GPIOC->AFR[1] |= (2U << ((9 - 8) * 4));

  // Timer: 4 MHz / (PSC+1) = 1 MHz tick
  TIM3->PSC = 3;
  TIM3->ARR = (1000000U / freq_hz) - 1;
  TIM3->CCR4 = TIM3->ARR / 2; // 50% duty

  // CH4: PWM mode 1 (OC4M = 110), preload enable
  TIM3->CCMR2 &= ~(TIM_CCMR2_OC4M | TIM_CCMR2_CC4S);
  TIM3->CCMR2 |= (6U << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;

  TIM3->CCER |= TIM_CCER_CC4E; // enable output
  TIM3->CR1 |= TIM_CR1_ARPE;
  TIM3->EGR |= TIM_EGR_UG; // load registers
}

static void Buzzer_on(void) {
  TIM3->CR1 |= TIM_CR1_CEN; // Start the timer to turn on the buzzer
}

static void Buzzer_off(void) {
  TIM3->CR1 &= ~TIM_CR1_CEN; // Stop the timer to turn off the buzzer
}

// =============================================================================
// Push Button Functions
// =============================================================================

void PushButtons_Init(void) {
  // SYSCFG (needed for EXTI routing)
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  (void)RCC->APB2ENR;

  // PC12 and PC13: input mode (00) with pull-up (01)
  GPIOC->MODER &= ~((3U << (12 * 2)) | (3U << (13 * 2)));
  GPIOC->PUPDR &= ~((3U << (12 * 2)) | (3U << (13 * 2)));
  GPIOC->PUPDR |= ((1U << (12 * 2)) | (1U << (13 * 2)));

  // Route EXTI12 and EXTI13 to port C (code 2)
  // Pins 12-15 are in EXTICR[3]: pin 12 at bits 3:0, pin 13 at bits 7:4
  SYSCFG->EXTICR[3] &= ~((0xFU << 0) | (0xFU << 4));
  SYSCFG->EXTICR[3] |= ((2U << 0) | (2U << 4));

  // Falling edge only
  EXTI->FTSR1 |= (1U << 12) | (1U << 13);
  EXTI->RTSR1 &= ~((1U << 12) | (1U << 13));

  // Clear stale pending flags, then unmask both lines
  EXTI->PR1 = (1U << 12) | (1U << 13);
  EXTI->IMR1 |= (1U << 12) | (1U << 13);

  // Both lines share one NVIC interrupt
  NVIC_SetPriority(EXTI15_10_IRQn, 1);
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}

// ---- Interrupt handler (name must match the startup file exactly) ----
void EXTI15_10_IRQHandler(void) {
  static uint32_t last_pc13 = 0, last_pc12 = 0;
  uint32_t now = HAL_GetTick();

  if (EXTI->PR1 & (1U << 13)) {
    EXTI->PR1 = (1U << 13);       // write 1 to clear
    if ((now - last_pc13) > 150) { // debounce
      last_pc13 = now;
      mute = 1; // Set mute flag when button on PC13 is pressed
    }
  }
  if (EXTI->PR1 & (1U << 12)) {
    EXTI->PR1 = (1U << 12);
    if ((now - last_pc12) > 150) { // debounce
      last_pc12 = now;
      // Cycle through modes: 0 -> 1 -> 2 -> 0
      // 0 = MODE_AUTO, 1 = MODE_MANUAL, 2 = MODE_OFF
      if (mode == MODE_OFF) {
        mode = MODE_AUTO; // Switch to auto mode
      } else {
        mode++; // Cycle through modes
      }
    }
  }
}

// =============================================================================
// 7seg Display Functions
// =============================================================================

void Seg7_Init(void) {
  // LCD_Init already initializes PA5 and PB5, so we only need to initialize
  // PC10 for the latch

  // PA5 = SCLK, PB5 = DATA, PC10 = Seg7_LATCH
  GPIOC->MODER &= ~(3U << (2 * 10)); // Clear mode bits for PC10
  GPIOC->MODER |= (1U << (2 * 10));  // Set PC10 as output
  GPIOC->OTYPER &= ~(1U << 10);      // Set PC10 as push-pull
  GPIOC->PUPDR &= ~(3U << (2 * 10)); // Set PC10 as no pull

  /* 4. Blank: all digit enables off (0x00), all segments off (0xFF, common
   * anode) */
  Write_SR_7S(0x00, 0xFF);
}

void Write_SR_7S(uint8_t enable, uint8_t digit) {
  uint8_t mask = 0b10000000;
  for (int i = 0; i < 8; i++) {
    if ((digit & mask) == 0)
      GPIOB->ODR &= ~(1 << 5); /* data = 0 */
    else
      GPIOB->ODR |= (1 << 5); /* data = 1 */
    GPIOA->ODR &= ~(1 << 5);
    GPIOA->ODR |= (1 << 5);

    mask = mask >> 1;
  }
  /* ... repeat the same loop for the "enable" byte ... */
  mask = 0b10000000;
  for (int i = 0; i < 8; i++) {
    if ((enable & mask) == 0)
      GPIOB->ODR &= ~(1 << 5);
    else
      GPIOB->ODR |= (1 << 5);
    /* Sclck */
    GPIOA->ODR &= ~(1 << 5);
    /*Delay(1);*/
    GPIOA->ODR |= (1 << 5);
    /*Delay(1); */
    mask = mask >> 1;
  }

  /* Latch: copy shifted bits to the outputs */
  GPIOC->ODR |= (1 << 10);
  GPIOC->ODR &= ~(1 << 10);
}

void Write_7Seg(uint8_t enable, uint8_t digit, uint8_t decimal) {
  uint8_t Enable[5] = {0x00, 0x08, 0x04, 0x02, 0x01};
  /* one-hot per display */
  static const uint8_t Digit[12] = {
      0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92,
      0x82, 0xF8, 0x80, 0x90, 0xFF, 0x8E}; // note: 0xFF = blank, 0x8E = F

  // If at a decimal point, clear bit 7 of the digit code to turn on the
  // decimal point
  uint8_t code = Digit[digit];
  if (decimal)
    code &= 0x7F; /* clear bit 7 = DP on */

  Write_SR_7S(Enable[enable], code);
}

// Show a temperature in tenths of a degree F as e.g. "72.3F"
void Seg7_SetTempF1(int32_t t_x10) {
  if (t_x10 < 0)
    t_x10 = 0; // negatives not handled
  if (t_x10 > 999)
    t_x10 = 999; // three digits max: 99.9
  uint16_t v = (uint16_t)t_x10;

  // Set the digits for the 7-segment display
  seg_digits[0] =
      (v >= 100) ? (v / 100) : SEG_BLANK; // tens (blank if under 10.0)
  seg_digits[1] = (v / 10) % 10;          // ones, shown with the point
  seg_digits[2] = v % 10;                 // tenths
  seg_digits[3] = SEG_F;
  seg_dp = (1 << 1); // DP on digit index 1
}

// Refresh
static void Seg7_Refresh(void) { // one digit per call
  static uint8_t pos = 0;
  Write_7Seg(pos + 1, seg_digits[pos], (seg_dp >> pos) & 1);
  pos = (pos + 1) & 3;
}

// =============================================================================
// UART Functions
// =============================================================================

void UART2_Init(void) {
  // Clocks
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
  (void)RCC->APB1ENR1; // dummy read for clock delay

  // PA2 (TX) and PA3 (RX): alternate function mode (10)
  GPIOA->MODER &= ~((3U << (2 * 2)) | (3U << (3 * 2)));
  GPIOA->MODER |= ((2U << (2 * 2)) | (2U << (3 * 2)));

  // AF7 for both: AFR[0] holds pins 0-7, 4 bits each
  GPIOA->AFR[0] &= ~((0xFU << (2 * 4)) | (0xFU << (3 * 4)));
  GPIOA->AFR[0] |= ((7U << (2 * 4)) | (7U << (3 * 4)));

  // Optional: pull-up on RX so the line idles high
  GPIOA->PUPDR &= ~(3U << (3 * 2));
  GPIOA->PUPDR |= (1U << (3 * 2));

  // Baud rate: 16x oversampling, USART2 clocked from PCLK1
  USART2->CR1 = 0;                           // UE off while configuring
  USART2->BRR = SystemCoreClock / BAUD_RATE; // assumes APB1 prescaler = 1

  // 8 data bits, no parity, 1 stop bit are the reset defaults
  USART2->CR1 |= USART_CR1_TE | USART_CR1_RE; // enable TX and RX
  USART2->CR1 |= USART_CR1_UE;                // enable USART
}

void UART2_SendChar(char c) {
  while (!(USART2->ISR & USART_ISR_TXE))
    ; // wait until TDR is empty
  USART2->TDR = (uint8_t)c;
}

void UART2_SendString(const char *s) {
  while (*s) {
    UART2_SendChar(*s++);
  }
  while (!(USART2->ISR & USART_ISR_TC))
    ; // wait for the last byte to finish
}

// Send an unsigned number in decimal
void UART2_SendUInt(uint32_t value) {
  char digits[10]; // max 10 digits for a 32-bit value
  int n = 0;

  if (value == 0) {
    UART2_SendChar('0');
    return;
  }

  while (value > 0) { // digits come out last-to-first
    digits[n++] = '0' + (value % 10);
    value /= 10;
  }
  while (n > 0) { // send them back in the right order
    UART2_SendChar(digits[--n]);
  }
}

// Send a signed number in decimal
void UART2_SendInt(int32_t value) {
  if (value < 0) {
    UART2_SendChar('-');
    UART2_SendUInt((uint32_t)(-(value + 1)) + 1U); // safe for INT32_MIN
  } else {
    UART2_SendUInt((uint32_t)value);
  }
}

// Send a value that is stored x10 as a decimal, e.g. 723 -> "72.3"
void UART2_SendFixed1(int32_t value_x10) {
  if (value_x10 < 0) {
    UART2_SendChar('-');
    value_x10 = -value_x10;
  }
  UART2_SendUInt(value_x10 / 10);
  UART2_SendChar('.');
  UART2_SendChar('0' + (value_x10 % 10));
}

void send_sensor_report(void) {
  UART2_SendString("Temp: ");
  UART2_SendFixed1(Read_temp_f_x10());
  UART2_SendString(" F  Light: ");
  UART2_SendUInt(Read_light_percent());
  UART2_SendString("  Pot: ");
  UART2_SendUInt(Read_pot_percent());
  UART2_SendString("%\r\n");
}

// =============================================================================
// LCD Functions
// =============================================================================

void LCD_Init() {
  uint32_t temp;

  // GPIOA & GPIOB already enabled
  /*PB5 MOSI, PA10 /CS_7 latch , PA5 shift clock */

  /*PA5 and PA10 are outputs*/ temp = GPIOA->MODER;
  temp &= ~(0x03 << (2 * 5));
  temp |= (0x01 << (2 * 5));
  temp &= ~(0x03 << (2 * 10));
  temp |= (0x01 << (2 * 10));
  GPIOA->MODER = temp;

  temp = GPIOA->OTYPER;
  temp &= ~(0x01 << 5);
  temp &= ~(0x01 << 10);
  GPIOA->OTYPER = temp;

  temp = GPIOA->PUPDR;
  temp &= ~(0x03 << (2 * 5));
  temp &= ~(0x03 << (2 * 10));
  GPIOA->PUPDR = temp;

  /*PB5 is output*/

  temp = GPIOB->MODER;
  temp &= ~(0x03 << (2 * 5));
  temp |= (0x01 << (2 * 5));
  GPIOB->MODER = temp;

  temp = GPIOB->OTYPER;
  temp &= ~(0x01 << 5);
  GPIOB->OTYPER = temp;

  temp = GPIOB->PUPDR;
  temp &= ~(0x03 << (2 * 5));
  GPIOB->PUPDR = temp;

  /* LCD controller reset sequence */
  HAL_Delay(20);
  LCD_nibble_write(0x30, 0);
  HAL_Delay(5);
  LCD_nibble_write(0x30, 0);
  HAL_Delay(1);
  LCD_nibble_write(0x30, 0);
  HAL_Delay(1);
  LCD_nibble_write(0x20, 0);
  HAL_Delay(1);

  Write_Instr_LCD(0x28); /* set 4 bit data LCD - two line display - 5x8 font*/
  Write_Instr_LCD(
      0x0E); /* turn on display, turn on cursor , turn off blinking */
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
    GPIOA->ODR |= (1 << 5);
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

void Keypad_Init() {
  uint32_t temp;
  // GPIOB already enabled

  /*configure input*/
  /* row0 to 3 are PB8, PB9, PB10, PB11*/
  temp = GPIOB->MODER;
  temp &= ~(0x03 << (2 * 11));
  temp &= ~(0x03 << (2 * 10));
  temp &= ~(0x03 << (2 * 9));
  temp &= ~(0x03 << (2 * 8));
  GPIOB->MODER = temp;
  temp = GPIOB->OTYPER;
  temp &= ~(0x01 << 11);
  temp &= ~(0x01 << 10);
  temp &= ~(0x01 << 9);
  temp &= ~(0x01 << 8);
  GPIOB->OTYPER = temp;

  temp = GPIOB->PUPDR;
  temp &= ~(0x03 << (2 * 11));
  temp &= ~(0x03 << (2 * 10));
  temp &= ~(0x03 << (2 * 9));
  temp &= ~(0x03 << (2 * 8));
  GPIOB->PUPDR = temp;

  /* Col 0 to 3 are PB1, PB2, PB3, PB4*/
  /*configure output*/
  temp = GPIOB->MODER;
  temp &= ~(0x03 << (2 * 1));
  temp |= (0x01 << (2 * 1));
  temp &= ~(0x03 << (2 * 2));
  temp |= (0x01 << (2 * 2));
  temp &= ~(0x03 << (2 * 3));
  temp |= (0x01 << (2 * 3));
  temp &= ~(0x03 << (2 * 4));
  temp |= (0x01 << (2 * 4));
  GPIOB->MODER = temp;

  temp = GPIOB->OTYPER;
  temp &= ~(0x01 << 1);
  temp &= ~(0x01 << 2);
  temp &= ~(0x01 << 3);
  temp &= ~(0x01 << 4);
  GPIOB->OTYPER = temp;

  temp = GPIOB->PUPDR;
  temp &= ~(0x03 << (2 * 1));
  temp &= ~(0x03 << (2 * 2));
  temp &= ~(0x03 << (2 * 3));
  temp &= ~(0x03 << (2 * 4));
  GPIOB->PUPDR = temp;
}

// Call every ~2 ms. Returns a key once per press, otherwise KEY_NONE.
uint8_t Keypad_Scan(void) {
  static uint8_t col = 0;
  static uint8_t primed = 0;       // first call only drives column 0
  static uint8_t found = KEY_NONE; // key seen during the current sweep
  static uint8_t candidate = KEY_NONE;
  static uint8_t stable = 0;   // consecutive sweeps showing the candidate
  static uint8_t empty = 0;    // consecutive sweeps with no key
  static uint8_t reported = 0; // this press was already returned
  uint8_t result = KEY_NONE;

  if (primed) {
    // Read the rows for the column driven on the previous call
    uint8_t rows = (GPIOB->IDR >> 8) & 0xF;
    for (uint8_t r = 0; r < 4; r++) {
      if ((rows & (1U << r)) && found == KEY_NONE)
        found = keymap[r][col];
    }

    col++;
    if (col == 4) { // end of one full sweep: judge it
      col = 0;
      if (found != KEY_NONE) {
        empty = 0;
        if (found == candidate) {
          if (stable < 255)
            stable++;
        } else {
          candidate = found;
          stable = 1;
        }
        if (stable >= 3 && !reported) {
          result = candidate;
          reported = 1;
        }
      } else {
        if (empty < 255)
          empty++;
        if (empty >= 3) { // key fully released
          reported = 0;
          candidate = KEY_NONE;
          stable = 0;
        }
      }
      found = KEY_NONE;
    }
  }
  primed = 1;

  // Drive the next column high and the others low (one atomic write)
  GPIOB->BSRR = (0x1EU << 16) | (1U << (col + 1));
  return result;
}

// Set up the system clock
// ! IMPORTANT ! This function is generated by STM32CubeMX and should not be
// modified
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // Configure the main internal regulator output voltage
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
    Error_Handler();
  }

  // Initializes the RCC Oscillators according to the specified parameters
  //  in the RCC_OscInitTypeDef structure.
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  // Initializes the CPU, AHB and APB buses clocks
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

// This function is called whenever there is an error, such as illegal memory
// access
void Error_Handler(void) {
  // We don't really need to do anything for errors, so just stay here forever
  __disable_irq();
  while (1) {
  }
}

// ! IMPORTANT ! All code below this line is generated by STM32CubeMX and should
// not be modified
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
     file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */