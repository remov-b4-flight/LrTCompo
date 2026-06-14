/*
 * usbd_compo.c
 *
 *	Created on: Jun 13, 2026
 *	Author: jenoki
 */

#include <midi.h>
#include "usbd_midi.h"
#include "usbd_hid.h"
#include "usbd_desc.h"
#include "stm32f0xx_hal_conf.h"
#include "usbd_ctlreq.h"
#include "stm32f0xx_hal.h"
#include "usbd_compo.h"

#define COMPO_DESC_SIZE		(83 + 25)

static uint8_t	USBD_COMPO_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t	USBD_COMPO_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t	USBD_COMPO_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t	USBD_COMPO_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);

static uint8_t	*USBD_COMPO_GetCfgDesc (uint16_t *length);
USBD_HandleTypeDef *pCompoInstance = NULL;
//__ALIGN_BEGIN uint8_t USB_Rx_Buffer[MIDI_DATA_OUT_PACKET_SIZE] __ALIGN_END ;

/* USB Composite device Configuration Descriptor */
__ALIGN_BEGIN uint8_t USBD_COMPO_CfgDesc[COMPO_DESC_SIZE] __ALIGN_END =
{
	// MIDI Configuration Descriptor (83 bytes)
	0x09, CONFIG, COMPO_DESC_SIZE, 0, NUM_INTF, CONFIG2, UNUSED_DESC_IDX, (BUSPOWERED | REMOTE_WAKEUP), MIDI_POWER,
	// Interface Association Descriptor for MIDI Streaming
	0x09, INTERFACE, INTF0, ALTER0, MS_NUM_EP, AUDIO, MIDI_STREAM, MIDI_UNUSED, MIDI_UNUSED,
	0x07, CS_INTERFACE, HEADER, 0x00, 0x01, 37, 0,
	0x06, CS_INTERFACE, MIDI_IN_JACK, MIDI_JTYPE_ENB, MIDI_JACK_NO_IN_ENB, UNUSED_DESC_IDX,
	0x09, CS_INTERFACE, MIDI_OUT_JACK, MIDI_JTYPE_EXT, MIDI_JACK_NO_OUT_EXT, 0x01, MIDI_JACK_NO_IN_ENB, 0x01, UNUSED_DESC_IDX,
	0x09, CS_INTERFACE, MIDI_OUT_JACK, MIDI_JTYPE_ENB, MIDI_JACK_NO_OUT_ENB, 0x01, MIDI_JACK_NO_IN_EXT, 0x01, UNUSED_DESC_IDX,
	0x06, CS_INTERFACE, MIDI_IN_JACK, MIDI_JTYPE_EXT, MIDI_JACK_NO_IN_EXT, UNUSED_DESC_IDX,
	0x09, ENDPOINT, MIDI_OUT_EP, BULK, MIDI_DATA_OUT_PACKET_SIZE, 0, INTERVAL, MIDI_UNUSED, MIDI_UNUSED,
	0x05, CS_ENDPOINT, MS_GENERAL, 0x01, MIDI_JACK_NO_IN_ENB,
	0x09, ENDPOINT, MIDI_IN_EP, BULK, MIDI_DATA_IN_PACKET_SIZE, 0, INTERVAL, MIDI_UNUSED, MIDI_UNUSED,
	0x05, CS_ENDPOINT, MS_GENERAL, 0x01, MIDI_JACK_NO_OUT_ENB,
#if 0
};

/* USB HID device FS Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_COMPO_HID_CfgFSDesc[USB_HID_CONFIG_DESC_SIZ]	__ALIGN_END =
{
	0x09,/* bLength: Configuration Descriptor size */
	USB_DESC_TYPE_CONFIGURATION,/* bDescriptorType: Configuration */
	USB_HID_CONFIG_DESC_SIZ,
	/* wTotalLength: Bytes returned */
	0x00,
	INTF1,	/*bNumInterfaces: 1 interface*/
	0x00,	/*bConfigurationValue: Configuration value*/
	0x00,	/*iConfiguration: Index of string descriptor describing the configuration*/
	0xE0,	/*bmAttributes: bus powered and Support Remote Wake-up */
	0x32,	/*MaxPower 100 mA: this current is used for detecting Vbus*/
