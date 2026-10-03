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
#include "ST7789_TFT.h"
#include "MCP25625.h"

/************************ コンフィギュレーションの設定 ************************/
#pragma config OSC = HSPLL, FCMEN = OFF, IESO = OFF					   /*CONFIG1H*/
#pragma config PWRT = ON, BOREN = OFF, BORV = 0						   /*CONFIG2L*/
#pragma config WDT = OFF, WDTPS = 1									   /*CONFIG2H*/
#pragma config MCLRE = ON, LPT1OSC = OFF, PBADEN = OFF, CCP2MX = PORTC /*CONFIG3H*/
#pragma config STVREN = ON, LVP = OFF, XINST = OFF, DEBUG = OFF		   /*CONFIG4L*/
#pragma config CP0 = OFF, CP1 = OFF, CP2 = OFF, CP3 = OFF			   /*CONFIG5L*/
#pragma config CPB = OFF, CPD = OFF									   /*CONFIG5H*/
#pragma config WRT0 = OFF, WRT1 = OFF, WRT2 = OFF, WRT3 = OFF		   /*CONFIG6L*/
#pragma config WRTB = OFF, WRTC = OFF, WRTD = OFF					   /*CONFIG6H*/
#pragma config EBTRB = OFF											   /*CONFIG7H*/

#define _XTAL_FREQ (40000000ul) //40MHz
#define MASTER_UNIT 1
#define SLAVE_UNIT 0

const uint8_t ASSIGN_ID[7] = { 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

typedef struct
{
	union
	{
		uint8_t full;
		struct
		{
			bool up : 1;
			bool right : 1;
			bool left : 1;
			bool down : 1;
			bool center : 1;
			bool a : 1;
			bool b : 1;
		} single;
	} data;
} _SW_Flag;


/************************ グローバル変数宣言 ************************/
MCP25625_Message message			 = { 0 };
_SW_Flag		 sw_flag			 = { 0 };
TimeCounter		 timer				 = { 0 };
ClockSettings	 clock_settings		 = { 0, 10, 0, CLOCK_RULE_NONE, 0 };
uint8_t			 turn_start_target	 = 0;
uint8_t			 turn_start_retry_ms = 0;
uint8_t			 turn_start_sequence = 0;

uint8_t			 seg_cnt	  = 0;
volatile uint8_t main_sw_flag = 0;
volatile uint8_t can_flag	  = 0;
volatile uint8_t buzzer_flag  = 0;

/****************************** 関数 ******************************/
extern void slave_sequence(void);

void	 buzzer(void);
void	 enable_disp_seg(uint8_t enable);
void	 enable_extenal_INT_and_Timer(uint8_t enable);
void	 set_timer(uint8_t hour, uint8_t minute, uint8_t second);
void	 add_fischer_time(void);
void	 clear_message(MCP25625_Message* message);
_SW_Flag get_sw(void);
uint8_t	 mode_select(void);
void	 count_down_mode(void);
void	 count_up_mode(void);
void	 setting_mode(void);
void	 mesurement_mode(void);
uint8_t	 connection_mode(uint8_t* num);
uint8_t	 decide_turn(uint8_t* turn_order, uint8_t member_num);
void	 next_turn(uint8_t* turn_order, uint8_t turn);

/**
 * @brief ブザーを100msだけ鳴らす
 * 
 */
void buzzer(void)
{
	T2CONbits.TMR2ON = 1;
	T3CONbits.TMR3ON = 1;
}

/**
 * @brief 7SEG表示の許可
 * 
 * @param enable 1:許可 0:不許可
 */
void enable_disp_seg(uint8_t enable)
{
	PIE1bits.TMR1IE = enable;
	SEG_EN			= enable;
}

/**
 * @brief メインSWとメインタイマーの許可
 * 
 * @param enable 1:許可 0:不許可
 */
void enable_extenal_INT_and_Timer(uint8_t enable)
{
	uint8_t gie_state = INTCONbits.GIE;

	INTCONbits.GIE	  = 0;
	INTCONbits.INT0IE = 0;
	T0CONbits.TMR0ON  = 0;
	INTCONbits.INT0IF = 0;
	INTCONbits.TMR0IF = 0;
	main_sw_flag	  = 0;
	if (enable != 0)
	{
		INTCONbits.INT0IE = 1;
		T0CONbits.TMR0ON  = 1;
	}
	INTCONbits.GIE = gie_state;
}

/**
 * @brief 持ち時間の設定
 * 
 * @param minute 
 */
void set_timer(uint8_t hour, uint8_t minute, uint8_t second)
{
	timer.hour			= hour;
	timer.min			= minute;
	timer.sec			= second;
	timer.ms			= 0;
	timer.timeUp_flag	= 0;
	timer.showHour_flag = hour > 0;

	TMR0H = 0xCF; /* 12500 10ms */
	TMR0L = 0x2C; /* 12500 10ms */
}

void add_fischer_time(void)
{
	if (timer.fischer_flag == 0) return;

	timer.sec += timer.rule_seconds;
	if (timer.sec >= 60)
	{
		timer.sec -= 60;
		timer.min++;
		if (timer.min >= 60)
		{
			timer.min = 0;
			timer.hour++;
			timer.showHour_flag = 1;
		}
	}
}


/**
 * @brief 次の人に手番を渡す
 * 
 * @param turn_order 
 * @param turn 
 */
void next_turn(uint8_t* turn_order, uint8_t turn)
{
	uint32_t id			= 0;
	timer.delay_ticks	= timer.delay_flag ? (uint16_t)timer.rule_seconds * 100 : 0;
	turn_start_target	= 0;
	turn_start_retry_ms = 0;
	turn_start_sequence++;

	if (turn_order[turn] == 0x01)
	{
		enable_extenal_INT_and_Timer(ENABLE);
	}
	else
	{
		enable_extenal_INT_and_Timer(DISABLE);
		turn_start_target	= turn_order[turn];
		turn_start_retry_ms = 10;
		id					= CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_TURN_START, turn_order[turn]);
		while (MCP25625_Send(id, &turn_start_sequence, 1) != MCP_OK);
	}
}

