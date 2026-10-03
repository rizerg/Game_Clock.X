#include "MCP25625.h"


static uint8_t	SPI_Transfer(uint8_t data);
static void		MCP25625_Reset(void);
static uint32_t MCP25625_ReadRxExtendedId(uint8_t baseAddress);
static void		MCP25625_ClearReceiveOverflow(void);

static uint8_t SPI_Transfer(uint8_t data)
{
	SSPBUF = data;
	while (!SSPSTATbits.BF);
	return SSPBUF;
}

static void MCP25625_Reset(void)
{
	CAN_CS = 0;

	SPI_Transfer(MCP_CMD_RESET);

	CAN_CS = 1;
}

static void MCP25625_ClearReceiveOverflow(void)
{
	uint8_t overflowFlags;

	overflowFlags = MCP25625_Read(MCP_EFLG) & MCP_RXOVR_MASK;
	if (overflowFlags != 0)
	{
		MCP25625_BitModify(MCP_EFLG, overflowFlags, 0x00);
	}
}

/**
 * @brief 指定したアドレス値を読み込み
 * 
 * @param addr アドレス値
 * @return uint8_t 読み込みデータ
 */
uint8_t MCP25625_Read(uint8_t addr)
{
	uint8_t data;

	CAN_CS = 0;

	SPI_Transfer(MCP_CMD_READ);
	SPI_Transfer(addr);

	data = SPI_Transfer(0xFF);

	CAN_CS = 1;

	return data;
}

/**
 * @brief 指定したアドレスに書き込み
 * 
 * @param addr アドレス値
 * @param data 書き込みデータ
 */
void MCP25625_Write(uint8_t addr, uint8_t data)
{
	CAN_CS = 0;

	SPI_Transfer(MCP_CMD_WRITE);
	SPI_Transfer(addr);
	SPI_Transfer(data);

	CAN_CS = 1;
}

/**
 * @brief 指定したアドレスのうち、マスクで選んだビットだけを書き換える
 * 		  マスクで選択すれば複数bitも書き換え可
 * 		  変更後 = (変更前 & ~mask) | (data & mask);
 * 		  使用例：RX0IFだけを0へクリア
 * 				 MCP25625_BitModify(MCP_CANINTF, MCP_RX0IF, 0x00U);
 * @param addr アドレス値
 * @param mask マスク値
 * @param data 書き込みデータ
 */
void MCP25625_BitModify(uint8_t addr, uint8_t mask, uint8_t data)
{
	CAN_CS = 0;

	SPI_Transfer(MCP_CMD_BITMOD);

	SPI_Transfer(addr);
	SPI_Transfer(mask);
	SPI_Transfer(data);

	CAN_CS = 1;
}

/**
 * @brief モード切替関数
 * 
 * @param mode 変更したいモード
 * @return uint8_t 通信結果		MCP_OK：成功 MCP_ERROR：失敗
 */
uint8_t MCP25625_SetMode(uint8_t mode)
{
	uint16_t timeout;
	mode &= MCP_MODE_MASK;
	timeout = 1000;
	MCP25625_BitModify(MCP_CANCTRL, MCP_MODE_MASK, mode);
	while (timeout != 0)
	{
		if ((MCP25625_Read(MCP_CANSTAT) & MCP_MODE_MASK) == mode)
		{
			return MCP_OK;
		}
		__delay_us(10);
		timeout--;
	}
	return MCP_ERROR;
}

/**
 * @brief 
 * 
 * @param enable 
 * @return uint8_t 
 */
uint8_t MCP25625_SetOneShot(uint8_t enable)
{
	uint8_t previousMode;

	previousMode = MCP25625_Read(MCP_CANSTAT) & MCP_MODE_MASK;
	if (MCP25625_SetMode(MCP_MODE_CONFIG) == MCP_ERROR)
	{
		return MCP_ERROR;
	}
	if (enable != 0)
	{
		MCP25625_BitModify(MCP_CANCTRL, MCP_CANCTRL_OSM, MCP_CANCTRL_OSM);
	}
	else
	{
		MCP25625_BitModify(MCP_CANCTRL, MCP_CANCTRL_OSM, 0x00);
	}
	if (MCP25625_SetMode(previousMode) == MCP_ERROR)
	{
		return MCP_ERROR;
	}
	return MCP_OK;
}

