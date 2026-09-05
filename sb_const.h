#ifndef SB_CONST_H_
#define SB_CONST_H_


#define FW_MAJOR	1
#define FW_MINOR	0
#define FW_PATCH	3

#define ENABLE_KEY_LOGIC

#define DP1_DP2_DP3_MODE	0
#define DP1_TEMP_RH_MODE	1

#define DEVICE_MODE			DP1_TEMP_RH_MODE

//************************************************************************/
// DATA FLASH PART SELECTION - must match the part fitted on the board
//   AT45DB321D  32 Mbit DataFlash, byte-rewritable, no explicit erase
//   XM25QH128A  128 Mbit NOR, 4 KB sector erase required before rewrite
// See Interface/dataflash.h for the data-map constraints a NOR part imposes.
//************************************************************************/
#define DATAFLASH_AT45DB321D	0
#define DATAFLASH_XM25QH128A	1

#define DATAFLASH_PART			DATAFLASH_AT45DB321D

//Set to 1 to build XM25_SelfTest() and its diagnostics into the image.  Leave at 0
//for production: the test costs about 600 bytes of code and erases the scratch
//sector at boot.  Result codes are listed in Interface/XM25QH128A.h (0 = pass).
#define XM25_ENABLE_SELFTEST	0

//************************************************************************/
// PRESSURE SENSOR PART SELECTION - must match the part fitted on the board
//   XGZP6891D   +/-625 Pa as fitted, I2C address 0xFE
//   WF200DP     WF200DPZ0.005B, +/-500 Pa, I2C address 0xDA (SDO/ADDR selects LSB)
// Both use the same register map; see Interface/pressure_sensor.h
//************************************************************************/
#define PRESSURE_SENSOR_XGZP6891D	0
#define PRESSURE_SENSOR_WF200DP		1

#define PRESSURE_SENSOR_PART		PRESSURE_SENSOR_XGZP6891D

//************************************************************************/
// LOGGING SUBSYSTEMS (compile time)
//
// Each switch builds one logging subsystem into the image - its code, its RAM,
// its UART commands, its display mode and its slice of the data flash.  Set one
// to 0 and none of that is built; the flash regions after it move down to close
// the gap, so nothing is reserved for a log that does not exist.
//
// These are COMPILE-TIME switches.  They are separate from the ENABLE_* bits in
// gu16_parameterWord below, which turn the same features on and off at RUN time:
// a subsystem has to be built in before its run-time bit can do anything.
//
// Any combination builds.  Two pairs are related, in one direction each:
//
//  * BUILD_MINMAX_LOG's daily 'mean' column is the average of the 24 hourly means
//    kept by BUILD_MEAN24_LOG.  With the mean log excluded the daily record is
//    still written and still carries min and max; its mean column is simply zero.
//
//  * BUILD_RAM_BUFFER owns RAMBuffer, which BUILD_LOG24_LOG also uses to stage one
//    record on its way to flash.  The buffer therefore sizes itself to whichever
//    is built - see RAM_BUF_SIZE further down - rather than belonging to either.
//************************************************************************/
#define BUILD_REGULAR_LOG	1	//60000-record event log, LogReading()  (RDLG_* commands)
#define BUILD_LOG24_LOG		1	//1440-record rolling 24 h ring         (FLASH24_* commands)
#define BUILD_MINMAX_LOG	1	//15-day min/max/mean archive           (MIN_MAX_MEAN_MODE)
#define BUILD_MEAN24_LOG	1	//24 hourly means                       (MEAN_HOUR_MODE)
#define BUILD_RAM_BUFFER	1	//rolling RAM copy of recent readings   (RAM_ALL_ID / RAM_IND_ID)

#define ENABLE_DP1			0x0001
#define ENABLE_DP2			0x0002
#define ENABLE_DP3			0x0004
#define ENABLE_RTC			0x0008
#define ENABLE_ALERT		0x0010
#define ENABLE_DATAFLASH	0x0020
#define ENABLE_LOG			0x0040
#define ENABLE_M3LOG		0x0080
#define ENABLE_RH			0x0100
#define ENABLE_TEMP			0x0200

#if (DEVICE_MODE==DP1_DP2_DP3_MODE)

	#define PARAMETER_WORD	(ENABLE_DP1 | ENABLE_DP2 | ENABLE_DP3 | ENABLE_RTC | ENABLE_ALERT)// | ENABLE_DATAFLASH)

#else

	#define PARAMETER_WORD	(ENABLE_DP1 | ENABLE_RH | ENABLE_TEMP | ENABLE_RTC | ENABLE_ALERT)// | ENABLE_DATAFLASH)

#endif


#define DP_SW_FACT_DIVISION			 15

//---------------------------------------------------------------------------------
// DP display stability
//
// Dpressure[] feeds BOTH the display and the alarm comparison, so filtering it harder
// would slow alarm response.  Instead the displayed number is held until the reading
// moves by more than this much, which stops the last digit dancing on sensor noise
// while leaving the alarm path exactly as responsive as before.
// The display resolution is 0.1 Pa, so anything below 0.1 has no effect.
//---------------------------------------------------------------------------------
#define DP_DISP_HYSTERESIS			0.2f	//Pa