/**
 * @brief CANで受け取ったメッセージを削除
 * 
 * @param message 
 */
void clear_message(MCP25625_Message* message)
{
	message->id		= 0;
	message->length = 0;
	memset(message->data, 0, 8);
	message->destinationType = 0;
	message->command		 = 0;
	message->targetId		 = 0;
}


_SW_Flag get_sw(void)
{
	static _SW_Flag old_sw = { 0 };
	_SW_Flag		buf	   = { 0 };
	_SW_Flag		ret	   = { 0 };

	//現在の値を取得
	buf.data.full	  = PORTA & 0x1f;
	buf.data.single.a = SW_A;
	buf.data.single.b = SW_B;

	//SWの立ち上がり検出
	ret.data.full = (buf.data.full ^ old_sw.data.full) & buf.data.full;

	old_sw = buf; //現在の状態保存

	return ret;
}


uint8_t mode_select(void)
{
	const uint8_t mode_str[4][20] = { "COUNT DOWN MODE", "COUNT UP MODE", "SETTING", "MESUREMENT" };
	int			  cursor		  = 0;
	int			  old_cursor	  = 0;

	ST7789_FillScreen(COLOR_WHITE);
	ST7789_Draw_Opening_Img();

	for (uint16_t i = 0; i < 4; i++)
	{
		/* 中央揃えでモード表示 */
		ST7789_StringSize_centing(mode_str[i], 125 + (i * 25), COLOR_BLACK, COLOR_WHITE, 2);
	}
	ST7789_StringSize(">", ((240 - (12 * strlen(mode_str[cursor]))) / 2) - 15, 125 + (cursor * 25), COLOR_RED, COLOR_WHITE, 2);

	sw_flag.data.full = 0;

	while (1)
	{
		sw_flag = get_sw();

		/* 上下選択 */
		if (sw_flag.data.single.up)
		{
			sw_flag.data.single.up = 0;
			cursor--;
			if (cursor < 0) cursor = 3;
		}
		if (sw_flag.data.single.down)
		{
			sw_flag.data.single.down = 0;
			cursor++;
			if (cursor > 3) cursor = 0;
		}

		/* 決定 */
		if (sw_flag.data.single.a)
		{
			sw_flag.data.single.a = 0;
			return cursor;
		}

		/* カーソルの更新 */
		if (cursor != old_cursor)
		{
			ST7789_StringSize(" ", ((240 - (12 * strlen(mode_str[old_cursor]))) / 2) - 15, 125 + (old_cursor * 25), COLOR_WHITE, COLOR_WHITE, 2);
			ST7789_StringSize(">", ((240 - (12 * strlen(mode_str[cursor]))) / 2) - 15, 125 + (cursor * 25), COLOR_RED, COLOR_WHITE, 2);
			old_cursor = cursor;
		}
		__delay_ms(10);
	}
}

/**
 * @brief メインのゲームモード
 * 		  参加募集（接続判断）->順番決め->ゲーム動作の順に行う
 * 		  カウントダウン動作時には追加でルール設定あり
 * 
 * @param mode 0：カウントダウン動作 1：カウントアップ動作
 */
