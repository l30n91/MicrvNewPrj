#include "cmsis_os2.h"
#include "stm32f4xx.h"
#include "EventRecorder.h"
#include <stdint.h>

/* =========================
 * Messaggio da 8 byte
 * ========================= */
typedef struct {
    uint8_t data[9];
} uart_pkt_t;

/* =========================
 * Oggetti RTOS
 * ========================= */
osMessageQueueId_t qUartTx;
osSemaphoreId_t semTxDone;

/* =========================
 * Prototipi
 * ========================= */
static void GPIO_Init(void);
static void USART2_Init(void);
static void DMA1_Stream6_Init(void);
static void USART2_StartTxDMA(uint8_t *buf, uint16_t len);

void TaskA(void *arg);
void TaskB(void *arg);
void TaskUartTx(void *arg);

/* =========================
 * Task A
 * ========================= */
void TaskA(void *arg)
{
    (void)arg;

    uart_pkt_t pkt = { .data = { 'A','A','A','A','A','A','A','\n','\r' } };

    for (;;) {
        osMessageQueuePut(qUartTx, &pkt, 0U, osWaitForever);
        osDelay(1000);
    }
}

/* =========================
 * Task B
 * ========================= */
void TaskB(void *arg)
{
    (void)arg;

    uart_pkt_t pkt = { .data = { 'B','B','B','B','B','B','B','\n','\r' } };

    for (;;) {
        osMessageQueuePut(qUartTx, &pkt, 0U, osWaitForever);
        osDelay(1500);
    }
}

/* =========================
 * Task UART TX
 * ========================= */
void TaskUartTx(void *arg)
{
    (void)arg;

    uart_pkt_t pkt;

    for (;;) {
        /* 1) prende 8 byte dalla queue */
        osMessageQueueGet(qUartTx, &pkt, NULL, osWaitForever);

        /* 2) avvia DMA verso USART2->DR */
        USART2_StartTxDMA(pkt.data, sizeof(pkt.data));

        /* 3) aspetta fine trasferimento DMA */
        osSemaphoreAcquire(semTxDone, osWaitForever);
    }
}

/* =========================
 * Avvio DMA TX su USART2
 * DMA1 Stream6 Channel4 è tipico per USART2_TX su STM32F4
 * ========================= */
static void USART2_StartTxDMA(uint8_t *buf, uint16_t len)
{
    /* Disabilita stream prima di riconfigurare */
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream6->CR & DMA_SxCR_EN) {}

    /* Pulisce i flag dello stream 6 */
    DMA1->HIFCR =
          DMA_HIFCR_CTCIF6
        | DMA_HIFCR_CHTIF6
        | DMA_HIFCR_CTEIF6
        | DMA_HIFCR_CDMEIF6
        | DMA_HIFCR_CFEIF6;

    /* Peripheral address = USART2 data register */
    DMA1_Stream6->PAR  = (uint32_t)&USART2->DR;

    /* Memory address = buffer sorgente */
    DMA1_Stream6->M0AR = (uint32_t)buf;

    /* Numero di byte */
    DMA1_Stream6->NDTR = len;

    /* Configurazione:
       - Channel 4
       - mem->periph
       - minc enable
       - peripheral increment disable
       - 8 bit / 8 bit
       - transfer complete interrupt enable
    */
    DMA1_Stream6->CR =
          (4U << DMA_SxCR_CHSEL_Pos)   /* Channel 4 */
        | DMA_SxCR_MINC                /* memory increment */
        | DMA_SxCR_DIR_0               /* memory-to-peripheral */
        | DMA_SxCR_TCIE;               /* transfer complete interrupt */

    /* Abilita richiesta DMA TX lato USART */
    USART2->CR3 |= USART_CR3_DMAT;

    /* Avvia stream */
    DMA1_Stream6->CR |= DMA_SxCR_EN;
}

/* =========================
 * IRQ DMA1 Stream6
 * ========================= */
void DMA1_Stream6_IRQHandler(void)
{
    /* Transfer complete stream 6? */
    if (DMA1->HISR & DMA_HISR_TCIF6) {

        /* Clear flag */
        DMA1->HIFCR = DMA_HIFCR_CTCIF6;

        /* Disabilita stream */
        DMA1_Stream6->CR &= ~DMA_SxCR_EN;

        /* opzionale: togli richiesta DMA su USART */
        USART2->CR3 &= ~USART_CR3_DMAT;

        /* Notifica task */
        osSemaphoreRelease(semTxDone);
    }

    /* Eventuali errori */
    if (DMA1->HISR & DMA_HISR_TEIF6) {
        DMA1->HIFCR = DMA_HIFCR_CTEIF6;
        DMA1_Stream6->CR &= ~DMA_SxCR_EN;
        USART2->CR3 &= ~USART_CR3_DMAT;
        osSemaphoreRelease(semTxDone);
    }
}

/* =========================
 * GPIO PA2 = USART2_TX
 * PA3 = USART2_RX
 * ========================= */
static void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA2, PA3 alternate function */
    GPIOA->MODER &= ~((3U << (2 * 2)) | (3U << (3 * 2)));
    GPIOA->MODER |=  ((2U << (2 * 2)) | (2U << (3 * 2)));

    /* AF7 = USART2 */
    GPIOA->AFR[0] &= ~((0xFU << (2 * 4)) | (0xFU << (3 * 4)));
    GPIOA->AFR[0] |=  ((7U   << (2 * 4)) | (7U   << (3 * 4)));
}

/* =========================
 * USART2 init minimale
 * PCLK1 = 16 MHz, baud ~115200
 * ========================= */
static void USART2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* Baudrate: 16 MHz / 115200 ˜ 138.9 -> 0x8B */
    USART2->BRR = 0x008B;

    /* TX enable, RX enable, USART enable */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/* =========================
 * DMA1 Stream6 init base
 * ========================= */
static void DMA1_Stream6_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    /* Disabilita stream */
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream6->CR & DMA_SxCR_EN) {}

    /* Clear flags */
    DMA1->HIFCR =
          DMA_HIFCR_CTCIF6
        | DMA_HIFCR_CHTIF6
        | DMA_HIFCR_CTEIF6
        | DMA_HIFCR_CDMEIF6
        | DMA_HIFCR_CFEIF6;

    NVIC_SetPriority(DMA1_Stream6_IRQn, 5);
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

/* =========================
 * main
 * ========================= */
int main(void)
{
    
    EventRecorderInitialize(EventRecordAll, 1U);
    EventRecorderStart();
    SystemCoreClockUpdate();

    GPIO_Init();
    USART2_Init();
    DMA1_Stream6_Init();

    osKernelInitialize();

    qUartTx  = osMessageQueueNew(8, sizeof(uart_pkt_t), NULL);
    semTxDone = osSemaphoreNew(1, 0, NULL);

    osThreadNew(TaskA, NULL, NULL);
    osThreadNew(TaskB, NULL, NULL);
    osThreadNew(TaskUartTx, NULL, NULL);

    osKernelStart();

    for (;;) {}
}