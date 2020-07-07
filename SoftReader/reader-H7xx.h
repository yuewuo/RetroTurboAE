/*
 * This header library provides basic functions for MCU operation
 * take whatever you like and ignore others, by pre-define the function you need
 */

// MEMORY_READER_H7XX_VERSION: uint32_t number, like 0x19032700, be sure to update this number when memory is different from before
#define MEMORY_READER_H7XX_VERSION 0x19102000

/*
 * Memory Structure for STM32H7xx
 */
#ifdef MEMORY_READER_H7XX
#undef MEMORY_READER_H7XX
#include "fifo.h"
#include "softio.h"

#ifdef ARM_MATH_CM7
#include "main.h"
#include "arm_math.h"
#else
// TODO 兼容性定义
typedef int16_t q15_t;
typedef int32_t q31_t;
typedef int64_t q63_t;
#endif

typedef struct {
	q15_t Ia, Qa, Ib, Qb;
} filter_out_t;

typedef struct {
	union { q15_t coeff[512]; unsigned coeffsimd[256]; };
	union { q15_t data[4][256];  // Mode 1: sparsely distributed
		struct { q15_t Ia[256]; q15_t Ib[256]; q15_t Qa[256]; q15_t Qb[256]; };
		q15_t data2[128][8];  // Ia Ia Ib Ib Qa Qa Qb Qb
	};  // 4 * 4 * 128; 2 + 2 + 7 = 11 = 2048; exactly matches ldrd offset
	union {
		struct { q15_t output[4]; };
		struct { q15_t Ia_out, Ib_out, Qa_out, Qb_out; };
	};
    uint8_t index;  // 指向当前数据的位置，正好0~255
    uint8_t rsh;  // 输出到output并saturate之前右移多少位
#define DEMOD_DOWNSAMPLE_FLAG_OUTPUT_8BYTE 0x10
    uint8_t flag;
    uint8_t resved;
} __attribute__((aligned(64))) QAMDemod_Downsampling_x32_x2_Filter_t;  // 双通道（给DMA的头字节），每通道独立解调+滤波

typedef struct {
	uint8_t use_tim4_not_lptim1 : 1;
	uint8_t select_lptim1tim4_not_lptim2 : 1;
} __attribute__((aligned(1))) Tx_Sample_t;

typedef struct {

	uint16_t period_lptim1;
	uint16_t pulse_lptim1;
	uint16_t period_lptim2;
	uint16_t pulse_lptim2;
	uint16_t period_tim4;
	uint16_t pulse_tim4;

	uint32_t period_tim2;
	uint32_t count;  // only non-zero value will get data from tx_data fifo. this variable is safe to write at any time
	uint32_t underflow;
	uint32_t count_add;  // write to this variable to add to the `count` variable without any risk.

	Tx_Sample_t default_sample;

} __attribute__((aligned(64))) Tx_Settings_t;

typedef struct {
// previous mrw
	#define COMMAND_NONE 0x44  // 什么都不做，这是command的常规状态，初始化为这个
	#define COMMAND_RUN_PENDING 0x55  // 主机控制单片机准备开始运行，单片机检测到这个信息后应当做硬件设置准备，准备完成后设为status_running
	#define COMMAND_STOP_PENDING 0x77  // 告诉STM32需要停下当前操作，并进行deinit操作，并随后转为 IDLE 态
	#define COMMAND_STR(command) ((command)==COMMAND_NONE?"none":\
								((command)==COMMAND_RUN_PENDING?"run_pending":\
								((command)==COMMAND_STOP_PENDING?"stop_pending":"ERROR")))
		uint8_t command;
	#define FLAG_MASK_HALT ((unsigned char)1<<0)  // 整机暂停操作，但不能停止定时器等，这个操作用来暂停while运行，详见main.c代码
	#define FLAG_MASK_DISABLE_UART3_OUT ((unsigned char)1<<1)  // 不让 uart3 fifo 内容输出到uart3，这样一来host可以去读取单片机的log并保存下来，省去再接一个串口调试了
	#define FLAG_MASK_2 ((unsigned char)1<<2)  // 暂无定义，之后用到再说
	#define FLAG_MASK_3 ((unsigned char)1<<3)
	#define FLAG_MASK_4 ((unsigned char)1<<4)
	#define FLAG_MASK_5 ((unsigned char)1<<5)
	#define FLAG_MASK_6 ((unsigned char)1<<6)
	#define FLAG_MASK_SYSTEM_RESET ((unsigned char)1<<7)  // 整机重启，代替手动按钮操作，调用HAL_NVIC_SystemReset
		uint8_t flags;
	#define VERBOSE_NONE 0x00
	#define VERBOSE_ERROR 0x20
	#define VERBOSE_WARN 0x40
	#define VERBOSE_INFO 0x60
	#define VERBOSE_DEBUG 0x80
	#define VERBOSE_REACH_LEVEL(level, threshold) ((level) >= (threshold))  // 使用方法：如果想要debug才打印的信息，设为 VERBOSE_DEBUG 即可
		uint8_t verbose_level;
	#define MODE_NONE 0x00  // 未定义mode
	#define MODE_STR(mode) ((mode)==MODE_NONE?"none":"ERROR")
		uint8_t mode;

	// previous mro
	#define STATUS_INIT 0x00  // 系统正在启动中，启动完毕后会自动变为IDLE态
	#define STATUS_IDLE 0x01  // 无正在进行的事情
	#define STATUS_TEST 0xFF  // 机器自检，具体操作待定
	#define STATUS_RUNNING 0x66  // 开始运行
	#define STATUS_STR(status) ((status)==STATUS_INIT?"init":\
								((status)==STATUS_IDLE?"idle":\
								((status)==STATUS_TEST?"test":\
								((status)==STATUS_RUNNING?"running":"ERROR"))))
    uint8_t status;  // 系统状态，只读
    uint8_t resv;  // 保留，为了4字节对齐，如果不对齐会引起内存错误，编译能通过，但跑起来进 HardFault_Handler
    uint16_t pid;
    uint32_t version;  // 系统版本，见compile_conf.h
	uint32_t mem_size;  // sizeof(ReaderH7_Mem_t)
	// uint32_t prsv_overflow;

	uint16_t dac_set;  // write this variable to set dac value
	uint16_t dac_now;

