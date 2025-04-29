/*! ----------------------------------------------------------------------------
*  @file    multi_device_main.c
*  @brief   Multiple device distance measurement example using single firmware
*
*           This example creates a single firmware that can be flashed onto 
*           multiple DWM1001 devices. Each device measures distance to other
*           devices using Single-Sided Two-Way Ranging (SS TWR). The devices
*           automatically detect each other and report distances via UART.
*
*           Each device can operate in both initiator and responder modes,
*           taking turns based on a simple protocol that uses device IDs to
*           determine timing.
*
* @attention
*
* Copyright 2024 (c)
*
* All rights reserved.
*/
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "port_platform.h"

#define APP_NAME "MULTI DEVICE DISTANCE v1.0"

/* Inter-ranging delay period, in milliseconds. */
#define RNG_DELAY_MS 100
#define RESP_TIMEOUT_MS 10  // Response timeout in milliseconds

/* Maximum number of devices in the network */
#define MAX_DEVICES 5

/* Device ID - can be 0-4, set this uniquely for each device before flashing */
#define DEVICE_ID 0  // Set this to 0, 1, or 2 for each of the three devices

/* Timing offset for each device's transmission window based on their ID */
#define TIME_SLOT_MS 10

/* Frame types */
#define POLL_MSG 0xE0
#define RESP_MSG 0xE1
#define BCAST_MSG 0xE2

/* Frames used in the ranging process. */
static uint8 tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', POLL_MSG, DEVICE_ID, 0, 0}; // Added device ID
static uint8 tx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', RESP_MSG, DEVICE_ID, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8 tx_bcast_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'B', 'C', 'S', 'T', BCAST_MSG, DEVICE_ID, 0, 0};

/* Length of the common part of the message (up to and including the function code) */
#define ALL_MSG_COMMON_LEN 10
/* Indexes to access some of the fields in the frames */
#define ALL_MSG_SN_IDX 2
#define ALL_MSG_DEVID_IDX 10
#define RESP_MSG_POLL_RX_TS_IDX 11
#define RESP_MSG_RESP_TX_TS_IDX 15
#define RESP_MSG_TS_LEN 4

/* Buffer to store received response message */
#define RX_BUF_LEN 24
static uint8 rx_buffer[RX_BUF_LEN];

/* Hold copy of status register state here for reference */
static uint32 status_reg = 0;

/* UWB microsecond (uus) to device time unit (dtu, around 15.65 ps) conversion factor.
* 1 uus = 512 / 499.2 μs and 1 μs = 499.2 * 128 dtu. */
#define UUS_TO_DWT_TIME 65536

/* Speed of light in air, in metres per second. */
#define SPEED_OF_LIGHT 299702547

/* Time-of-flight and distance calculation variables */
static double tof;
static double distance;

/* Default antenna delay values for 64 MHz PRF. See NOTE 1 below. */
#define TX_ANT_DLY 16436
#define RX_ANT_DLY 16436

/* Default communication configuration. We use default non-STS DW mode. */
static dwt_config_t config = {
    5,               /* Channel number. */
    DWT_PRF_64M,     /* Pulse repetition frequency. */
    DWT_PLEN_128,    /* Preamble length. Used in TX only. */
    DWT_PAC8,        /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    1,               /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    (129 + 8 - 8)    /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
};

/* Delay between frames, in UWB microseconds. */
#define POLL_TX_TO_RESP_RX_DLY_UUS 500
#define PRE_TIMEOUT 8

/* Not enough time to write the data so TX timeout extended for nRF operation. */
#define POLL_RX_TO_RESP_TX_DLY_UUS 1100

/* Timestamps of frames transmission/reception */
typedef unsigned long long uint64;
static uint64 poll_tx_ts;
static uint64 resp_rx_ts;
static uint64 poll_rx_ts;
static uint64 resp_tx_ts;

/* Detected devices and their distances */
static struct {
    uint8 device_id;
    double distance;
    uint32 last_update_time;
    bool active;
} detected_devices[MAX_DEVICES];

/* Function declarations */
static void resp_msg_get_ts(uint8 *ts_field, uint32 *ts);
static void resp_msg_set_ts(uint8 *ts_field, const uint64 ts);
static uint64 get_rx_timestamp_u64(void);
static uint64 get_tx_timestamp_u64(void);
static void initialize_devices_array(void);
static void update_device_distance(uint8 id, double dist);
static void print_device_distances(void);
static void process_poll_message(uint8 sender_id);
static bool process_response_message(void);
static int send_poll_message(void);