void game_mode(uint8_t mode)
{
	ClockSettings settings		= clock_settings;
	uint8_t		  turn_order[8] = { 0 };
	uint8_t		  turn			= 0;
	uint8_t		  member_num	= 0;
	int8_t		  stop_flag		= 0;
	int8_t		  game_end_flag = 0;
	uint8_t		  ret			= MCP_ERROR;
	int8_t		  retry_cnt		= 10;
	uint8_t		  child_index;
	uint8_t		  ready_flags[7]	= { 0 };
	uint8_t		  ready_retry_ms[7] = { 0 };
	uint8_t		  ready_count		= 0;

	uint32_t id = 0;
	uint8_t	 start_data[5];

	MCP25625_SetOneShot(ENABLE);

	/* 接続先へカウントダウンモードの通知 */
	id = CAN_CreateExtendedId(CAN_DEST_BROADCAST, CAN_CMD_MODE_COUNT_DOWN + mode, 0x0000);

	while ((ret == MCP_ERROR) && (retry_cnt > 0))
	{
		ret = MCP25625_Send(id, 0, 0);
		retry_cnt--;
		__delay_ms(100);
	}
	if (ret == MCP_OK)
	{
		MCP25625_SetOneShot(DISABLE);

		ST7789_FillScreen(COLOR_WHITE);
		if (mode == 0)
			ST7789_StringSize_centing("COUNT DOWN MODE", 10, COLOR_BLACK, COLOR_WHITE, 2);
		else
			ST7789_StringSize_centing("COUNT UP MODE", 10, COLOR_BLACK, COLOR_WHITE, 2);

		ret = connection_mode(&member_num);
		if (ret == 0) goto GAME_END;


		buzzer();
		ret = decide_turn(turn_order, member_num);
		if (ret == 0) goto GAME_END;

		ST7789_FillScreen(COLOR_WHITE);
		ST7789_StringSize_centing("GAME START!", 80, COLOR_BLACK, COLOR_WHITE, 2);
		ST7789_StringSize_centing("Stop/ReStart->SW_A", 160, COLOR_BLACK, COLOR_WHITE, 2);
		ST7789_StringSize_centing("END->SW_B", 180, COLOR_BLACK, COLOR_WHITE, 2);
		/* 参加者にゲームスタートを通知 */
		for (uint16_t i = 0; i < member_num; i++)
		{
			id			  = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_GAME_START, ASSIGN_ID[i]);
			start_data[0] = settings.hours;
			start_data[1] = settings.minutes;
			start_data[2] = settings.seconds;
			start_data[3] = settings.rule;
			start_data[4] = settings.rule_seconds;
			while (MCP25625_Send(id, start_data, sizeof(start_data)) != MCP_OK);
			ready_retry_ms[i] = 50;
		}
		while (ready_count < member_num)
		{
			if (can_flag != 0)
			{
				can_flag = 0;
				while (MCP25625_Receive(&message) == MCP_OK)
				{
					if (message.command == CAN_CMD_GAME_READY)
					{
						for (child_index = 0; child_index < member_num; child_index++)
						{
							if (message.targetId == ASSIGN_ID[child_index]) break;
						}
						if ((child_index < member_num) && (ready_flags[child_index] == 0))
						{
							ready_flags[child_index] = 1;
							ready_count++;
						}
					}
					clear_message(&message);
				}
			}
			for (child_index = 0; child_index < member_num; child_index++)
			{
				if (ready_flags[child_index] == 0)
				{
					if (ready_retry_ms[child_index] == 0)
					{
						id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_GAME_START, ASSIGN_ID[child_index]);
						while (MCP25625_Send(id, start_data, sizeof(start_data)) != MCP_OK);
						ready_retry_ms[child_index] = 50;
					}
					else
					{
						ready_retry_ms[child_index]--;
					}
				}
			}
			__delay_ms(1);
		}
		member_num++; /* 親機も台数に混ぜる */

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

		buzzer();

		enable_extenal_INT_and_Timer(DISABLE);
		enable_disp_seg(ENABLE);
		turn = 0;
		next_turn(turn_order, turn);

		while (1)
		{
			sw_flag = get_sw();
			if (can_flag != 0)
			{
				can_flag = 0;
				while (MCP25625_Receive(&message) == MCP_OK)
				{
					if (message.command == CAN_CMD_TURN_END)
					{
						for (child_index = 0; child_index < (member_num - 1); child_index++)
						{
							if (message.targetId == ASSIGN_ID[child_index]) break;
						}
						if (child_index < (member_num - 1))
						{
							id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_TURN_END_ACK, message.targetId);
							while (MCP25625_Send(id, 0, 0) != MCP_OK);
							if ((turn_order[turn] != 0x01) && (message.targetId == turn_order[turn]) && (message.length >= 1) && (message.data[0] == turn_start_sequence))
							{
								add_fischer_time();
								turn = (turn + 1) % member_num;
								next_turn(turn_order, turn);
							}
						}
					}
					else if ((message.command == CAN_CMD_TURN_START_ACK) && (message.targetId == turn_start_target) && (message.length >= 1) &&
							 (message.data[0] == turn_start_sequence))
					{
						turn_start_target	= 0;
						turn_start_retry_ms = 0;
					}
					clear_message(&message);
				}
			}
			if (turn_start_target != 0)
			{
				if (turn_start_retry_ms == 0)
				{
					id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_TURN_START, turn_start_target);
					while (MCP25625_Send(id, &turn_start_sequence, 1) != MCP_OK);
					turn_start_retry_ms = 10;
				}
				else
				{
					turn_start_retry_ms--;
					__delay_ms(1);
				}
			}

			if (turn_order[turn] == 0x01) /* 自機の手番か */
			{
				if (main_sw_flag != 0)
				{
					/* 手番終了 */
					add_fischer_time();
					buzzer();
					enable_extenal_INT_and_Timer(DISABLE);

					turn = (turn + 1) % member_num;
					next_turn(turn_order, turn);
				}
			}

			/* ゲーム一時停止／再開 */
			if (sw_flag.data.single.a)
			{
				sw_flag.data.single.a = 0;
				buzzer();

				if (turn_order[turn] == 0x01)
				{
					enable_extenal_INT_and_Timer(stop_flag);
				}
				else
				{
					id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_GAME_STOP + stop_flag, turn_order[turn]);
					if (stop_flag)
						while (MCP25625_Send(id, &turn_start_sequence, 1) != MCP_OK);
					else
						while (MCP25625_Send(id, 0, 0) != MCP_OK);
				}
				if (stop_flag)
					ST7789_StringSize_centing("GAME START!", 80, COLOR_BLACK, COLOR_WHITE, 2);
				else
					ST7789_StringSize_centing(" GAME STOP ", 80, COLOR_BLACK, COLOR_WHITE, 2);
				stop_flag = 1 - stop_flag;
			}

			/* ゲーム中断 */
			if (sw_flag.data.single.b)
			{
				sw_flag.data.single.b = 0;
				break;
			}
		}
	}
	else
	{
		ST7789_StringSize_centing("NOT CONNECTED...", 10, COLOR_BLACK, COLOR_WHITE, 2);
		ST7789_StringSize_centing("END->SW_B", 180, COLOR_BLACK, COLOR_WHITE, 2);
		while (1)
		{
			sw_flag = get_sw();
			/* ゲーム中断 */
			if (sw_flag.data.single.b)
			{
				sw_flag.data.single.b = 0;
				break;
			}
		}
	}