// previous statistics
	uint16_t siorx_overflow;
	uint16_t siotx_overflow;
	uint16_t usart3_tx_overflow;
	uint32_t adc_data_overflow;
	
// application data
	// 主动发送数据到PC，提高系统鲁棒性。当此变量不为0时，一旦ADC积攒了1个包的大小[254/8]=31=248/8就发送给上位机处理，并把此数值减一
	uint32_t adc_data_push_packets_count;

	uint32_t guard_small_varibles;

// other structs
	QAMDemod_Downsampling_x32_x2_Filter_t filter;
	Tx_Settings_t tx;

	char siorx_buf[8192];
	char siotx_buf[32768];
	char usart3_tx_buf[1024];
	char testio_buf[1024];  // used for test fifo, keep this if you wanna use Tester/DebugTest/ReaderH7SoftIO.cpp to test device
	char adc_data_buf[32768];
	char tx_data_buf[8192];
#define ReaderH7_Mem_FifoInit(mem) do {\
	FIFO_STD_INIT(mem, siorx);\
	FIFO_STD_INIT(mem, siotx);\
	FIFO_STD_INIT(mem, usart3_tx);\
	FIFO_STD_INIT(mem, testio);\
	FIFO_STD_INIT(mem, adc_data);\
	FIFO_STD_INIT(mem, tx_data);\
} while(0)
	Fifo_t siorx;  // must be the first 
	Fifo_t siotx;
	Fifo_t usart3_tx;
	Fifo_t testio;
	Fifo_t adc_data;
	Fifo_t tx_data;

} ReaderH7_Mem_t;

#define print_debug(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_DEBUG))    printf("D: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_info(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_INFO))      printf("I: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_warn(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_WARN))      printf("W: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_error(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_ERROR))    printf("E: " format "\r\n",##__VA_ARGS__); } while(0)
#endif

#ifdef MEMORY_READER_H7XX_EXTERN
#undef MEMORY_READER_H7XX_EXTERN
extern ReaderH7_Mem_t mem;
extern SoftIO_t sio;
#endif

/*
 * Memory Structure Initiator for STM32H7xx, you need to predefine the following things:
 * MEMORY_READER_H7XX_PID: uint16_t number, like 0x3270 means march/27th, the first device in this day
 */
#ifdef MEMORY_READER_H7XX_INIT
#undef MEMORY_READER_H7XX_INIT
void memory_reader_h7xx_init_user_code_begin_1(void) {
	mem.status = STATUS_INIT;  // host could read this status word, change to STATUS_IDLE when init successs
    mem.version = MEMORY_READER_H7XX_VERSION;
    mem.pid = MEMORY_READER_H7XX_PID;
	mem.verbose_level = VERBOSE_DEBUG;  // set verbose level
    mem.command = COMMAND_NONE;
    mem.flags = 0;
    mem.mode = MODE_NONE;
	mem.mem_size = sizeof(ReaderH7_Mem_t);
	mem.tx.period_lptim1 = 64-1;
	mem.tx.pulse_lptim1 = 32-1;
	// mem.tx.period_lptim2 = 20000-1;  // 10kHz
	// mem.tx.pulse_lptim2 = 10000-1;
	mem.tx.period_lptim2 = 125-1;  // 32kHz for tag to use (accurate clock synchronization for data collection lasting 2min * 100 = 3hours)
	mem.tx.pulse_lptim2 = 63-1;
	mem.tx.period_tim4 = 182-1;  // 1.0989MHz ~ 1.1MHz, 0.1% difference
	mem.tx.pulse_tim4 = 91-1;
	mem.tx.period_tim2 = 199999;  // 1kHz
	mem.tx.default_sample.use_tim4_not_lptim1 = 0;  // default is 455kHz
	mem.tx.default_sample.select_lptim1tim4_not_lptim2 = 1;  // default select lptim1
	mem.tx.underflow = 0;
	mem.tx.count = 0;
	mem.adc_data_push_packets_count = 0;
	SOFTIO_QUICK_INIT(sio, mem, ReaderH7_Mem_FifoInit);
}
static inline void memory_reader_h7xx_printhello(const char* MCU) {
	print_info("RetroTurboReader on %s, version 0x%08x, pid 0x%04X, compiled at %s, %s", MCU, mem.version, mem.pid, __TIME__, __DATE__);
	print_debug("mem size=%u, check this with your host to ensure struct correctness", sizeof(mem));
}
#endif