//Kalman filter tuning (see Kalman_Init).  Lower Q = smoother but slower: with R = 0.1,
// Q = 0.0100 -> gain 0.270, settles in ~1.9 s   (current)
// Q = 0.0020 -> gain 0.132, settles in ~3.8 s
// Q = 0.0010 -> gain 0.095, settles in ~5.3 s
//Raising the filtering here DOES slow alarm detection - change it deliberately.
#define KALMAN_Q					0.01f
#define KALMAN_R					0.1f

//************************************************************************/
// RS485 PARAMETER
//************************************************************************/
	
#define PARA_WRITE_CMD				0x11
#define PARA_READ_CMD				0x10
#define PARA_WITH_ALM_READ_CMD		0x00
#define READ_DP1_VALUE				0x01
#define READ_DP2_VALUE				0x02
#define READ_TEMP_VALUE         	0x03
#define READ_HUMIDITY_VALUE     	0x04
#define READ_DP3_VALUE				0x05
//-------------------------------------------------
#define NO_ERROR			0x00
#define INVALID_CMD			0x01
#define INVALID_PARA		0x02
#define DP1_FAULTY			0x04
#define RH_TEMP_FAULTY		0x08
#define DP2_FAULTY			0x10
#define DP3_FAULTY			0x20
#define RTC_INVALID			0x40	//RTC integrity lost - timestamps not trustworthy
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


//************************************************************************/
// EEPROM LOCATION FOR PARAMETERS
//************************************************************************/
#define FIRST_BOOT_CHECK		CONFIG_PARA_ADDR
#define DISP_PARA_SELECT		(FIRST_BOOT_CHECK+1)

#define DP1_UP_ALM_ON			(DISP_PARA_SELECT+2)
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
#define TEMP_UP_ALM_ON			(DP3_LO_ALM_OFF+2)
#define TEMP_UP_ALM_OFF			(TEMP_UP_ALM_ON+2)
#define TEMP_LO_ALM_ON			(TEMP_UP_ALM_OFF+2)
#define TEMP_LO_ALM_OFF			(TEMP_LO_ALM_ON+2)
#define RH_UP_ALM_ON			(TEMP_LO_ALM_OFF+2)
#define RH_UP_ALM_OFF			(RH_UP_ALM_ON+2)
#define RH_LO_ALM_ON			(RH_UP_ALM_OFF+2)
#define RH_LO_ALM_OFF			(RH_LO_ALM_ON+2)

//Last alarm state per channel - reserved hole on the XM25, see CFG_MINMAX_BLOCK
#define CFG_ALRM_STAT_BLOCK		(RH_LO_ALM_OFF+2)	/* 5 bytes: DP1,DP2,DP3,TM,RH */

#define TEMP_UNIT				(CFG_ALRM_STAT_BLOCK+5)

#define DEVICE_ID				(TEMP_UNIT+1)
#define BUZZER_ON_TIME			(DEVICE_ID+1)
#define BUZZER_OFF_TIME			(BUZZER_ON_TIME+2)
#define LOG_INTERVAL			(BUZZER_OFF_TIME+2)
#define UART_BAUDRATE			(LOG_INTERVAL+2)

//Min/max slots.  On the AT45 they live here; on the XM25 they move to the
//real-time store further down, and this stays a reserved hole so that every
//COLD address below keeps the byte offset it has always had.
#define CFG_MINMAX_BLOCK		(UART_BAUDRATE+1)	/* 40 bytes */

#define CUSTOMER_PASSWORD		(CFG_MINMAX_BLOCK+40)
#define FAC_CUSTOMER_PASSWORD	(CUSTOMER_PASSWORD+2)
#define ACK_TIMER				(FAC_CUSTOMER_PASSWORD+2)
#define ACK_PWD_IND				(ACK_TIMER+2)
#define ACK_PASSWORD			(ACK_PWD_IND+1)
#define DEVICE_SR_NO			(ACK_PASSWORD+30)
//Log read pointer + overflow flag - reserved hole on the XM25
#define CFG_LOG_PTR_BLOCK		(DEVICE_SR_NO+16)	/* 3 bytes: RDLC(2) + OVF(1) */
#define CURR_LOG_IND			(CFG_LOG_PTR_BLOCK+3)
#define CFG_LOG24_PTR_BLOCK		(CURR_LOG_IND+400)	/* 1 byte: 24 h RDLC */
#define CURR_LOG24_IND			(CFG_LOG24_PTR_BLOCK+1)
#define RTC_SET_FLAG_ADDR		(CURR_LOG24_IND+200)

#define DP1_CAL_DATE_ADDR		(RTC_SET_FLAG_ADDR+1)
#define DP2_CAL_DATE_ADDR		(DP1_CAL_DATE_ADDR+12)
#define DP3_CAL_DATE_ADDR		(DP2_CAL_DATE_ADDR+12)
#define TM_CAL_DATE_ADDR		(DP3_CAL_DATE_ADDR+12)
#define RH_CAL_DATE_ADDR		(TM_CAL_DATE_ADDR+12)

