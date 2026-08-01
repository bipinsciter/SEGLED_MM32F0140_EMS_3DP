#ifndef SB_CONST_H_
#define SB_CONST_H_

#define ENABLE_KEY_LOGIC

#define DP_SW_FACT_DIVISION			 15

//************************************************************************/
// RS485 PARAMETER
//************************************************************************/
	
#define PARA_WRITE_CMD				0x11
#define PARA_READ_CMD				0x10
#define PARA_WITH_ALM_READ_CMD		0x00
//-------------------------------------------------
#define NO_ERROR			0x00
#define INVALID_CMD			0x01
#define INVALID_PARA		0x02
#define DP1_FAULTY			0x04
#define DP2_FAULTY			0x08
#define DP3_FAULTY			0x10
//-------------------------------------------------
#define DP1UAOFF_ID			0x01
#define DP1UAON_ID			0x02
#define DP1LAOFF_ID			0x03
#define DP1LAON_ID			0x04
#define DP2UAOFF_ID			0x05				
#define DP2UAON_ID			0x06		
#define DP2LAOFF_ID			0x07		
#define DP2LAON_ID			0x08		
#define TMUAOFF_ID			0x09		
#define TMUAON_ID			0x0A		
#define TMLAOFF_ID			0x0B		
#define TMLAON_ID			0x0C		
#define RHUAOFF_ID			0x0D		
#define RHUAON_ID			0x0E		
#define RHLAOFF_ID			0x0F	
#define RHLAON_ID			0x10		
#define HR_ID				0x11
#define MN_ID				0x12		
#define SEC_ID				0x13
#define YR_ID				0x14		
#define MNTH_ID				0x15		
#define DT_ID				0x16		
#define LOGINTVAL_ID		0x19		
#define DVCID_ID			0x1A		
#define DP1MIN_ID			0x1C
#define DP1MAX_ID			0x1D
#define DP2MIN_ID			0x1E		
#define DP2MAX_ID			0x1F
#define TMMIN_ID			0x20		
#define TMMAX_ID			0x21
#define RHMIN_ID			0x22		
#define RHMAX_ID			0x23
#define TMUNT_ID			0x2E
#define DP1CAL_ID			0x30
#define DP2CAL_ID			0x31
#define TMCAL_ID			0x32
#define RHCAL_ID			0x33
#define SFVER_ID			0x34
#define BZRON_ID			0x35		
#define BZROFF_ID			0x36
#define CAL_FPWD_ID			0x37		
#define CAL_CPWD_ID			0x38
#define CPWD_ID				0x39
#define ACK_TIMER_ID		0x3A
#define ACK_PW_ID			0x3B
#define ALM_ACK_ID			0x3C
#define BATPER_ID			0x3D
#define BRDSTP_ID			0x3E
#define BRDSTR_ID			0x3F
#define SRNO_ID				0x40
#define UBRT_ID				0x41
#define UDBT_ID				0x42
#define UPRT_ID				0x43
#define USTB_ID				0x44
#define RAM_ALL_ID			0x45
#define RAM_IND_ID			0x46
#define FLASH24_IND_ID		0x47
#define REALTIME_VAL_ID		0x48
#define RDLG_DT_ID			0x49
#define RDLG_CNT_ID			0x4A	
#define DATETIME_ID			0x4B
#define FLASH24_CUR_IND_ID	0x4C
#define FCPWD_ID			0x4D
#define SET_DPARA_PWD_ID	0x4E
#define SCROLL_TIME_ID		0x4F
#define EXT_FLASH_ERASE_ID	0x50
#define DFLT_RTC_ID			0x51
#define DFLT_CAL_ID			0x52
#define MINMAXMEAN_IND_ID	0x53
#define MEAN_HR_ID			0x54
#define BACKLIT_ID			0x55
#define TM_RH_SCAN_TIME_ID	0x56
#define BIG_FONT_LED_SET_ID	0x57
#define MENB_ID				0x58
#define DOOR_SENSE_POLARITY_ID		0x5A
#define DOOR_SENSE_TIME_ID			0x5B
#define DP1_ALM_SENSE_TIME_ID		0x5C
#define DP2_ALM_SENSE_TIME_ID		0x5D
#define LCD_BRIGHT_CNT_ID			0x5E
#define DP_SW_FACT_ID				0x5F
#define RELAY_CNTL_ID				0x60
#define DP3UAOFF_ID					0x61				
#define DP3UAON_ID					0x62		
#define DP3LAOFF_ID					0x63		
#define DP3LAON_ID					0x64
#define DP3MIN_ID					0x65		
#define DP3MAX_ID					0x66
#define DP3CAL_ID					0x67
#define DP3_ALM_SENSE_TIME_ID		0x68
#define AUTO_SENT_INTERVAL_ID		0x69
#define XBEE_RST_INTERVAL_ID		0x6A
#define XBEE_MAC_ADDR_ID			0x6B
#define DEVICES_IN_GROUP_ID			0x6C
#define XBEE_SELF_MAC_ADDR_ID		0x6D
#define DP_LIMIT_ID					0x6E
#define LCD_CONTROL_ID				0x6F
#define COM_CONTROL_ID				0x70
#define DP_OFFSET_ID				0x71

