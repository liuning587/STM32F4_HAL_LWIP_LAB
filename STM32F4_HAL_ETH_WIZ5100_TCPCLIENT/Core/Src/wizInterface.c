/*
 * wizInterface.c
 *
 *  Created on: 2020. 5. 31.
 *      Author: eziya76@gmail.com
 */

#include "wizInterface.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"

extern SPI_HandleTypeDef hspi1;
#define WIZ_SPI_HANDLE	&hspi1

static bool ip_assigned = 0;
static uint8_t buff_size[] = { 2, 2, 2, 2 };

#ifdef USE_DHCP
static uint8_t dhcp_buffer[1024];
static uint16_t dhcp_retry = 0;
#endif

//network information
wiz_NetInfo netInfo = {
		.mac = { 0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef },
		.ip = { 192, 168, 1, 180 },
		.sn = { 255, 255, 255, 0 },
		.gw = { 192, 168, 1, 1 } };

void WIZ_SPI_Select(void)
{
	HAL_GPIO_WritePin(WIZ_SPI1_CS_GPIO_Port, WIZ_SPI1_CS_Pin, GPIO_PIN_RESET);
}

void WIZ_SPI_Deselect(void)
{
	HAL_GPIO_WritePin(WIZ_SPI1_CS_GPIO_Port, WIZ_SPI1_CS_Pin, GPIO_PIN_SET);
}

void WIZ_SPI_TxByte(uint8_t byte)
{
	HAL_SPI_Transmit(WIZ_SPI_HANDLE, &byte, 1, HAL_MAX_DELAY);
}

uint8_t WIZ_SPI_RxByte(void)
{
	uint8_t ret;
	HAL_SPI_Receive(WIZ_SPI_HANDLE, &ret, 1, HAL_MAX_DELAY);
	return ret;
}


//dhcp callbacks
static void cbIPAddrAssigned(void) {
	printf("IP Address is assigned.\n");
	ip_assigned = true;
}

static void cbIPAddrConfict(void) {
	printf("IP Address is conflicted.\n");
	ip_assigned = false;
}

bool WIZ_ChipInit(void)
{
	int32_t ret;
	uint8_t tmpstr[6] = { 0, };

	//power reset arduino ethernet shield
	HAL_GPIO_WritePin(WIZ_RESET_GPIO_Port, WIZ_RESET_Pin, GPIO_PIN_RESET);
	osDelay(500);
	HAL_GPIO_WritePin(WIZ_RESET_GPIO_Port, WIZ_RESET_Pin, GPIO_PIN_SET);
	osDelay(500);

	//register spi functions
	reg_wizchip_cs_cbfunc(WIZ_SPI_Select, WIZ_SPI_Deselect);
	reg_wizchip_spi_cbfunc(WIZ_SPI_RxByte, WIZ_SPI_TxByte);

	//check chip id
	if (ctlwizchip(CW_GET_ID, (void*) tmpstr) == 0) {
		printf("wizchip id: %s\n", tmpstr);
		if(strncmp((const char*)tmpstr, "W5100", strlen("W5100")))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	//set rx,tx buffer sizes
	ret = wizchip_init(buff_size, buff_size);
	if (ret < 0) {
		HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
		printf("wozchip_init failed.\n");
		return false;
	}

	return true;
}

bool WIZ_NetworkInit(void)
{
#ifdef USE_DHCP
	setSHAR(netInfo.mac); //set MAC address
	DHCP_init(DHCP_SOCKET, dhcp_buffer); //init DHCP
	reg_dhcp_cbfunc(cbIPAddrAssigned, cbIPAddrAssigned, cbIPAddrConfict); //register DHCP callbacks

	//get ip from dhcp server
	dhcp_retry = 0;
	while (!ip_assigned && dhcp_retry < 100000) {
		dhcp_retry++;
		DHCP_run();
	}

	//if dhcp assigned an ip address.
	if (ip_assigned) {
		getIPfromDHCP(netInfo.ip);
		getGWfromDHCP(netInfo.gw);
		getSNfromDHCP(netInfo.sn);
	}
#endif

	//set network information
	wizchip_setnetinfo(&netInfo);

	//get network information
	wizchip_getnetinfo(&netInfo);
	printf("IP: %03d.%03d.%03d.%03d\nGW: %03d.%03d.%03d.%03d\nNet: %03d.%03d.%03d.%03d\n", netInfo.ip[0], netInfo.ip[1],
			netInfo.ip[2], netInfo.ip[3], netInfo.gw[0], netInfo.gw[1], netInfo.gw[2], netInfo.gw[3], netInfo.sn[0],
			netInfo.sn[1], netInfo.sn[2], netInfo.sn[3]);

	return true;
}