GAME_END:
	/* ゲーム終了処理 */
	enable_extenal_INT_and_Timer(DISABLE);
	enable_disp_seg(DISABLE);

	id = CAN_CreateExtendedId(CAN_DEST_EMERGENCY, CAN_CMD_GAME_END, 0x00);
	MCP25625_Send(id, 0, 0);
	clear_message(&message);
	return;
}


/**
 * @brief 参加募集モード
 * 		  Aボタン：ゲームへ移行		Bボタン：メインメニューに戻る
 * 
 * @param num 参加者数を格納するポインタ
 * @return uint8_t 進行情報を返す	1:ゲームへ移行	0：メインメニューに戻る
 */
uint8_t connection_mode(uint8_t* num)
{
	enum REGISTER_STATE
	{
		NOT_REGISTER = 0,
		TEMP_REGISTER,
		REGISTER_COMPLETE,
	};
	uint8_t	 assignNum		   = 0;
	uint8_t	 deviceNum		   = 0;
	uint8_t	 str[2]			   = { 0 };
	uint16_t temp_list[7]	   = { 0 };
	uint8_t	 resister_state[7] = { NOT_REGISTER };
	uint32_t id				   = 0;
	int		 i				   = 0;

	ST7789_StringSize("CONNECT_NUM: 0", 30, 100, COLOR_BLACK, COLOR_WHITE, 2);
	sw_flag.data.full = 0;

	/* 参加募集通知 */
	id = CAN_CreateExtendedId(CAN_DEST_BROADCAST, CAN_CMD_JOIN_OPEN, 0x0000);
	while (MCP25625_Send(id, 0, 0) != MCP_OK);

	while (1)
	{
		/* 受信処理 */
		if (can_flag != 0)
		{
			can_flag = 0;
			while (MCP25625_Receive(&message) == MCP_OK)
			{
				if (message.command == CAN_CMD_JOIN_REQUEST)
				{
					/* 参加要求があった場合、送られてきた仮IDをリストに追加 */
					for (i = 0; i < deviceNum; i++)
					{
						if (message.targetId == temp_list[i]) break;
					}

					if (i == deviceNum)
					{
						if (deviceNum < 7)
						{
							/* 新しい仮IDに正式なIDを割り当てる */
							temp_list[deviceNum]	  = message.targetId;
							resister_state[deviceNum] = TEMP_REGISTER;

							id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_ASSIGN_ID, message.targetId);
							while (MCP25625_Send(id, &ASSIGN_ID[deviceNum], 1) != MCP_OK);
							deviceNum++;
						}
					}
					else
					{
						/* 再送要求には同じ正式IDを返す */
						id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_ASSIGN_ID, message.targetId);
						while (MCP25625_Send(id, &ASSIGN_ID[i], 1) != MCP_OK);
					}
				}
				else if (message.command == CAN_CMD_ASSIGN_ACK)
				{
					/* 子機が正式IDを正常に受信して、返答してきた */
					for (i = 0; i < deviceNum; i++)
					{
						if ((message.length >= 1) && (message.targetId == temp_list[i]) && (message.data[0] == ASSIGN_ID[i])) break;
					}

					if ((i < deviceNum) && (message.length >= 1))
					{
						id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_ASSIGN_COMMIT, message.targetId);
						while (MCP25625_Send(id, &ASSIGN_ID[i], 1) != MCP_OK);

						if (resister_state[i] != REGISTER_COMPLETE)
						{
							resister_state[i] = REGISTER_COMPLETE;
							assignNum++;
							str[0] = assignNum + 0x30;
							ST7789_StringSize(str, 186, 100, COLOR_BLACK, COLOR_WHITE, 2);
							ST7789_StringSize_centing("Start->SW_A", 140, COLOR_BLACK, COLOR_WHITE, 2);
						}
					}
				}
				else
				{
					/* 一致するものがなかった */
				}
			}
			clear_message(&message);
		}

		sw_flag = get_sw();

		/* ゲーム移行処理 */
		if (sw_flag.data.single.a)
		{
			sw_flag.data.single.a = 0;

			if (assignNum > 0)
			{
				*num = assignNum;
				for (uint16_t i = 0; i < assignNum; i++)
				{
					id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_JOIN_COMPLETE, ASSIGN_ID[i]);
					while (MCP25625_Send(id, 0, 0) != MCP_OK);
				}
				return 1;
			}
		}

		/* ゲーム中断処理 */
		if (sw_flag.data.single.b)
		{
			sw_flag.data.single.b = 0;
			return 0;
		}
	}
}