#define CORR_RTC_DATA_ID			0x99	
#define PUT_DFU_ID					0xAA




//************************************************************************/
// EEPROM LOCATION FOR PARAMETERS
//************************************************************************/
#define FIRST_BOOT_CHECK		0
#define DISP_PARA_SELECT		(FIRST_BOOT_CHECK+1)
#define DUMMY_ADDR				(DISP_PARA_SELECT+2)
#define DP1_UP_ALM_ON			(DUMMY_ADDR+1)
#define DP1_UP_ALM_OFF			(DP1_UP_ALM_ON+2)
#define DP1_LO_ALM_ON			(DP1_UP_ALM_OFF+2)
#define DP1_LO_ALM_OFF			(DP1_LO_ALM_ON+2)
#define DP2_UP_ALM_ON			(DP1_LO_ALM_OFF+2)
#define DP2_UP_ALM_OFF			(DP2_UP_ALM_ON+2)
#define DP2_LO_ALM_ON			(DP2_UP_ALM_OFF+2)
#define DP2_LO_ALM_OFF			(DP2_LO_ALM_ON+2)
#define DP3_UP_ALM_ON			(DP2_LO_ALM_OFF+2)
#define DP3_UP_ALM_OFF			(DP3_UP_ALM_ON+2)
#define DP3_LO_ALM_ON			(DP3_UP_ALM_OFF+2)
#define DP3_LO_ALM_OFF			(DP3_LO_ALM_ON+2)

#define DP1_CAL_CNT				(DP3_LO_ALM_OFF+2)
#define DP1_CAL_CNT_C			(DP1_CAL_CNT+2)
#define DP2_CAL_CNT				(DP1_CAL_CNT_C+2)
#define DP2_CAL_CNT_C			(DP2_CAL_CNT+2)
#define DP3_CAL_CNT				(DP2_CAL_CNT_C+2)
#define DP3_CAL_CNT_C			(DP3_CAL_CNT+2)

#define LAST_DP1_ALRM_STAT		(DP3_CAL_CNT_C+2)
#define LAST_DP2_ALRM_STAT		(LAST_DP1_ALRM_STAT+1)
#define LAST_DP3_ALRM_STAT		(LAST_DP2_ALRM_STAT+1)

#define DEVICE_ID				(LAST_DP3_ALRM_STAT+1)
#define BUZZER_ON_TIME			(DEVICE_ID+1)
#define BUZZER_OFF_TIME			(BUZZER_ON_TIME+2)
#define LOG_INTERVAL			(BUZZER_OFF_TIME+2)
#define UART_BAUDRATE			(LOG_INTERVAL+2)
#define UART_DATBITS			(UART_BAUDRATE+1)
#define UART_PARITY				(UART_DATBITS+1)
#define UART_STOPBIT			(UART_PARITY+1)
#define DP1_MAXIMUM				(UART_STOPBIT+1)
#define DP2_MAXIMUM				(DP1_MAXIMUM+4)
#define DP3_MAXIMUM				(DP2_MAXIMUM+4)
#define DP1_MINIMUM				(DP3_MAXIMUM+4)
#define DP2_MINIMUM				(DP1_MINIMUM+4)
#define DP3_MINIMUM				(DP2_MINIMUM+4)

