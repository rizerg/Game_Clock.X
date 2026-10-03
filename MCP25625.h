#ifndef _MCP25625_H_
#define _MCP25625_H_

#include <xc.h>
#include <stdint.h>

#ifndef _XTAL_FREQ
#define _XTAL_FREQ 40000000UL
#endif

/* =========================
 * MCP25625 control pins
 * ========================= */

#define CAN_CS LATCbits.LATC0
#define CAN_RESET LATCbits.LATC1
#define CAN_STBY LATCbits.LATC6

#define CAN_CS_DIR TRISCbits.TRISC0
#define CAN_RESET_DIR TRISCbits.TRISC1
#define CAN_STBY_DIR TRISCbits.TRISC6

/* =========================
 * SPI commands
 * ========================= */

#define MCP_CMD_RESET 0xC0U
#define MCP_CMD_READ 0x03U
#define MCP_CMD_WRITE 0x02U
#define MCP_CMD_BITMOD 0x05U
#define MCP_CMD_READ_STATUS 0xA0U
#define MCP_CMD_RTS_TXB0 0x81U

#define MCP_OK 1
#define MCP_ERROR 0

/* =========================
 * Acceptance filters
 * ========================= */

#define MCP_RXF0SIDH 0x00
#define MCP_RXF0SIDL 0x01
#define MCP_RXF0EID8 0x02
#define MCP_RXF0EID0 0x03

#define MCP_RXF1SIDH 0x04
#define MCP_RXF1SIDL 0x05
#define MCP_RXF1EID8 0x06
#define MCP_RXF1EID0 0x07

#define MCP_RXF2SIDH 0x08
#define MCP_RXF2SIDL 0x09
#define MCP_RXF2EID8 0x0A
#define MCP_RXF2EID0 0x0B

#define MCP_RXF3SIDH 0x10
#define MCP_RXF3SIDL 0x11
#define MCP_RXF3EID8 0x12
#define MCP_RXF3EID0 0x13

#define MCP_RXF4SIDH 0x14
#define MCP_RXF4SIDL 0x15
#define MCP_RXF4EID8 0x16
#define MCP_RXF4EID0 0x17

#define MCP_RXF5SIDH 0x18
#define MCP_RXF5SIDL 0x19
#define MCP_RXF5EID8 0x1A
#define MCP_RXF5EID0 0x1B

/* =========================
 * Acceptance masks
 * ========================= */

#define MCP_RXM0SIDH 0x20
#define MCP_RXM0SIDL 0x21
#define MCP_RXM0EID8 0x22
#define MCP_RXM0EID0 0x23

#define MCP_RXM1SIDH 0x24
#define MCP_RXM1SIDL 0x25
#define MCP_RXM1EID8 0x26
#define MCP_RXM1EID0 0x27

/* =========================
 * CAN configuration
 * ========================= */

#define MCP_CNF3 0x28
#define MCP_CNF2 0x29
#define MCP_CNF1 0x2A

#define MCP_CANINTE 0x2B
#define MCP_CANINTF 0x2C
#define MCP_EFLG 0x2D

#define MCP_CANSTAT 0x0E
#define MCP_CANCTRL 0x0F
#define MCP_CANCTRL_OSM 0x08

#define MCP_TXREQ 0x08
#define MCP_TXERR 0x10
#define MCP_ABTF 0x40
#define MCP_MLOA 0x20

#define MCP_RXM_MASK 0x60
#define MCP_BUKT 0x04

#define MCP_EXIDE 0x08


/* =========================
 * TX buffer 0
 * ========================= */

#define MCP_TXB0CTRL 0x30
#define MCP_TXB0SIDH 0x31
#define MCP_TXB0SIDL 0x32
#define MCP_TXB0EID8 0x33
#define MCP_TXB0EID0 0x34
#define MCP_TXB0DLC 0x35
#define MCP_TXB0D0 0x36

/* =========================
 * RX buffer 0
 * ========================= */

#define MCP_RXB0CTRL 0x60
#define MCP_RXB0SIDH 0x61
#define MCP_RXB0SIDL 0x62
#define MCP_RXB0EID8 0x63
#define MCP_RXB0EID0 0x64
#define MCP_RXB0DLC 0x65
#define MCP_RXB0D0 0x66

/* =========================
 * RX buffer 1
 * ========================= */

#define MCP_RXB1CTRL 0x70
#define MCP_RXB1SIDH 0x71
#define MCP_RXB1SIDL 0x72
#define MCP_RXB1EID8 0x73
#define MCP_RXB1EID0 0x74
#define MCP_RXB1DLC 0x75
#define MCP_RXB1D0 0x76

/* =========================
 * CAN modes
 * ========================= */

#define MCP_MODE_MASK 0xE0
#define MCP_MODE_NORMAL 0x00
#define MCP_MODE_SLEEP 0x20
#define MCP_MODE_LOOPBACK 0x40
#define MCP_MODE_LISTEN 0x60
#define MCP_MODE_CONFIG 0x80

/* =========================
 * Interrupt bits
 * ========================= */