/**
 * @brief 順番決めモード
 * 
 * @param turn_order 順番格納用配列
 * @param member_num 参加者数
 * @return uint8_t 
 */
uint8_t decide_turn(uint8_t* turn_order, uint8_t member_num)
{
	uint32_t id					  = 0;
	uint8_t	 order_flag[8]		  = { 0 };
	uint8_t	 assigned_position[7] = { 0 };
	uint8_t	 order_num			  = 0;
	uint8_t	 i					  = 0;
	uint16_t retry_ms			  = 0;

	ST7789_FillScreen(COLOR_WHITE);
	ST7789_StringSize_centing("DECIDE THE TURN", 80, COLOR_BLACK, COLOR_WHITE, 2);
	ST7789_StringSize_centing("push -> MAIN_SW", 160, COLOR_BLACK, COLOR_WHITE, 2);

	for (i = 0; i < member_num; i++)
	{
		id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_DECIDE_TURN, ASSIGN_ID[i]);
		while (MCP25625_Send(id, 0, 0) != MCP_OK);
	}

	INTCONbits.INT0IF = 0;
	INTCONbits.INT0IE = 1;
	sw_flag.data.full = 0;

	while (1)
	{
		if (can_flag != 0)
		{
			can_flag = 0;
			while (MCP25625_Receive(&message) == MCP_OK)
			{
				if (message.command == CAN_CMD_TURN_QUEUE)
				{
					for (i = 0; i < member_num; i++)
					{
						if (message.targetId == ASSIGN_ID[i]) break;
					}
					if (i < member_num)
					{
						if (order_flag[i + 1] == 0 && order_num < (member_num + 1))
						{
							order_flag[i + 1]	  = 1;
							turn_order[order_num] = ASSIGN_ID[i];
							order_num++;
							assigned_position[i] = order_num;
						}
						if (order_flag[i + 1] != 0)
						{
							id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_ASSIGN_TURN, ASSIGN_ID[i]);
							while (MCP25625_Send(id, &assigned_position[i], 1) != MCP_OK);
						}
					}
				}
				clear_message(&message);
			}
		}

		if ((order_num < (member_num + 1)) && (main_sw_flag != 0) && (order_flag[0] == 0))
		{
			enable_extenal_INT_and_Timer(DISABLE);
			buzzer();
			order_flag[0]		  = 1;
			turn_order[order_num] = 0x01;
			order_num++;
			LATD = 0xB0 | order_num;
		}

		if (order_num < (member_num + 1))
		{
			retry_ms++;
			if (retry_ms >= 100)
			{
				for (i = 0; i < member_num; i++)
				{
					if (order_flag[i + 1] == 0)
					{
						id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_DECIDE_TURN, ASSIGN_ID[i]);
						while (MCP25625_Send(id, 0, 0) != MCP_OK);
					}
				}
				retry_ms = 0;
			}
			__delay_ms(1);
		}

		sw_flag = get_sw();
		if (sw_flag.data.single.a)
		{
			sw_flag.data.single.a = 0;
			if (order_num == (member_num + 1)) return 1;
		}

		if (sw_flag.data.single.b)
		{
			sw_flag.data.single.b = 0;
			for (i = 0; i < member_num; i++)
			{
				id = CAN_CreateExtendedId(CAN_DEST_CHILD, CAN_CMD_TURN_CLEAR, ASSIGN_ID[i]);
				while (MCP25625_Send(id, 0, 0) != MCP_OK);
			}
			memset(order_flag, 0, sizeof(order_flag));
			memset(assigned_position, 0, sizeof(assigned_position));
			memset(turn_order, 0, member_num + 1);
			order_num		  = 0;
			LATD			  = 0xB0;
			retry_ms		  = 100;
			INTCONbits.INT0IF = 0;
			INTCONbits.INT0IE = 1;
		}
	}
}
void setting_mode(void)
{
	ClockSettings edited_settings = clock_settings;
	const char*	  rule_names[3]	  = { "NONE", "FISCHER", "DELAY" };
	char		  line[24];
	uint8_t		  selected = 0;
	uint8_t		  previous_selected;
	uint8_t		  previous_hours;
	uint8_t		  previous_minutes;
	uint8_t		  previous_seconds;
	uint8_t		  previous_rule;
	uint8_t		  previous_rule_seconds;
	uint8_t		  redraw_rows = 0x1F;
	uint8_t		  value_step  = 1;
	uint8_t		  row;
	uint8_t		  minimum;
	uint8_t		  maximum;
	uint8_t*	  selected_value;
	uint16_t	  y;
	uint16_t	  color;

	uint8_t	 ret		 = MCP_ERROR;
	uint8_t	 retry_count = 10;
	uint32_t id;

	MCP25625_SetOneShot(ENABLE);
	id	= CAN_CreateExtendedId(CAN_DEST_BROADCAST, CAN_CMD_MODE_SETTING, 0);
	ret = MCP25625_Send(id, 0, 0);
	while ((ret == MCP_ERROR) && (retry_count > 0))
	{
		retry_count--;
		__delay_ms(100);
		ret = MCP25625_Send(id, 0, 0);
	}
	MCP25625_SetOneShot(DISABLE);

	// ST7789_FillScreen(COLOR_WHITE);
	ST7789_StringSize_centing("CLOCK SETTINGS", 20, COLOR_BLACK, COLOR_WHITE, 2);
	ST7789_StringSize_centing("CENTER: STEP 1/10", 165, COLOR_BLACK, COLOR_WHITE, 1);
	ST7789_StringSize_centing("UP/DOWN: SELECT", 180, COLOR_BLACK, COLOR_WHITE, 1);
	ST7789_StringSize_centing("LEFT/RIGHT: CHANGE", 190, COLOR_BLACK, COLOR_WHITE, 1);
	ST7789_StringSize_centing("A: SAVE    B: BACK", 200, COLOR_BLACK, COLOR_WHITE, 1);

	while (1)
	{
		for (row = 0; row < 5; row++)
		{
			if (redraw_rows & (1 << row))
			{
				y = 45 + (row * 25);
				ST7789_StringSize("                    ", 0, y, COLOR_WHITE, COLOR_WHITE, 2);
				if (row == 0)
				{
					if (value_step == 10)
						sprintf(line, "HOUR x10: %02u", (unsigned int)edited_settings.hours);
					else
						sprintf(line, "HOUR x1 : %02u", (unsigned int)edited_settings.hours);
				}
				else if (row == 1)
				{
					if (value_step == 10)
						sprintf(line, "MIN x10: %02u", (unsigned int)edited_settings.minutes);
					else
						sprintf(line, "MIN x1 : %02u", (unsigned int)edited_settings.minutes);
				}
				else if (row == 2)
				{
					if (value_step == 10)
						sprintf(line, "SEC x10: %02u", (unsigned int)edited_settings.seconds);
					else
						sprintf(line, "SEC x1 : %02u", (unsigned int)edited_settings.seconds);
				}
				else if (row == 3)
					sprintf(line, "RULE: %s", rule_names[edited_settings.rule]);
				else
				{
					if (value_step == 10)
						sprintf(line, "VALUE x10: %02u SEC", (unsigned int)edited_settings.rule_seconds);
					else
						sprintf(line, "VALUE x1 : %02u SEC", (unsigned int)edited_settings.rule_seconds);
				}
				color = selected == row ? COLOR_RED : COLOR_BLACK;
				ST7789_StringSize_centing(line, y, color, COLOR_WHITE, 2);
			}
		}
		redraw_rows = 0;

		previous_selected = selected;
		sw_flag			  = get_sw();
		if (sw_flag.data.single.up) selected = (selected + 4) % 5;
		if (sw_flag.data.single.down) selected = (selected + 1) % 5;
		if (selected != previous_selected)
		{
			redraw_rows |= (1 << previous_selected) | (1 << selected);
		}
		if (sw_flag.data.single.center && selected != 3)
		{
			value_step = value_step == 1 ? 10 : 1;
			redraw_rows |= 1 << selected;
		}

		if (sw_flag.data.single.left || sw_flag.data.single.right)
		{
			int8_t change		  = sw_flag.data.single.right ? 1 : -1;
			previous_hours		  = edited_settings.hours;
			previous_minutes	  = edited_settings.minutes;
			previous_seconds	  = edited_settings.seconds;
			previous_rule		  = edited_settings.rule;
			previous_rule_seconds = edited_settings.rule_seconds;
			selected_value		  = 0;
			minimum				  = 0;
			maximum				  = 0;

			if (selected == 0)
			{
				selected_value = &edited_settings.hours;
				maximum		   = 98;
			}
			else if (selected == 1)
			{
				selected_value = &edited_settings.minutes;
				maximum		   = 59;
			}
			else if (selected == 2)
			{
				selected_value = &edited_settings.seconds;
				maximum		   = 59;
			}
			else if (selected == 3)
			{
				edited_settings.rule = (edited_settings.rule + (change > 0 ? 1 : 2)) % 3;
				if (edited_settings.rule == CLOCK_RULE_NONE)
					edited_settings.rule_seconds = 0;
				else if (edited_settings.rule_seconds == 0)
					edited_settings.rule_seconds = 5;
			}
			else if (edited_settings.rule != CLOCK_RULE_NONE)
			{
				selected_value = &edited_settings.rule_seconds;
				minimum		   = 1;
				maximum		   = 30;
			}
			if (selected_value != 0)
			{
				if (change > 0 && *selected_value <= maximum - value_step) *selected_value += value_step;
				if (change < 0 && *selected_value >= minimum + value_step) *selected_value -= value_step;
			}

			if (edited_settings.hours != previous_hours) redraw_rows |= 1 << 0;
			if (edited_settings.minutes != previous_minutes) redraw_rows |= 1 << 1;
			if (edited_settings.seconds != previous_seconds) redraw_rows |= 1 << 2;
			if (edited_settings.rule != previous_rule) redraw_rows |= 1 << 3;
			if (edited_settings.rule_seconds != previous_rule_seconds) redraw_rows |= 1 << 4;
		}
		if (sw_flag.data.single.a)
		{
			clock_settings = edited_settings;
			return;
		}
		if (sw_flag.data.single.b) return;
		__delay_ms(10);
	}
}