#define DP1_CAL_CERT_ADDR		(RH_CAL_DATE_ADDR+12)
#define DP2_CAL_CERT_ADDR		(DP1_CAL_CERT_ADDR+15)
#define DP3_CAL_CERT_ADDR		(DP2_CAL_CERT_ADDR+15)
#define TM_CAL_CERT_ADDR		(DP3_CAL_CERT_ADDR+15)
#define RH_CAL_CERT_ADDR		(TM_CAL_CERT_ADDR+15)

#define DP1_USER_CAL_DATE_IND_ADDR	(RH_CAL_CERT_ADDR+15)
#define DP2_USER_CAL_DATE_IND_ADDR	(DP1_USER_CAL_DATE_IND_ADDR+1)
#define DP3_USER_CAL_DATE_IND_ADDR	(DP2_USER_CAL_DATE_IND_ADDR+1)
#define TM_USER_CAL_DATE_IND_ADDR	(DP3_USER_CAL_DATE_IND_ADDR+1)
#define RH_USER_CAL_DATE_IND_ADDR	(TM_USER_CAL_DATE_IND_ADDR+1)

#define DP1_USER_CAL_DATE_ADDR	(RH_USER_CAL_DATE_IND_ADDR+1)
#define DP2_USER_CAL_DATE_ADDR	(DP1_USER_CAL_DATE_ADDR+100)
#define DP3_USER_CAL_DATE_ADDR	(DP2_USER_CAL_DATE_ADDR+100)
#define TM_USER_CAL_DATE_ADDR	(DP3_USER_CAL_DATE_ADDR+100)
#define RH_USER_CAL_DATE_ADDR	(TM_USER_CAL_DATE_ADDR+100)

#define DP1_CAL_VAL_F_ADDR		(RH_USER_CAL_DATE_ADDR+100)
#define DP2_CAL_VAL_F_ADDR		(DP1_CAL_VAL_F_ADDR+2)
#define DP3_CAL_VAL_F_ADDR		(DP2_CAL_VAL_F_ADDR+2)
#define TM_CAL_VAL_F_ADDR		(DP3_CAL_VAL_F_ADDR+2)
#define RH_CAL_VAL_F_ADDR		(TM_CAL_VAL_F_ADDR+2)

#define DP1_CAL_VAL_C_ADDR		(RH_CAL_VAL_F_ADDR+2)
#define DP2_CAL_VAL_C_ADDR		(DP1_CAL_VAL_C_ADDR+2)
#define DP3_CAL_VAL_C_ADDR		(DP2_CAL_VAL_C_ADDR+2)
#define TM_CAL_VAL_C_ADDR		(DP3_CAL_VAL_C_ADDR+2)
#define RH_CAL_VAL_C_ADDR		(TM_CAL_VAL_C_ADDR+2)

//Day index of the min/max/mean log - reserved hole on the XM25
#define CFG_MINMAX_IND_BLOCK	(RH_CAL_VAL_C_ADDR+2)	/* 5 bytes */
#define MASTER_ENABLE_ADDR		(CFG_MINMAX_IND_BLOCK+5)

#define DOOR_SENSE_POLARITY_ADDR		(MASTER_ENABLE_ADDR+1)
#define DOOR_SENSE_TIME_ADDR			(DOOR_SENSE_POLARITY_ADDR+1)
#define DP1_ALM_SENSE_TIME_ADDR			(DOOR_SENSE_TIME_ADDR+1)
#define DP2_ALM_SENSE_TIME_ADDR			(DP1_ALM_SENSE_TIME_ADDR+1)
#define DP3_ALM_SENSE_TIME_ADDR			(DP2_ALM_SENSE_TIME_ADDR+1)
#define LCD_BRIGHT_CNT_ADDR				(DP3_ALM_SENSE_TIME_ADDR+1)
#define DP_SW_FACT_ADDR					(LCD_BRIGHT_CNT_ADDR+1)
#define RELAY_STAT_ADDR					(DP_SW_FACT_ADDR+10)
#define AUTO_SENT_INTERVAL_ADDR			(RELAY_STAT_ADDR+1)
#define XBEE_RST_INTERVAL_ADDR			(AUTO_SENT_INTERVAL_ADDR+1)
#define BROADCAST_ENB_ADDR				(XBEE_RST_INTERVAL_ADDR+2)
#define DEVICES_IN_GROUP_ADDR			(BROADCAST_ENB_ADDR+1)
#define XBEE_MAC_ADDR					(DEVICES_IN_GROUP_ADDR+1)
#define DP_LIMIT_ADDR					(XBEE_MAC_ADDR+32)		//0x46A=1130
#define DP_OFFSET_ADDR					(DP_LIMIT_ADDR+6)
#define LCD_CONTROL_ADDR				(DP_OFFSET_ADDR+6)
#define COM_CONTROL_ADDR				(LCD_CONTROL_ADDR+1)

