#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include "serial_win32.h"

#define BUF_LEN 240
#define kSerialOptSuccess 200
#define kSerialOptFailed 100
static unsigned char cmd_buf[BUF_LEN];
static unsigned char data[BUF_LEN];
static unsigned char result_data[BUF_LEN];

static unsigned char frame_head = 0xaa;   //֡ͷ �̶�Ϊ0xaa
static unsigned char frame_length = 0x00; //Index + CMD +Data + check �ĳ���
static unsigned char frame_index = 0x01;  //������0x01 �նˣ�0x01~0xFF
static unsigned char frame_cmd = 0x00;    //��������֡������
//Data[N](����) ��֡��������
static unsigned char frame_check = 0x00;  //У�鷶Χ��Length+ Index+ CMD+ Data��
static unsigned char frame_end = 0xdd;	  //֡β �̶�Ϊ0xDD

static unsigned char wwj_step_size_ = 150;

HANDLE serial_read_thread_id;
BOOL serial_close_flag = FALSE;
wwj_callback_func wwj_opt_cb_;

enum wwj_cmd
{
	frame_cmd_query = 0x01,//��ѯ���� ��ָ���û���ѯ�����Ƿ�������
	frame_cmd_score = 0x03,//���Ϸ� ��ָ���������ն˷�������Ͷ������,���ֽ����ݵ�λ��ǰ��
	frame_cmd_account = 0x04, //��ָ�����ڲ�ѯ�豸����Ŀ״̬������״̬�ȡ�
	frame_cmd_query_incremental = 0x09, //��ѯ���������޻���
	frame_cmd_upload_incremental_ack = 0x13, //�ն������ϴ���Ŀ����Ӧ�����޻��� 
	frame_cmd_query_signal_ack = 0x19,//��ѯ�ź�Ӧ��
	frame_cmd_timer_query_total_accounts = 0x1A,//��ʱ��ѯ���ˣ����޻���
	frame_cmd_rocker_control = 0xA1,//ҡ�˿���
	frame_cmd_rocker_reset = 0xfc,//�쳵��λ��������
	frame_cmd_set = 0x06,//����
	frame_cmd_end=0xa2,//�쳵��λ
	frame_cmd_prize=0x13 //�н�
};


//���ó�ʱ
static int set_serial_com_timeout(HANDLE SerialHandle)
{
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		COMMTIMEOUTS com_timeout;
		com_timeout.ReadIntervalTimeout = 0;
		com_timeout.ReadTotalTimeoutConstant = 500;
		com_timeout.ReadTotalTimeoutMultiplier = 100;
		com_timeout.WriteTotalTimeoutConstant = 500;
		com_timeout.WriteTotalTimeoutMultiplier = 100;
		SetCommTimeouts(SerialHandle, &com_timeout);
		return 0;
	}
	return -1;
}

//read_size ������Ӧ��ȡ���ֽ���
//buf_offset ���ݻ���ƫ��
static int serial_read_data(HANDLE SerialHandle,int read_size,int buf_offset)
{
	DWORD  dwRead = 0;
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		BOOL bReadOk = ReadFile(SerialHandle, result_data+buf_offset, read_size, &dwRead, NULL);
		if (!bReadOk || (dwRead <= 0))
		{
			//printf("serial receive data failed\n");
		}
	}
	return dwRead;
}


static int serial_write_data(HANDLE SerialHandle, unsigned char* data, size_t length)
{
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		DWORD WriteNum = 0;
		if (WriteFile(SerialHandle, data, length, &WriteNum, 0))
			return 0;
	}
	return -1;
}



static int serial_cmd_help(unsigned char* buf,unsigned int buf_size,unsigned char cmd,unsigned char* data,size_t data_size,int* length)
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
	 for (size_t i = 0;i < data_size;i++)
	 {
		 frame_check += data[i];
		 buf[4 + i] = data[i];
	 }
	 buf[4 + data_size] = frame_check ;
	 buf[4 + data_size + 1] = frame_end;
	 *length = 6 + data_size;
	 return 0;
}

