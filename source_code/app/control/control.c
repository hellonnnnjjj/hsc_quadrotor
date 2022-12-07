#include "control.h"

#include "main.h"



#include "motor_config.h"
#include "mpu6050_config.h"

//#include "bmp280.h"
//#include "spl06.h"


#include "remote_data.h"

#include "quaternion.h"
#include "remote_task.h"
#include "pid.h"
#include "common.h"
#include "filter.h"


biquadFilter_t biquad_PitchDItemParam = {0};
biquadFilter_t biquad_RollDItemParam = {0};
biquadFilter_t biquad_YawDItemParam = {0};


static PID_Base_t PID_DItemFilterPitchRate(PID_Base_t in)
{
	return  biquad_filter(&biquad_PitchDItemParam, in);
}

static PID_Base_t PID_DItemFilterRollRate(PID_Base_t in)
{
	return  biquad_filter(&biquad_RollDItemParam, in);
}

static PID_Base_t PID_DItemFilterYawRate(PID_Base_t in)
{
	return  biquad_filter(&biquad_YawDItemParam, in);
}



//俯仰角角速度环PID
PID_t PID_PitchRate = 
{
	.P = 3.6,
	.I = 0.02,
	.D = 52,    //18
	.LastDeviation = 0,
	.ISum = 0,
	.ISumMin = -500,
	.ISumMax = 500,
	.OutMin = -ATTITUDE_CONTROL_OUT_MAX,
	.OutMax = ATTITUDE_CONTROL_OUT_MAX,

	.DFilterFunc = PID_DItemFilterPitchRate,
};

//俯仰角角度环PID
PID_t PID_PitchAngle = 
{
	.P = 5,       
	.I = 0.0,
	.D = 0.0,     
	.ISum = 0,
	.LastDeviation = 0,
	.ISumMin = -0,
	.ISumMax = 0,
	.OutMin = -300,
	.OutMax = 300,

	.DFilterFunc = NULL,
};

//横滚角角速度环PID
PID_t PID_RollRate = 
{
	.P = 3.6 * 0.75,
	.I = 0.02 * 0.75,
	.D = 52 * 0.75,
	.ISum = 0,
	.LastDeviation = 0,
	.ISumMin = -500,
	.ISumMax = 500,
	.OutMin = -ATTITUDE_CONTROL_OUT_MAX,
	.OutMax = ATTITUDE_CONTROL_OUT_MAX,

	.DFilterFunc = PID_DItemFilterRollRate,
};

//横滚角角度环PID
PID_t PID_RollAngle = 
{
	.P = 5,
	.I = 0,
	.D = 0,
	.ISum = 0,
	.LastDeviation = 0,
	.ISumMin = 0,
	.ISumMax = 0,
	.OutMin = -300,
	.OutMax = 300,

	.DFilterFunc = NULL,
};

//偏航角角速度环PID
PID_t PID_YawRate = 
{
	.P = 8,
	.I = 0.010,
	.D = 0,
	.ISum = 0,
	.LastDeviation = 0,
	.ISumMin = -500,
	.ISumMax = 500,
	.OutMin = -ATTITUDE_CONTROL_OUT_MAX,
	.OutMax = ATTITUDE_CONTROL_OUT_MAX,

	.DFilterFunc = PID_DItemFilterYawRate,
};

//偏航角角度环PID
PID_t PID_YawAngle = 
{
	.P = 5,
	.I = 0,
	.D = 0,
	.ISum = 0,
	.LastDeviation = 0,
	.ISumMin = 0,
	.ISumMax = 0,
	.OutMin = -300,
	.OutMax = 300,

	.DFilterFunc = NULL,
};


PID_t PID_VelocityZ = {0};
PID_t PID_PositionZ = {0};

biquadFilter_t biquad_GyroParameterX = {0};
biquadFilter_t biquad_GyroParameterY = {0};
biquadFilter_t biquad_GyroParameterZ = {0};