/**
 * @brief MCP25625の初期化
 * 		  拡張CAN ID構成に合わせて (受信ID & maskId) == (filterId & maskId)となったときだけ受信が行われる
 * 
 * @param maskId 受信したidのマスク値 bit=1：その位置を比較する
 * @param receiveId 受信したいidの設定値
 * @return uint8_t 通信結果		MCP_OK：成功 MCP_ERROR：失敗
 */
uint8_t MCP25625_Init(uint32_t maskId, uint32_t filterId)
{
	CAN_CS_DIR	  = 0;
	CAN_RESET_DIR = 0;
	CAN_STBY_DIR  = 0;

	CAN_CS = 1;

	/* Hardware Reset */
	CAN_RESET = 0;
	__delay_ms(10);

	CAN_RESET = 1;
	__delay_ms(10);

	CAN_STBY = 0;

	maskId &= CAN_ID_MASK;
	filterId &= CAN_ID_MASK;

	/* Software Reset */
	MCP25625_Reset();

	__delay_ms(10);

	/* Configuration Mode */
	if (MCP25625_SetMode(MCP_MODE_CONFIG) == MCP_ERROR) return MCP_ERROR;

	/* OSC:16MHz, baudrate:100kbps, SamplePoint:75% */
	MCP25625_Write(MCP_CNF1, 0x04);
	MCP25625_Write(MCP_CNF2, 0xAC);
	MCP25625_Write(MCP_CNF3, 0x03);

	/* RXB0マスク設定 */
	MCP25625_Write(MCP_RXM0SIDH, (uint8_t)(maskId >> 21));
	/* SIDLだけbit7~4,1~0だけ値が入る, bit3のMIDE(拡張形式での比較をONにする) */
	MCP25625_Write(MCP_RXM0SIDH + 1, (uint8_t)(((maskId >> 13) & 0xE0) | ((maskId >> 16) & 0x03) | 0x08));
	MCP25625_Write(MCP_RXM0SIDH + 2, (uint8_t)(maskId >> 8));
	MCP25625_Write(MCP_RXM0SIDH + 3, (uint8_t)maskId);

	/* RXB0用フィルタRXF0 */
	MCP25625_Write(MCP_RXF0SIDH, (uint8_t)(filterId >> 21));
	/* SIDLだけbit7~4,1~0だけ値が入る, bit3のEXIDE(拡張形式での比較をONにする) */
	MCP25625_Write(MCP_RXF0SIDH + 1, (uint8_t)(((filterId >> 13) & 0xE0) | ((filterId >> 16) & 0x03) | 0x08));
	MCP25625_Write(MCP_RXF0SIDH + 2, (uint8_t)(filterId >> 8));
	MCP25625_Write(MCP_RXF0SIDH + 3, (uint8_t)filterId);

	/* RXB0用フィルタRXF1 */
	MCP25625_Write(MCP_RXF1SIDH, (uint8_t)(filterId >> 21));
	/* SIDLだけbit7~4,1~0だけ値が入る, bit3のEXIDE(拡張形式での比較をONにする) */
	MCP25625_Write(MCP_RXF1SIDH + 1, (uint8_t)(((filterId >> 13) & 0xE0) | ((filterId >> 16) & 0x03) | 0x08));
	MCP25625_Write(MCP_RXF1SIDH + 2, (uint8_t)(filterId >> 8));
	MCP25625_Write(MCP_RXF1SIDH + 3, (uint8_t)filterId);

	MCP25625_BitModify(MCP_CANINTF, MCP_RX0IF, 0x00);
	/* Normal Mode */
	if (MCP25625_SetMode(MCP_MODE_NORMAL) == MCP_ERROR) return MCP_ERROR;

	MCP25625_Write(MCP_CANINTE, MCP_RX0IE);

	return MCP_OK;
}


void MCP25625_RequestToSendTXB0(void)
{
	CAN_CS = 0;
	SPI_Transfer(MCP_CMD_RTS_TXB0);
	CAN_CS = 1;
}

/**
 * @brief マスク値の追加設定
 * 		  2個の受信マスクまで設定可能
 * @param maskNum マスク番号 0 or 1
 * @param maskId 受信したidのマスク値 bit=1：その位置を比較する
 * @return uint8_t 通信結果		MCP_OK：成功 MCP_ERROR：失敗
 */