//�������߳�
unsigned int __stdcall serial_read_data_thread(LPVOID lpParam)
{
	HANDLE SerialHandle = (HANDLE)lpParam;
	int data_len = 0;
	BOOL bResult = TRUE;
	DWORD dwError = 0;
	COMSTAT com_stat;
	int ret = -1;
	while (!serial_close_flag)
	{
		bResult = ClearCommError(SerialHandle, &dwError, &com_stat);
		if (com_stat.cbInQue == 0)
			continue;
		memset(result_data, 0, BUF_LEN);
		data_len = serial_read_data(SerialHandle,1,0);
		//��������ָ��
		if (data_len > 0&&result_data[0]==frame_head)
		{
			data_len = serial_read_data(SerialHandle, 1, 1);
			printf("data[0]:%x data[1] %x\n", result_data[0], result_data[1]);
			if (data_len > 0)
			{
				data_len = serial_read_data(SerialHandle, result_data[1]+1, 2);
				printf("data[2]:%x data[3] %x\n", result_data[2], result_data[3]);
				if (data_len > 0)
				{
					switch (result_data[3])
					{
					case frame_cmd_score: //���Ϸ�Ӧ��
					{
						ret = result_data[4];
						//�ص�
						if (wwj_opt_cb_ != NULL)
						{
							wwj_opt_cb_(kgamepay, ret == 1 ? kSerialOptSuccess : kSerialOptFailed);
						}
						if (ret == 1)
						{
							printf("pay coin success\n");
						}
						else
						{
							printf("pay coin failed\n");
						}
					}
					break;
					case frame_cmd_end:
					{
						if (wwj_opt_cb_ != NULL)
						{
							wwj_opt_cb_(kgameend, kSerialOptSuccess);
						}
					}
					break;
					case frame_cmd_prize:
					{
						if (wwj_opt_cb_ != NULL)
						{
							wwj_opt_cb_(kgameprize, kSerialOptSuccess);
						}
					}
					default:
						break;
					}
				}
			}
		}
	}

	printf("read thread exit\n");
	return 0;
}
//�򿪴���
HANDLE wwj_open_serial(char* com)
{
	HANDLE SerialHandle;

	char buf[256] = { 0 };
	_snprintf(buf,256,"\\\\.\\%s", com);         //��ʽ���ַ���									//�򿪴���
	SerialHandle = CreateFileA(buf, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);    //ͬ��ģʽ�򿪴���
	if (SerialHandle == INVALID_HANDLE_VALUE)  //�򿪴���ʧ��
	{
		printf("open serial error!\n");
	}
	if (wwj_opt_cb_ != NULL)
	{
		wwj_opt_cb_(0, 1);
	}
	serial_close_flag = FALSE;
	return SerialHandle;
}


int wwj_init_setting_param(wwj_set_param_t* param)
{
	int ret = -1;
	if (param != NULL)
	{
		param->once_need_coin=1;//����һ��                                          
		param->game_paly_levle=10;//�н�����(1-99)  (�����N��ǿץ����������������Ϸ�Ѷ�) 
		param->auto_down_claw=45;//�Զ���ץʱ��(5-60)
		param->power_time=20;//ǿ��ʱ��(5-99)
		param->weak_time = 10; //����ʱ��(1 - 60) = 12
		param->prize_pattern=0;//�н�ģʽ(0 / 1 / 2  ������̶���ȡ��)
		param->sell_pattern=0;//����ģʽ(0/1/2  ǿ����ǿ�ͣ�ǿͶ)
		param->prize_score=0;   //����Ͷ���ͷ�(0 - 20, 0Ϊ�ر�)  
		param->prize_claw_power=8;//�н�ץ��(5-50)      (����ܰ汾��Ч)
		param->power_claw=38;//	ǿצץ��(5-50)        (��צ��ͣǿ����)
		param->top_claw_power=10;//����ץ��(0 - 40, ����������צץ����ͬ)   (��ͣͣ��ץ��)
		param->weak_claw_power=11;// 	��צץ��(0 - 40)      (�쳵�ع�����)
		param->weak_strong = 1;//������ǿ   (0-2)= 1��     0����ģʽ��1��ǿģʽ��2ħ��ģʽ
		param->sell_function=0;//	�������ܿ���(0 / 1)
		param->sell_count = 10; //   ��������   (1 -100)= 10��
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
		wwj_opt_cb_ = NULL;
		set_serial_com_timeout(SerialHandle);//���ö�д��ʱ
		serial_read_thread_id= (HANDLE)_beginthreadex(NULL, 0, serial_read_data_thread, SerialHandle, 0, NULL);
		return 0;
	}
	return -1;
}