#define CUSTOMER_PASSWORD		(DP3_MINIMUM+4)
#define FAC_CUSTOMER_PASSWORD	(CUSTOMER_PASSWORD+2)
#define ACK_TIMER				(FAC_CUSTOMER_PASSWORD+2)
#define ACK_PWD_IND				(ACK_TIMER+2)
#define ACK_PASSWORD			(ACK_PWD_IND+1)
#define DEVICE_SR_NO			(ACK_PASSWORD+30)
#define CURR_LOG_IND_RDLC		(DEVICE_SR_NO+16)
#define FLSH_OVF_IND			(CURR_LOG_IND_RDLC+2)
#define CURR_LOG_IND			(FLSH_OVF_IND+1)
#define CURR_LOG24_IND_RDLC		(CURR_LOG_IND+400)
#define CURR_LOG24_IND			(CURR_LOG24_IND_RDLC+1)
#define PARA_SCROLL_TIME		(CURR_LOG24_IND+2)
#define RTC_SET_FLAG_ADDR		(PARA_SCROLL_TIME+1)
#define DP1_CAL_DATE_ADDR		(RTC_SET_FLAG_ADDR+1)
#define DP2_CAL_DATE_ADDR		(DP1_CAL_DATE_ADDR+12)
#define DP3_CAL_DATE_ADDR		(DP2_CAL_DATE_ADDR+12)
#define DP1_CAL_CERT_ADDR		(DP3_CAL_DATE_ADDR+12)
#define DP2_CAL_CERT_ADDR		(DP1_CAL_CERT_ADDR+15)
#define DP3_CAL_CERT_ADDR		(DP2_CAL_CERT_ADDR+15)
#define DP1_USER_CAL_DATE_IND_ADDR	(DP3_CAL_CERT_ADDR+15)
#define DP2_USER_CAL_DATE_IND_ADDR	(DP1_USER_CAL_DATE_IND_ADDR+1)
#define DP3_USER_CAL_DATE_IND_ADDR	(DP2_USER_CAL_DATE_IND_ADDR+1)
#define DP1_USER_CAL_DATE_ADDR	(DP3_USER_CAL_DATE_IND_ADDR+1)
#define DP2_USER_CAL_DATE_ADDR	(DP1_USER_CAL_DATE_ADDR+100)
#define DP3_USER_CAL_DATE_ADDR	(DP2_USER_CAL_DATE_ADDR+100)
#define DP1_CAL_VAL_F_ADDR		(DP3_USER_CAL_DATE_ADDR+100)
#define DP2_CAL_VAL_F_ADDR		(DP1_CAL_VAL_F_ADDR+2)
#define DP3_CAL_VAL_F_ADDR		(DP2_CAL_VAL_F_ADDR+2)
#define DP1_CAL_VAL_C_ADDR		(DP3_CAL_VAL_F_ADDR+2)
#define DP2_CAL_VAL_C_ADDR		(DP1_CAL_VAL_C_ADDR+2)
#define DP3_CAL_VAL_C_ADDR		(DP2_CAL_VAL_C_ADDR+2)

#define RTC_SET_FLAG1_ADDR		(DP3_CAL_VAL_C_ADDR+2)
#define RTC_SET_FLAG2_ADDR		(RTC_SET_FLAG1_ADDR+1)
#define CORRUPT_RTC_IND_ADDR	(RTC_SET_FLAG2_ADDR+1)
#define CORRUPT_RTC_DATA_ADDR	(CORRUPT_RTC_IND_ADDR+1)
#define MIN_MAX_LOG_IND_ADDR	(CORRUPT_RTC_DATA_ADDR+150)
#define BACKLIT_ON_OFF_ADDR		(MIN_MAX_LOG_IND_ADDR+5)
#define TM_RH_SCAN_TIME_ADDR	(BACKLIT_ON_OFF_ADDR+1)
#define DP1_LED_SETTING_ADDR	(TM_RH_SCAN_TIME_ADDR+1)
#define DP2_LED_SETTING_ADDR	(DP1_LED_SETTING_ADDR+1)
#define DP3_LED_SETTING_ADDR	(DP2_LED_SETTING_ADDR+1)
#define MASTER_ENABLE_ADDR		(DP3_LED_SETTING_ADDR+1)
#define DOOR_SENSE_POLARITY_ADDR		(MASTER_ENABLE_ADDR+1)
#define DOOR_SENSE_TIME_ADDR			(DOOR_SENSE_POLARITY_ADDR+1)
#define DP1_ALM_SENSE_TIME_ADDR			(DOOR_SENSE_TIME_ADDR+1)
#define DP2_ALM_SENSE_TIME_ADDR			(DP1_ALM_SENSE_TIME_ADDR+1)
#define DP3_ALM_SENSE_TIME_ADDR			(DP2_ALM_SENSE_TIME_ADDR+1)
#define DP_AUTO_CAL_FLAG				(DP3_ALM_SENSE_TIME_ADDR+1)
#define LCD_BRIGHT_CNT_ADDR				(DP_AUTO_CAL_FLAG+1)
#define DP_SW_FACT_ADDR					(LCD_BRIGHT_CNT_ADDR+1)
#define DP_FACT_ENB_ADDR				(DP_SW_FACT_ADDR+10)
#define RELAY_STAT_ADDR					(DP_FACT_ENB_ADDR+2)
#define AUTO_SENT_INTERVAL_ADDR			(RELAY_STAT_ADDR+1)
#define XBEE_RST_INTERVAL_ADDR			(AUTO_SENT_INTERVAL_ADDR+1)
#define BROADCAST_ENB_ADDR				(XBEE_RST_INTERVAL_ADDR+2)
#define DEVICES_IN_GROUP_ADDR			(BROADCAST_ENB_ADDR+1)
#define XBEE_MAC_ADDR					(DEVICES_IN_GROUP_ADDR+1)
#define DP_LIMIT_ADDR					(XBEE_MAC_ADDR+6)		//0x46A=1130
#define DP_OFFSET_ADDR					(DP_LIMIT_ADDR+6)
#define LCD_CONTROL_ADDR				(DP_OFFSET_ADDR+6)
#define COM_CONTROL_ADDR				(LCD_CONTROL_ADDR+1)