/*
 * Bug-fixed(64byte) version of USB setup for STM32H7xx
 */
#ifdef BUG_FIXED_USB_SETUP_H7XX_FS_MAIN_C
#undef BUG_FIXED_USB_SETUP_H7XX_FS_MAIN_C
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
static uint8_t BUG_FIXED_USBD_CDC_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum) {
	USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*) pdev->pClassData;
	PCD_HandleTypeDef *hpcd = pdev->pData;
	PCD_EPTypeDef *ep;
	ep = &hpcd->IN_ep[epnum];
	if (ep->xfer_len > 0 && ep->xfer_len % ep->maxpacket == 0) {
		USBD_LL_Transmit(pdev, epnum, NULL, 0);
		return USBD_OK;
	} else {
		if(pdev->pClassData != NULL) {
			hcdc->TxState = 0;
			return USBD_OK;
		} else {
			return USBD_FAIL;
		}
	}
}
extern __ALIGN_BEGIN uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END;
static void fixed_usb_handling_h7xx_user_code_begin_2() {
	USBD_CDC.DataIn = BUG_FIXED_USBD_CDC_DataIn;
	USBD_FS_DeviceDesc[10] = (uint8_t)(MEMORY_READER_H7XX_PID & 0x00FF);
	USBD_FS_DeviceDesc[11] = (uint8_t)((MEMORY_READER_H7XX_PID & 0xFF00) >> 8);
	HAL_PWREx_EnableUSBVoltageDetector();  // must do this for usb to work !!! this must be a bug of CubeMX !!!
}
static void WTF_fix_pwr_while_1() {
	__IO uint32_t tmpreg = 0x00;
	MODIFY_REG(PWR->CR3, PWR_CR3_BYPASS, 1);
	/* Delay after an RCC peripheral clock enabling */
	tmpreg = READ_BIT(PWR->CR3, PWR_CR3_BYPASS);
	UNUSED(tmpreg);
}
#endif

/*
 * USB callback for STM32H7xx
 */
#ifdef USB_CALLBACK_H7XX_FS_USBD_CDC_IF_C
#undef USB_CALLBACK_H7XX_FS_USBD_CDC_IF_C
extern USBD_HandleTypeDef hUsbDeviceFS;
static inline int8_t USB_CALLBACK_H7XX_FS_CDC_Receive_FS(uint8_t* Buf, uint32_t *Len) {
	// unsigned int i;
	// for (i=0; i<*Len; ++i) {
	// 	if (fifo_full(&mem.siorx)) {
	// 		mem.siorx_overflow += (*Len - i);  // record them to err, host should check them sometimes
	// 		break;
	// 	} else {
	// 		fifo_enque(&mem.siorx, (char)Buf[i]);
	// 	}
	// }
	uint32_t actually_received = fifo_copy_from_buffer(&mem.siorx, (char*)Buf, *Len);
	mem.siorx_overflow += *Len - actually_received;
	// CDC_Transmit_FS(Buf, *Len);  // loop test
	USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
	USBD_CDC_ReceivePacket(&hUsbDeviceFS);
	return (USBD_OK);
}
#endif

/*
 * USB fifo transmitter for STM32H7xx
 */
#ifdef USB_FIFO_TRANSMITTER_FS_MAIN_C
#undef USB_FIFO_TRANSMITTER_FS_MAIN_C
extern void usb_fifo_transmit(void);
#endif
#ifdef USB_FIFO_TRANSMITTER_FS_USBD_CDC_IF_C
#undef USB_FIFO_TRANSMITTER_FS_USBD_CDC_IF_C
void usb_fifo_transmit(void) {
	if (((USBD_CDC_HandleTypeDef*)(hUsbDeviceFS.pClassData))->TxState == 0 && !fifo_empty(&mem.siotx)) {  // has to be this!
		// int i; for (i=0; i<APP_TX_DATA_SIZE && !fifo_empty(&mem.siotx); ++i) UserTxBufferFS[i] = fifo_deque(&mem.siotx);
		uint32_t i = fifo_move_to_buffer((char*)UserTxBufferFS, &mem.siotx, APP_TX_DATA_SIZE-1);  // improve performance
		if (i) CDC_Transmit_FS(UserTxBufferFS, i);  // print_debug("send %d byte", i);
    }
}
#endif

/*
 * printf needs this
 */