//************************************************************************/
// HOT (real-time) PARAMETERS
//
// These are rewritten while the device runs - min/max on every new extreme,
// alarm state on every transition, the log pointers on every log record.  The
// rest of the block above changes only when a user presses a key or sends a
// UART command.
//
// The AT45DB321D rewrites any byte in place, so there they stay exactly where
// they have always been.  The XM25QH128A can only erase whole 4 KB sectors, so
// leaving them in the configuration sector meant every min/max update had to
// read-modify-write the entire block of user settings - slow, and it exposed
// those settings to corruption on every power cut.  On that part they move to
// their own two-sector store (RT_PARA_ADDR, see the data-flash map below).
//************************************************************************/
#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)

	#define DP1_MAXIMUM				(RT_PARA_ADDR+0)
	#define DP2_MAXIMUM				(DP1_MAXIMUM+4)
	#define DP3_MAXIMUM				(DP2_MAXIMUM+4)
	#define DP1_MINIMUM				(RT_PARA_ADDR+12)
	#define DP2_MINIMUM				(DP1_MINIMUM+4)
	#define DP3_MINIMUM				(DP2_MINIMUM+4)
	#define TEMP_MAXIMUM			(RT_PARA_ADDR+24)
	#define TEMP_MINIMUM			(RT_PARA_ADDR+28)
	#define RH_MAXIMUM				(RT_PARA_ADDR+32)
	#define RH_MINIMUM				(RT_PARA_ADDR+36)

	#define LAST_DP1_ALRM_STAT		(RT_PARA_ADDR+40)
	#define LAST_DP2_ALRM_STAT		(LAST_DP1_ALRM_STAT+1)
	#define LAST_DP3_ALRM_STAT		(LAST_DP2_ALRM_STAT+1)
	#define LAST_TM_ALRM_STAT		(LAST_DP3_ALRM_STAT+1)
	#define LAST_RH_ALRM_STAT		(LAST_TM_ALRM_STAT+1)

	#define FLSH_OVF_IND			(RT_PARA_ADDR+45)
	#define MIN_MAX_LOG_IND_ADDR	(RT_PARA_ADDR+46)	/* 5 bytes reserved */
	#define CURR_LOG_IND_RDLC		(RT_PARA_ADDR+51)	/* 2 bytes */
	#define CURR_LOG24_IND_RDLC		(RT_PARA_ADDR+53)

	//Bytes 54..RT_PARA_SIZE-1 are spare.  Grow RT_PARA_SIZE if more is needed,
	//never past RT_SLOT_SIZE-1 - the last byte of a slot is the completion tag.

#else

	#define DP1_MAXIMUM				(CFG_MINMAX_BLOCK+0)
	#define DP2_MAXIMUM				(DP1_MAXIMUM+4)
	#define DP3_MAXIMUM				(DP2_MAXIMUM+4)
	#define DP1_MINIMUM				(DP3_MAXIMUM+4)
	#define DP2_MINIMUM				(DP1_MINIMUM+4)
	#define DP3_MINIMUM				(DP2_MINIMUM+4)
	#define TEMP_MAXIMUM			(DP3_MINIMUM+4)
	#define TEMP_MINIMUM			(TEMP_MAXIMUM+4)
	#define RH_MAXIMUM				(TEMP_MINIMUM+4)
	#define RH_MINIMUM				(RH_MAXIMUM+4)

	#define LAST_DP1_ALRM_STAT		(CFG_ALRM_STAT_BLOCK+0)
	#define LAST_DP2_ALRM_STAT		(LAST_DP1_ALRM_STAT+1)
	#define LAST_DP3_ALRM_STAT		(LAST_DP2_ALRM_STAT+1)
	#define LAST_TM_ALRM_STAT		(LAST_DP3_ALRM_STAT+1)
	#define LAST_RH_ALRM_STAT		(LAST_TM_ALRM_STAT+1)

	#define CURR_LOG_IND_RDLC		(CFG_LOG_PTR_BLOCK+0)
	#define FLSH_OVF_IND			(CFG_LOG_PTR_BLOCK+2)
	#define CURR_LOG24_IND_RDLC		(CFG_LOG24_PTR_BLOCK+0)
	#define MIN_MAX_LOG_IND_ADDR	(CFG_MINMAX_IND_BLOCK+0)

#endif

//How often SecondTick() commits the real-time mirror to flash.  Only meaningful
//on a NOR part - see the real-time store in Interface/XM25QH128A.c.
//
//The trade-off is wear against loss: one bank holds RT_SLOTS_PER_BANK (63) records
//before its partner has to be erased, so at 60 s a bank is erased roughly every
//63 minutes - about 4200 erase cycles a year against the part's rated 100,000.
//Shortening it costs endurance proportionally; lengthening it risks losing more
//min/max history on an unexpected power cut.
#define RT_FLUSH_INTERVAL_SEC	60