/*! ------------------------------------------------------------------------------------------------------------------
* @fn initialize_devices_array()
*
* @brief Initialize the array of detected devices.
*/
static void initialize_devices_array(void)
{
    for (int i = 0; i < MAX_DEVICES; i++) {
        detected_devices[i].device_id = i;
        detected_devices[i].distance = 0.0;
        detected_devices[i].last_update_time = 0;
        detected_devices[i].active = false;
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn update_device_distance()
*
* @brief Update the distance to a specific device and mark it as active.
*
* @param id     Device ID to update
* @param dist   Distance measured to that device
*/
static void update_device_distance(uint8 id, double dist)
{
    if (id < MAX_DEVICES && id != DEVICE_ID) {
        detected_devices[id].distance = dist;
        detected_devices[id].last_update_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        detected_devices[id].active = true;
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn print_device_distances()
*
* @brief Print the distances to all active devices.
*/
static void print_device_distances(void)
{
    uint32 current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    printf("Device %d distances:\r\n", DEVICE_ID);
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (i != DEVICE_ID && detected_devices[i].active) {
            // Check if data is not too old (5 seconds timeout)
            if (current_time - detected_devices[i].last_update_time < 5000) {
                printf("  Device %d: %.2f m\r\n", i, detected_devices[i].distance);
            } else {
                detected_devices[i].active = false;
                printf("  Device %d: TIMEOUT\r\n", i);
            }
        }
    }
    printf("\r\n");
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn process_poll_message()
*
* @brief Process received poll message and send response.
*
* @param sender_id  ID of the device that sent the poll
*/
static void process_poll_message(uint8 sender_id)
{
    uint32 resp_tx_time;
    int ret;

    /* Retrieve poll reception timestamp */
    poll_rx_ts = get_rx_timestamp_u64();

    /* Compute response message transmission time */
    resp_tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
    dwt_setdelayedtrxtime(resp_tx_time);

    /* Response TX timestamp is the transmission time we programmed plus the antenna delay */
    resp_tx_ts = (((uint64)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;

    /* Write timestamps in the response message */
    resp_msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts);
    resp_msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], resp_tx_ts);

    /* Write and send the response message */
    tx_resp_msg[ALL_MSG_SN_IDX] = 0;
    tx_resp_msg[ALL_MSG_DEVID_IDX] = DEVICE_ID;
    dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0);
    dwt_writetxfctrl(sizeof(tx_resp_msg), 0, 1);
    ret = dwt_starttx(DWT_START_TX_DELAYED);

    if (ret == DWT_SUCCESS) {
        /* Poll DW1000 until TX frame sent event set */
        while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS)) {};

        /* Clear TXFRS event */
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);
    } else {
        /* If we couldn't transmit, reset RX */
        dwt_rxreset();
    }
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn process_response_message()
*
* @brief Process received response message and calculate distance.
*
* @return true if distance calculation was successful, false otherwise
*/
static bool process_response_message(void)
{
    uint32 frame_len;
    uint8 sender_id;
    
    /* Clear good RX frame event in the DW1000 status register */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);

    /* A frame has been received, read it into the local buffer */
    frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
   
    if (frame_len <= RX_BUF_LEN) {
        dwt_readrxdata(rx_buffer, frame_len, 0);
    } else {
        return false;
    }

    /* Check that the frame is a response message */
    if (rx_buffer[9] != RESP_MSG) {
        return false;
    }
    
    /* Get sender ID */
    sender_id = rx_buffer[ALL_MSG_DEVID_IDX];
    
    /* Record sequence number and sender ID for validation */
    rx_buffer[ALL_MSG_SN_IDX] = 0;

    /* Verify this is a valid response message */
    if (memcmp(rx_buffer, tx_resp_msg, ALL_MSG_COMMON_LEN) == 0) {
        uint32 poll_tx_ts_32, resp_rx_ts_32, poll_rx_ts_32, resp_tx_ts_32;
        int32 rtd_init, rtd_resp;
        float clockOffsetRatio;

        /* Retrieve poll transmission and response reception timestamps */
        poll_tx_ts_32 = (uint32)poll_tx_ts;
        resp_rx_ts_32 = (uint32)resp_rx_ts;

        /* Get timestamps embedded in response message */
        resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts_32);
        resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts_32);

        /* Read carrier integrator value and calculate clock offset ratio */
        clockOffsetRatio = dwt_readcarrierintegrator() * (FREQ_OFFSET_MULTIPLIER * HERTZ_TO_PPM_MULTIPLIER_CHAN_5 / 1.0e6);

        /* Compute time of flight and distance */
        rtd_init = resp_rx_ts_32 - poll_tx_ts_32;
        rtd_resp = resp_tx_ts_32 - poll_rx_ts_32;

        tof = ((rtd_init - rtd_resp * (1.0f - clockOffsetRatio)) / 2.0f) * DWT_TIME_UNITS;
        distance = tof * SPEED_OF_LIGHT;
        
        /* Update detected devices array with this measurement */
        update_device_distance(sender_id, distance);
        
        return true;
    }
    
    return false;
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn send_poll_message()
*
* @brief Send a poll message to initiate distance measurement.
*
* @return DWT_SUCCESS if success, error code otherwise
*/
static int send_poll_message(void)
{
    /* Write frame data to DW1000 and prepare transmission */
    tx_poll_msg[ALL_MSG_SN_IDX] = 0;
    tx_poll_msg[ALL_MSG_DEVID_IDX] = DEVICE_ID;
    
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS);
    dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
    dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);

    /* Start transmission, indicating that a response is expected */
    poll_tx_ts = get_tx_timestamp_u64();
    return dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn get_rx_timestamp_u64()