uint8_t MCP25625_SetReceiveMask(uint8_t maskNum, uint32_t maskId)
{
	const uint8_t maskAdress[2]	  = { MCP_RXM0SIDH, MCP_RXM1SIDH };
	uint8_t		  interruptEnable = 0;

	if (maskNum > 1) return MCP_ERROR;
	maskId &= CAN_ID_MASK;

	interruptEnable = MCP25625_Read(MCP_CANINTE);
	MCP25625_Write(MCP_CANINTE, 0x00);

	/* Configuration Mode */
	if (MCP25625_SetMode(MCP_MODE_CONFIG) == MCP_ERROR) return MCP_ERROR;

	/* RXB用マスク */
	MCP25625_Write(maskAdress[maskNum], (uint8_t)(maskId >> 21));
	/* SIDLだけbit7~4,1~0だけ値が入る, bit3のMIDE(拡張形式での比較をONにする) */
	MCP25625_Write(maskAdress[maskNum] + 1, (uint8_t)(((maskId >> 13) & 0xE0) | ((maskId >> 16) & 0x03) | 0x08));
	MCP25625_Write(maskAdress[maskNum] + 2, (uint8_t)(maskId >> 8));
	MCP25625_Write(maskAdress[maskNum] + 3, (uint8_t)maskId);

	MCP25625_BitModify(MCP_CANINTF, MCP_RX0IF | MCP_RX1IF, 0x00);
	/* Normal Mode */
	if (MCP25625_SetMode(MCP_MODE_NORMAL) == MCP_ERROR) return MCP_ERROR;

	MCP25625_Write(MCP_CANINTE, interruptEnable);

	return MCP_OK;
}

/**
 * @brief フィルタ値の追加設定
 * 		  6個の受信フィルタまで設定可能
 * 		  マスク0にフィルタ0～1、マスク1にフィルタ2～5が対応している
 * @param filterNum フィルタ番号 0 ～ 5
 * @param receiveId 受信したいidの設定値
 * @return uint8_t 通信結果		MCP_OK：成功 MCP_ERROR：失敗
 */
uint8_t MCP25625_SetReceiveFilter(uint8_t filterNum, uint32_t receiveId)
{
	const uint8_t filterAdress[6] = { MCP_RXF0SIDH, MCP_RXF1SIDH, MCP_RXF2SIDH, MCP_RXF3SIDH, MCP_RXF4SIDH, MCP_RXF5SIDH };
	uint8_t		  interruptEnable = 0;

	if (filterNum > 5) return MCP_ERROR;
	receiveId &= CAN_ID_MASK;

	interruptEnable = MCP25625_Read(MCP_CANINTE);
	MCP25625_Write(MCP_CANINTE, 0x00);

	/* Configuration Mode */
	if (MCP25625_SetMode(MCP_MODE_CONFIG) == MCP_ERROR) return MCP_ERROR;

	/* RXB0用フィルタ */
	MCP25625_Write(filterAdress[filterNum], (uint8_t)(receiveId >> 21));
	/* SIDLだけbit7~4,1~0だけ値が入る, bit3のEXIDE(拡張形式での比較をONにする) */
	MCP25625_Write(filterAdress[filterNum] + 1, (uint8_t)(((receiveId >> 13) & 0xE0) | ((receiveId >> 16) & 0x03) | 0x08));
	MCP25625_Write(filterAdress[filterNum] + 2, (uint8_t)(receiveId >> 8));
	MCP25625_Write(filterAdress[filterNum] + 3, (uint8_t)receiveId);

	if (filterNum < 2)
	{
		interruptEnable |= MCP_RX0IE;
		/* RXB0CTRL, ID Filter：ON, ロールオーバー：Disabled */
		MCP25625_BitModify(MCP_RXB0CTRL, MCP_RXM_MASK | MCP_BUKT, 0x00);
	}
	else
	{
		interruptEnable |= MCP_RX1IE;
		/* RXB1CTRL, ID Filter：ON, ロールオーバー：Disabled */
		MCP25625_BitModify(MCP_RXB1CTRL, MCP_RXM_MASK, 0x00);
	}

	MCP25625_BitModify(MCP_CANINTF, MCP_RX0IF | MCP_RX1IF, 0x00);

	/* Normal Mode */
	if (MCP25625_SetMode(MCP_MODE_NORMAL) == MCP_ERROR) return MCP_ERROR;

	MCP25625_Write(MCP_CANINTE, interruptEnable);

	return MCP_OK;
}