/**
 * @brief 
 * 
 */
/**
 * @brief 
 * 
 */
void mesurement_mode(void)
{
	sw_flag.data.full = 0;
	while (1)
	{
		sw_flag = get_sw();
		if (sw_flag.data.single.b)
		{
			sw_flag.data.single.b = 0;
			return;
		}
	}
}


/* --- 初期化関数 --- */
void Init(void)
{
	/* I/Oポート設定 */
	TRISA = 0xFF;
	TRISB = 0xFF;
	TRISC = 0x10;
	TRISD = 0x00;
	TRISE = 0x00;


	/* 出力値 */
	LATC = 0x13;
	LATD = 0x00;
	LATE = 0x07;

	/* Analog/Digital設定 */
	ADCON1bits.PCFG = 0b1111; /*全ポートDigポート*/

	/* INT0初期設定 */
	INTCONbits.INT0IF	= 0;
	INTCONbits.INT0IE	= 0; /* Disables the INT0 external interrupt */
	INTCON2bits.INTEDG0 = 1; /* Interrupt on Rising edge */

	/* INT1初期設定 */
	INTCON3bits.INT1IF	= 0;
	INTCON3bits.INT1IE	= 1; /* Enables the INT1 external interrupt */
	INTCON2bits.INTEDG1 = 0; /* Interrupt on Falling edge */

	/* Timer0初期設定 10msタイマー*/
	T0CONbits.T08BIT  = 0;	  /* 16bit */
	T0CONbits.T0CS	  = 0;	  /* 内部クロック 1/40Mx4=0.1us*/
	T0CONbits.PSA	  = 0;	  /* PS ON */
	T0CONbits.T0PS	  = 2;	  /* 1/8 0.8us */
	TMR0H			  = 0xCF; /* 12500 10ms */
	TMR0L			  = 0x2C; /* 12500 10ms */
	T0CONbits.TMR0ON  = 0;	  /* timer0 ON */
	INTCONbits.TMR0IF = 0;
	INTCONbits.TMR0IE = 1; /* Disables the TMR0 overflow interrupt */

	/* TIMER1初期設定 1ms 7SEG表示用*/
	T1CONbits.RD16	  = 1;	  /* 16bit mode */
	T1CONbits.T1CKPS  = 3;	  /* 11 = 1:8 Prescale value */
	T1CONbits.TMR1CS  = 0;	  /* 0 = internal clock   */
	T1CONbits.T1OSCEN = 0;	  /* ext off */
	TMR1H			  = 0xFB; /* 1250 1ms */
	TMR1L			  = 0x1D; /* 1250 1ms */
	T1CONbits.TMR1ON  = 1;	  /* 1 = Enables Timer1  */
	PIR1bits.TMR1IF	  = 1;
	PIE1bits.TMR1IE	  = 1; /* Enables the TMR1 ocerflow interrupt */

	/* TIMER2初期設定 BUZZER出力用*/
	T2CON			 = 0x03;
	T2CONbits.TMR2ON = 0;
	CCP1CON			 = 0x0C;
	PR2				 = 255;
	CCPR1			 = PR2 / 2;

	/* TIMER3初期設定 BUZZER用タイマー*/
	T3CONbits.RD16	 = 1; /* 16bit */
	T3CONbits.T3CCP1 = 0;
	T3CONbits.T3CCP2 = 0;
	T3CONbits.T3CKPS = 0b11; /* PS 1/8 0.8us */
	T3CONbits.TMR3CS = 0;	 /* 0 = internal clock */
	T3CONbits.TMR3ON = 0;
	TMR3H			 = 0x00;
	TMR3L			 = 0x00;
	PIR2bits.TMR3IF	 = 1;
	PIE2bits.TMR3IE	 = 1; /* Enables the TMR3 ocerflow interrupt */

	/* SPI 設定 */
	SSPSTAT = 0x40;
	SSPCON1 = 0x20;

	INTCONbits.PEIE = 1; /* Peripheral Interrupt Enable bit */
	INTCONbits.GIE	= 0; /* Global Interrupt Enable bit */

	return;
}

