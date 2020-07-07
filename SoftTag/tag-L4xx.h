/*
 * This header library provides basic functions for MCU operation
 * take whatever you like and ignore others, by pre-define the function you need
 */

// MEMORY_TAG_L4XX_VERSION: uint32_t number, like 0x19052101, be sure to update this number when memory is different from before
#define MEMORY_TAG_L4XX_VERSION 0x19101900

/*
 * Memory Structure for STM32L4xx
 */
#ifdef MEMORY_TAG_L4XX
#undef MEMORY_TAG_L4XX
#include "fifo.h"
#include "softio.h"

#ifndef TAG_L4XX_SAMPLE_BYTE
#define TAG_L4XX_SAMPLE_BYTE 16  // allows 128 individual LCD pieces
#endif
struct Tag_Sample_t {
	uint8_t s[TAG_L4XX_SAMPLE_BYTE];
#ifdef __cplusplus
	Tag_Sample_t() { memset(s, 0, TAG_L4XX_SAMPLE_BYTE); }
	Tag_Sample_t& operator |= (const Tag_Sample_t& o) { for (int i=0; i<TAG_L4XX_SAMPLE_BYTE; ++i) s[i] |= o.s[i]; return *this; }
	uint8_t& le(int i) { assert(i>=0&&i<TAG_L4XX_SAMPLE_BYTE); return s[TAG_L4XX_SAMPLE_BYTE-1-i]; }
	uint8_t le(int i) const { assert(i>=0&&i<TAG_L4XX_SAMPLE_BYTE); return s[TAG_L4XX_SAMPLE_BYTE-1-i]; }
	uint8_t* les(int NLCD) { return s + TAG_L4XX_SAMPLE_BYTE - NLCD; }
	const uint8_t* les(int NLCD) const { return s + TAG_L4XX_SAMPLE_BYTE - NLCD; }
	bool operator == (const Tag_Sample_t& o) { return 0 == memcmp(les(TAG_L4XX_SAMPLE_BYTE), o.les(TAG_L4XX_SAMPLE_BYTE), TAG_L4XX_SAMPLE_BYTE); }
	bool equal(const Tag_Sample_t& o, int NLCD) { return 0 == memcmp(les(NLCD), o.les(NLCD), NLCD); }
#endif
} __attribute__((aligned(1)));
typedef struct Tag_Sample_t Tag_Sample_t;

typedef struct {
	#define COMMAND_NONE 0x44  // 什么都不做，这是command的常规状态，初始化为这个
	#define COMMAND_RUN_PENDING 0x55  // 主机控制单片机准备开始运行，单片机检测到这个信息后应当做硬件设置准备，准备完成后设为status_running
	#define COMMAND_STOP_PENDING 0x77  // 告诉STM32需要停下当前操作，并进行deinit操作，并随后转为 IDLE 态
	#define COMMAND_STR(command) ((command)==COMMAND_NONE?"none":\
								((command)==COMMAND_RUN_PENDING?"run_pending":\
								((command)==COMMAND_STOP_PENDING?"stop_pending":"ERROR")))
		uint8_t command;
	#define FLAG_MASK_HALT ((unsigned char)1<<0)  // 整机暂停操作，但不能停止定时器等，这个操作用来暂停while运行，详见main.c代码
	#define FLAG_MASK_DISABLE_LPUART1_OUT ((unsigned char)1<<1)  // 不让 lpuart1 fifo 内容输出到lpuart1，这样一来host可以去读取单片机的log并保存下来，省去再接一个串口调试了
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
	uint32_t mem_size;  // sizeof(ReaderL4_Mem_t)

	uint16_t siorx_overflow;
	uint16_t lpuart1_tx_overflow;

	uint16_t period_lptim2;
	uint16_t tx_underflow;
	uint32_t tx_count;  // atomic write ensured
	uint32_t tx_count_add;  // write this variable to atomically add to tx_count
	Tag_Sample_t default_sample;

	uint8_t PIN_EN9;
	uint8_t PIN_PWSEL;
	uint8_t PIN_D0;
	uint8_t PIN_RXEN;

	uint16_t NLCD;
#define REPEAT_STATE_NONE 0
#define REPEAT_STATE_SENDING 1
#define REPEAT_STATE_INTERVAL 2
	uint16_t repeat_state;
	uint32_t repeat_count;  // if not zero then will start repeating
	uint32_t repeat_interval;  // interval between each packet
	uint32_t repeat_idx;  // either count the index or count the interval

	char siorx_buf[16384];
	char siotx_buf[4096];
	char lpuart1_tx_buf[1024];
	char tx_data_buf[TAG_L4XX_SAMPLE_BYTE * 768];