biquadFilter_t biquad_AccParameterX = {0};
biquadFilter_t biquad_AccParameterY = {0};
biquadFilter_t biquad_AccParameterZ = {0};



//姿态四元数
Quaternion_t Quaternion = 
{
	.q0=1.0f,
	.q1=0.0f,
	.q2=0.0f,
	.q3=0.0f,

	.RotationMatrix = {0},
};

//用于姿态解算中通过加速度计和磁力计修正角度
//P越大回正速度越快
Quaternion_PIOffset_t Quaternion_PIOffset = 
{
	.P = 0.4f,
	.I = 0.000f,

	.exInt = 0,
	.eyInt = 0,
	.ezInt = 0,
};




biquadFilter_t position_z_d_ltem_biquad_parameter = {0};

#define ALTITUDE_SLIDING_FILTER_SIZE    5
float altitude_sliding_filter_buff[ALTITUDE_SLIDING_FILTER_SIZE] = {0};


#define GRAVITY_ACC         980

TriaxialData_t earth_acc = {0};

TriaxialData_t estimate_velocity = {0};
TriaxialData_t estimate_position = {0};
TriaxialData_t estimate_acc = {0};

TriaxialData_t Origion_NamelessQuad_velocity = {0};
TriaxialData_t Origion_NamelessQuad_position = {0};
TriaxialData_t Origion_NamelessQuad_acc = {0};


float baro_compensate_k = 0.02;




//滤波参数初始化 暂时放这里
void FilterInit()
{


	biquad_filter_init_lpf(&biquad_GyroParameterX, 500, 70);
	biquad_filter_init_lpf(&biquad_GyroParameterY, 500, 70);
	biquad_filter_init_lpf(&biquad_GyroParameterZ, 500, 70);

	
	biquad_filter_init_lpf(&biquad_AccParameterX, 500, 15);	
	biquad_filter_init_lpf(&biquad_AccParameterY, 500, 15);	
	biquad_filter_init_lpf(&biquad_AccParameterZ, 500, 15);	


	biquad_filter_init_lpf(&biquad_PitchDItemParam, 500, 70);
	biquad_filter_init_lpf(&biquad_RollDItemParam, 500, 70);
	biquad_filter_init_lpf(&biquad_YawDItemParam, 500, 70);

	

	biquad_filter_init_lpf(&position_z_d_ltem_biquad_parameter, 500, 50);

}


//所有电机停转
static void MotorStopAll()
{
	uint32_t i;

	for(i = 0;i < 4;++i)
	{
		Motor_Out(&Motor[i], MOTOR_STOP_PWM);
	}
}


//姿态控制
void AttitudeControl(uint32_t throttleOut, AttitudeData_t * setAngle, AttitudeData_t * nowAngle, MPU6050_ConvertData_t * gyro)
{
	uint32_t i;
	static uint8_t attitude_control_count = 0;


	float yaw_error;

	float  pitchRateOut = 0;
	int32_t pitchOut = 0;

	float rollRateOut = 0;
	int32_t rollOut = 0;

	float yawRateOut = 0;
	int32_t yawOut = 0;
	

	int32_t motorOut[4];

	yaw_error = setAngle->Yaw - nowAngle->Yaw;
	if(yaw_error > 180)
	{
		yaw_error -= 360;
	}
	else if(yaw_error < -180)
	{
		yaw_error += 360;
	}
	
	setAngle->Pitch += PITCH_ZERO;
	setAngle->Roll += ROLL_ZERO;


	attitude_control_count++;
	if(1 == attitude_control_count)
	{
		pitchRateOut = PID_Control(&PID_PitchAngle, nowAngle->Pitch - setAngle->Pitch);
	}
	else if(2 == attitude_control_count)
	{
		rollRateOut = PID_Control(&PID_RollAngle, nowAngle->Roll - setAngle->Roll);
	}
	else if(3 == attitude_control_count)
	{
		attitude_control_count = 0;
		yawRateOut = PID_Control(&PID_YawAngle, yaw_error); 
	}

	pitchOut = (int32_t)PID_Control(&PID_PitchRate, -gyro->Y- pitchRateOut);
	rollOut = (int32_t)PID_Control(&PID_RollRate, -gyro->X- rollRateOut);
	yawOut = (int32_t)PID_Control(&PID_YawRate, gyro->Z- yawRateOut);

	
	motorOut[0] = MOTOR_PWM_MIN + pitchOut - rollOut + yawOut + throttleOut;  //400
	motorOut[1] = MOTOR_PWM_MIN + pitchOut + rollOut - yawOut + throttleOut;     
	motorOut[2] = MOTOR_PWM_MIN - pitchOut - rollOut - yawOut + throttleOut;   //400
	motorOut[3] = MOTOR_PWM_MIN - pitchOut + rollOut + yawOut + throttleOut;

	for(i = 0;i < 4;++i)
	{
		motorOut[i] = int_range(motorOut[i],MOTOR_PWM_MIN,MOTOR_PWM_MAX);
		Motor_Out(&Motor[i], motorOut[i]);
	}

	
}


