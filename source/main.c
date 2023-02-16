/******************************************************************************
* File Name: main.c
*
* Description: This is the source code for the PMG1 MCU SPI Slave Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/


/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include <stdio.h>
#include <inttypes.h>
#include "cy_pdl.h"
#include "cybsp.h"
#include "SpiSlave.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Number of elements in the transmit and receive buffer */
/* There are three elements - one for head, one for command and one for tail */
#define NUMBER_OF_ELEMENTS   (3UL)
#define SIZE_OF_ELEMENT      (1UL)
#define SIZE_OF_PACKET       (NUMBER_OF_ELEMENTS * SIZE_OF_ELEMENT)
#define CY_ASSERT_FAILED     (0U)

/* Debug print macro to enable UART print */
/* (For S0 - Debug print will be always zero as SCB UART is not available) */
#if (!defined(CY_DEVICE_CCG3PA))
#define DEBUG_PRINT         (0u)
#endif

/*******************************************************************************
* Function Prototypes
********************************************************************************/
/* Function to turn ON or OFF the LED based on the SPI Master command. */
static void update_led(uint8_t);

#if DEBUG_PRINT
cy_stc_scb_uart_context_t CYBSP_UART_context; /* Global variable for UART */
/* Variable used for tracking the print status */
volatile bool ENTER_LOOP = true;

/*******************************************************************************
* Function Name: check_status
********************************************************************************
* Summary:
*  Prints the error message.
*
* Parameters:
*  error_msg - message to print if any error encountered.
*  status - status obtained after evaluation.
*
* Return:
*  void
*
*******************************************************************************/
void check_status(char *message, cy_rslt_t status)
{
    char error_msg[50];

    sprintf(error_msg, "Error Code: 0x%08" PRIX32 "\n", status);

    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n=====================================================\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\nFAIL: ");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, message);
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, error_msg);
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n=====================================================\r\n");
}
#endif

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  System entrance point. This function performs
*  - initial setup of device
*  - configure the SCB block as SPI slave
*  - check for spi transfer complete status
*  - update the LED status based on the command received from the SPI master
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Buffer to save the received data by the slave */
    uint32_t status = 0;

    uint8_t rx_buffer[SIZE_OF_PACKET] = {0};
    uint8_t tx_buffer[SIZE_OF_PACKET] = {0};

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }

#if DEBUG_PRINT

    /* Configure and enable the UART peripheral */
    Cy_SCB_UART_Init(CYBSP_UART_HW, &CYBSP_UART_config, &CYBSP_UART_context);
    Cy_SCB_UART_Enable(CYBSP_UART_HW);

    /* Sequence to clear screen */
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\x1b[2J\x1b[;H");

    /* Print "SPI slave" */
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "****************** ");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "PMG1 MCU: SPI slave");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "****************** \r\n\n");
#endif

    /* Initialize the SPI Slave */
    status = init_slave();
    if(status == INIT_FAILURE)
    {
#if DEBUG_PRINT
        check_status("API init_slave failed with error code", status);
#endif
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Enable global interrupts */
    __enable_irq();

    for (;;)
    {
        /* Form the status packet */
        tx_buffer[PACKET_SOP_POS] = PACKET_SOP;
        tx_buffer[PACKET_CMD_POS] = rx_buffer[PACKET_CMD_POS];
        tx_buffer[PACKET_EOP_POS] = PACKET_EOP;

        /* Get the bytes received by the slave */
        status = read_packet(tx_buffer, rx_buffer, SIZE_OF_PACKET);

        /* Check whether the slave succeeded in receiving the required number
         * of bytes and in the right format */
        if(status == TRANSFER_COMPLETE)
        {
            /* Communication succeeded. Update the LED. */
            update_led(rx_buffer[PACKET_CMD_POS]);
        }
        else
        {
            CY_ASSERT(CY_ASSERT_FAILED);
        }
#if DEBUG_PRINT
        if (ENTER_LOOP)
        {
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "Entered for loop\r\n");
            ENTER_LOOP = false;
        }
#endif
    }
}


/*******************************************************************************
* Function Name: update_led
********************************************************************************
*
* Summary:
*  This function updates the LED based on the command received by
*  the SPI Slave from Master.
*
* Parameters:
*  (uint8_t) LED_Cmd - command to turn LED ON or OFF
*
* Return:
*  None
*
*******************************************************************************/
static void update_led(uint8_t LED_Cmd)
{
    /* Control the LED based on command received from Master */
    if(LED_Cmd == CYBSP_LED_STATE_ON)
    {
        /* Turn ON the LED */
        Cy_GPIO_Clr(CYBSP_USER_LED_PORT, CYBSP_USER_LED_NUM);
    }

    if(LED_Cmd == CYBSP_LED_STATE_OFF)
    {
        /* Turn OFF the LED */
        Cy_GPIO_Set(CYBSP_USER_LED_PORT, CYBSP_USER_LED_NUM);
    }
}

/* [] END OF FILE */

