#include <stdlib.h>
#include <stdio.h>
#include "serial_win32.h"

#define BUF_LEN 240
static char cmd_buf[BUF_LEN];
static char data[BUF_LEN];

static char frame_head = 0xaa;   //֡ͷ �̶�Ϊ0xaa
static char frame_length = 0x00; //Index + CMD +Data + check �ĳ���
static char frame_index = 0x01;  //������0x01 �նˣ�0x01~0xFF
static char frame_cmd = 0x00;    //��������֡������
//Data[N](����) ��֡��������
static char frame_check = 0x00;  //У�鷶Χ��Length+ Index+ CMD+ Data��
static char frame_end = 0xdd;	  //֡β �̶�Ϊ0xDD

static char frame_cmd_query = 0x01;//��ѯ���� ��ָ���û���ѯ�����Ƿ�������
static char frame_cmd_score = 0x03;//���Ϸ� ��ָ���������ն˷�������Ͷ������,���ֽ����ݵ�λ��ǰ��
static char frame_cmd_account = 0x04; //��ָ�����ڲ�ѯ�豸����Ŀ״̬������״̬�ȡ�
static char frame_cmd_query_incremental = 0x09; //��ѯ���������޻���
static char frame_cmd_upload_incremental_ack = 0x13; //�ն������ϴ���Ŀ����Ӧ�����޻��� 
static char frame_cmd_query_signal_ack = 0x19;//��ѯ�ź�Ӧ��
static char frame_cmd_timer_query_total_accounts = 0x1A;//��ʱ��ѯ���ˣ����޻���
static char frame_cmd_rocker_control = 0xA1;//ҡ�˿���
static char frame_cmd_rocker_reset = 0xfc;//�쳵��λ��������
static char frame_cmd_set = 0x06;//����




static int serial_write_data(HANDLE SerialHandle, char* data, size_t length)
{
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		DWORD WriteNum = 0;
		if (WriteFile(SerialHandle, data, length, &WriteNum, 0))
			return 0;
		return -1;
	}
}

static int serial_cmd_help(char* buf,int buf_size,char cmd, char* data,int data_size,int* length)
{
	if (buf_size < (6 + data_size))
		return -1;
	//Index + CMD +Data + check �ĳ���
	 frame_length = 0x1 + 0x1 + data_size + 0x1;
	//Length+ Index+ CMD+ Data
	 frame_check = frame_length + frame_index + cmd;
	 buf[0] = frame_head;
	 buf[1] = frame_length;
	 buf[2] = frame_index;
	 buf[3] = cmd;
	 for (int i = 0;i < data_size;i++)
	 {
		 frame_check += data[i];
		 buf[4 + i] = data[i];
	 }
	 buf[4 + data_size] = frame_check ;
	 buf[4 + data_size + 1] = frame_end;
	 *length = 6 + data_size;
	 return 0;
}


//�򿪴���
HANDLE wwj_open_serial(const char* com)
{
	HANDLE SerialHandle;

	char buf[256] = { 0 };
	_snprintf(buf,256,"\\\\.\\%s", com);         //��ʽ���ַ���									//�򿪴���
	SerialHandle = CreateFileA(buf, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);    //ͬ��ģʽ�򿪴���
	if (SerialHandle == INVALID_HANDLE_VALUE)  //�򿪴���ʧ��
	{
		printf("open serial error!\n");
	}
	return SerialHandle;
}


int wwj_init_setting_param(wwj_set_param_t* param)
{
	int ret = -1;
	if (param != NULL)
	{
		param->once_need_coin=1;//����һ��
		param->game_paly_levle=5;//�н�����(1-99)  (�����N��ǿץ����������������Ϸ�Ѷ�)
		param->auto_down_claw=30;//�Զ���ץʱ��(5-60)
		param->power_time=20;//ǿ��ʱ��(5-99)
		param->prize_pattern=0;//�н�ģʽ(0 / 1 / 2  ������̶���ȡ��)
		param->sell_pattern=0;//����ģʽ(0/1/2  ǿ����ǿ�ͣ�ǿͶ)
		param->prize_score=20;   //����Ͷ���ͷ�(0 - 20, 0Ϊ�ر�)
		param->prize_claw_power=20;//�н�ץ��(5-50)      (����ܰ汾��Ч)
		param->power_claw=20;//	ǿצץ��(5-50)        (��צ��ͣǿ����)
		param->top_claw_power=20;//����ץ��(0 - 40, ����������צץ����ͬ)   (��ͣͣ��ץ��)
		param->weak_claw_power=20;// 	��צץ��(0 - 40)      (�쳵�ع�����)
		param->sell_function=0;//	�������ܿ���(0 / 1)
		param->prize_inductor=1;//     �н���Ӧ������(0 / 1)
		param->air_claw_thing=0; //    ����ץ�﹦�ܿ���(0 / 1)
		param->start_score_remain=0;//  	����������������(0 / 1)
		param->shake_clean_score_funtion=0;// 	ҡ����ֹ��ܿ���(0 / 1)
		param->front_back_speed=50;//     ǰ���ٶ�(5 - 50, Ĭ��50)
		param->left_right_speed=50;//     �����ٶ�(5 - 50, Ĭ��50)
		param->up_down_speed = 50;//     �����ٶ�(5 - 50, Ĭ��50)
		ret = 0;
	}
	return ret;
}