#define TagL4_Mem_FifoInit(mem) do {\
	FIFO_STD_INIT(mem, siorx);\
	FIFO_STD_INIT(mem, siotx);\
	FIFO_STD_INIT(mem, lpuart1_tx);\
	FIFO_STD_INIT(mem, tx_data);\
} while(0)
	Fifo_t siorx;  // must be the first 
	Fifo_t siotx;
	Fifo_t lpuart1_tx;
	Fifo_t tx_data;

} TagL4_Mem_t;

#define print_debug(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_DEBUG))    printf("D: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_info(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_INFO))      printf("I: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_warn(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_WARN))      printf("W: " format "\r\n",##__VA_ARGS__); } while(0)
#define print_error(format, ...) do { if (VERBOSE_REACH_LEVEL(mem.verbose_level, VERBOSE_ERROR))    printf("E: " format "\r\n",##__VA_ARGS__); } while(0)
#endif

#ifdef MEMORY_TAG_L4XX_EXTERN
#undef MEMORY_TAG_L4XX_EXTERN
extern TagL4_Mem_t mem;
extern SoftIO_t sio;
#endif

/*
 * Memory Structure Initiator for STM32L4xx, you need to predefine the following things:
 * MEMORY_TAG_L4XX_PID: uint16_t number, like 0x5210 means may/21st, the first device in this day
 */
#ifdef MEMORY_TAG_L4XX_INIT
#undef MEMORY_TAG_L4XX_INIT
void memory_tag_l4xx_init_user_code_begin_sysinit(void) {
	mem.status = STATUS_INIT;  // host could read this status word, change to STATUS_IDLE when init successs
    mem.version = MEMORY_TAG_L4XX_VERSION;
    mem.pid = MEMORY_TAG_L4XX_PID;
	mem.verbose_level = VERBOSE_DEBUG;  // set verbose level
    mem.command = COMMAND_NONE;
    mem.flags = 0;
    mem.mode = MODE_NONE;
	mem.mem_size = sizeof(TagL4_Mem_t);
	mem.period_lptim2 = 4-1;  // default is 32k / 4 = 8kS/s
	mem.NLCD = TAG_L4XX_SAMPLE_BYTE;
	mem.repeat_state = REPEAT_STATE_NONE;
	mem.repeat_count = 0;
	mem.repeat_interval = 0;
	mem.repeat_idx = 0;
	SOFTIO_QUICK_INIT(sio, mem, TagL4_Mem_FifoInit);
}
static inline void memory_tag_l4xx_post_init_printhello(const char* MCU) {
	LPUART1->CR1 |= USART_CR1_TXEIE;  // enable tx empty interrupt
	LPUART1->CR1 |= USART_CR1_RXNEIE;  // enable rx not empty interrupt
	LPUART1->CR1 |= USART_CR1_UE;  // enable peripheral
	print_info("RetroTurboTag on %s, version 0x%08x, pid 0x%04X, compiled at %s, %s", MCU, mem.version, mem.pid, __TIME__, __DATE__);
	print_debug("mem size=%u, check this with your host to ensure struct correctness", sizeof(mem));
	LPTIM2->CFGR &= ~LPTIM_CFGR_WAVE;  // to set PWM mode
	LPTIM2->CR |= LPTIM_CR_ENABLE;  // enable and set continuous mode
	LPTIM2->CR |= LPTIM_CR_CNTSTRT;
	LPTIM2->ARR = mem.period_lptim2;
	LPTIM2->CMP = 1;
	LPTIM2->IER |= LPTIM_IT_ARRM;  // enable ARRM interrupt
	HAL_GPIO_WritePin(D0_GPIO_Port, D0_Pin, GPIO_PIN_SET);  // turn on LED
}
#endif

/*
 * USB callback for STM32L4xx
 */
#ifdef USB_CALLBACK_L4XX_FS_USBD_CDC_IF_C
#undef USB_CALLBACK_L4XX_FS_USBD_CDC_IF_C
extern USBD_HandleTypeDef hUsbDeviceFS;
static inline int8_t USB_CALLBACK_L4XX_FS_CDC_Receive_FS(uint8_t* Buf, uint32_t *Len) {
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
		uint32_t i = fifo_move_to_buffer((char*)UserTxBufferFS, &mem.siotx, APP_TX_DATA_SIZE);  // improve performance
		CDC_Transmit_FS(UserTxBufferFS, i);  // print_debug("send %d byte", i);
    }
}
#endif