//--------------------------------------------------
#define MIN_LOG_INTERVAL		1
#define MAX_LOG_INTERVAL		1440

#define DEFAUT_DP1_MIN				981.0
#define DEFAUT_DP1_MAX				(-981.0)
#define DEFAUT_DP2_MIN				981.0
#define DEFAUT_DP2_MAX				(-981.0)
#define DEFAUT_DP3_MIN				981.0
#define DEFAUT_DP3_MAX				(-981.0)
#define DEFAULT_DP1_UPPER_ALM_ON	550
#define DEFAULT_DP1_UPPER_ALM_OFF	500
#define DEFAULT_DP1_LOWER_ALM_ON	(-550)
#define DEFAULT_DP1_LOWER_ALM_OFF	(-500)
#define DEFAULT_DP2_UPPER_ALM_ON	550
#define DEFAULT_DP2_UPPER_ALM_OFF	500
#define DEFAULT_DP2_LOWER_ALM_ON	(-550)
#define DEFAULT_DP2_LOWER_ALM_OFF	(-500)
#define DEFAULT_DP3_UPPER_ALM_ON	550
#define DEFAULT_DP3_UPPER_ALM_OFF	500
#define DEFAULT_DP3_LOWER_ALM_ON	(-550)
#define DEFAULT_DP3_LOWER_ALM_OFF	(-500)
#define DEFAULT_DEVICE_ID			1
#define DEFAULT_BUZZER_ON_TIME		1		//In seconds
#define DEFAULT_BUZZER_OFF_TIME		2		//In seconds
#define DEFAULT_LOG_INTERVAL		1		//In Minutes
#define DEFAULT_UART_BAUDRATE		8		//57600
#define DEFAULT_CUSTOMER_PWD		100
#define DEFAULT_FACTORY_PWD			4321
#define DEFAULT_PARA_SCROLL_TIME	5
//-------------------------------------------------
#define FACTORY_PARASET_PWD		1234
#define FACTORY_PASSWORD		1000
#define DFU_PASSWORD			3123
#define SOFT_VER				920  //means 9.20
#define NO_OF_ACKPWD			15
#define NO_OF_XBEE_MAC			2
#define NO_OF_DEVICES_IN_GROUP	5
#define XBEE_MAC_SIZE			16
#define FACT_ACK_PWD			1
#define NO_OF_USER_CAL_DATE		10

#define DEFAULT_LCD_BRIGHTNESS	 10
#define DEFAULT_AUTO_SENT_INTERVAL	 5
#define DEFAULT_XBEE_RST_INTERVAL	 360
#define DEFAULT_DEVICES_IN_GROUP	 5

//Display Mode ---------------------------------------------------------------------------------------------
#define NORMAL_MODE			0
#define PROG_MODE			1
#define DP_AUTO_CAL_MODE	2
#define MIN_MAX_MEAN_MODE	3
#define MEAN_HOUR_MODE		4

#define PROG_ENT				0x01
#define PARA_SELECT				0x02