//--------------------------------------------------
//The logo blinks to acknowledge UART traffic: it runs for LOGO_ACK_BLINK_MS from
//the last valid message served, at the 500 ms rate the TIM1 ISR already keeps in
//bool_mec500_blink_flag.  Counted in the 50 ms display ticks that gate
//bool_msec50_flag, so the two stay in step.
#define LOGO_ACK_BLINK_MS		5000
#define LOGO_ACK_BLINK_TICKS	(LOGO_ACK_BLINK_MS/50)

#define MIN_LOG_INTERVAL		1
#define MAX_LOG_INTERVAL		1440

//************************************************************************/
// MIN / MAX SEEDS AND RANGE
//
// DEFAUT_x_MIN starts at the TOP of the sensor's range and DEFAUT_x_MAX at the
// BOTTOM, so the first reading beats both and the tracked extremes converge at
// once.  boot_data() reuses the same two numbers as the validity bounds for the
// stored extremes, so ANYTHING THE SENSOR CAN REPORT has to fit inside them -
// otherwise a genuine reading is thrown away at the next power up.
//
// Pressure: this is the span the DRIVER can produce, which is wider than the
// calibrated span on the XGZP part.
//
//   XGZP6891D  Calibrated -500..+500 Pa, but Pressure = PARA_A*ADC + PARA_B is a
//              straight line across the whole 24-bit code range: PARA_B = -625 Pa
//              at code 0, rising to +1875 Pa at 0xFFFFFF.  Readings beyond the
//              calibrated span are not guaranteed accurate, but they ARE reported,
//              so they must not be discarded.  See Interface/XGZP6891D.h.
//   WF200DP    Two's complement normalised to -1.0..+1.0 then scaled by
//              WF200DP_FULL_SCALE_PA, so it cannot leave -500..+500 Pa.
//
// Temperature / RH: the SHT25's rated range, and already correct.  Its conversion
// formulas can compute slightly beyond it at the ends of the raw code range
// (-46.85..+128.87 C from T = -46.85 + 175.72*ST/2^16, and -6..+119 %RH from
// RH = -6 + 125*SRH/2^16); readings out there are outside the part's
// specification, and clamping them to the rated range is deliberate.
//************************************************************************/
//Rated span of the fitted part, as a whole number of Pa.  Kept as an int so the
//display-width check below can be a real #error - the preprocessor cannot compare
//floats.
#if (PRESSURE_SENSOR_PART == PRESSURE_SENSOR_WF200DP)
	//WF200DPZ0.005B: +/-0.005 bar, and the reading is normalised to -1.0..+1.0
	//before scaling, so the part cannot report outside this span at all.
	#define DP_SENSOR_RATED_INT		500
#else
	//XGZP6891D on the -500..+500 Pa row of the transfer-function table.
	#define DP_SENSOR_RATED_INT		500
#endif

#define DP_SENSOR_RATED_PA			((float)DP_SENSOR_RATED_INT)

//The min/max SEEDS use the rated span too, and deliberately not the wider span the
//XGZP driver can report (-625..+1875 Pa, where the transfer-function line simply
//carries on past the calibrated row).  Two reasons:
//
//  * Those extremes are extrapolation artefacts, not measurements - zero sits at
//    a quarter of the code range on that part, so the top of the line lands a long
//    way out.
//  * Each min/max field on the normal-mode display is THREE characters, and
//    convert_float() writes as many digits as the value needs without bounding it.
//    A four-digit seed overruns into the next channel's field.
//
//Trade-off: if EVERY reading sits outside the rated span, the corresponding
//extreme stays at its seed instead of tracking - which on a +/-500 Pa part means
//it is saturated and the reading is not trustworthy anyway.
#define DEFAUT_DP_SENSOR_MIN		DP_SENSOR_RATED_PA
#define DEFAUT_DP_SENSOR_MAX		(-DP_SENSOR_RATED_PA)

#if (DP_SENSOR_RATED_INT > 999)
	#error "DP min/max seeds and alarm setpoints are drawn in 3-digit fields (see convert_float call sites in conv_value) - a rated span above 999 Pa overruns the next channel's display field"
#endif

#define DEFAUT_DP1_MIN				DEFAUT_DP_SENSOR_MIN
#define DEFAUT_DP1_MAX				DEFAUT_DP_SENSOR_MAX
#define DEFAUT_DP2_MIN				DEFAUT_DP_SENSOR_MIN
#define DEFAUT_DP2_MAX				DEFAUT_DP_SENSOR_MAX
#define DEFAUT_DP3_MIN				DEFAUT_DP_SENSOR_MIN
#define DEFAUT_DP3_MAX				DEFAUT_DP_SENSOR_MAX

//SHT25 rated range - unchanged
#define DEFAUT_RH_MIN				100
#define DEFAUT_RH_MAX				0
#define DEFAUT_TEMP_C_MIN			125.0
#define DEFAUT_TEMP_C_MAX			(-40.0)
#define DEFAUT_TEMP_F_MIN			257.0
#define DEFAUT_TEMP_F_MAX			(-40.0)

