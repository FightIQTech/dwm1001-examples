/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "bsp.h"
#include "boards.h"
#include "nordic_common.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_spi.h"
#include "nrf_uart.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf.h"
#include "app_error.h"
#include "app_util_platform.h"
#include "app_error.h"
#include <string.h>
#include "port_platform.h"
#include "deca_types.h"
#include "deca_param_types.h"
#include "deca_regs.h"
#include "deca_device_api.h"
#include "UART.h"
#include <stdio.h>

/* Device ID - can be 0-4, set this uniquely for each device before flashing */
#define DEVICE_ID 0  // Set this to 0, 1, or 2 for each of the three devices

//-----------------dw1000----------------------------

static dwt_config_t config = {
    5,                /* Channel number. */
    DWT_PRF_64M,      /* Pulse repetition frequency. */
    DWT_PLEN_128,     /* Preamble length. Used in TX only. */
    DWT_PAC8,         /* Preamble acquisition chunk size. Used in RX only. */
    10,               /* TX preamble code. Used in TX only. */
    10,               /* RX preamble code. Used in RX only. */
    0,                /* 0 to use standard SFD, 1 to use non-standard SFD. */
    DWT_BR_6M8,       /* Data rate. */
    DWT_PHRMODE_STD,  /* PHY header mode. */
    (129 + 8 - 8)     /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
};

/* Preamble timeout, in multiple of PAC size. See NOTE 3 below. */
#define PRE_TIMEOUT 1000

/* Delay between frames, in UWB microseconds. See NOTE 1 below. */
#define POLL_TX_TO_RESP_RX_DLY_UUS 100 

/* We'll reuse the antenna delay values from port_platform.h */
// #define TX_ANT_DLY 16300
// #define RX_ANT_DLY 16456	

//--------------dw1000---end---------------


#define TASK_DELAY        200           /**< Task delay. Delays a LED0 task for 200 ms */
#define TIMER_PERIOD      2000          /**< Timer period. LED1 timer will expire after 1000 ms */

#ifdef USE_FREERTOS

TaskHandle_t  ss_initiator_task_handle;   /**< Reference to SS TWR Initiator FreeRTOS task. */
extern void ss_initiator_task_function (void * pvParameter);
TaskHandle_t  led_toggle_task_handle;   /**< Reference to LED0 toggling FreeRTOS task. */
TimerHandle_t led_toggle_timer_handle;  /**< Reference to LED1 toggling FreeRTOS timer. */
#endif

#ifdef USE_FREERTOS

/**@brief LED0 task entry function.
 *
 * @param[in] pvParameter   Pointer that will be used as the parameter for the task.
 */
/* Removed unused LED functions */

/**@brief The function to call when the LED1 FreeRTOS timer expires.
 *
 * @param[in] pvParameter   Pointer that will be used as the parameter for the timer.
 */
/* Removed unused LED functions */

#else

  extern int ss_init_run(void);

#endif   // #ifdef USE_FREERTOS

/* Task priorities. */
#define TASK_PRIORITY      ( tskIDLE_PRIORITY + 1 )

extern void multi_device_task_function (void * pvParameter);

/* Declaration of static functions. */
static void prvSetupHardware (void);

/* External function declarations */
extern void reset_DW1000(void);
extern void port_set_dw1000_slowrate(void);
extern void port_set_dw1000_fastrate(void);

/**
 * Application entry point.
 * 
 * @return 0
 */
int main(void)
{
  /* Setup hardware. */
  prvSetupHardware();

  /* Compile time verification of parameters. */

  printf("\nMulti-Device Distance Measurement Example\n");
  printf("Device ID: %d\n", DEVICE_ID);

  /* Create a task for the multi-device distance measurement. */
  xTaskCreate(multi_device_task_function, "MultiDev", configMINIMAL_STACK_SIZE + 200, NULL, TASK_PRIORITY, NULL);

  /* Start the FreeRTOS scheduler. */
  vTaskStartScheduler();

  return 0;
}

