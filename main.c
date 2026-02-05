#include "rtx_os.h"
#include "stm32f4xx.h"                 
#include "cmsis_os2.h"
#include "EventRecorder.h"

void GPIO_Init(void);

//typedef void (*osThreadFunc_t) (void *argument);
osThreadFunc_t Task1_p;   //un puntatore a funzione è un puntatore non è una funzione, quindi è
                         //una variabile che contiene l'indirizzo di quel tipo di funzione							


void Task1(void*);
void Task1(void* arg)
{
 (void)arg;
	for(;;)
   {
//     GPIOA->ODR ^= GPIO_ODR_OD5_Msk;
		 osDelay(2000);
	 }
}

void createTask(osThreadFunc_t Task)
{
  osThreadAttr_t attr = {0};
  attr.name = "LedBlink";
  attr.stack_size = 512;
  attr.priority = osPriorityNormal;
  osThreadNew(Task,NULL,&attr);
 }


 
 int main (void)
{
	
	//SystemCoreClockUpdate();
	EventRecorderInitialize(EventRecordAll, 1U);
	EventRecorderStart();
	GPIO_Init();
	Task1_p=Task1; /*solo a scopo dimostrativo passo il puntatore, potrei passare direttamente la funzione Task1 ad osThreadNew*/
  osKernelInitialize();
	createTask(Task1_p);
	osKernelStart();
	for(;;) {}
	
}


void GPIO_Init(void)
{

  //RCC->AHB1ENR |=  1;             /* enable GPIOA clock */
  //GPIOA->MODER &= ~0x00000C00;    /* clear pin mode */
  //GPIOA->MODER |=  0x00000400;    /* set pin to output mode */
  //GPIOA->MODER |=  (1U << (5 * 2));  // set PA5 come output	
}
 