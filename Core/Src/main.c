#include "stm32f4xx_hal.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "string.h"

// Motor control states
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_CW,
    MOTOR_CCW
} MotorState;

// Define pin mappings for motor control and button
#define PUL_PIN        GPIO_PIN_2     // PUL+ (PA2)
#define DIR_PIN        GPIO_PIN_3     // DIR+ (PA3)
#define BUTTON_PIN     GPIO_PIN_0     // B1 button (PA0)
#define MOTOR_PORT     GPIOA          // Motor GPIO Port

MotorState motorState = MOTOR_STOP;   // Initial state: motor stopped

// USB buffer
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
 uint8_t buffer[64];
char data[64] = "Motor Control Ready\n";
// Forward declaration of functions
void SystemClock_Config(void);
void GPIO_Init(void);
void Motor_Control(MotorState state);
void Process_USB_Command(uint8_t* Buf, uint32_t Len);

// CDC Transmit function
void CDC_Transmit_Data(char* data);

int main(void)
{
    // HAL and system initialization
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    MX_USB_DEVICE_Init(); // Initialize USB device

    char* initMsg = "Motor Control Ready\n";
    CDC_Transmit_Data(initMsg); // Send an initial message to PuTTY

    // Main loop
    while (1)
    {
        // Check if a new command has been received over USB
        if (strlen((char*)buffer) > 0) {
            // Process the received command
            Process_USB_Command(buffer, strlen((char*)buffer));

            // Clear the buffer after processing
            memset(buffer, 0, sizeof(buffer));
        }

        // Run the motor based on the current state
        Motor_Control(motorState);

        // Small delay for smoother control
        HAL_Delay(.1);
    }
}
// Function to control motor based on its state
void Motor_Control(MotorState state)
{
    switch (state) {
        case MOTOR_CW:
            // Set DIR+ high for clockwise direction
            HAL_GPIO_WritePin(MOTOR_PORT, DIR_PIN, GPIO_PIN_SET);

            // Generate a pulse on PUL+
            HAL_GPIO_WritePin(MOTOR_PORT, PUL_PIN, GPIO_PIN_SET);
            HAL_Delay(.1);  // Short pulse
            HAL_GPIO_WritePin(MOTOR_PORT, PUL_PIN, GPIO_PIN_RESET);
            break;

        case MOTOR_CCW:
            // Set DIR+ low for counterclockwise direction
            HAL_GPIO_WritePin(MOTOR_PORT, DIR_PIN, GPIO_PIN_RESET);

            // Generate a pulse on PUL+
            HAL_GPIO_WritePin(MOTOR_PORT, PUL_PIN, GPIO_PIN_SET);
            HAL_Delay(.1);  // Short pulse
            HAL_GPIO_WritePin(MOTOR_PORT, PUL_PIN, GPIO_PIN_RESET);
            break;

        case MOTOR_STOP:
        default:
            // Stop motor (no pulses generated)
            HAL_GPIO_WritePin(MOTOR_PORT, PUL_PIN, GPIO_PIN_RESET);
            break;
    }
}

// Function to process USB commands received from PuTTY
void Process_USB_Command(uint8_t* Buf, uint32_t Len)
{
    if (strncmp((char*)Buf, "start_cw", Len) == 0) {
        motorState = MOTOR_CW;
        CDC_Transmit_Data("Motor started clockwise\n");
    }
    else if (strncmp((char*)Buf, "start_ccw", Len) == 0) {
        motorState = MOTOR_CCW;
        CDC_Transmit_Data("Motor started counterclockwise\n");
    }
    else if (strncmp((char*)Buf, "stop", Len) == 0) {
        motorState = MOTOR_STOP;
        CDC_Transmit_Data("Motor stopped\n");
    }
    else {
        CDC_Transmit_Data("Invalid command\n");
    }
}

// Function to transmit data via USB CDC to PuTTY
void CDC_Transmit_Data(char* data)
{
    CDC_Transmit_FS((uint8_t*)data, strlen(data));
    HAL_Delay(.1);  // Delay to ensure data transmission
}

// GPIO initialization function
void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();  // Enable GPIOA clock

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure PUL+ and DIR+ as output
    GPIO_InitStruct.Pin = PUL_PIN | DIR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(MOTOR_PORT, &GPIO_InitStruct);

    // Configure B1 button as input (PA0)
    GPIO_InitStruct.Pin = BUTTON_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MOTOR_PORT, &GPIO_InitStruct);
}

// System clock configuration function (standard for STM32F407)
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Configure the main internal regulator output voltage
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Initializes the RCC Oscillators
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        // Initialization Error
        while(1);
    }

    // Initializes the CPU, AHB and APB buses clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        // Initialization Error
        while(1);
    }
}
void Error_Handler(void)
{
    // Error handling code (infinite loop or system reset)
    while(1)
    {
        // Optionally toggle an LED or other indicator
    }
}
