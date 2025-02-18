#ifndef __DMA_SUN8IW11__
#define __DMA_SUN8IW11__

#define DRQSRC_SRAM		0
#define DRQSRC_SDRAM		0
#define DRQSRC_SPDIFRX		2
#define DRQSRC_OWARX		DRQSRC_SPDIFRX
#define DRQSRC_DAUDIO_0_RX	3
#define DRQSRC_DAI0_RX		DRQSRC_DAUDIO_0_RX
#define DRQSRC_DAUDIO_1_RX	4
#define DRQSRC_DAI1_RX		DRQSRC_DAUDIO_1_RX
#define DRQSRC_AC97		5
#define DRQSRC_DAUDIO_2_RX	6
#define DRQSRC_DAI2_RX		DRQSRC_DAUDIO_2_RX
#define DRQSRC_NAND0		7
#define DRQSRC_UART0RX		8
#define DRQSRC_UART1RX 		9
#define DRQSRC_UART2RX		10
#define DRQSRC_UART3RX		11
#define DRQSRC_UART4RX		12
#define DRQSRC_UART5RX		13
#define DRQSRC_UART6RX		14
#define DRQSRC_UART7RX		15

/* #define DRQSRC_RESEVER		16 */

#define DRQSRC_OTG_EP1		17

/* #define DRQSRC_RESEVER		18 */

#define DRQSRC_AUDIO_CODEC	19
#define DRQSRC_CODEC		DRQSRC_AUDIO_CODEC
#define DRQSRC_CODEC_ADC	DRQSRC_AUDIO_CODEC
#define DRQSRC_IR0RX		20
#define DRQSRC_IR1RX		21
#define DRQSRC_EMAC		22
#define DRQSRC_TP		23
#define DRQSRC_TPRX		DRQSRC_TP
#define DRQSRC_SPI0_RX		24
#define DRQSRC_SPI1_RX		25
#define DRQSRC_SPI2_RX		26
#define DRQSRC_SPI3_RX		27

#define DRQSRC_OTG_EP2		28
#define DRQSRC_OTG_EP3		29
#define DRQSRC_OTG_EP4		30
#define DRQSRC_OTG_EP5		31


/*
 * The destination DRQ type and port corresponding relation
 *
 */
#define DRQDST_SRAM		0
#define DRQDST_SDRAM		0
#define DRQDST_SPDIFTX		2
#define DRQDST_OWATX		DRQDST_SPDIFTX
#define DRQDST_DAUDIO_0_TX	3
#define DRQDST_DAI0_TX		DRQDST_DAUDIO_0_TX
#define DRQDST_DAUDIO_1_TX	4
#define DRQDST_DAI1_TX		DRQDST_DAUDIO_1_TX
#define DRQDST_AC97		5
#define DRQDST_DAUDIO_2_TX	6
#define DRQDST_DAI2_TX		DRQDST_DAUDIO_2_TX
#define DRQDST_NAND0		7
#define DRQDST_UART0TX		8
#define DRQDST_UART1TX 		9
#define DRQDST_UART2TX		10
#define DRQDST_UART3TX		11
#define DRQDST_UART4TX		12
#define DRQDST_UART5TX		13
#define DRQDST_UART6TX		14
#define DRQDST_UART7TX		15

/* #define DRQDST_RESEVER		16 */

#define DRQDST_OTG_EP1		17

/* #define DRQDST_RESEVER		18 */

#define DRQDST_AUDIO_CODEC	19
#define DRQDST_CODEC		DRQDST_AUDIO_CODEC
#define DRQDST_CODEC_ADC	DRQDST_AUDIO_CODEC
#define DRQDST_IR0TX		20
#define DRQDST_IR1TX		21
#define DRQDST_EMAC		22
/* #define DRQDST_RESEVER		23 */
#define DRQDST_SPI0_TX		24
#define DRQDST_SPI1_TX		25
#define DRQDST_SPI2_TX		26
#define DRQDST_SPI3_TX		27

#define DRQDST_OTG_EP2		28
#define DRQDST_OTG_EP3		29
#define DRQDST_OTG_EP4		30
#define DRQDST_OTG_EP5		31


#endif /*__DMA_SUN8IW11__  */
