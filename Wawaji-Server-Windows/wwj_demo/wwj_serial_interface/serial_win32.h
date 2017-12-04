#ifndef SERIAL_WIN32_H
#define SERIAL_WIN32_H
#include <windows.h>

//���޻����ò���
typedef struct wwj_set_param_s
{
	unsigned char once_need_coin;//����һ��
	unsigned char game_paly_levle;//�н�����(1-99)  (�����N��ǿץ����������������Ϸ�Ѷ�)
	unsigned char auto_down_claw;//�Զ���ץʱ��(5-60)
	unsigned char power_time;//ǿ��ʱ��(1-60)
	unsigned char weak_time;//����ʱ�䣨1-60)
	unsigned char prize_pattern;//�н�ģʽ(0 / 1 / 2  ������̶���ȡ��)
	unsigned char sell_pattern;//����ģʽ(0/1/2  ǿ����ǿ�ͣ�ǿͶ)
	unsigned char prize_score;   //����Ͷ���ͷ�(0 - 20, 0Ϊ�ر�)
	unsigned char prize_claw_power;//�н�ץ��(5-50)      (����ܰ汾��Ч)
	unsigned char power_claw;//	ǿצץ��(5-50)        (��צ��ͣǿ����)
	unsigned char top_claw_power;//����ץ��(0 - 40, ����������צץ����ͬ)   (��ͣͣ��ץ��)
	unsigned char weak_claw_power;// 	��צץ��(2 - 40)      (�쳵�ع�����)
	unsigned char weak_strong;//������ǿ   (0-2)= 1��     0����ģʽ��1��ǿģʽ��2ħ��ģʽ
	unsigned char sell_function;//	�������ܿ���(0 / 1)
	unsigned char sell_count;//��������  (1 -100)= 10��
	unsigned char prize_inductor;//     �н���Ӧ������(0 / 1)
	unsigned char air_claw_thing; //    ����ץ�﹦�ܿ���(0 / 1)
	unsigned char start_score_remain;//  	����������������(0 / 1)
	unsigned char shake_clean_score_funtion;// 	ҡ����ֹ��ܿ���(0 / 1)
	unsigned char front_back_speed;//     ǰ���ٶ�(5 - 50, Ĭ��50)
	unsigned char left_right_speed;//     �����ٶ�(5 - 50, Ĭ��50)
	unsigned char up_down_speed;//     �����ٶ�(5 - 50, Ĭ��50)
}wwj_set_param_t;

typedef struct serial_param_s
{
	int bound_rate; //������
	int byte_size; //����λ
	int parity;   //��żУ�顣Ĭ��Ϊ��У�顣NOPARITY 0�� ODDPARITY 1��EVENPARITY 2��MARKPARITY 3��SPACEPARITY 4
	int stop_bits;//ֹͣλ
	int fbinary;//������ģʽ
	int fparity;//��żУ��
}serial_param_t;

typedef enum 
{
	kfront=0, /*ǰ*/
	kback, /*��*/
	kleft, /*��*/
	kright,/*��*/
	kclaw /*��ץ*/
}serial_direction_opt_type_e;

typedef enum
{
	kgamepay = 0,//���Ϸ�
	kgameend ,//��Ϸ����
	kgameprize,//�н�
}wwj_callback_func_type_e;

typedef enum
{
	error = -1,//�ӿڵ���ʧ��
	normal = 0, //����
	left_error, //�������
	right_error,//���ҹ���
	front_error,//��ǰ����
	back_error,//������
	down_error,//���¹���
	up_error,//���Ϲ���
	shake_error,//ҡ�ι���
	light_eye_error,//���۹���
}wwj_device_status_e;

//�ص�����
typedef void(*wwj_callback_func)(wwj_callback_func_type_e type, int code);


//�򿪴���
HANDLE wwj_open_serial(char* com);

//���ô��ڲ���
int wwj_set_serial_param(HANDLE SerialHandle, serial_param_t param);

//���wwj�豸Ĭ�ϵĲ���
//param[in] param wwj�豸�����ṹ��
int wwj_init_setting_param(wwj_set_param_t* param);

//param[in] param wwj�豸�����ṹ��
int wwj_set_param(HANDLE SerialHandle, wwj_set_param_t param);

//fn:��ѯ����, �û���ѯ�Ƿ�����
// param[in] SerialHanle ���
//return��0������-1�쳣
int wwj_check_normal(HANDLE SerialHandle);

//���Ϸ�
int wwj_pay(HANDLE SerialHandle,int coins);

//���ò���
//param[in] step_size ���ֹͣ�ӳ�ʱ��
void wwj_serial_set_step_size(unsigned char step_size);

//�������
//param[in] SerialHandle���
//param[in] type ��������
int wwj_serial_directect_opt(HANDLE SerialHandle,serial_direction_opt_type_e type);

//�쳵��λ
//param[in] SerialHandle���
int wwj_crown_block_reset(HANDLE SerialHandle);


//��ѯ�ն���Ŀ�����ڲ�ѯ�豸����Ŀ״��������״̬��
wwj_device_status_e wwj_query_device_info(HANDLE SerialHandle);

//�رմ���
//param[in] SerialHandle���
int wwj_close_serial(HANDLE SerialHandle);

//���ûص�����
void wwj_set_func_cb(wwj_callback_func cb);

#endif