/**
 * Setup all system peripherals
 * Initialises board
 * Initialises uart
 * Initialises interrupt
 * 
 * @return none
 */
static void prvSetupHardware (void)
{
  /* Setup some LEDs for debug Green and Blue on DWM1001-DEV */
  LEDS_CONFIGURE(BSP_LED_0_MASK | BSP_LED_1_MASK | BSP_LED_2_MASK);
  LEDS_ON(BSP_LED_0_MASK | BSP_LED_1_MASK | BSP_LED_2_MASK);

  /* Initialise decawave device. */
  reset_DW1000(); /* Target specific drive of RSTn line into DW1000 low for a period. */

  port_set_dw1000_slowrate();
  
  /* Wake DW1000 up. */    
  if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR)
  {
    printf("INIT FAILED");
    while (1)
    { };
  }

  port_set_dw1000_fastrate();

  /* Calibrates decawave device. */
  dwt_configuresleep(DWT_PRESRV_SLEEP | DWT_CONFIG, DWT_WAKE_SLPCNT | DWT_WAKE_CS | DWT_SLP_EN);

  /* Configure DW1000. */
  /* If the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device */
  dwt_configure(&config);

  /* Configure DW1000 LEDs */
  dwt_setleds(1);

  /* Enable wanted interrupts (TX confirmation, RX good frames, RX timeouts, RX errors). */
  dwt_setinterrupt(DWT_INT_TFRS | DWT_INT_RFCG | DWT_INT_RFTO | DWT_INT_RXPTO | DWT_INT_RPHE | DWT_INT_RFCE | DWT_INT_RFSL | DWT_INT_SFDT, 1);

  /* Apply default antenna delay value. See NOTE 1 below. */
  dwt_setrxantennadelay(RX_ANT_DLY);
  dwt_settxantennadelay(TX_ANT_DLY);

  /* Set expected response's delay and timeout. */
  /* This applies to the initiator only not for the responder */
  dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
  /* Configure preamble timeout to maximum value. */
  dwt_setpreambledetecttimeout(PRE_TIMEOUT);
}

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn assert_failed()
 *
 * @brief Callback for asserts.
 *
 * @param  file  the file where the assert occurred
 *         line  the line where the assert occurred
 *
 * @return none
 */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* Assertion failed! */
    while (1) { }
}

/*****************************************************************************************************************************************************
 * NOTES:
 *
 * 1. The single-sided two-way ranging scheme implemented here has to be considered carefully as the accuracy of the distance measured is highly
 *    sensitive to the clock offset error between the devices and the length of the response delay between frames. To achieve the best possible
 *    accuracy, this response delay must be kept as low as possible. In order to do so, 6.8 Mbps data rate is used in this example and the response
 *    delay between frames is defined as low as possible. The user is referred to User Manual for more details about the single-sided two-way ranging
 *    process.  NB:SEE ALSO NOTE 11.
 * 2. The sum of the values is the TX to RX antenna delay, this should be experimentally determined by a calibration process. Here we use a hard coded
 *    value (expected to be a little low so a positive error will be seen on the resultant distance estimate. For a real production application, each
 *    device should have its own antenna delay properly calibrated to get good precision when performing range measurements.
 * 3. This timeout is for complete reception of a frame, i.e. timeout duration must take into account the length of the expected frame. Here the value
 *    is arbitrary but chosen large enough to make sure that there is enough time to receive the complete response frame sent by the responder at the
 *    6.8M data rate used (around 200 �s).
 * 4. In a real application, for optimum performance within regulatory limits, it may be necessary to set TX pulse bandwidth and TX power, (using
 *    the dwt_configuretxrf API call) to per device calibrated values saved in the target system or the DW1000 OTP memory.
 * 5. The user is referred to DecaRanging ARM application (distributed with EVK1000 product) for additional practical example of usage, and to the
 *     DW1000 API Guide for more details on the DW1000 driver functions.
 *
 ****************************************************************************************************************************************************/