#ifdef FPUTC_ON_USART3
#undef FPUTC_ON_USART3
int fputc(int ch, FILE *f) {
	if (mem.flags & FLAG_MASK_DISABLE_UART3_OUT) {
		if (fifo_full(&mem.usart3_tx))
			mem.usart3_tx_overflow ++;
		else fifo_enque(&mem.usart3_tx, ch);
	} else {
		while (!(mem.flags & FLAG_MASK_DISABLE_UART3_OUT) && fifo_full(&mem.usart3_tx));  // wait others to send
		if (fifo_full(&mem.usart3_tx))
			mem.usart3_tx_overflow ++;
		else {
			fifo_enque(&mem.usart3_tx, ch);
			SET_BIT(USART3->CR1, USART_CR1_TXEIE);  // make sure to start txe
		}
	}
	return ch;
}
#endif
#ifdef FPUTC_ON_UART7
#undef FPUTC_ON_UART7
int fputc(int ch, FILE *f) {
	if (mem.flags & FLAG_MASK_DISABLE_UART3_OUT) {
		if (fifo_full(&mem.usart3_tx))
			mem.usart3_tx_overflow ++;
		else fifo_enque(&mem.usart3_tx, ch);
	} else {
		while (!(mem.flags & FLAG_MASK_DISABLE_UART3_OUT) && fifo_full(&mem.usart3_tx));  // wait others to send
		if (fifo_full(&mem.usart3_tx))
			mem.usart3_tx_overflow ++;
		else {
			fifo_enque(&mem.usart3_tx, ch);
			SET_BIT(UART7->CR1, USART_CR1_TXEIE);  // make sure to start txe
		}
	}
	return ch;
}
#endif

/*
 * USART3_IRQHandler
 */
#ifdef PUTC_USART3_IRQHandler_IT_C
#undef PUTC_USART3_IRQHandler_IT_C
static inline void putc_usart3_irqhandler(void) {
	if(USART3->ISR & USART_ISR_TXE_TXFNF_Msk) {
		if (fifo_empty(&mem.usart3_tx) || mem.flags & FLAG_MASK_DISABLE_UART3_OUT) CLEAR_BIT(USART3->CR1, USART_CR1_TXEIE);
		else {
			USART3->TDR = fifo_deque(&mem.usart3_tx);
		}
	}
}
#endif
#ifdef PUTC_UART7_IRQHandler_IT_C
#undef PUTC_UART7_IRQHandler_IT_C
static inline void putc_uart7_irqhandler(void) {
	if(UART7->ISR & USART_ISR_TXE_TXFNF_Msk) {
		if (fifo_empty(&mem.usart3_tx) || mem.flags & FLAG_MASK_DISABLE_UART3_OUT) CLEAR_BIT(UART7->CR1, USART_CR1_TXEIE);
		else {
			UART7->TDR = fifo_deque(&mem.usart3_tx);
		}
	}
}
#endif

/*
 * Tx functions
 */
#ifdef TX_FUNCTIONS
#undef TX_FUNCTIONS
static inline void tx_set_D13_use_tim4_or_lptim1(char use_tim4_not_lptim1) {  // only 0 or 1 is permitted
	uint32_t temp = GPIOD->AFR[1];
	temp &= ~(0xFU << 20);
	temp |= (((0x1U << use_tim4_not_lptim1) & 0x03) << 20);
	// if (!use_tim4_not_lptim1) temp |= ((1) << 20);  // lptim1
	// else temp |= ((2) << 20);  // tim4
	GPIOD->AFR[1] = temp;
}
// 1: lptim1 / tim4
// 0: lptim2
static inline void tx_out(unsigned char x) {
	GPIOA->BSRR = (uint32_t)GPIO_PIN_9 << (!x << 4);  // <<0 or <<16
}
#endif

/*
 * ADC Header
 */
#ifdef ADC_READER_H7
#undef ADC_READER_H7
#define BUF_DMA_ADC_LENGTH 128
extern uint16_t buf_dma_adc[BUF_DMA_ADC_LENGTH];
#endif

/*
 * ADC initialize
 */
