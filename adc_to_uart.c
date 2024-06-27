#include "ti_msp_dl_config.h"

/**
 * @brief Global flag to check ADC conversion.
 */
volatile bool gCheckADC;

/**
 * @brief Array to store ADC results (3 samples per pulse).
 */
volatile uint16_t gADCResults[3]; // Array for 3 samples

/**
 * @brief Pulse counter.
 */
volatile uint8_t pulseCount = 0;  // Pulse counter

/**
 * @brief Setup UART configuration.
 */
void setupUART(void);

/**
 * @brief Setup ADC configuration.
 */
void setupADC(void);

/**
 * @brief Transmit ADC results over UART.
 */
void transmitADCResults(void);

/**
 * @brief Delay function in milliseconds.
 * 
 * @param ms Milliseconds to delay.
 */
void delay_ms(uint32_t ms);

/**
 * @brief Main function to initialize peripherals and handle ADC to UART transmission.
 */
int main(void)
{
    SYSCFG_DL_init();
    setupUART();
    setupADC();

    NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);
    gCheckADC = false;

    while (1) {
        if (pulseCount >= 10) {
            // 1 second delay
            delay_ms(1000); // 1 second
            pulseCount = 0;
        }
        
        DL_ADC12_startConversion(ADC12_0_INST);

        while (false == gCheckADC) {
            __WFE();
        }

        gCheckADC = false;
        transmitADCResults();
        pulseCount++;
    }
}

/**
 * @brief ADC interrupt handler to capture ADC results.
 */
void ADC12_0_INST_IRQHandler(void)
{
    static uint8_t sampleIndex = 0;

    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) {
        case DL_ADC12_IIDX_MEM0_RESULT_LOADED:
            gADCResults[sampleIndex++] = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
            if (sampleIndex >= 3) { // 3 samples captured
                sampleIndex = 0;
                gCheckADC = true;
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Function to setup UART with specified configurations.
 */
void setupUART(void)
{
    DL_UART_Main_ClockConfig uartClockConfig = {
        .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
    };

    DL_UART_Main_Config uartConfig = {
        .mode        = DL_UART_MAIN_MODE_NORMAL,
        .direction   = DL_UART_MAIN_DIRECTION_TX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity      = DL_UART_MAIN_PARITY_NONE,
        .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
    };

    DL_UART_Main_setClockConfig(UART_0_INST, &uartClockConfig);
    DL_UART_Main_init(UART_0_INST, &uartConfig);
    DL_UART_Main_setOversampling(UART_0_INST, DL_UART_OVERSAMPLING_RATE_16X);

    // Set calculated baud rate divisors for 115200 bps
    DL_UART_Main_setBaudRateDivisor(UART_0_INST, 17, 24);
    DL_UART_Main_enableFIFOs(UART_0_INST);
    DL_UART_Main_setTXFIFOThreshold(UART_0_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);
    DL_UART_Main_enable(UART_0_INST);
}

/**
 * @brief Function to setup ADC with specified configurations.
 */
void setupADC(void)
{
    DL_ADC12_ClockConfig adcClockConfig = {
        .clockSel       = DL_ADC12_CLOCK_ULPCLK,
        .divideRatio    = DL_ADC12_CLOCK_DIVIDE_8,
        .freqRange      = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
    };

    DL_ADC12_setClockConfig(ADC12_0_INST, &adcClockConfig);

    DL_ADC12_configConversionMem(
        ADC12_0_INST,
        DL_ADC12_MEM_IDX_0,
        DL_ADC12_INPUT_CHAN_2,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED
    );

    DL_ADC12_setSampleTime0(ADC12_0_INST, 500); // Set sampling time for 500 ns pulses
    DL_ADC12_clearInterruptStatus(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableInterrupt(ADC12_0_INST, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
    DL_ADC12_enableConversions(ADC12_0_INST);
}

/**
 * @brief Function to transmit ADC results via UART.
 */
void transmitADCResults(void)
{
    for (int i = 0; i < 3; i++) {
        uint8_t lowbyte  = (uint8_t)(gADCResults[i] & 0xFF);
        uint8_t highbyte = (uint8_t)((gADCResults[i] >> 8) & 0xFF);
        DL_UART_Main_transmitData(UART_0_INST, highbyte);
        DL_UART_Main_transmitData(UART_0_INST, lowbyte);
    }
}

/**
 * @brief Delay function implementation using a busy loop.
 * 
 * @param ms Number of milliseconds to delay.
 */
void delay_ms(uint32_t ms)
{
    // Assuming the system clock is 48 MHz
    // Approximate delay using loop
    for (uint32_t i = 0; i < ms * 48000; i++) {
        __NOP();
    }
}