/* --- メイン関数 --- */
void main(void)
{
	uint8_t	 ret			 = 0;
	uint8_t	 mode			 = 0;
	uint32_t id				 = 0;
	uint16_t destinationType = 0;
	uint16_t command		 = 0;
	uint16_t targetId		 = 0;
	uint8_t	 str[20]		 = { 0 };
	uint8_t	 data[20]		 = { "ok" };

	Init(); /* MFB初期設定 */
	enable_disp_seg(0);


	if (UNIT_IDENTIFY == SLAVE_UNIT)
	{
		/* 子機の場合、子機の動作を行い続ける */
		slave_sequence();
	}

	ST7789_Init(); /* TFT初期化 */
	ST7789_FillScreen(COLOR_WHITE);

	/* CAN通信モジュール初期化 */
	while (MCP25625_Init(CAN_DEST_MASK, (uint32_t)CAN_DEST_MASTER << 24) != MCP_OK);

	INTCONbits.GIE = 1; /* Global Interrupt Enable bit */
	while (1)
	{
		mode = mode_select();
		ST7789_FillScreen(COLOR_WHITE);

		switch (mode)
		{
			case 0:
				game_mode(0);
				break;
			case 1:
				game_mode(1);
				break;
			case 2:
				setting_mode();
				break;
			case 3:
				mesurement_mode();
				break;
			default:
				break;
		}
		ST7789_FillScreen(COLOR_WHITE);
	}
}