#define TIME_CONTANST_Z     5.0f
#define K_ACC_Z (5.0f / (TIME_CONTANST_Z * TIME_CONTANST_Z * TIME_CONTANST_Z))
#define K_VEL_Z (3.0f / (TIME_CONTANST_Z * TIME_CONTANST_Z))
#define K_POS_Z (3.0f / TIME_CONTANST_Z)

float  High_Filter[3]={
	0.015,  //0.03
	0.05,//0.05
	0.02,//0.03
};


float Altitude_Dealt=0;

float acc_correction[3] = {0};
float vel_correction[3] = {0};
float pos_correction[3] = {0};

float SpeedDealt[3] = {0};

 
/*
void Strapdown_INS_High()
{
	TriaxialData_t acc;
	float dt = 0.002f;

	acc.x = Origion_NamelessQuad_acc.x * GRAVITY_ACC;
	acc.y = Origion_NamelessQuad_acc.y * GRAVITY_ACC;
	acc.z = Origion_NamelessQuad_acc.z * GRAVITY_ACC;

	imu_body_to_Earth(&acc,&earth_acc);
	earth_acc.z -= GRAVITY_ACC;

	Altitude_Dealt=bmp280_data.altitude - estimate_position.z;//气压计(超声波)与SINS估计量的差，单位cm

	acc_correction[2] += High_Filter[0]*Altitude_Dealt* K_ACC_Z;//加速度校正量
	vel_correction[2] += High_Filter[1]*Altitude_Dealt* K_VEL_Z;//速度校正量
	pos_correction[2] += High_Filter[2]*Altitude_Dealt* K_POS_Z;//位置校正量

	//原始加速度+加速度校正量=融合后的加速度
	estimate_acc.z = earth_acc.z + acc_correction[2];

	//融合后的加速度积分得到速度增量
	SpeedDealt[2]=estimate_acc.z * dt;

	//得到速度增量后，更新原始位置
	Origion_NamelessQuad_position.z += (estimate_velocity.z + 0.5 * SpeedDealt[2]) * dt,

	//原始位置+位置校正量=融合后位置
	estimate_position.z = Origion_NamelessQuad_position.z + pos_correction[2];

	//原始速度+速度校正量=融合后的速度
	Origion_NamelessQuad_velocity.z += SpeedDealt[2];
	estimate_velocity.z = Origion_NamelessQuad_velocity.z + vel_correction[2];
}



float altitude_error;
void get_estimate_position(float observe_altitude)
{
	Triaxial_Data_t acc;
	float dt = 0.002;

//float altitude_error,

	acc.x = Origion_NamelessQuad_acc.x * GRAVITY_ACC;
	acc.y = Origion_NamelessQuad_acc.y * GRAVITY_ACC;
	acc.z = Origion_NamelessQuad_acc.z * GRAVITY_ACC;

	imu_body_to_Earth(&acc,&earth_acc);
	earth_acc.z -= GRAVITY_ACC + 40;

	altitude_error = observe_altitude - estimate_position.z;
	
	//偏差过大 直接给值
	if(ABS(altitude_error) > 500)
	{
		estimate_position.z = observe_altitude;
		estimate_velocity.z = 0;
	}
	else
	{
		estimate_velocity.z += earth_acc.z * dt;
		estimate_position.z += estimate_velocity.z * dt + 0.5*earth_acc.z*dt*dt;
		

		estimate_position.z += baro_compensate_k * altitude_error;
		estimate_velocity.z += baro_compensate_k  * altitude_error;
	}

}


//位置控制
uint32_t position_control(float now_altitude, float set_altitude)
{
	uint8_t count = 0;
	int32_t throttle_out = 0;
	static float set_velocity_z = 0;
	
	count++,
	if(count >= 5)
	{
		count = 0,
		set_velocity_z = PID_control(&position_z_pid, now_altitude - set_altitude);
		
	}
	throttle_out = (int32_t)PID_control_biquad(&velocity_z_pid, set_velocity_z - estimate_velocity.z, &position_z_d_ltem_biquad_parameter);
	throttle_out += THROTTLE_BASE;

	if(throttle_out < 0)
	{
		throttle_out = 0;
	}

	return throttle_out;
}

*/