#ifdef ADC_READER_H7_FUNCTIONS
#undef ADC_READER_H7_FUNCTIONS
#include "ls_255_910K_10K_25K_70dB.h"
uint16_t buf_dma_adc[BUF_DMA_ADC_LENGTH] __attribute__((section(".ARM.__at_0x24000000")));  // buf here
extern DMA_HandleTypeDef hdma_adc1;
static inline void QAMDemod_Downsampling_x32_x2_init() {
	mem.filter.flag = 0;
	for (int i=0; i<256; ++i) {
		mem.filter.coeff[i] = mem.filter.coeff[i+256] = demod_coeff_ls_255_910K_10K_25K_70dB[i];
	}
	memset(mem.filter.data, 0, sizeof(mem.filter.data));
	memset(mem.filter.output, 0, sizeof(mem.filter.output));
	mem.filter.index = 0;
	mem.filter.rsh = demod_rsh_ls_255_910K_10K_25K_70dB;
}
static void QAMDemod_Downsampling_x32_x2_run(uint16_t* samples, Fifo_t* out) {
	SCB_InvalidateDCache();
	//Ia Ib Qa Qb Ia Ib Qa Qb
	//Ia Ia Ib Ib Qa Qa Qb Qb
	uint16_t (*S)[8] = (uint16_t(*)[8])samples; 
	uint16_t (*T)[8] = (uint16_t(*)[8])&mem.filter.data2[mem.filter.index >> 1];
	for (int i = 0; i < 8; ++i) {
#define H(x, y)  T[i][(x)] = S[i][(y)]
		H(0, 0); H(1, 4); H(2, 1); H(3, 5); 
		H(4, 2); H(5, 6); H(6, 3); H(7, 7);
#undef H
	}
	register const unsigned *coeff_base = (const unsigned*)(mem.filter.coeff + 256 - mem.filter.index);
	register const unsigned *D = (const unsigned*)mem.filter.data;
	register unsigned ret0 = 0, ret1 = 0, ret2 = 0, ret3 = 0;
#define T(i)	do {  \
	register unsigned u1, u2, r0, r1; \
	u1 = coeff_base[i];\
	u2 = coeff_base[i + 1];\
	r0 = D[i * 4 + 0];  \
	r1 = D[i * 4 + 1];  \
	ret0 = __SMLAD(r0, u1, ret0);\
	ret1 = __SMLAD(r1, u1, ret1);\
	r0 = D[i * 4 + 2];  \
	r1 = D[i * 4 + 3];  \
	ret2 = __SMLAD(r0, u1, ret2); \
	ret3 = __SMLAD(r1, u1, ret3); \
	r0 = D[i * 4 + 4];  \
	r1 = D[i * 4 + 5];  \
	ret0 = __SMLAD(r0, u2, ret0);\
	ret1 = __SMLAD(r1, u2, ret1);\
	r0 = D[i * 4 + 6];  \
	r1 = D[i * 4 + 7];  \
	ret2 = __SMLAD(r0, u2, ret2); \
	ret3 = __SMLAD(r1, u2, ret3); \
} while(0)
	T(0); T(2); T(4); T(6); T(8); T(10); T(12); T(14); T(16); T(18); T(20); T(22); T(24); T(26); T(28); T(30); T(32); T(34); T(36); T(38); T(40); T(42); T(44); T(46); T(48); T(50); T(52); T(54); T(56); T(58); T(60); T(62); T(64); T(66); T(68); T(70); T(72); T(74); T(76); T(78); T(80); T(82); T(84); T(86); T(88); T(90); T(92); T(94); T(96); T(98); T(100); T(102); T(104); T(106); T(108); T(110); T(112); T(114); T(116); T(118); T(120); T(122); T(124); T(126);
#undef T
#define O(i) mem.filter.output[i] = (int)ret##i >> mem.filter.rsh
  O(0); O(1); O(2); O(3);
#undef O
	mem.filter.index += 16;
	if (mem.filter.flag & DEMOD_DOWNSAMPLE_FLAG_OUTPUT_8BYTE) {
		// in Ia, Qa, Ib, Qb order
		// for (int i=0; i<3; ++i)  // stress test of throughput
		if (out->length - fifo_data_count(out) <= 8) {
			mem.adc_data_overflow++;
		} else {
			fifo_enque(out, mem.filter.Ia_out);
			fifo_enque(out, mem.filter.Ia_out >> 8);
			fifo_enque(out, mem.filter.Qa_out);
			fifo_enque(out, mem.filter.Qa_out >> 8);
			fifo_enque(out, mem.filter.Ib_out);
			fifo_enque(out, mem.filter.Ib_out >> 8);
			fifo_enque(out, mem.filter.Qb_out);
			fifo_enque(out, mem.filter.Qb_out >> 8);
		}
	}
}
void halfcplt_callback(struct __DMA_HandleTypeDef * hdma) {
	QAMDemod_Downsampling_x32_x2_run(buf_dma_adc, &mem.adc_data);
}
void cplt_callback(struct __DMA_HandleTypeDef * hdma) {
	QAMDemod_Downsampling_x32_x2_run(buf_dma_adc + 64, &mem.adc_data);
}
void adc_reader_h7_initialize(void) {
	QAMDemod_Downsampling_x32_x2_init();
	HAL_ADCEx_MultiModeStart_DMA(&hadc1, (void*)buf_dma_adc, sizeof(uint16_t) * BUF_DMA_ADC_LENGTH / 4);
	// very strange! the header files tells me that this is in bytes, but /4 works well ...
	hdma_adc1.XferHalfCpltCallback = &halfcplt_callback;
	hdma_adc1.XferCpltCallback = &cplt_callback;
	HAL_LPTIM_PWM_Start(&hlptim1, mem.tx.period_lptim1, mem.tx.pulse_lptim1);
	HAL_LPTIM_PWM_Start(&hlptim3, 16-1, 8-1);  // lptim3 is ADC DMA trigger, exactly 4x lptim1
	HAL_LPTIM_PWM_Start(&hlptim2, mem.tx.period_lptim2, mem.tx.pulse_lptim2);  // lptim2 for 32khz
	htim4.Instance->ARR = mem.tx.period_tim4;  // htim4.Init.Period = 
	htim4.Instance->CCR2 = mem.tx.pulse_tim4;
	// HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
	tx_set_D13_use_tim4_or_lptim1(mem.tx.default_sample.use_tim4_not_lptim1);
	tx_out(mem.tx.default_sample.select_lptim1tim4_not_lptim2);
	htim2.Instance->ARR = mem.tx.period_tim2;
	// HAL_TIM_Base_Start_IT(&htim2);
}
#endif

// 设计需求：
// 由于采样率定为1.82MS/s，平均每个点只能分配小于200个指令周期去的计算力，这部分需要精心设计
// 首先，对于一个FIR滤波器，多级滤波器没有显著的好处，反而引入更多的overhead，故而需要一次性将1.82MHz降到一个可以慢速处理的频率
// 我们将这个频率定为1820/32=56.875kHz