*
* @brief Get the RX time-stamp in a 64-bit variable.
*        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
*
* @param  none
*
* @return  64-bit value of the read time-stamp.
*/
static uint64 get_rx_timestamp_u64(void)
{
  uint8 ts_tab[5];
  uint64 ts = 0;
  int i;
  dwt_readrxtimestamp(ts_tab);
  for (i = 4; i >= 0; i--)
  {
    ts <<= 8;
    ts |= ts_tab[i];
  }
  return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn get_tx_timestamp_u64()
*
* @brief Get the TX time-stamp in a 64-bit variable.
*        /!\ This function assumes that length of time-stamps is 40 bits, for both TX and RX!
*
* @param  none
*
* @return  64-bit value of the read time-stamp.
*/
static uint64 get_tx_timestamp_u64(void)
{
  uint8 ts_tab[5];
  uint64 ts = 0;
  int i;
  dwt_readtxtimestamp(ts_tab);
  for (i = 4; i >= 0; i--)
  {
    ts <<= 8;
    ts |= ts_tab[i];
  }
  return ts;
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn resp_msg_get_ts()
*
* @brief Read a given timestamp value from the response message.
*
* @param  ts_field  pointer on the first byte of the timestamp field to get
*         ts  timestamp value
*
* @return none
*/
static void resp_msg_get_ts(uint8 *ts_field, uint32 *ts)
{
  int i;
  *ts = 0;
  for (i = 0; i < RESP_MSG_TS_LEN; i++)
  {
    *ts += ts_field[i] << (i * 8);
  }
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn resp_msg_set_ts()
*
* @brief Fill a given timestamp field in the response message with the given value.
*
* @param  ts_field  pointer on the first byte of the timestamp field to fill
*         ts  timestamp value
*
* @return none
*/
static void resp_msg_set_ts(uint8 *ts_field, const uint64 ts)
{
  int i;
  for (i = 0; i < RESP_MSG_TS_LEN; i++)
  {
    ts_field[i] = (ts >> (i * 8)) & 0xFF;
  }
}

/*! ------------------------------------------------------------------------------------------------------------------
* @fn multi_device_task_function()
*
* @brief Main task function that handles both initiator and responder roles.
*
* @param[in] pvParameter   Pointer that will be used as the parameter for the task.
*/
void multi_device_task_function(void * pvParameter)
{
    UNUSED_PARAMETER(pvParameter);
    
    /* Initialize device array */
    initialize_devices_array();
    
    /* Enable DW1000 LEDs */
    dwt_setleds(DWT_LEDS_ENABLE);
    
    /* Set timeout */
    dwt_setrxtimeout(RESP_TIMEOUT_MS * 1000);  // Convert to UWB microseconds
    
    printf("Device %d started. Multi-device distance measurement.\r\n", DEVICE_ID);
    
    TickType_t last_poll_time = 0;
    TickType_t last_print_time = 0;
    
    while (true) {
        TickType_t current_time = xTaskGetTickCount();
        
        /* Time to send a poll message (each device has its own time slot) */
        if ((current_time - last_poll_time) >= pdMS_TO_TICKS(RNG_DELAY_MS)) {
            /* Start a new ranging cycle */
            if (send_poll_message() == DWT_SUCCESS) {
                last_poll_time = current_time;
                
                /* Wait for the response or timeout */
                while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & 
                        (SYS_STATUS_RXFCG | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {};
                
                resp_rx_ts = get_rx_timestamp_u64();
                
                if (status_reg & SYS_STATUS_RXFCG) {
                    /* A response has been received - process it */
                    if (process_response_message()) {
                        /* Distance calculation was successful */
                    }
                } else {
                    /* Clear RX error/timeout events */
                    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
                    /* Reset RX */
                    dwt_rxreset();
                }
            }
        }
        
        /* Time to print distances to all active devices (every 1 second) */
        if ((current_time - last_print_time) >= pdMS_TO_TICKS(1000)) {
            print_device_distances();
            last_print_time = current_time;
        }
        
        /* Listen for poll messages from other devices */
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
        
        /* Short wait to listen for incoming polls */
        vTaskDelay(pdMS_TO_TICKS(5));
        
        status_reg = dwt_read32bitreg(SYS_STATUS_ID);
        
        if (status_reg & SYS_STATUS_RXFCG) {
            uint32 frame_len;
            uint8 message_type, sender_id;
            
            /* A frame has been received */
            frame_len = dwt_read32bitreg(RX_FINFO_ID) & RX_FINFO_RXFL_MASK_1023;
            
            if (frame_len <= RX_BUF_LEN) {
                dwt_readrxdata(rx_buffer, frame_len, 0);
                
                /* Get message type and sender ID */
                message_type = rx_buffer[9];
                sender_id = rx_buffer[10];
                
                if (message_type == POLL_MSG && sender_id != DEVICE_ID) {
                    /* Process the poll message and send a response */
                    process_poll_message(sender_id);
                }
            }
            
            /* Clear good RX frame event */
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG);
        } else if (status_reg & (SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR)) {
            /* Clear RX error/timeout events */
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
            /* Reset RX */
            dwt_rxreset();
        }
    }
} 