#define MAX_SUPPORTED_DP		3
#define ZERO_DP1_COUNT			8192
#define ZERO_DP2_COUNT			8192
#define ZERO_DP3_COUNT			8192
//-------------------------------------------------
#define BIT0			0x01
#define BIT1			0x02
#define BIT2			0x04
#define BIT3			0x08
#define BIT4			0x10
#define BIT5			0x20
#define BIT6			0x40
#define BIT7			0x80

#define OPEN			1
#define CLOSE			0

#define ON			1
#define OFF			0

#define YES			1
#define NO			0

#define DP1			0
#define DP2			1
#define DP3			2

#define ENABLE_DP1			0x0001
#define ENABLE_DP2			0x0002
#define ENABLE_DP3			0x0004
#define ENABLE_RTC			0x0008
#define ENABLE_ALERT		0x0010
#define ENABLE_DATAFLASH	0x0020
#define ENABLE_LOG			0x0040
#define ENABLE_M3LOG		0x0080

#define PARAMETER_WORD	(ENABLE_DP1 | ENABLE_DP2 | ENABLE_DP3 | ENABLE_RTC | ENABLE_ALERT)// | ENABLE_DATAFLASH)

//------------------------------------------------------------------------------------
#define NORMAL_LOG				0
#define DP1_ALM_OCCURE_LOG		1
#define DP1_ALM_RESTORE_LOG		2
#define DP2_ALM_OCCURE_LOG		3
#define DP2_ALM_RESTORE_LOG		4
#define DP3_ALM_OCCURE_LOG		5
#define DP3_ALM_RESTORE_LOG		6
#define ALM_ACK_LOG				7
#define POWER_UP_LOG			8

#define RAM_BUF_SIZE			2000
#define RAM_FILL_START			5
#define RAW_DP_CNT_IND			10
#define XBEE_RX_IND_MAX			20
#define RX_IND_MAX				100
#define TX_IND_MAX				100

//DATA LOG ADDRESS IN DATA FLASH -----------------------------------------------------
#define CONFIG_PARA_ADDR 		0
#define REGULAR_LOG_ADDR 		(CONFIG_PARA_ADDR+2048)
#define LAST_LOG_ADDR 			(REGULAR_LOG_ADDR+65000)

#define LAST_LOG24_ADDR_OFFSET 	LAST_LOG_ADDR
#define LAST_LOG24_ADDR 		1440

//MIN_MAX LOG ADDRESS IN DATA FLASH --------------------------------------------------
#define TOTAL_MIN_MAX_MEAN_LOG		15
#define MIN_MAX_MEAN_LOG_SIZE		16
#define MIN_MAX_MEAN_LOG_SPACE		(TOTAL_MIN_MAX_MEAN_LOG*MIN_MAX_MEAN_LOG_SIZE)

#define MIN_MAX_LOG_ADDR_OFFSET		((LAST_LOG24_ADDR_OFFSET+LAST_LOG24_ADDR)*LOG_SIZE)
#define LAST_DP1_MIN_MAX_OFFSET		(MIN_MAX_LOG_ADDR_OFFSET)
#define LAST_DP2_MIN_MAX_OFFSET		(LAST_DP1_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define LAST_DP3_MIN_MAX_OFFSET		(LAST_DP2_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)

#define TOTAL_MEAN_HOUR				24
#define HOUR_MEAN_VALUE_SPACE		(TOTAL_MEAN_HOUR*4)

#define DP1_CURR_24HR_MEAN_OFFSET	(LAST_DP3_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define DP2_CURR_24HR_MEAN_OFFSET	(DP1_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
#define DP3_CURR_24HR_MEAN_OFFSET	(DP2_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)

//Display Mode ---------------------------------------------------------------------------------------------
#define NORMAL_MODE			0
#define PROG_MODE			1
#define DP_AUTO_CAL_MODE	2
//----------------------------------------------------------------------------------------------------------

#define PROG_CNT			5
#define DEBOUNCE			3

#define NO_ALARM				0
#define UPPER_ALARM				1
#define LOWER_ALARM				2

//************************************************************************/
// UART PARAMETER
//************************************************************************/

#define BAUD_1200		0
#define BAUD_2400		1
#define BAUD_4800		2
#define BAUD_9600		3
#define BAUD_14400		4
#define BAUD_19200		5
#define BAUD_28800		6 
#define BAUD_38400		7
#define BAUD_57600		8
#define BAUD_115200		9