#endif
	/************** Interface Descriptor of HID ****************/
	0x09,			/*bLength: Interface Descriptor size*/
	INTERFACE,		/*bDescriptorType: Interface descriptor type*/
	INTF1,			/*bInterfaceNumber: Number of Interface*/
	ALTER0,			/*bAlternateSetting: Alternate setting*/
	0x01,			/*bNumEndpoints*/
	HID_CLASS_INTF,	/*bInterfaceClass: HID*/
	0x01,			/*bInterfaceSubClass : 1=BOOT, 0=no boot*/
	HID_PROTO_KB,	/*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
	0,				/*iInterface: Index of string descriptor*/
	/******************** HID Descriptor of keyboard ********************/
	0x09,					/*bLength: HID Descriptor size*/
	DSC_TYPE_HID,			/*bDescriptorType: HID*/
	0x11,					/*bcdHID: HID Class Spec release number*/
	0x01,
	0x00,					/*bCountryCode: Hardware target country*/
	0x01,					/*bNumDescriptors: Number of HID class descriptors to follow*/
	DSC_RPT,				/*bDescriptorType*/
	HID_KEYBOARD_REPORT_DESC_SIZE,			/*wItemLength: Total length of Report descriptor*/
	0x00,
	/******************** Endpoint Descriptor of keyboard ********************/
	0x07,					/*bLength: Endpoint Descriptor size*/
	USB_DESC_TYPE_ENDPOINT,	/*bDescriptorType:*/
	HID_EPIN_ADDR,			/*bEndpointAddress: Endpoint Address (IN)*/
	TFR_INTERRUPT,			/*bmAttributes: Interrupt endpoint*/
	HID_EPIN_SIZE,			/*wMaxPacketSize: 4 Byte max */
	0x00,
	HID_FS_BINTERVAL,		/*bInterval: Polling Interval */
};

/* USB MIDI interface class callbacks structure */
USBD_ClassTypeDef USBD_COMPO =
{
	USBD_COMPO_Init,
	USBD_COMPO_DeInit,
	NULL,
	NULL,
	NULL,
	USBD_COMPO_DataIn,
	USBD_COMPO_DataOut,
	NULL,
	NULL,
	NULL,
	NULL,// HS
	USBD_COMPO_GetCfgDesc,// FS
	NULL,// OTHER SPEED
	NULL,// DEVICE_QUALIFIER
};

static uint8_t USBD_COMPO_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx){
	pCompoInstance = pdev;
	uint8_t ret;

	ret = USBD_MIDI_Init(pdev, cfgidx);
	if (ret != USBD_OK) return ret;

	ret = USBD_HID_Init(pdev,cfgidx);
	if (ret != USBD_OK) return ret;

	return USBD_OK;
}

static uint8_t USBD_COMPO_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx){
	pCompoInstance = NULL;
	USBD_MIDI_DeInit(pdev,cfgidx);
	USBD_HID_DeInit(pdev,cfgidx);
	return 0;
}
static uint8_t USBD_COMPO_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum){
	if (epnum == HID_EPIN_ADDR) {
		return USBD_HID_DataIn(pdev, epnum);
	} else if (epnum == MIDI_IN_EP) {
		return USBD_MIDI_DataIn(pdev, epnum);
	}
	return USBD_OK;
}

static uint8_t	USBD_COMPO_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
	// HID OUT endpoint is not used in LrTCompo
	return USBD_MIDI_DataOut(pdev, epnum);
}

static uint8_t *USBD_COMPO_GetCfgDesc (uint16_t *length){
	*length = sizeof (USBD_COMPO_CfgDesc);
	return USBD_COMPO_CfgDesc;
}

