/******************************************************************************
    第64回技能五輪全国大会エレクトロニクス職種
    競技3 2026/10/06実施
    Game Clock プログラム
    File name	: .c
    CPU			: Microchip PIC18F4620-I/P
    Clock		: 40MHz
    Date		: 2026/08/17
 *****************************************************************************/

#include <xc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "Game_clock.h"
#include "MCP25625.h"


extern MCP25625_Message message		 = { 0 };
extern TimeCounter		timer		 = { 0 };
extern volatile uint8_t main_sw_flag = 0;
extern volatile uint8_t can_flag	 = 0;
extern volatile uint8_t buzzer_flag	 = 0;


/****************************** 関数 ******************************/
extern void buzzer(void);
extern void clear_message(MCP25625_Message* message);
extern void enable_disp_seg(uint8_t enable);
extern void enable_extenal_INT_and_Timer(uint8_t enable);
extern void set_timer(uint8_t hour, uint8_t minute, uint8_t second);
extern void add_fischer_time(void);

uint8_t mode_select_slave(void);
void	game_mode_slave(uint8_t mode);
void	setting_mode_slave(void);
uint8_t connection_mode_slave(uint8_t* ASSIGN_ID);
uint8_t decide_turn_slave(uint8_t* ASSIGN_ID, ClockSettings* settings);


uint8_t mode_select_slave(void)
{
	int		cnt	 = 0;
	uint8_t disp = 0;

	SEG_DATA(0);
	SEG_EN = 1;

	while (1)
	{
		/* モード通知待機 */
		if (can_flag != 0)
		{
			can_flag = 0;
			if (MCP25625_Receive(&message) == MCP_OK)
			{
				switch (message.command)
				{
					case CAN_CMD_MODE_COUNT_DOWN:
						return 0;
					case CAN_CMD_MODE_COUNT_UP:
						return 1;
					case CAN_CMD_MODE_SETTING:
						return 2;
					default:
						break;
				}
				clear_message(&message);
			}
		}


		cnt++;
		if (cnt >= 300)
		{
			cnt	 = 0;
			LATD = 0x80 | (disp << 4);
			disp++;
			disp %= 6;
		}
		__delay_ms(1);
	}
}

void game_mode_slave(uint8_t mode)
{
	ClockSettings settings	= { 0, 10, 0, CLOCK_RULE_NONE, 0 };
	uint8_t		  ASSIGN_ID = 0;
	uint8_t		  ret		= 0;
	uint8_t		  paused	= 0;
	uint32_t	  id		= 0;

	if (connection_mode_slave(&ASSIGN_ID))
	{
		LATD = 0x90 | mode;
		decide_turn_slave(&ASSIGN_ID, &settings);

		if (mode == 0)
		{
			timer.increment = -1;
			set_timer(settings.hours, settings.minutes, settings.seconds);
			timer.fischer_flag = settings.rule == CLOCK_RULE_FISCHER;
			timer.delay_flag   = settings.rule == CLOCK_RULE_DELAY;
			timer.rule_seconds = settings.rule_seconds;
		}
		else
		{
			timer.increment = 1;
			set_timer(0, 0, 0);
			timer.fischer_flag = 0;
			timer.delay_flag   = 0;
			timer.rule_seconds = 0;
		}
		enable_extenal_INT_and_Timer(DISABLE);
		enable_disp_seg(ENABLE);
		while (1)
		{
			if (can_flag != 0)
			{
				can_flag = 0;
				if (MCP25625_Receive(&message) == MCP_OK)
				{
					if (message.command == CAN_CMD_GAME_STOP)
					{
						enable_extenal_INT_and_Timer(DISABLE);
						paused = 1;
					}
					else if (message.command == CAN_CMD_TURN_START)
					{
						if (!paused) timer.delay_ticks = timer.delay_flag ? (uint16_t)timer.rule_seconds * 100 : 0;
						paused = 0;
						enable_extenal_INT_and_Timer(ENABLE);
					}
					else if (message.command == CAN_CMD_GAME_END)
					{
						break;
					}
					else
					{
					}
					clear_message(&message);
				}
			}


			if (T0CONbits.TMR0ON != 0)
			{
				if (main_sw_flag != 0)
				{
					add_fischer_time();
					buzzer();
					enable_extenal_INT_and_Timer(DISABLE);

					id = CAN_CreateExtendedId(CAN_DEST_MASTER, CAN_CMD_TURN_END, ASSIGN_ID);
					while (MCP25625_Send(id, 0, 0) != MCP_OK);
				}
			}
		}
	}
	enable_extenal_INT_and_Timer(DISABLE);
	enable_disp_seg(DISABLE);
	clear_message(&message);
	return;
}

void setting_mode_slave(void)
{
}

void mesurement_mode_slave(void)
{
}

