/******************************************************************************
    第64回技能五輪全国大会エレクトロニクス職種
    競技3 2026/10/06実施
    Game Clock プログラム
    File name	: .c
    CPU			: Microchip PIC18F4620-I/P
    Clock		: 40MHz
    Date		: 2026/08/17
 *****************************************************************************/

#ifndef GAME_CLOCK_H
#define GAME_CLOCK_H

/************************************ 定義 ************************************/

#define MASTER_UNIT 1
#define SLAVE_UNIT 0

#define SW_UP PORTAbits.RA0
#define SW_RIGHT PORTAbits.RA1
#define SW_LEFT PORTAbits.RA2
#define SW_DOWN PORTAbits.RA3
#define SW_CENTER PORTAbits.RA4

#define MAIN_SW PORTBbits.RB0
#define CAN_INT PORTBbits.RB1
#define SW_A PORTBbits.RB2
#define SW_B PORTBbits.RB3
#define UNIT_IDENTIFY PORTBbits.RB4


#define SEG_DP LATCbits.LATC7
#define BUZZER LATCbits.LATC2
#define SEG_EN LATDbits.LATD7

#define SEG_DATA(a) (LATD = (LATD & 0xf0) | (a & 0x0f))

#define ENABLE 1
#define DISABLE 0

typedef struct
{
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint8_t rule;
	uint8_t rule_seconds;
} ClockSettings;

#define CLOCK_RULE_NONE 0
#define CLOCK_RULE_FISCHER 1
#define CLOCK_RULE_DELAY 2

/* -------------------------------- 関数宣言 -------------------------------- */
/****************************** プロトタイプ宣言 ******************************/
typedef struct
{
	int8_t ms;
	int8_t sec;
	int8_t min;
	int8_t hour;
	int8_t increment;

	uint8_t	 timeUp_flag;
	uint8_t	 showHour_flag;
	uint8_t	 delay_flag;
	uint8_t	 fischer_flag;
	uint8_t	 rule_seconds;
	uint16_t delay_ticks;

} TimeCounter;

/******************************* extern宣言 *******************************/


#endif /* GAME_CLOCK_H */