// 设计参数：
// 根据MATLAB的filter designer给出的FIR结果，一个阻带为28.4375kHz的FIR滤波器，使用最小二乘设计，第一个峰在-70dB左右，足够使用，此时通带为12kHz
// 相应地，观察阻带，基本上在-100dB左右，就算56.875kHz的倍频全部叠加，也只有大约30倍，也就是6dB左右，同样满足设计要求
// 得出结论，这样一个设计是符合要求的，即511阶FIR滤波器，共有512个参数

// 具体实现：
// 由于我们需要在数字域进行同步解调，进入的数据需要乘以相应的sin和cos，然后分别进行低通滤波，这样一来效率堪忧，尤其是每个中断都需要计算乘法
// 这里我们限制只进行4倍频采样，即，对于455kHz的载波频段，采用1.82Mhz频率采样，然后sin序列就是[1,0,-1,0]，cos序列就是[0,1,0,-1]，这样就没有了乘法
// 进一步地，乘后的序列，中间有很多的0值，这些0值的计算资源可以省出来，实际每一路每次只需要计算256个16bit乘法
// 使用SIMD指令，多次循环展开，极限情况可以达到每个运算1.5个指令周期，这样每一路是384个指令周期，加上杂七杂八的东西，假设是500个指令周期
// 接下来，仅需要以56.875kHz的频率去计算这个数值就可以了，这个的计算力要求是60kHz*500=30MHz，两路一起是60MHz，对于计算力要求并不是很高
// 对于0的数据直接忽略，实际喂入滤波器的数据是910kS/s，即16倍降采样

// 实现验证：
// 给定一个参数个数为256的滤波器，观察实际运行这个函数的速度如何
// 这个函数1s钟可以运行7406662次，也就是平均每次大约为54个周期，比较合理
// 实际这个函数被调用的频率是1.82MHz（两路），占用CPU 25%的计算资源

// 进一步验证方案：
// 接下来测试读取GPIO并口并推入FIFO，然后不断地从FIFO中取数据，再喂入输出流，最后从输出流取数据这样一个过程，但不考虑中断
// 这样测试下来1s可以运行3032804次，而实际运行频率为1.82MHz，这部分占用CPU 2/3的资源了，只剩下100MHz左右的计算力，这也是为什么一定要用H7
// 至此，这个方案完全通过，比较惊险地符合了计算力的要求

// 进一步优化：
// 尝试减少循环展开次数，发现性能变差
// 尝试增加循环展开次数，发现区别不大，故就用现在这种吧
// 但是周期还是不对，怀疑取指和取数据都不是单周期行为，查阅资料需要使用ITCM和DTCM才行
// 先尝试关掉Icache和Dcache，居然性能掉到了1M左右，这样一来就完全不够了！
// 看到一篇博客 http://forum.armfly.com/forum.php?mod=viewthread&tid=86207&extra=
// 设置程序在ITCM里，但是速度反而变慢了......
// 优化失败，还是保持原状
// 尝试打开optimized for time，获得增幅，到了3202848
// 打开cross-module optimization 无用
// 切换到v6 gnu99编译器，加上static inline操作，增加到3508960
// 把filter函数也加上static inline，变为4243615
// 最后把滤波器函数原型改为数组输入，最终每秒钟能跑4123360个数据，占用CPU大约1.82/4.12=44%计算力，比较合理

// 板子画完了，进行下一步验证，即加入中断观察性能影响
// 发现1.82MHz的中断太快了，不能满足要求，直接变成1817258次，几乎处于不能跑的地步，必须要优化。
// 首先尝试DTCM优化，又看到一篇https://blog.csdn.net/qq413886183/article/details/7848927，8并找到了DTCM其实是在0x20010000的位置，而不是0x20000000
// 尝试了发现没有效果，还是改回去...有点不太明白，总觉得官方库已经是用的DTCM了啊，因为RAM只有128K？
// 好吧，那再试试ITCM？
// 先把main.c的内容移到user.c里面，发现性能居然变好了一点点？1836840...奇怪
// 接下来把user下面的所有文件放入ITCM，这里面没有HAL库相关的东西
// 然后变成了1836918，也没有快多少
// 接下来优化中断，首先是中断的开启，不能直接调用HAL库的中断，不然会出现很多很多奇怪的中断
// 中断只开启Compare match的中断，变成了2681824，这已经是能够运行的程度了！cong.！
// 但还能进一步优化中断调用函数，函数里面改为自己实现的，省去很多判断语句，试试看优化到什么程度？
// 因为中断只可能是compare match，不要判断直接__HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_CMPM);
// 优化到了3802290，已经和不加中断的4123360相差不大了，感觉很不错！
// 接下来在中断函数里加入GPIO设置，用来看看中断的调用频率到底是多少？
// PD13原本是LPTIM1的输出，现在取消输出改为手动输出toggle，如果正确，则应该是1.82MHz/2=910kHz频率的方波
// 发现频率才300kHz左右，不大对劲，修改参数以后调到了910kHz，即中断函数运行周期为1.82MHz，正常‭，数据为3233167‬，还是处于能用的状态，占用了1.82/3.23=56%的计算力，还算OK吧