/**
 * @brief 送信用関数 本機器でのidの構成は以下のように設定すること
 * 		  28~24bit(5bit)：宛先の種別
 *		  23~16bit(8bit)：コマンド
 *		  15~0bit(16bit)：対象ID／識別値／パラメータ
 * @param id 拡張CAN ID値 29bit
 * @param data 送信するデータの配列
 * @param length dataのデータ数 0～8 length = 0ならばdataは送信されない
 * @return uint8_t 通信結果		MCP_OK：成功 MCP_ERROR：失敗
 */
uint8_t MCP25625_Send(uint32_t id, const uint8_t *data, uint8_t length)
{
	uint8_t	 index;
	uint8_t	 txControl;
	uint16_t timeout = 5000;

	if ((id > CAN_ID_MASK) || (length > 8) || ((length != 0) && (data == 0)))
	{
		return MCP_ERROR;
	}
	if ((MCP25625_Read(MCP_TXB0CTRL) & 0x08) != 0)
	{
		return MCP_ERROR;
	}

	/* 前回の送信完了フラグを消去 */
	MCP25625_BitModify(MCP_CANINTF, MCP_TX0IF, 0x00);

	/* ID書き込み 29bit */
	MCP25625_Write(MCP_TXB0SIDH, (uint8_t)(id >> 21));
	MCP25625_Write(MCP_TXB0SIDL, (uint8_t)(((id >> 13) & 0xE0) | ((id >> 16) & 0x03) | MCP_EXIDE));
	MCP25625_Write(MCP_TXB0EID8, (uint8_t)(id >> 8));
	MCP25625_Write(MCP_TXB0EID0, (uint8_t)id);

	/* データ長書き込み */
	MCP25625_Write(MCP_TXB0DLC, length);

	/* データ書き込み */
	for (index = 0; index < length; index++)
	{
		MCP25625_Write((uint8_t)(MCP_TXB0D0 + index), data[index]);
	}

	/* 送信要求 */
	CAN_CS = 0;
	SPI_Transfer(MCP_CMD_RTS_TXB0);
	CAN_CS = 1;

	/* 送信完了待ち */
	while (timeout != 0)
	{
		txControl = MCP25625_Read(MCP_TXB0CTRL);
		if ((txControl & MCP_TXREQ) == 0)
		{
			if ((txControl & (MCP_ABTF | MCP_TXERR | MCP_MLOA)) != 0)
			{
				return MCP_ERROR;
			}
			if ((MCP25625_Read(MCP_CANINTF) & MCP_TX0IF) == 0)
			{
				return MCP_ERROR;
			}
			MCP25625_BitModify(MCP_CANINTF, MCP_TX0IF, 0x00);
			return MCP_OK;
		}
		__delay_us(10);
		timeout--;
	}
	MCP25625_BitModify(MCP_TXB0CTRL, MCP_TXREQ, 0x00);
	return MCP_ERROR;
}

/**
 * @brief 受信用関数
 * 		  受信中は割り込み不許可
 * 
 * @param message 受信用構造体のポインタ
 * @return uint8_t 通信結果		MCP_OK：成功 MCP_ERROR：失敗
 */