int wwj_set_serial_param(HANDLE SerialHandle,serial_param_t param)
{
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		//���ô��ڲ���
		//������ԭ���Ĳ�������
		DCB dcb;
		GetCommState(SerialHandle, &dcb);   //���ڴ򿪷�ʽ
											//���ڲ������� 
		dcb.DCBlength = sizeof(DCB);
		dcb.BaudRate = param.bound_rate;
		dcb.ByteSize = param.byte_size;
		//dcb.Parity = EVENPARITY;
		dcb.StopBits = ONESTOPBIT;
		dcb.fBinary = TRUE;                 //  ����������ģʽ
		dcb.fParity = TRUE;

		if (!SetCommState(SerialHandle, &dcb))
		{
			printf("set serial param error!\n");
			return -1;
		}

		SetupComm(SerialHandle, 1024, 1024);    //���û�����
		return 0;
	}
	return -1;
}

int wwj_set_param(HANDLE SerialHandle, wwj_set_param_t param)
{
	int ret = -1;
	int length = 0;
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		data[0] = param.once_need_coin;//����һ��
		data[1] = param.game_paly_levle;//�н�����(1-99)  (�����N��ǿץ����������������Ϸ�Ѷ�)
		data[2] = param.auto_down_claw;//�Զ���ץʱ��(5-60)
		data[3] = param.power_time;//ǿ��ʱ��(5-99)
		data[4] = param.prize_pattern;//�н�ģʽ(0 / 1 / 2  ������̶���ȡ��)
		data[5] = param.sell_pattern;//����ģʽ(0/1/2  ǿ����ǿ�ͣ�ǿͶ)
		data[6] = param.prize_score;   //����Ͷ���ͷ�(0 - 20, 0Ϊ�ر�)
		data[7] = param.prize_claw_power;//�н�ץ��(5-50)      (����ܰ汾��Ч)
		data[8] = param.power_claw;//	ǿצץ��(5-50)        (��צ��ͣǿ����)
		data[9] = param.top_claw_power;//����ץ��(0 - 40, ����������צץ����ͬ)   (��ͣͣ��ץ��)
		data[10] = param.weak_claw_power;// 	��צץ��(0 - 40)      (�쳵�ع�����)
		data[11] = param.sell_function;//	�������ܿ���(0 / 1)
		data[12] = param.prize_inductor;//     �н���Ӧ������(0 / 1)
		data[13] = param.air_claw_thing; //    ����ץ�﹦�ܿ���(0 / 1)
		data[14] = param.start_score_remain;//  	����������������(0 / 1)
		data[15] = param.shake_clean_score_funtion;// 	ҡ����ֹ��ܿ���(0 / 1)
		data[16] = param.front_back_speed;//     ǰ���ٶ�(5 - 50, Ĭ��50)
		data[17] = param.left_right_speed;//     �����ٶ�(5 - 50, Ĭ��50)
		data[18] = param.up_down_speed;//     �����ٶ�(5 - 50, Ĭ��50)
		
		if (serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_rocker_control, data, 19, &length) == 0)
		{
			serial_write_data(SerialHandle, cmd_buf, length);
			ret = 0;
		}
		else
			printf("func:%s get cmd error\n", __FUNCTION__);
	}
	return ret;
}

int wwj_serial_directect_opt(HANDLE SerialHandle, serial_direction_opt_type_e type)
{
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		int length;
		int data_len = 1;
		switch (type)
		{
		case kfront:
			data[0] = 0x01;
			break;
		case kclaw:
			data[0] = 0x10;
			data[1] = 0x02;
			data_len = 2;
			break;
		case kleft:
			data[0] = 0x08;
			break;
		case kright:
			data[0] = 0x04;
			break;
		case kback:
			data[0] = 0x02;
			break;
		}

		if (serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_rocker_control, data, data_len, &length) == 0)
			serial_write_data(SerialHandle, cmd_buf, length);
		else
			printf("func:%s get cmd error\n", __FUNCTION__);
		
	}
}

//�رմ���
int wwj_close_serial(HANDLE SerialHandle)
{
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(SerialHandle);
		SerialHandle = INVALID_HANDLE_VALUE;
		return 0;
	}
	else
		return -1;
}

int wwj_pay(HANDLE SerialHandle, int coins)
{
	int ret = -1;
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		static char count = 0;
		if (count > 50)
			count = 0;
		count++;
		data[0] = 0x02 + count;
		data[1] =(signed short) coins;
		//data[2]
		data[3] = (signed short)coins;
		//data[4]
		int length = 0;;
		if (serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_score, data, 8, &length) == 0)
		{
			serial_write_data(SerialHandle, cmd_buf, length);
			ret = 0;
		}
		else
		{
			printf("func:%s get cmd error\n", __FUNCTION__);
		}
	}
	return ret;
}

int wwj_crown_block_reset(HANDLE SerialHandle)
{
	int length = 0;
	int ret = -1;
	if (SerialHandle != INVALID_HANDLE_VALUE&&serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_rocker_reset, data, 0, &length) == 0)
	{
		serial_write_data(SerialHandle, cmd_buf, length);
		ret = 0;
	}
	else
	{
		printf("func:%s get cmd error\n", __FUNCTION__);
	}
	return ret; 
}

int wwj_check_normal(HANDLE SerialHandle)
{
	int ret = -1;
	int length = 0;
	if (SerialHandle != INVALID_HANDLE_VALUE&&serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_query, data, 8, &length) == 0)
	{
		serial_write_data(SerialHandle, cmd_buf, length);
		ret = 0;
	}
	return ret;
}