#define DATABIT_5		0
#define DATABIT_6		1
#define DATABIT_7		2
#define DATABIT_8		3

#define PARITY_NONE		0
#define PARITY_EVEN		1
#define PARITY_ODD		2

#define STOP_BIT_1		0
#define STOP_BIT_2		1

#define NO_DIGIT	13

#define RH_MIN_on			final_buffer[7] |= BIT0
#define RH_MIN_ALM_on		final_buffer[7] |= BIT1
#define RH_UNIT_on 			final_buffer[7] |= BIT6
#define RH_LOGO_ALM_on 		{final_buffer[28] |= BIT2;final_buffer[29] |= BIT2;final_buffer[30] |= BIT2;final_buffer[31] |= BIT2;}
#define RH_LOGO_on 			{final_buffer[28] |= BIT3;final_buffer[29] |= BIT3;final_buffer[30] |= BIT3;final_buffer[31] |= BIT3;}

//-------------------------------------------------
#define TM_MIN_on			final_buffer[15] |= BIT2
#define TM_MIN_ALM_on		final_buffer[15] |= BIT3
#define TM_UNIT_C_on 		final_buffer[27] |= BIT1
#define TM_UNIT_F_on 		final_buffer[28] |= BIT1
#define TM_LOGO_ALM_on 			{final_buffer[23] |= BIT2;final_buffer[24] |= BIT2;final_buffer[25] |= BIT2;final_buffer[26] |= BIT2;final_buffer[27] |= BIT2;}
#define TM_LOGO_on 		{final_buffer[23] |= BIT3;final_buffer[24] |= BIT3;final_buffer[25] |= BIT3;final_buffer[26] |= BIT3;final_buffer[27] |= BIT3;}

//-------------------------------------------------
#define DP_UNIT_on 			{final_buffer[20] |= BIT6;final_buffer[21] |= BIT6;}
#define DP_MIN_on			final_buffer[31] |= BIT4
#define DP_MIN_ALM_on		final_buffer[31] |= BIT5
#define DP_LOGO_ALM_on 		{final_buffer[16] |= BIT6;final_buffer[17] |= BIT6;final_buffer[18] |= BIT6;final_buffer[19] |= BIT6;}
#define DP_LOGO_on 			{final_buffer[16] |= BIT7;final_buffer[17] |= BIT7;final_buffer[18] |= BIT7;final_buffer[19] |= BIT7;}
	
//-------------------------------------------------
#define RTC_BC1_on 			{final_buffer[23] |= BIT1; final_buffer[24] |= BIT1;}
#define RTC_COL_on			final_buffer[23] |= BIT0
#define RTC_AM_on 			final_buffer[25] |= BIT1
#define RTC_PM_on 			final_buffer[26] |= BIT1

//-------------------------------------------------
#define MIN_on 				final_buffer[4] |= BIT6
#define MAX_on 				final_buffer[9] |= BIT6
#define MEAN_on 			final_buffer[10] |= BIT6
#define SET_on 				final_buffer[6] |= BIT6
#define ID_on 				final_buffer[5] |= BIT6
#define ACK_on 				final_buffer[8] |= BIT6
#define LOGO_on 			{final_buffer[0] |= BIT6;final_buffer[1] |= BIT6;final_buffer[2] |= BIT6;final_buffer[3] |= BIT6;}

#define DOOR_on 			{final_buffer[22] |= BIT6; final_buffer[23] |= BIT6;}
#define DOOR_SYM_on 		{final_buffer[29] |= BIT1; final_buffer[30] |= BIT1;}
#define DOOR_ALM_on 		{final_buffer[22] |= BIT7; final_buffer[23] |= BIT7;}


#define BLANK	20
#define DASH	21
#define C		22
#define A		23
#define L		24
#define D		25
#define N		26
#define E		27
#define B		28
#define t		29
#define r		30
#define P		31
#define I		32
#define F		33
#define U		34
#define H		35
#define M		36
#define Y		37
#define V		38

//EPOCH
#define SHR(a, b) (-1 >> 1 == -1 ? (a) >> (b) : (a) / (1 << (b)) - ((a) % (1 << (b)) < 0))
#define EPOCH_YEAR				1970
#define TM_YEAR_BASE			2000
#define LEAPYEAR(year)          (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year)          (LEAPYEAR(year) ? 366 : 365)

#endif	// SB_CONST_H_