uint8_t MCP25625_Receive(MCP25625_Message *message)
{
	uint8_t flags;
	uint8_t baseAddress;
	uint8_t clearMask;
	uint8_t sidl;
	uint8_t index;
	uint8_t preview_gie;

	if (message == 0) return MCP_ERROR;
	preview_gie	   = INTCONbits.GIE;
	INTCONbits.GIE = 0; /* 受取中の割り込み不許可 */

	MCP25625_ClearReceiveOverflow();
	flags = MCP25625_Read(MCP_CANINTF);

	/* 受信データがなければ終了 */
	if ((flags & MCP_RX0IF) != 0)
	{
		baseAddress = MCP_RXB0SIDH;
		clearMask	= MCP_RX0IF;
	}
	else if ((flags & MCP_RX1IF) != 0)
	{
		baseAddress = MCP_RXB1SIDH;
		clearMask	= MCP_RX1IF;
	}
	else
	{
		INTCONbits.GIE = preview_gie; /* 割り込み戻す */
		return MCP_ERROR;
	}

	/* 拡張形式でなければエラー */
	sidl = MCP25625_Read((uint8_t)(baseAddress + 1));
	if ((sidl & MCP_EXIDE) == 0)
	{
		MCP25625_BitModify(MCP_CANINTF, clearMask, 0x00);
		MCP25625_ClearReceiveOverflow();
		INTCONbits.GIE = preview_gie; /* 割り込み戻す */
		return MCP_ERROR;
	}

	message->id = MCP25625_ReadRxExtendedId(baseAddress);

	/* DLC読み取り */
	message->length = MCP25625_Read((uint8_t)(baseAddress + 4)) & 0x0F;
	if (message->length > 8)
	{
		MCP25625_BitModify(MCP_CANINTF, clearMask, 0x00);
		MCP25625_ClearReceiveOverflow();
		INTCONbits.GIE = preview_gie; /* 割り込み戻す */
		return MCP_ERROR;
	}


	/* データ読み取り */
	for (index = 0; index < message->length; index++)
	{
		message->data[index] = MCP25625_Read((uint8_t)(baseAddress + 5 + index));
	}

	message->destinationType = CAN_GetDestinationType(message->id);
	message->command		 = CAN_GetCommand(message->id);
	message->targetId		 = CAN_GetTargetId(message->id);

	/* 全データを読んだ後にRXIFをクリア */
	MCP25625_BitModify(MCP_CANINTF, clearMask, 0x00);
	MCP25625_ClearReceiveOverflow();
	INTCONbits.GIE = preview_gie; /* 割り込み戻す */

	return MCP_OK;
}

/**
 * @brief 拡張CAN ID整形関数
 * 		  引数の値から29bitの拡張CAN IDの形にして返す
 * 
 * @param destinationType 宛先の値
 * @param command コマンド値
 * @param targetId 対象ID／識別値／パラメータなど
 * @return uint32_t 29bitの拡張CAN ID
 */
uint32_t CAN_CreateExtendedId(uint8_t destinationType, uint8_t command, uint16_t targetId)
{
	uint32_t id;
	id = ((uint32_t)(destinationType & 0x1F) << 24);
	id |= ((uint32_t)command << 16);
	id |= (uint32_t)targetId;
	return id;
}

/**
 * @brief 受信したデータから29bitの拡張CAN IDの形にして返す
 * 
 * @param baseAddress 受信したレジスタのアドレス MCP_RXB0SIDH or MCP_RXB1SIDH
 * @return uint32_t 29bitの拡張CAN ID
 */
static uint32_t MCP25625_ReadRxExtendedId(uint8_t baseAddress)
{
	uint8_t	 sidh;
	uint8_t	 sidl;
	uint8_t	 eid8;
	uint8_t	 eid0;
	uint32_t id;

	sidh = MCP25625_Read(baseAddress);
	sidl = MCP25625_Read((uint8_t)(baseAddress + 1));
	eid8 = MCP25625_Read((uint8_t)(baseAddress + 2));
	eid0 = MCP25625_Read((uint8_t)(baseAddress + 3));

	id = ((uint32_t)sidh << 21);
	id |= ((uint32_t)(sidl & 0xE0) << 13);
	id |= ((uint32_t)(sidl & 0x03) << 16);
	id |= ((uint32_t)eid8 << 8);
	id |= (uint32_t)eid0;
	return id;
}

/**
 * @brief 29bitの拡張CAN IDから宛先を取得する
 * 
 * @param id 29bitの拡張CAN ID
 * @return uint8_t 宛先値
 */
uint8_t CAN_GetDestinationType(uint32_t id)
{
	return (uint8_t)((id >> 24) & 0x1F);
}

/**
 * @brief 29bitの拡張CAN IDからコマンド値を取得する
 * 
 * @param id 29bitの拡張CAN ID
 * @return uint8_t コマンド値
 */
uint8_t CAN_GetCommand(uint32_t id)
{
	return (uint8_t)((id >> 16) & 0xFF);
}

/**
 * @brief 29bitの拡張CAN IDから対象ID／識別値／パラメータなどを取得する
 * 
 * @param id 29bitの拡張CAN ID
 * @return uint16_t 対象ID／識別値／パラメータなど
 */
uint16_t CAN_GetTargetId(uint32_t id)
{
	return (uint16_t)(id & 0xFFFF);
}