/*
 * printf needs this
 */
#ifdef FPUTC_ON_LPUART1
#undef FPUTC_ON_LPUART1
int fputc(int ch, FILE *f) {
	if (mem.flags & FLAG_MASK_DISABLE_LPUART1_OUT) {
		if (fifo_full(&mem.lpuart1_tx))
			mem.lpuart1_tx_overflow ++;
		else fifo_enque(&mem.lpuart1_tx, ch);
	} else {
		while (!(mem.flags & FLAG_MASK_DISABLE_LPUART1_OUT) && fifo_full(&mem.lpuart1_tx));  // wait others to send
		if (fifo_full(&mem.lpuart1_tx)) mem.lpuart1_tx_overflow ++;
		else {
			fifo_enque(&mem.lpuart1_tx, ch);
			LPUART1->CR1 |= USART_CR1_TXEIE;  // make sure to start txe
		}
	}
	return ch;
}
#endif

/*
 * LPUART1_IRQHandler
 */
#ifdef PUTC_LPUART1_IRQHandler_IT_C
#undef PUTC_LPUART1_IRQHandler_IT_C
static inline void putc_lpuart1_irqhandler(void) {
	if (LPUART1->ISR & USART_ISR_TXE) {
		if (fifo_empty(&mem.lpuart1_tx) || mem.flags & FLAG_MASK_DISABLE_LPUART1_OUT) CLEAR_BIT(LPUART1->CR1, USART_CR1_TXEIE);
		else {
			LPUART1->TDR = fifo_deque(&mem.lpuart1_tx);
		}
	}
	if (LPUART1->ISR & USART_ISR_RXNE) {
		LPUART1->RDR;  // must read it to clear RXE flag !!!
	}
}
#endif

/*
 * Hooks for tag L4
 */