//How far a DP alarm setpoint may be set, in Pa.  This follows the sensor's RATED
//span, not the wider span the driver can report.  Two reasons:
//
//  * An alarm is only meaningful where the reading is guaranteed accurate.
//  * The setpoint is drawn by convert_char(dummy,&data[6],4) - FOUR digits of
//    tenths - so nothing above 999.9 Pa can be displayed.  The XGZP part reports
//    up to +1875 Pa, which would need five.
//
//The min/max bounds above deliberately use the wider reported span instead:
//discarding a stored extreme is worse than keeping an uncalibrated one, and those
//are not entered through this four-digit field.
//
//Setpoints are held in TENTHS of a Pa - DEFAULT_DP1_UPPER_ALM_ON = 550 is 55.0 Pa,
//and the alarm test is Dpressure > DP_Upper_Alm_ON/10.0 - hence the *10 at each
//use site.  int16_t caps a setpoint at +/-3276.7 Pa whatever this is set to.
//
//NOTE: this narrows the settable range from the +/-981 Pa it has always been.  A
//unit already holding a setpoint outside +/-DP_SENSOR_RATED_PA has it reset to the
//default on the first boot with this firmware.
#define DP_ALM_LIMIT_MIN			DP_SENSOR_RATED_PA
#define DP_ALM_LIMIT_MAX			(-DP_SENSOR_RATED_PA)
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
#define DEFAULT_TM_C_UPPER_ALM_ON	800
#define DEFAULT_TM_C_UPPER_ALM_OFF	750
#define DEFAULT_TM_C_LOWER_ALM_ON	(-350)
#define DEFAULT_TM_C_LOWER_ALM_OFF	(-300)
#define DEFAULT_TM_F_UPPER_ALM_ON	1760
#define DEFAULT_TM_F_UPPER_ALM_OFF	1400
#define DEFAULT_TM_F_LOWER_ALM_ON	(-310)
#define DEFAULT_TM_F_LOWER_ALM_OFF	(-220)
#define DEFAULT_RH_UPPER_ALM_ON		900
#define DEFAULT_RH_UPPER_ALM_OFF	850
#define DEFAULT_RH_LOWER_ALM_ON		150
#define DEFAULT_RH_LOWER_ALM_OFF	200
#define DEFAULT_DEVICE_ID			1
#define DEFAULT_BUZZER_ON_TIME		1		//In seconds
#define DEFAULT_BUZZER_OFF_TIME		2		//In seconds
#define DEFAULT_LOG_INTERVAL		1		//In Minutes
#define DEFAULT_UART_BAUDRATE		8		//57600
#define DEFAULT_CUSTOMER_PWD		100
#define DEFAULT_FACTORY_PWD			4321
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

#define DEFAULT_LCD_BRIGHTNESS	 15
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
#define TEMPERATURE_ID			1
#define HUMIDITY_ID			2

//------------------------------------------------------------------------------------
#define NORMAL_LOG				0
#define DP1_ALM_OCCURE_LOG		1
#define DP1_ALM_RESTORE_LOG		2
#define DP2_ALM_OCCURE_LOG		3
#define DP2_ALM_RESTORE_LOG		4
#define DP3_ALM_OCCURE_LOG		5
#define DP3_ALM_RESTORE_LOG		6
#define TM_ALM_OCCURE_LOG		7
#define TM_ALM_RESTORE_LOG		8
#define RH_ALM_OCCURE_LOG		9
#define RH_ALM_RESTORE_LOG		10
#define ALM_ACK_LOG				11
#define POWER_UP_LOG			12

//RAMBuffer holds the most recent readings.  Built in full it keeps RAM_LOG_SLOTS
//records for the RAM_ALL_ID / RAM_IND_ID reads; without that feature the 24 hour
//ring still needs somewhere to stage ONE record on its way to flash, and if neither
//is built the buffer collapses to nothing.  At 2000 bytes this is the single
//largest object in RAM, so the difference is worth having.
#if BUILD_RAM_BUFFER
	#define RAM_LOG_SLOTS		30
	#define RAM_BUF_SIZE		2000
#elif BUILD_LOG24_LOG
	#define RAM_LOG_SLOTS		1
	//One whole record.  LOG_SIZE is declared in the data-flash driver header,
	//which not every translation unit has seen by this point, so the size is
	//spelled out here; dataflash.h checks the two still agree.
	#define RAM_BUF_SIZE		(RAM_FILL_START + 50)
#else
	#define RAM_LOG_SLOTS		1
	#define RAM_BUF_SIZE		1
#endif
#define RAM_FILL_START			5
#define RAW_DP_CNT_IND			10
#define XBEE_RX_IND_MAX			20
#define RX_IND_MAX				100
#define TX_IND_MAX				100