uint32_t code_time = 0;

void ControlTask(void * parameters)
{

	TickType_t time = xTaskGetTickCount();
	uint32_t timeCount = 0;
	

	//uint16_t remote_data[IBUS_Channel_MAX] = {0};
	RemoteData_t remoteData = {0};
	int32_t remote_lose_count = 0;
	uint8_t remote_lose_flag = 0;

	float set_yaw = 0;
	float set_altitude = 0;
	flight_mode_t last_remote_mode = Stabilize_Mode;


	MPU6050_BaseData_t gyroBaseData = {0};
	MPU6050_BaseData_t accBaseData = {0};
	
	MPU6050_ConvertData_t gyroConvertData = {0};
	MPU6050_ConvertData_t accConvertData = {0};

	TriaxialData_t magData = {0, 0, 0};
	//TriaxialData_t magData = {0, 1, 0};//yaw -90
	//TriaxialData_t magData = {0, -1, 0};//yaw 90
	//TriaxialData_t magData = {1, 1, 1};//

	AttitudeData_t nowAngle = {0};
	AttitudeData_t setAngle = {0};

	for(;;)
	{
		timeCount++;
		if(timeCount >= 1000)
		{
			timeCount = 0;
		}
		//time_count_start_us();

		//接收遥控器数据
		
		if(xQueueReceive(RemoteDataQueue,&remoteData,0) == pdFALSE)
		{
			remote_lose_count++;
			if(remote_lose_count > 100)
			{
				remote_lose_count = 0;
				remote_lose_flag = 1;
			}
		}
		else
		{
			remote_lose_count = 0;
			remote_lose_flag = 0;
		}

		if((timeCount % 20) == 0)
		{
			//接收bmp180海拔高度 
			//bmp280_data_get(&bmp280_data),
			//bmp280_data.altitude = sliding_filter(bmp280_data.altitude,altitude_sliding_filter_buff,ALTITUDE_SLIDING_FILTER_SIZE),
			
			//spl06_get_all_data(),
		}
		
		//获取mpu6050原始数据
		MPU6050_GetBaseAcc(&MPU6050, &accBaseData);
		MPU6050_GetBaseGyro(&MPU6050, &gyroBaseData);
		
		gyroBaseData.X -= -7;
		gyroBaseData.Y -= 54;
		gyroBaseData.Z -= -6;

		MPU6050_ConvertDataAcc(&MPU6050, &accBaseData, &accConvertData);
		MPU6050_ConvertDataGyro(&MPU6050, &gyroBaseData, &gyroConvertData);

		biquad_filter(&biquad_GyroParameterX, gyroConvertData.X);
		biquad_filter(&biquad_GyroParameterY, gyroConvertData.Y);
		biquad_filter(&biquad_GyroParameterZ, gyroConvertData.Z);

		biquad_filter(&biquad_AccParameterX, accConvertData.X);
		biquad_filter(&biquad_AccParameterY, accConvertData.Y);
		biquad_filter(&biquad_AccParameterZ, accConvertData.Z);
		
		
		accConvertData.Y = -accConvertData.Y;
		accConvertData.Z = -accConvertData.Z;
		gyroConvertData.Y = -gyroConvertData.Y;
		gyroConvertData.Z = -gyroConvertData.Z;
		
		//单位转换并滤波

		//Origion_NamelessQuad_acc.x = Acc.x;
		//Origion_NamelessQuad_acc.y = Acc.y;
		//Origion_NamelessQuad_acc.z = Acc.z;

		//Quaternion_t Quaternion;
		//Quaternion_PIOffset_t QuaOffset;ternion_PI
		
		
		//四元数姿态解算
		Quaternion_IMUCalculation(&Quaternion, &Quaternion_PIOffset, &accConvertData, &gyroConvertData, &magData, &nowAngle, 0.002);


		//预估机体速度和位置
		//get_estimate_position(bmp280_data.altitude);
		//bmp280_data.altitude = 0,
		//Strapdown_INS_High();
		
		//需要打印的位置信息
		//xQueueSend(printf_position_queue,&bmp280_data.altitude,0);
		//xQueueSend(printf_position_queue,&bmp280_data.pressure,0);
		//xQueueSend(printf_position_queue,&estimate_position.z,0);

		//需要打印的速度信息
		//xQueueSend(printf_velocity_queue,&estimate_velocity.z,0);
		
		//需要打印的加速度信息
		//xQueueSend(printf_acc_queue,&estimate_acc.z,0);
		//xQueueSend(printf_acc_queue,&earth_acc.z,0);
		//xQueueSend(printf_acc_queue,&Origion_NamelessQuad_acc.z,0);
		
		
		if((RemoteData_GetRockerValue(&remoteData, E_REMOTE_DATA_LEFT_ROCKER_Y)  < THROTTLE_DEAD_ZONE) || (1 == remote_lose_flag))
		//if(FALSE)
		{
			//电机停转
			MotorStopAll();
			//清除PID角速度环I项
			PID_ISumClean(&PID_PitchRate);
			PID_ISumClean(&PID_RollRate);
			PID_ISumClean(&PID_YawRate);


			//将设定偏航角设置为当前偏航角，防止起飞旋转
			set_yaw = nowAngle.Yaw;

		}
		else
		{
			set_yaw += RemoteData_GetRockerValue(&remoteData, E_REMOTE_DATA_LEFT_ROCKER_X);
			
			if(set_yaw > 180)
			{
				set_yaw -= 360;
			}
			else if(set_yaw < -180)
			{
				set_yaw += 360;
			}

			/*if(remote_data.mode == Auto_Mode)
			{
				if(last_remote_mode != Auto_Mode)
				{
					set_altitude = estimate_position.z;
				}
				remote_data.throttle_out = position_control(estimate_position.z, set_altitude);
			}
			last_remote_mode = remote_data.mode;*/

			setAngle.Pitch = RemoteData_GetRockerValue(&remoteData, E_REMOTE_DATA_RIGHT_ROCKER_Y);
			setAngle.Roll = RemoteData_GetRockerValue(&remoteData, E_REMOTE_DATA_RIGHT_ROCKER_X);
			setAngle.Yaw = set_yaw;
			AttitudeControl(RemoteData_GetRockerValue(&remoteData, E_REMOTE_DATA_LEFT_ROCKER_Y), &setAngle, &nowAngle, &gyroConvertData);
		}
		
		vTaskDelayUntil(&time,2);
	}
}