#ifdef HOOKS_TAG_L4
#undef HOOKS_TAG_L4
void my_before(void* softio, SoftIO_Head_t* head) {
	if (head->type == SOFTIO_HEAD_TYPE_WRITE || head->type == SOFTIO_HEAD_TYPE_READ) {
		uint8_t need_disable_irq = 0;
		if (softio_is_variable_included(sio, *head, mem.tx_count)) need_disable_irq = 1;  // to avoid race condition
		if (softio_is_variable_included(sio, *head, mem.tx_count_add)) need_disable_irq = 1;
		if (need_disable_irq) __disable_irq();
	}
}
void my_after(void* softio, SoftIO_Head_t* head) {
	if (head->type == SOFTIO_HEAD_TYPE_WRITE || head->type == SOFTIO_HEAD_TYPE_READ) {
		uint8_t need_enable_irq = 0;
		if (softio_is_variable_included(sio, *head, mem.tx_count)) need_enable_irq = 1;  // to avoid race condition
		if (softio_is_variable_included(sio, *head, mem.tx_count_add)) {
			if (head->type == SOFTIO_HEAD_TYPE_WRITE) mem.tx_count += mem.tx_count_add;  // add without race condition
			need_enable_irq = 1;
		}
		if (need_enable_irq) __enable_irq();
		if (head->type == SOFTIO_HEAD_TYPE_WRITE && softio_is_variable_included(sio, *head, mem.period_lptim2)) {  // adjust frequency
			LPTIM2->ARR = mem.period_lptim2;
		}
		if (head->type == SOFTIO_HEAD_TYPE_WRITE) {
			if (softio_is_variable_included(sio, *head, mem.PIN_EN9)) {
				uint16_t out = mem.PIN_EN9 ? EN9_Pin : 0;
				EN9_GPIO_Port->BSRR = out | ( ((uint32_t)(~out & EN9_Pin))<<16 );  // atomic write
			}
			if (softio_is_variable_included(sio, *head, mem.PIN_PWSEL)) {
				uint16_t out = mem.PIN_PWSEL ? PWSEL_Pin : 0;
				PWSEL_GPIO_Port->BSRR = out | ( ((uint32_t)(~out & PWSEL_Pin))<<16 );  // atomic write
			}
			if (softio_is_variable_included(sio, *head, mem.PIN_D0)) {
				uint16_t out = mem.PIN_D0 ? D0_Pin : 0;
				D0_GPIO_Port->BSRR = out | ( ((uint32_t)(~out & D0_Pin))<<16 );  // atomic write
			}
			if (softio_is_variable_included(sio, *head, mem.PIN_RXEN)) {
				uint16_t out = mem.PIN_RXEN ? RXEN_Pin : 0;
				RXEN_GPIO_Port->BSRR = out | ( ((uint32_t)(~out & RXEN_Pin))<<16 );  // atomic write
			}
		}
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
#ifdef INTERRUPT_TAG_L4
#undef INTERRUPT_TAG_L4
static volatile Tag_Sample_t dma_tx_sample;
extern SPI_HandleTypeDef hspi1;
__attribute__((always_inline)) void tag_l4_send_sample(uint16_t NLCD) {
	uint16_t out = (dma_tx_sample.s[TAG_L4XX_SAMPLE_BYTE-1] & 0x01) ? LCD_Pin : 0;
	LCD_GPIO_Port->BSRR = out | ( ((uint32_t)(~out & LCD_Pin))<<16 );  // atomic write
	HAL_SPI_Transmit_DMA(&hspi1, (((uint8_t*)&dma_tx_sample) + TAG_L4XX_SAMPLE_BYTE - NLCD), NLCD);
}
__attribute__((always_inline)) void tag_l4_lptim2_callback() {
	if (LPTIM2->ISR & LPTIM_ISR_ARRM) {  // autoreload match interrupt
		LPTIM2->ICR = LPTIM_ICR_ARRMCF;
		uint16_t NLCD = mem.NLCD;
		if (NLCD > TAG_L4XX_SAMPLE_BYTE) return;  // not send strange ones
		switch (mem.repeat_state) {
		case REPEAT_STATE_NONE:
			if (mem.repeat_count) {  // switch into repeat mode
				assert(mem.tx_data.read == 0 && "repeat mode must start from 0");
				if (mem.repeat_count != -1) --mem.repeat_count;  // -1 for always repeat
				mem.repeat_idx = 0;
				mem.repeat_state = REPEAT_STATE_SENDING;
			} else {  // normal stream
				if (mem.tx_count) {  // if waiting to send
					if (fifo_count(&mem.tx_data) < NLCD) {  // sample not available
						++mem.tx_underflow;
						dma_tx_sample = mem.default_sample;
						tag_l4_send_sample(NLCD);
					} else {
						for (int i=0; i<NLCD; ++i) dma_tx_sample.s[TAG_L4XX_SAMPLE_BYTE - NLCD + i] = fifo_deque(&mem.tx_data);
						tag_l4_send_sample(NLCD);
					}
					--mem.tx_count;
				} else {
					dma_tx_sample = mem.default_sample;
					tag_l4_send_sample(NLCD);
				}
			}
			break;
		case REPEAT_STATE_SENDING:
			if (mem.repeat_idx * NLCD < fifo_count(&mem.tx_data)) {
				// with prerequisite that (mem.tx_data.read == 0)
				int base_idx = mem.repeat_idx * NLCD;
				for (int i=0; i<NLCD; ++i) dma_tx_sample.s[TAG_L4XX_SAMPLE_BYTE - NLCD + i] = mem.tx_data_buf[base_idx + i];
				tag_l4_send_sample(NLCD);
				++mem.repeat_idx;
			} else {
				dma_tx_sample = mem.default_sample;
				tag_l4_send_sample(NLCD);
				if (mem.repeat_count) {  // still need to repeat
					mem.repeat_idx = 0;
					mem.repeat_state = REPEAT_STATE_INTERVAL;
				} else {  // no need to repeat, back to stream
					mem.repeat_state = REPEAT_STATE_NONE;
				}
			}
			break;
		case REPEAT_STATE_INTERVAL:
			if (mem.repeat_idx < mem.repeat_interval) {
				dma_tx_sample = mem.default_sample;  // send default sample
				tag_l4_send_sample(NLCD);
				++mem.repeat_idx;
			} else {
				dma_tx_sample = mem.default_sample;
				tag_l4_send_sample(NLCD);
				if (mem.repeat_count) {  // still need to repeat
					if (mem.repeat_count != -1) --mem.repeat_count;  // -1 for always repeat
					mem.repeat_idx = 0;
					mem.repeat_state = REPEAT_STATE_SENDING;
				} else {  // no need to repeat, back to stream
					mem.repeat_state = REPEAT_STATE_NONE;
				}
			}
			break;
		}
	}
}
#endif