#define MCP_RX0IE 0x01
#define MCP_RX1IE 0x02

#define MCP_RX0IF 0x01
#define MCP_RX1IF 0x02
#define MCP_TX0IF 0x04

#define MCP_RX0OVR 0x40
#define MCP_RX1OVR 0x80
#define MCP_RXOVR_MASK (MCP_RX0OVR | MCP_RX1OVR)

/* =========================
* Receive message
* ========================= */

typedef struct
{
	/* 拡張CAN IDの構成(29bit ID) */
	/* 28~24bit(5bit)：宛先の種別 */
	/* 23~16bit(8bit)：コマンド */
	/* 15~0bit(16bit)：対象ID／識別値／パラメータ */
	uint32_t id;
	uint8_t	 length;
	uint8_t	 data[8];
	uint8_t	 destinationType;
	uint8_t	 command;
	uint16_t targetId;
} MCP25625_Message;

/* =========================
 * CAN ID define
 * ========================= */
#define CAN_ID_MASK 0x1FFFFFFFUL /* IDマスク値 */

#define CAN_DEST_EMERGENCY 0x01	   /* 非常時用 */
#define CAN_DEST_BROADCAST 0x02	   /* 全子機 */
#define CAN_DEST_CHILD 0x03		   /* 親機から特定子機 */
#define CAN_DEST_MASTER 0x04	   /* 子機から親機 */
#define CAN_DEST_CHILD_DIRECT 0x05 /* 特定の子機間 */

#define CAN_DEST_MASK 0x1F000000UL /* 宛先のマスク値 */

#define CAN_CMD_GAME_END 0x01		 /* 非常停止 */
#define CAN_CMD_GAME_STOP 0x02		 /* ゲームの一時停止 */
#define CAN_CMD_TURN_START 0x03		 /* ターン開始 */
#define CAN_CMD_TURN_END 0x04		 /* ターン終了 */
#define CAN_CMD_TURN_END_ACK 0x43	 /* ターン終了受信確認 */
#define CAN_CMD_TURN_START_ACK 0x44	 /* ターン開始確認 */
#define CAN_CMD_MODE_COUNT_DOWN 0x10 /* カウントダウンモード */
#define CAN_CMD_MODE_COUNT_UP 0x11	 /* カウントアップモード */
#define CAN_CMD_MODE_SETTING 0x12	 /* 設定モード */
#define CAN_CMD_JOIN_OPEN 0x20		 /* 参加募集通知 */
#define CAN_CMD_JOIN_REQUEST 0x21	 /* 参加要求 */
#define CAN_CMD_ASSIGN_ID 0x22		 /* ID割当通知 */
#define CAN_CMD_ASSIGN_ACK 0x23		 /* 登録完了通知 */
#define CAN_CMD_ASSIGN_COMMIT 0x24	 /* 登録完了通知 */
#define CAN_CMD_JOIN_COMPLETE 0x25	 /* 参加募集完了通知 */
#define CAN_CMD_DECIDE_TURN 0x30	 /* 順番決め通知 */
#define CAN_CMD_TURN_QUEUE 0x31		 /* 順番要求 */
#define CAN_CMD_TURN_CLEAR 0x32		 /* 順番決めなおし */
#define CAN_CMD_ASSIGN_TURN 0x33	 /* 順番確定通知 */
#define CAN_CMD_GAME_START 0x40		 /* 状態確認用 */
#define CAN_CMD_GAME_TIME_UP 0x41	 /* 時間切れ */
#define CAN_CMD_GAME_READY 0x42		 /* ゲーム開始準備完了 */
#define CAN_CMD_STATUS 0x50			 /* 状態確認用 */

#define CAN_CMD_MASK 0x00FF0000UL /* コマンドのマスク値 */

#define CAN_PARAMS_MASK 0xFFFF /* パラメータのマスク値 */

/* =========================
 * Public functions
 * ========================= */

uint8_t	 MCP25625_Init(uint32_t maskId, uint32_t filterId);
uint8_t	 MCP25625_Read(uint8_t addr);
void	 MCP25625_Write(uint8_t addr, uint8_t data);
void	 MCP25625_BitModify(uint8_t addr, uint8_t mask, uint8_t data);
uint8_t	 MCP25625_SetMode(uint8_t mode);
uint8_t	 MCP25625_SetOneShot(uint8_t enable);
uint8_t	 MCP25625_Send(uint32_t id, const uint8_t *data, uint8_t length);
uint8_t	 MCP25625_Receive(MCP25625_Message *message);
uint8_t	 MCP25625_SetReceiveMask(uint8_t maskNum, uint32_t maskId);
uint8_t	 MCP25625_SetReceiveFilter(uint8_t filterNum, uint32_t receiveId);
uint8_t	 MCP25625_Receive(MCP25625_Message *message);
uint32_t CAN_CreateExtendedId(uint8_t destinationType, uint8_t command, uint16_t targetId);
uint8_t	 CAN_GetDestinationType(uint32_t id);
uint8_t	 CAN_GetCommand(uint32_t id);
uint16_t CAN_GetTargetId(uint32_t id);


#endif