int wwj_set_param(HANDLE SerialHandle, wwj_set_param_t param)
{
	int ret = -1;
	int length = 0;
	size_t data_size = 0;
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		//��һ������
// 		data[0] = param.once_need_coin;//����һ��
// 		data[1] = param.game_paly_levle;//�н�����(1-99)  (�����N��ǿץ����������������Ϸ�Ѷ�)
// 		data[2] = param.auto_down_claw;//�Զ���ץʱ��(5-60)
// 		data[3] = param.power_time;//ǿ��ʱ��(5-99)
// 		data[4] = param.prize_pattern;//�н�ģʽ(0 / 1 / 2  ������̶���ȡ��)
// 		data[5] = param.sell_pattern;//����ģʽ(0/1/2  ǿ����ǿ�ͣ�ǿͶ)
// 		data[6] = param.prize_score;   //����Ͷ���ͷ�(0 - 20, 0Ϊ�ر�)
// 		data[7] = param.prize_claw_power;//�н�ץ��(5-50)      (����ܰ汾��Ч)
// 		data[8] = param.power_claw;//	ǿצץ��(5-50)        (��צ��ͣǿ����)
// 		data[9] = param.top_claw_power;//����ץ��(0 - 40, ����������צץ����ͬ)   (��ͣͣ��ץ��)
// 		data[10] = param.weak_claw_power;// 	��צץ��(0 - 40)      (�쳵�ع�����)
// 		data[11] = param.sell_function;//	�������ܿ���(0 / 1)
// 		data[12] = param.prize_inductor;//     �н���Ӧ������(0 / 1)
// 		data[13] = param.air_claw_thing; //    ����ץ�﹦�ܿ���(0 / 1)
// 		data[14] = param.start_score_remain;//  	����������������(0 / 1)
// 		data[15] = param.shake_clean_score_funtion;// 	ҡ����ֹ��ܿ���(0 / 1)
// 		data[16] = param.front_back_speed;//     ǰ���ٶ�(5 - 50, Ĭ��50)
// 		data[17] = param.left_right_speed;//     �����ٶ�(5 - 50, Ĭ��50)
// 		data[18] = param.up_down_speed;//     �����ٶ�(5 - 50, Ĭ��50)

		data[0] = param.auto_down_claw;//�Զ���ץʱ��
		data[1] = param.once_need_coin;//����һ��
		data[2] = param.prize_score;// ����Ͷ���ͷ�(0-20 , 0Ϊ�ر�)
		data[3] = param.air_claw_thing;	//����ȡ��   ,(0-1)= 1
		data[4] = param.prize_inductor;  //�н���Ӧ��(0 - 1) = 0  �� ��
		data[5] = param.prize_pattern;	//�н�ģʽ   (0-1)= 0,    0 �̶���1 ���
		data[6] = param.power_time;   //ǿ��ʱ��   (1-60)=  10��
		data[7] = param.weak_time; //����ʱ��(1 - 60) = 12
		data[8] = param.weak_claw_power;//	 ��ץץ��   (2-40)= 8
		data[9] = param.weak_strong;//������ǿ   (0-2)= 1��     0����ģʽ��1��ǿģʽ��2ħ��ģʽ
		data[10] = param.power_claw;// 	ǿץץ��   (2-50)= 38��
		data[11] = param.game_paly_levle;//	  N ��ǿץ��   (1 -100)= 10��
		data[12] = param.sell_function;//    ��������   (0-1)=   �ؿ�
		data[13] = param.sell_count; //   ��������   (1 -100)= 10��
		data[14] = param.sell_pattern;// ����ģʽ   (0-2)= 0    0 ǿ��  1  ǿ��   2  ǿͶ
//		data[15] = param.shake_clean_score_funtion;//  ������Ŀ����    
// 		data[16] = param.front_back_speed;//����ϵͳ 
// 		data[17] = param.left_right_speed;// �ָ���������
// 		data[18] = param.up_down_speed;//�����ٶ�(5 - 50, Ĭ��50)
		data_size = 15;

		if (serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_set, data, data_size, &length) == 0)
		{
			printf("frame_cmd_set length:%d\n", length);
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
		int data_len = 2;
		data[1] = wwj_step_size_;
		switch (type)
		{
		case kfront:
			data[0] = 0x01;
			break;
		case kclaw:
			data[0] = 0x10;
			data[1] = 0x02;
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
	return 0;
}

void wwj_serial_set_step_size(unsigned char step_size)
{
	wwj_step_size_ = step_size;
}

//�رմ���
int wwj_close_serial(HANDLE SerialHandle)
{
	if (SerialHandle != INVALID_HANDLE_VALUE)
	{
		serial_close_flag = TRUE;
		WaitForSingleObject(serial_read_thread_id, INFINITE);   //�ȴ��߳�ִ����  
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
		data[1] =(unsigned char) coins;
		//data[2]
		data[3] = (unsigned char)coins;
		//data[4]
		int length = 0;;
		if (serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_score, data, 8, &length) == 0)
		{
			int data_len = 0;		
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

wwj_device_status_e wwj_query_device_info(HANDLE SerialHandle)
{
	int length = 0;
	wwj_device_status_e  ret = error;
	if (SerialHandle != INVALID_HANDLE_VALUE&&serial_cmd_help(cmd_buf, BUF_LEN, frame_cmd_timer_query_total_accounts, data, 0, &length) == 0)
	{
		serial_write_data(SerialHandle, cmd_buf, length);
		ret = normal;
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

void wwj_set_func_cb(wwj_callback_func cb)
{
	wwj_opt_cb_ = cb;
}