//DATA LOG ADDRESS IN DATA FLASH -----------------------------------------------------
#if (DATAFLASH_PART == DATAFLASH_XM25QH128A)

	// A NOR part erases in 4 KB sectors, so every region must start on a sector
	// boundary - otherwise erasing one region's first sector destroys the tail of
	// the region before it.  (On the AT45 map below, REGULAR_LOG_ADDR = 2048 shares
	// sector 0 with the whole config block.)
	#define DF_SECTOR_SIZE			4096UL
	#define DF_ALIGN_UP(x)			((((x)+DF_SECTOR_SIZE-1UL)/DF_SECTOR_SIZE)*DF_SECTOR_SIZE)

	#define CONFIG_PARA_ADDR		0							/* sector 0 - COLD data only */

	// Real-time parameter store (XM25_Rt* in Interface/XM25QH128A.c).
	//
	// Two sectors used as a ping-pong pair, mirrored in RAM.  The application still
	// addresses these fields as plain EEPROM bytes; RT_PARA_ADDR is a VIRTUAL base
	// outside the 16 MByte device, so ReadEEPROMData()/WriteEEPROMData() recognise
	// them and route to the mirror instead of the bus.  Picking a base the part can
	// never answer to means a raw access that slips past the check fails visibly
	// rather than quietly corrupting whatever a truncated address would land on.
	//
	// The mirror is appended to flash as one whole record once a minute, so a burst
	// of min/max updates costs a single 64-byte page program instead of one 4 KB
	// read-modify-write each.
	#define RT_PARA_SECTOR_A		(CONFIG_PARA_ADDR + DF_SECTOR_SIZE)		/* sector 1 */
	#define RT_PARA_SECTOR_B		(RT_PARA_SECTOR_A + DF_SECTOR_SIZE)		/* sector 2 */
	#define RT_PARA_ADDR			0x10000000UL
	#define RT_PARA_SIZE			60UL		/* payload; see HOT PARAMETERS above */
	#define RT_SLOT_SIZE			64UL		/* payload + trailing completion tag */
	#define RT_SLOTS_PER_BANK		((DF_SECTOR_SIZE/RT_SLOT_SIZE)-1UL)	/* 63; slot 0 is the header */

	// Wear-levelled log-pointer store.  Each pointer gets TWO sectors used as a
	// ping-pong pair: saves append to the active bank (always erased space, one page
	// program), and when it fills, the pointer is carried into the already-erased
	// spare BEFORE the old bank is erased - so a valid pointer always exists, even if
	// power is lost mid-swap.  Each bank starts with a 4-byte header (magic +
	// generation) that identifies which bank is newer.  See XM25_PtrSave().
	#define PTR_HDR_SIZE			4UL
	#define PTR_SLOT_SIZE			4UL			/* 24-bit value + 8-bit validity tag */
	#define CURR_LOG_IND_SECTOR_A	(RT_PARA_SECTOR_B + DF_SECTOR_SIZE)
	#define CURR_LOG_IND_SECTOR_B	(CURR_LOG_IND_SECTOR_A + DF_SECTOR_SIZE)
	#define CURR_LOG24_IND_SECTOR_A	(CURR_LOG_IND_SECTOR_B + DF_SECTOR_SIZE)
	#define CURR_LOG24_IND_SECTOR_B	(CURR_LOG24_IND_SECTOR_A + DF_SECTOR_SIZE)
	#define PTR_SLOTS_PER_BANK		((DF_SECTOR_SIZE-PTR_HDR_SIZE)/PTR_SLOT_SIZE)	/* 1023 */

	// TOTAL_REGULAR_LOG is a RECORD COUNT (log-index wrap-around); the region base
	// below is a BYTE address, padded up to a sector boundary so no two regions ever
	// share a sector.  See the region chain after the #endif.
	#define REGULAR_LOG_ADDR		(CURR_LOG24_IND_SECTOR_B + DF_SECTOR_SIZE)
	#define TOTAL_REGULAR_LOG		60000

#else

	#define CONFIG_PARA_ADDR 		0
	#define REGULAR_LOG_ADDR 		(CONFIG_PARA_ADDR+2048)
	// Regular log ring.  TOTAL_REGULAR_LOG is a RECORD COUNT (use it for log-index
	// wrap-around).  The AT45 rewrites any byte in place, so its regions need no
	// padding - DF_ALIGN_UP is the identity here and the shared region chain after
	// the #endif produces exactly the byte addresses this part has always used.
	#define TOTAL_REGULAR_LOG		60000
	#define DF_ALIGN_UP(x)			(x)

#endif



//MIN_MAX LOG ADDRESS IN DATA FLASH --------------------------------------------------
#define LAST_LOG24_ADDR 			1440

#define TOTAL_MIN_MAX_MEAN_LOG		15
#define MIN_MAX_MEAN_LOG_SIZE		16
#define MIN_MAX_MEAN_LOG_SPACE		(TOTAL_MIN_MAX_MEAN_LOG*MIN_MAX_MEAN_LOG_SIZE)

#define TOTAL_MEAN_HOUR				24
#define HOUR_MEAN_VALUE_SPACE		(TOTAL_MEAN_HOUR*4)