// 新发现是H7的ADC自带16bit最高3.6MS/s，而14bit可以到4MS/s，足够我们使用，比外置ADC省空间和布线（预计2层板可以做）
// 再加上现在方案确定使用2路的信号，不会有更多的路数，那么能不能一个单片机直接采集两路信号送到电脑上，这样就不需要额外的开销了，这样性能OK吗？
// 由于之前全部是中断方案，引入了大量的overhead，比如从44到56的算力差别，这个的overhead还是挺大的，占了总计算的25%左右。那么不要这些overhead会不会很快呢？
// 直接测试跑IQ滤波器，即2个256点的滤波器，一秒钟可以运行35万次。我们实际要的输出是2个56.875kHz的数，也即大约11万次每秒，占了30%的算力，大约120MHz，这样的话完全没有问题，而且慢速的中断也减少很多中断的overhead

// 接下来假设ADC有DMA功能，能够把数据点一个个送入一个指针的地址位置
// 尝试在cubemx上配置一下这个

// 上次尝试因为把这部分暂缓了，所以没有继续做下去，现在因为F4也需要重新做硬件，在MobiCom之前最后尝试一次H7！
// 首先需要把CPU性能标定一下，编写一个程序叫做computation_test();
// 正常情况下单独运行filter_tester可以到46万次每秒
// 只运行computation_test，每秒102万次，每周期加上一个computation_test变为每秒32万次，听起来蛮合理的
// 接下来就是测试实际运行的时候了。设计是这样的，首先测试ADC的DMA中断，看看能不能连续进入中断

// 测试USB的STM32主动发送，可以到800KB/s的速度，也即6.4mbps。传56.875kHz的8byte sample需要455KB/s，大约为极限测试的一半，感觉很OK

// 接下来实际部署滤波器。最小二乘设计，阶数511采样率1820K和阶数255采样率910K效果是一模一样的
// 实际为了方便，使用255阶的910K采样滤波器，通带10K（陶瓷滤波器限制），阻带25K，完成70dB以上的阻带衰减

// 每次调用需要喂入32个sample，也就是32*4byte=128byte，DMA开两倍大小防止数据冲突，也就是256byte
// 每次调用 QAMDemod_Downsampling_x32_x2

// 最终性能测试为18万，CPU只剩下17%的性能，剩下相当于70MHz的算力，算是还行了吧

/*
 * DAC functions
 */
#ifdef DAC_FUNCTIONS
#undef DAC_FUNCTIONS
// #define DAC8411_SYNC_SET() DAC_NSYNC_GPIO_Port->BSRRL = DAC_NSYNC_Pin
// #define DAC8411_SYNC_CLR() DAC_NSYNC_GPIO_Port->BSRRH = DAC_NSYNC_Pin
// #define DAC8411_SCLK_SET() DAC_SCK_GPIO_Port->BSRRL = DAC_SCK_Pin
// #define DAC8411_SCLK_CLR() DAC_SCK_GPIO_Port->BSRRH = DAC_SCK_Pin
// #define DAC8411_DIN_SET() DAC_MOSI_GPIO_Port->BSRRL = DAC_MOSI_Pin
// #define DAC8411_DIN_CLR() DAC_MOSI_GPIO_Port->BSRRH = DAC_MOSI_Pin
#define DAC8411_SYNC_SET() DAC_NSYNC_GPIO_Port->BSRR = DAC_NSYNC_Pin
#define DAC8411_SYNC_CLR() DAC_NSYNC_GPIO_Port->BSRR = (uint32_t)DAC_NSYNC_Pin << 16U
#define DAC8411_SCLK_SET() DAC_SCK_GPIO_Port->BSRR = DAC_SCK_Pin
#define DAC8411_SCLK_CLR() DAC_SCK_GPIO_Port->BSRR = (uint32_t)DAC_SCK_Pin << 16U
#define DAC8411_DIN_SET() DAC_MOSI_GPIO_Port->BSRR = DAC_MOSI_Pin
#define DAC8411_DIN_CLR() DAC_MOSI_GPIO_Port->BSRR = (uint32_t)DAC_MOSI_Pin << 16U
static void DAC8411_DELAY(void) {
	// __asm__ __volatile__( "1:\n subs %0, #1\n bne 1b\n" :: "r"(400) );
    volatile int i=40;
    while (i--);
}
// refer to http://www.ti.com/product/dac8411
// from https://github.com/qpalzmqaz123/magnetic but modified with datasheet as reference
void dac8411_update(unsigned short data) {  // TODO: check how long time does it take to run this function? about 1us?
    unsigned int tmp = data;
    tmp <<= 6;  // DAC8411需写24bit=2bit+16bit+6bit，前2bit保留为PD0和PD1，后6bit don't care
    DAC8411_SCLK_SET();  // added by wy@181219, must be CLK=1 now
    DAC8411_DELAY();
    DAC8411_SYNC_SET();
	DAC8411_DELAY(); // 拉高SYNC至少一个CLK
	DAC8411_SCLK_CLR();
	DAC8411_DELAY();
	DAC8411_SYNC_CLR(); // 开始写入
	DAC8411_DELAY();
    for(unsigned int i = 0; i < 24; i++) {
		DAC8411_SCLK_SET(); // 上升沿准备数据
		if(tmp & (1UL << (23 - i))) DAC8411_DIN_SET();
		else DAC8411_DIN_CLR();
		DAC8411_DELAY();
		DAC8411_SCLK_CLR(); // 下降沿读取
		DAC8411_DELAY();
	}
}
#endif