/* --- 割り込み関数 --- */
void __interrupt() ISR(void)
{
	/* Timer0割り込み 10ms */
	if ((INTCONbits.TMR0IF == 1) && (INTCONbits.TMR0IE == 1))
	{
		INTCONbits.TMR0IF = 0;
		TMR0H			  = 0xCF; /* 12500 10ms */
		TMR0L			  = 0x2C; /* 12500 10ms */

		if ((timer.increment < 0) && timer.delay_flag && (timer.delay_ticks > 0))
		{
			timer.delay_ticks--;
		}
		else
		{
			timer.ms += timer.increment;
			if (timer.increment > 0)
			{
				if (timer.ms >= 100)
				{
					timer.ms = 0;
					timer.sec++;
					if (timer.sec >= 60)
					{
						timer.sec = 0;
						timer.min++;
						if (timer.min >= 60)
						{
							timer.min = 0;
							timer.hour++;
							timer.showHour_flag = 1;
							if (timer.hour >= 99)
							{
								timer.hour		  = 0;
								INTCONbits.TMR0IE = 0;
							}
						}
					}
				}
			}
			else
			{
				if ((timer.ms == 0) && (timer.sec == 0) && (timer.min == 0) && (timer.hour == 0))
				{
					timer.timeUp_flag = 1;
					INTCONbits.TMR0IE = 0;
				}

				if (timer.ms < 0)
				{
					timer.ms = 99;
					timer.sec--;
					if (timer.sec < 0)
					{
						timer.sec = 59;
						timer.min--;
						if (timer.min < 0)
						{
							timer.min = 59;
							timer.hour--;
							timer.showHour_flag = 0;
						}
					}
				}
			}
		}
	}

	/* Timer1割込 1ms */
	if ((PIR1bits.TMR1IF == 1) && (PIE1bits.TMR1IE == 1))
	{
		PIR1bits.TMR1IF = 0;
		TMR1H			= 0xFB; /* 1250 1ms */
		TMR1L			= 0x1D; /* 1250 1ms */

		SEG_DP = 0;
		SEG_EN = 0;
		switch (seg_cnt)
		{
			case 0:
				LATD = 0x00;
				if (timer.showHour_flag)
					SEG_DATA(timer.hour / 10);
				else
					SEG_DATA(timer.min / 10);
				break;
			case 1:
				LATD = 0x10;
				if (timer.showHour_flag)
					SEG_DATA(timer.hour % 10);
				else
					SEG_DATA(timer.min % 10);
				break;
			case 2:
				LATD = 0x20;
				if (timer.showHour_flag)
					SEG_DATA(timer.min / 10);
				else
					SEG_DATA(timer.sec / 10);
				break;
			case 3:
				LATD = 0x30;
				if (timer.showHour_flag)
					SEG_DATA(timer.min % 10);
				else
				{
					SEG_DATA(timer.sec % 10);
					SEG_DP = 1;
				}
				break;
			case 4:
				LATD = 0x40;
				if (timer.showHour_flag)
					SEG_DATA(timer.sec / 10);
				else
					SEG_DATA(timer.ms / 10);
				break;
			case 5:
				LATD = 0x50;
				if (timer.showHour_flag)
					SEG_DATA(timer.sec % 10);
				else
					SEG_DATA(timer.ms % 10);
				break;
		}
		SEG_EN = 1;
		seg_cnt++;
		seg_cnt %= 6;
	}


	/* Timer3割り込み 100ms */
	if ((PIR2bits.TMR3IF == 1) && (PIE2bits.TMR3IE == 1))
	{
		PIR2bits.TMR3IF	 = 0;
		T2CONbits.TMR2ON = 0;
		T3CONbits.TMR3ON = 0;
		TMR3H			 = 0x00;
		TMR3L			 = 0x00;
	}

	/* INT0外部割り込み */
	if ((INTCONbits.INT0IF == 1) && (INTCONbits.INT0IE == 1))
	{
		INTCONbits.INT0IF = 0;
		main_sw_flag	  = 1;
	}

	/* INT1外部割込み */
	if ((INTCON3bits.INT1IF == 1) && (INTCON3bits.INT1IE == 1))
	{
		INTCON3bits.INT1IF = 0;
		can_flag		   = 1;
	}
	return;
}