uint8_t connection_mode_slave(uint8_t* ASSIGN_ID)
{
	uint16_t temp_id = 0;
	uint32_t id		 = 0;
	uint16_t i		 = 0;
	uint8_t	 state	 = 0;
	uint16_t ad_rand = 0;


	while (1)
	{
		/* 接続待機 */
		if (can_flag != 0)
		{
			can_flag = 0;
			if (MCP25625_Receive(&message) == MCP_OK)
			{
				if (message.command == CAN_CMD_GAME_END)
				{
					clear_message(&message);
					MCP25625_BitModify(MCP_CANINTE, MCP_RX1IE, 0x00);
					return 0;
				}

				switch (state)
				{
					case 0:
						if (message.command == CAN_CMD_JOIN_OPEN)
						{
							/* A/D変換設定 */
							ADCON0 = 0b00000001; /* A/D変換ON */
							ADCON1 = 0b00001100; /* AN0-1：アナログポート設定*/
							ADCON2 = 0b10100101; /* 右詰め 16Tad Fosc/16*/

							/* オープンADポートを使って乱数生成 */
							/* 乱数値で適当にディレイさせることで最初の通信衝突を回避したい */
							do
							{
								for (uint16_t i = 0; i < 10; i++)
								{
									ADCON0bits.CHS	  = i;
									ADCON0bits.GODONE = 1;
									while (ADCON0bits.GODONE != 0) continue;
									ad_rand <<= 1;
									ad_rand |= (ADRESL & 0x01);
								}
							} while (ad_rand == 0);
							for (uint16_t i = 0; i < ad_rand; i++) __delay_us(10);

							/* 乱数値使って初期通信する */
							/* 正式IDと被らないようにシフトさせておく */
							temp_id = ad_rand << 6;
							while (MCP25625_SetReceiveMask(1, ((uint32_t)CAN_DEST_MASK << 24) | CAN_PARAMS_MASK) == MCP_ERROR);
							while (MCP25625_SetReceiveFilter(2, ((uint32_t)CAN_DEST_CHILD << 24) | temp_id) == MCP_ERROR);

							id = CAN_CreateExtendedId(CAN_DEST_MASTER, CAN_CMD_JOIN_REQUEST, temp_id);
							while (MCP25625_Send(id, 0, 0) != MCP_OK);

							state++;
						}
						break;
					case 1:
						if (message.command == CAN_CMD_ASSIGN_ID)
						{
							*ASSIGN_ID = message.data[0];
							id		   = CAN_CreateExtendedId(CAN_DEST_MASTER, CAN_CMD_ASSIGN_ACK, temp_id);
							while (MCP25625_Send(id, ASSIGN_ID, 1) != MCP_OK);

							state++;
						}
						else
						{
							/* 1回のJOIN_REQUESTが欠けても再送して接続を継続する */
							id = CAN_CreateExtendedId(CAN_DEST_MASTER, CAN_CMD_JOIN_REQUEST, temp_id);
							while (MCP25625_Send(id, 0, 0) != MCP_OK);
						}
						for (uint16_t i = 0; i < ad_rand; i++) __delay_us(10);
						break;
					case 2:
						if (message.command == CAN_CMD_ASSIGN_COMMIT)
						{
							while (MCP25625_SetReceiveFilter(2, ((uint32_t)CAN_DEST_CHILD << 24) | *ASSIGN_ID) == MCP_ERROR);

							state++;
						}
						for (uint16_t i = 0; i < ad_rand; i++) __delay_us(10);
						break;
					case 3:
						if (message.command == CAN_CMD_JOIN_COMPLETE)
						{
							clear_message(&message);
							return 1;
						}
						break;
				}
				clear_message(&message);
			}
		}
	}
}


uint8_t decide_turn_slave(uint8_t* ASSIGN_ID, ClockSettings* settings)
{
	uint32_t id	   = 0;
	uint16_t i	   = 0;
	uint8_t	 state = 0;

	while (1)
	{
		/* 接続待機 */
		if (can_flag != 0)
		{
			can_flag = 0;
			if (MCP25625_Receive(&message) == MCP_OK)
			{
				if (message.command == CAN_CMD_TURN_CLEAR)
				{
					clear_message(&message);
					state			  = 1;
					INTCONbits.INT0IF = 0;
					INTCONbits.INT0IE = 1;
				}

				switch (state)
				{
					case 0:
						if (message.command == CAN_CMD_DECIDE_TURN)
						{
							state++;
							INTCONbits.INT0IF = 0;
							INTCONbits.INT0IE = 1;
						}

						break;

					case 2:
						if (message.command == CAN_CMD_ASSIGN_TURN)
						{
							state++;
							LATD = 0xB0 | message.data[0];
						}
						break;

					case 3:
						if (message.command == CAN_CMD_GAME_START)
						{
							if (message.length >= 5)
							{
								settings->hours		   = message.data[0];
								settings->minutes	   = message.data[1];
								settings->seconds	   = message.data[2];
								settings->rule		   = message.data[3];
								settings->rule_seconds = message.data[4];
							}
							return 1;
						}
						break;
				}
				clear_message(&message);
			}
		}
		if (state == 1)
		{
			if (main_sw_flag)
			{
				state++;
				enable_extenal_INT_and_Timer(DISABLE);
				buzzer();

				id = CAN_CreateExtendedId(CAN_DEST_MASTER, CAN_CMD_TURN_QUEUE, *ASSIGN_ID);
				while (MCP25625_Send(id, 0, 0) != MCP_OK);
			}
		}
	}
}


/* --- メイン関数 --- */
void slave_sequence(void)
{
	uint8_t	 ret  = 0;
	uint8_t	 mode = 0;
	uint32_t id	  = 0;

	/* CAN通信モジュール初期化 */
	while (MCP25625_Init(CAN_DEST_MASK, (uint32_t)CAN_DEST_EMERGENCY << 24) == MCP_ERROR);
	while (MCP25625_SetReceiveFilter(1, (uint32_t)CAN_DEST_BROADCAST << 24) == MCP_ERROR);

	INTCONbits.GIE = 1; /* Global Interrupt Enable bit */
	while (1)
	{
		mode = mode_select_slave();

		LATD = 0x80 | mode;

		switch (mode)
		{
			case 0:
				game_mode_slave(0);
				break;
			case 1:
				game_mode_slave(1);
				break;
			case 2:
				setting_mode_slave();
				break;
			case 3:
				mesurement_mode_slave();
				break;
			default:
				break;
		}
	}
}
