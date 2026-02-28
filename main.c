#include "rtx_os.h"
#include "stm32f4xx.h"                 
#include "cmsis_os2.h"
#include "EventRecorder.h"

void GPIO_Init(void);
void USART2_init(void);
void USART2_WriteTest(void);
void USART2_WriteString(uint8_t*); 

//typedef void (*osThreadFunc_t) (void *argument);
osThreadFunc_t Task1_p;   //un puntatore a funzione è un puntatore non è una funzione, quindi è
                         //una variabile che contiene l'indirizzo di quel tipo di funzione							
typedef struct {
  char array [200];
} msg_t;
osMessageQueueId_t shared_queue;

void Task1(void*);
void Task1(void* arg)
{
 (void)arg;
	//msg_t m1 = { .data = {1,2,3,4,5,6,7,8} };
	const char string[]="Hello World from task1\n\r";
	for(;;)
   {
     
		 osMessageQueuePut(shared_queue, string, 0U, osWaitForever);
		 GPIOA->ODR ^= GPIO_ODR_OD5_Msk;
		 osDelay(2000);
	 }
}


void Task2(void*);
void Task2(void* arg)
{
 (void)arg;
	//msg_t m2;
	const char string[]="Hello World from task2\n\r";
	for(;;)
   {
     osMessageQueuePut(shared_queue, string, 0U, osWaitForever);
		 //osMessageQueueGet(shared_queue, &m2, NULL, osWaitForever);
    // m.data contiene una COPIA sicura
		 //GPIOA->ODR ^= GPIO_ODR_OD5_Msk;
		 osDelay(2000);
	 }
}

void Task3(void*);
void Task3(void* arg)
{
 (void)arg;
	msg_t m2;
	uint8_t* p;
	p= (uint8_t*)&m2;
	for(;;)
   {
     osMessageQueueGet(shared_queue, &m2, NULL, osWaitForever);
		 USART2_WriteString((uint8_t*)p);
    // m.data contiene una COPIA sicura
		 //GPIOA->ODR ^= GPIO_ODR_OD5_Msk;
		 osDelay(2000);
	 }
}

void createTask1(osThreadFunc_t Task)
{
  
  osThreadAttr_t attr = {0};
  attr.name = "Task1";
  attr.stack_size = 512;
  attr.priority = osPriorityNormal;
  osThreadNew(Task,NULL,&attr);
 }

 
 void createTask2(osThreadFunc_t Task)
{
  
  osThreadAttr_t attr = {0};
  attr.name = "Task2";
  attr.stack_size = 512;
  attr.priority = osPriorityNormal;
  osThreadNew(Task,NULL,&attr);
 }
void createTask3_UsartWrite(osThreadFunc_t Task)
{
  osThreadAttr_t attr = {0};
  attr.name = "Task3";
  attr.stack_size = 512;
  attr.priority = osPriorityNormal;
  osThreadNew(Task,NULL,&attr);
 }
 
 int main (void)
{
	
	//SystemCoreClockUpdate();
	GPIO_Init();
	USART2_init();
  USART2_WriteTest(); /* Check if USART2 Writes on the terminal*/
  EventRecorderInitialize(EventRecordAll, 1U);
	EventRecorderStart();
	Task1_p = Task1; /*solo a scopo dimostrativo passo il puntatore, potrei passare direttamente la funzione Task1 ad osThreadNew*/
	osKernelInitialize();
	shared_queue = osMessageQueueNew(4, 200*sizeof(uint8_t), NULL);
	createTask1(Task1_p);
	createTask2(Task2);
	createTask3_UsartWrite(Task3);
	osKernelStart();
	for(;;) {
  
  }
	
}


void GPIO_Init(void)
{

  RCC->AHB1ENR |=  1;             /* enable GPIOA clock */
  GPIOA->MODER &= ~0x00000C00;    /* clear pin mode */
  GPIOA->MODER |=  0x00000400;    /* set pin to output mode */
  GPIOA->MODER |=  (1U << (5 * 2));  // set PA5 come output	
}



void USART2_init(void) {
    // Clock GPIOA e USART2
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // PA2 = TX (AF7), PA3 = RX (AF7)
    GPIOA->MODER &= ~((3U<<(2*2)) | (3U<<(2*3)));   // clear PA2, PA3
    GPIOA->MODER |=  ((2U<<(2*2)) | (2U<<(2*3)));   // Alternate Function
    GPIOA->AFR[0] &= ~((0xFU<<(4*2)) | (0xFU<<(4*3)));
    GPIOA->AFR[0] |=  ((7U  <<(4*2)) | (7U  <<(4*3))); // AF7

    // UART: 8N1, no flow control
    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = 0;
    // Scegli il BRR giusto per il tuo PCLK1
    USART2->BRR = 0x08B;  // 115200 @ PCLK1=16 MHz
    //USART2->BRR = 0x16D;     // 115200 @ PCLK1=42 MHz
    USART2->CR1 |= USART_CR1_TE | USART_CR1_RE;  // << abilita TX e RX
    USART2->CR1 |= USART_CR1_UE;                 // abilita USART
}


void USART2_WriteTest(void) {
char string_[]= "Test OK software started\n\r";
     uint8_t* p_string=(uint8_t*)string_;
     
     while(*p_string !='\0')
     {
       while(!(USART2->SR & USART_SR_TXE)); /* wait until TX is enabled*/
       
       USART2->DR = *p_string;
       
       p_string ++;
     }
}


void USART2_WriteString(uint8_t* p_string) {
//char string_[]= "anna ";
     //uint8_t* p_string=(uint8_t*)string_;
     
     while(*p_string !='\0')
     {
       while(!(USART2->SR & USART_SR_TXE)); /* wait until TX is enabled*/
       USART2->DR= *p_string;
       p_string ++;
     }
}