//------------------------------------------------------------------------------------
// Region chain.  Every region is placed immediately after the one before it, and a
// subsystem that is not built contributes nothing - so excluding a log genuinely
// hands its flash back rather than leaving a hole.  DF_ALIGN_UP pads each region to
// a 4 KB boundary on the XM25 (where erase granularity forces it) and is the
// identity on the AT45.
//
// With all four subsystems built the addresses are identical to the original map.
//------------------------------------------------------------------------------------
#if BUILD_REGULAR_LOG
	//1UL forces the 32-bit arithmetic that a (uint32_t) cast used to.  It has to be
	//a multiplier and not a cast: these macros are reached from the #if guards in
	//Interface/dataflash.h, and the preprocessor has no casts - `uint32_t' there is
	//just an undefined identifier, which expands to 0 and breaks the expression.
	#define REGULAR_LOG_SPACE		DF_ALIGN_UP(1UL*TOTAL_REGULAR_LOG*LOG_SIZE)
#else
	#define REGULAR_LOG_SPACE		0UL
#endif

#if BUILD_LOG24_LOG
	#define LOG24_SPACE				DF_ALIGN_UP(1UL*LAST_LOG24_ADDR*LOG_SIZE)
#else
	#define LOG24_SPACE				0UL
#endif

#if BUILD_MINMAX_LOG
	#define MINMAX_SPACE			DF_ALIGN_UP(5UL*MIN_MAX_MEAN_LOG_SPACE)
#else
	#define MINMAX_SPACE			0UL
#endif

#if BUILD_MEAN24_LOG
	#define MEAN24_SPACE			DF_ALIGN_UP(5UL*HOUR_MEAN_VALUE_SPACE)
#else
	#define MEAN24_SPACE			0UL
#endif

#define LAST_LOG24_ADDR_OFFSET		(REGULAR_LOG_ADDR + REGULAR_LOG_SPACE)
#define MIN_MAX_LOG_ADDR_OFFSET		(LAST_LOG24_ADDR_OFFSET + LOG24_SPACE)
#define MEAN24_LOG_ADDR_OFFSET		(MIN_MAX_LOG_ADDR_OFFSET + MINMAX_SPACE)
#define DATA_FLASH_END_OFFSET		(MEAN24_LOG_ADDR_OFFSET + MEAN24_SPACE)

#define LAST_DP1_MIN_MAX_OFFSET		(MIN_MAX_LOG_ADDR_OFFSET)
#define LAST_DP2_MIN_MAX_OFFSET		(LAST_DP1_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define LAST_DP3_MIN_MAX_OFFSET		(LAST_DP2_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define LAST_TM_MIN_MAX_OFFSET		(LAST_DP3_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)
#define LAST_RH_MIN_MAX_OFFSET		(LAST_TM_MIN_MAX_OFFSET+MIN_MAX_MEAN_LOG_SPACE)

#define DP1_CURR_24HR_MEAN_OFFSET	(MEAN24_LOG_ADDR_OFFSET)
#define DP2_CURR_24HR_MEAN_OFFSET	(DP1_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
#define DP3_CURR_24HR_MEAN_OFFSET	(DP2_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
#define TM_CURR_24HR_MEAN_OFFSET	(DP3_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)
#define RH_CURR_24HR_MEAN_OFFSET	(TM_CURR_24HR_MEAN_OFFSET+HOUR_MEAN_VALUE_SPACE)

//Display Mode ---------------------------------------------------------------------------------------------
#define NORMAL_MODE			0
#define PROG_MODE			1
#define DP_AUTO_CAL_MODE	2
//----------------------------------------------------------------------------------------------------------

#define PROG_CNT			5
//check_key() runs on the 50 ms tick, so DEBOUNCE is in 50 ms units.  1 = act on the
//first sample that sees the new state; the 50 ms sampling period is what rejects
//contact bounce (a few ms), and the state machine cannot re-fire until a released
//sample is seen.  Raise to 2 if any switch proves bouncy enough to double-trigger,
//at the cost of needing a 100 ms press to register.
#define DEBOUNCE			1

//---------------------------------------------------------------------------------
// UP / DOWN key auto-repeat.  CheckUpDnKey() runs on the 50 ms tick, so these are
// in 50 ms units.  The ladder gives one step on the initial press, a hold-off so a
// tap cannot double-step, then fine repeat, faster repeat, and finally coarse steps.
//---------------------------------------------------------------------------------
#define KEY_REPEAT_DELAY	10		//0.5 s  hold-off before auto-repeat starts
#define KEY_REPEAT_MEDIUM	40		//2.0 s  until one step per tick (20 steps/s)
#define KEY_REPEAT_FAST		80		//4.0 s  until coarse steps of 10
#define KEY_COUNT_MAX		200		//counter ceiling (fits uint8_t)

//Page / parameter navigation is NOT value editing: it must step once per press and then
//repeat slowly.  Before the key scan moved to the 50 ms tick these actions were limited
//only by the old 500 ms poll, so at 20 Hz one press advanced them several times.
#define KEY_NAV_DELAY		20		//1.0 s hold-off before navigation auto-repeats
#define KEY_NAV_REPEAT		10		//then one step every 500 ms, the original cadence

//Long-press hold times for the key combinations, also in 50 ms ticks
#define KEY_HOLD_5SEC		100
#define KEY_HOLD_10SEC		200

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