/*
 * Loop for reader H7
 */
#ifdef LOOP_READER_H7
#undef LOOP_READER_H7
static inline void reader_h7_loop(void) {
	if (mem.flags & FLAG_MASK_HALT) return;  // prevent loop
    // if (mem.flags & FLAG_MASK_SYSTEM_RESET) {
	// 	for (int i=3; i>0; --i) { printf("reset in %d seconds...\n", i); HAL_Delay(1000); }  // delay 1s, for usb to close
	// 	HAL_NVIC_SystemReset();  // reset system
    // }
    // loop();

	// add 191016: MCU should push data to PC directly, instead of waiting for queue read command
	while (mem.adc_data_push_packets_count && fifo_count(&mem.adc_data) >= 248 && fifo_remain(&mem.siotx) >= 248 + 200) {
		softio_delay_write_fifo_part(sio, mem.adc_data, 248);
		mem.adc_data_push_packets_count -= 1;
	}
	// printf("sio.read: %d, write: %d\n", sio.read, sio.write);  // TODO print read and write properly
}
#endif

/*
 * Hooks for reader H7
 */
#ifdef HOOKS_READER_H7
#undef HOOKS_READER_H7
void my_before(void* softio, SoftIO_Head_t* head) {
	if (head->type == SOFTIO_HEAD_TYPE_WRITE || head->type == SOFTIO_HEAD_TYPE_READ) {
		uint8_t need_disable_irq = 0;
		if (softio_is_variable_included(sio, *head, mem.tx.count)) need_disable_irq = 1;  // to avoid race condition
		if (softio_is_variable_included(sio, *head, mem.tx.count_add)) need_disable_irq = 1;
		if (need_disable_irq) __disable_irq();
	}
}
void my_after(void* softio, SoftIO_Head_t* head) {
	if (head->type == SOFTIO_HEAD_TYPE_WRITE || head->type == SOFTIO_HEAD_TYPE_READ) {
		uint8_t need_enable_irq = 0;
		if (softio_is_variable_included(sio, *head, mem.tx.count)) need_enable_irq = 1;;  // to avoid race condition
		if (softio_is_variable_included(sio, *head, mem.tx.count_add)) {
			mem.tx.count += mem.tx.count_add;  // add without race condition
			need_enable_irq = 1;
		}

		if (softio_is_variable_included(sio, *head, mem.dac_set)) {
			dac8411_update(mem.dac_set);
			mem.dac_now = mem.dac_set;
		}
		if (softio_is_variable_included(sio, *head, mem.tx.period_lptim1)) hlptim1.Instance->ARR = mem.tx.period_lptim1;
		if (softio_is_variable_included(sio, *head, mem.tx.pulse_lptim1)) hlptim1.Instance->CMP = mem.tx.pulse_lptim1;
		if (softio_is_variable_included(sio, *head, mem.tx.period_lptim2)) hlptim2.Instance->ARR = mem.tx.period_lptim2;
		if (softio_is_variable_included(sio, *head, mem.tx.pulse_lptim2)) hlptim2.Instance->CMP = mem.tx.pulse_lptim2;
		if (softio_is_variable_included(sio, *head, mem.tx.period_tim4)) htim4.Instance->ARR = mem.tx.period_tim4;
		if (softio_is_variable_included(sio, *head, mem.tx.pulse_tim4)) htim4.Instance->CCR2 = mem.tx.pulse_tim4;
		if (softio_is_variable_included(sio, *head, mem.tx.period_tim2)) htim2.Instance->ARR = mem.tx.period_tim2;
		if (need_enable_irq) __enable_irq();
	}
}
void hooks_init() {
	sio.before = my_before;
	sio.after = my_after;
}
#endif

/*
 * Tx Interrupt
 */
#ifdef INTERRUPT_READER_H7
#undef INTERRUPT_READER_H7

#include "stm32h7xx_hal_tim.h"
extern TIM_HandleTypeDef htim2;
#define tx_set_sample(sample) do {\
	tx_set_D13_use_tim4_or_lptim1(sample.use_tim4_not_lptim1); \
	tx_out(sample.select_lptim1tim4_not_lptim2); \
} while(0)
static inline __attribute__((always_inline)) void reader_h7_tim2_callback() {
	__HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);  // clear flag
    if (mem.tx.count) {  // if waiting to send
		if (fifo_empty(&mem.tx_data)) {
			++mem.tx.underflow;
			tx_set_sample(mem.tx.default_sample);
		} else {
			uint8_t data = fifo_deque(&mem.tx_data);
			Tx_Sample_t sample = *(Tx_Sample_t*)&data;
			tx_set_sample(sample);
		}
		--mem.tx.count;
	} else {
		tx_set_sample(mem.tx.default_sample);
	}
}

#endif
