
/* Standard includes. */
#include <string.h>
#include <math.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Library includes. */
#include "LPC17xx.h"
#include "LPC17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_pwm.h"
#include "lpc17xx_systick.h"
#include "lpc17xx_clkpwr.h"
#include "lpc17xx_libcfg_default.h"
//Definicion de Palabras

#define	ADELANTE	1
#define	ATRAS		0
#define	DIFERENCIA	150 // cuanta diferencia de distancia tiene que haber de 0 A 4048
//#define configTICK_RATE_HZ 1000 // cambiar para que los tick ahora se hagan 1 000 000 en un segundo para que haga una cuenta en micrsoegundos
//#define RES 150 // RESOLUCION DEL SENSOR DE PISO
#define RES 10
// VARIABLES GLOBALES
int MEM;
int x;
int ADCDIFI;
int ADCDIFD;
uint32_t mcTicks = 0; //variable para guardar los microsegundos
Bool meSali = FALSE;
Bool daleReversa = FALSE;
Bool zumbador = FALSE;
const portTickType xTicksToWait = 100 / portTICK_RATE_MS;
/*-----------------------------------------------------------*/
//PRIMERO SE DECLARAN LOS PROTOTIPOS DE FUNCIONES
//DE TASKS
void vTaskRevisarSensoresPiso(void *pvParameters);
void vTaskQuienSensoPiso(void *pvParameters);
void vTaskBuscarEncontrar(void *pvParameters);
void vTaskEsperarBoton(void *pvParameters);


//DE FUNCIONES
void motorDerecho(int direccion, int PWM); // direccion> adelante = 1, reversa = 0
void motorIzquierdo(int direccion, int PWM);
void configurePWMpin(const unsigned int pin);
void setPWM(const unsigned int channel, const unsigned int value);
void initADC();
void readADC(const int channel);
void buscoEncuentro();
int leerSensoresPiso(int pin);
Bool buscarOponenteDerecha();
void delay(int us);
void inicializarMotores();
void config();

//Declaracion del Handler

xTaskHandle xTask1Handle;
xTaskHandle xTaskBotonHandle;
xTaskHandle xTaskRevisarHandle;
xTaskHandle xTaskBuscarHandle;


// Crear Queues

xQueueHandle visionQueue;

/*-------
 *       FIO_ByteSetDir(1, 3, INT3_LED, 1);      PARTE EN 4 PARTES DE 8 AL REGISTRO DE 32 BITS
		 FIO_ByteClearValue(1, 3, INT3_LED);	EL PRIMERO ES EL PUERTO, EL SEGUNDO LA PARTE, EL TERCERO EL BIT
		 FIO_ByteSetValue(1, 3, INT3_LED);		INT3_LED        (1<<5)          // P1.29
 *
------- */
/* Used in the run time stats calculations. */
static uint32_t ulClocksPer10thOfAMilliSecond = 0UL;


/*-----------------------------------------------------------*/

int main( void )
{
	config();
	inicializarMotores();
	initADC();


	//creacion de la Queue para 5 elementos en RAM que permite guardar un dando de tipo uint32_t
		visionQueue = xQueueCreate(5, sizeof(int));
		if (visionQueue !=NULL){   ///PROTECCION EN CASO DE QUE LA QUEUE NO SE PUEDA CREAR
	xTaskCreate( vTaskEsperarBoton, "EsperarBoton", 1000, NULL, 1, &xTaskBotonHandle );
		}
	//xTaskCreate( vTaskBuscarEncontrar, "BuscarEncontrar", 1000, NULL, 0, NULL );

	//Esta tarea va a estar corriendo simultaneamente con todas las demas para checar los sensores en el piso
	//al momento de sensar se va a cambiar su prioridad y entrara otra tarea al multitasking
	//prioridad de 0 es la menor, mayor numero es mas prioridad

	//xTaskCreate( vTaskRevisarSensoresPiso, "Revisar Sensores Piso", 1000, NULL, 0, &xTaskRevisarHandle );

	/* Start the FreeRTOS scheduler. */
	vTaskStartScheduler();
	for(;;);
}
/*-----------------------------------------------------------*/
void config()
{
	LPC_GPIO0->FIODIR &= ~(1 << 11); //DECLARACION COMO ENTRADA DEL PIN PARA EL BOTON P0.11
	LPC_GPIO0->FIODIR |= (1 << 9); //DECLARACION COMO salida del led        DEL PIN PARA EL BOTON P0.11
	LPC_GPIO2->FIODIR |= (1 << 7); //DECLARACION COMO salida del led  IR P2.7
	LPC_GPIO2->FIODIR |= (1 << 13); //DECLARACION COMO salida del led  IR P2.13

	LPC_GPIO2->FIOSET = (1 << 13) ;

	PINSEL_CFG_Type PinCfg;   /// configracion del pin que controla la luz del piso IR P2.7
			PinCfg.Funcnum= PINSEL_FUNC_0;
			PinCfg.OpenDrain = PINSEL_PINMODE_OPENDRAIN;
			PinCfg.Pinmode = PINSEL_PINMODE_PULLDOWN;
			PinCfg.Portnum = PINSEL_PORT_2;
			PinCfg.Pinnum = PINSEL_PIN_7;
		PINSEL_ConfigPin(&PinCfg);

}

void configurePWMpin(const unsigned int pin) {
  const int channel = pin;

  /* Configure each PWM channel: --------------------------------------------- */
  /* - Single edge
   * - PWM Duty on each PWM channel determined by
   * the match on channel 0 to the match of that match channel.
   * Example: PWM Duty on PWM channel 1 determined by
   * the match on channel 0 to the match of match channel 1.
   */

  /* Configure PWM channel edge option
   * Note: PWM Channel 1 is in single mode as default state and
   * can not be changed to double edge mode */
  if (channel > 1) {
    PWM_ChannelConfig(LPC_PWM1, channel, PWM_CHANNEL_SINGLE_EDGE);
  }

  /* Configure match value for each match channel */
  /* Set up match value */
  PWM_MatchUpdate(LPC_PWM1, channel, 0, PWM_MATCH_UPDATE_NOW);

  /* Configure match option */
  PWM_MATCHCFG_Type PWMMatchCfgDat;
  PWMMatchCfgDat.MatchChannel = channel;
  PWMMatchCfgDat.IntOnMatch = DISABLE;
  PWMMatchCfgDat.ResetOnMatch = DISABLE;
  PWMMatchCfgDat.StopOnMatch = DISABLE;
  PWM_ConfigMatch(LPC_PWM1, &PWMMatchCfgDat);

  /* Enable PWM Channel Output */
  PWM_ChannelCmd(LPC_PWM1, channel, ENABLE);


}

void setPWM(const unsigned int channel, const unsigned int value) {

  PWM_MatchUpdate(LPC_PWM1, channel, value, PWM_MATCH_UPDATE_NOW);
}

void inicializarMotores()
{

	  /* PWM block section -------------------------------------------- */
	  /* Initialize PWM peripheral, timer mode
	   * PWM prescale value = 1 (absolute value - tick value) */
	  PWM_TIMERCFG_Type PWMCfgDat;
	  PWMCfgDat.PrescaleOption = PWM_TIMER_PRESCALE_TICKVAL;
	  PWMCfgDat.PrescaleValue = 1;
	  PWM_Init(LPC_PWM1, PWM_MODE_TIMER, (void *) &PWMCfgDat);

	  /* Set match value for PWM match channel 0 = 256, update immediately */
	  PWM_MatchUpdate(LPC_PWM1, 0, 256, PWM_MATCH_UPDATE_NOW);   /// <-------------------
	  /* PWM Timer/Counter will be reset when channel 0 matching
	   * no interrupt when match
	   * no stop when match */
	  PWM_MATCHCFG_Type PWMMatchCfgDat;
	  PWMMatchCfgDat.IntOnMatch = DISABLE;
	  PWMMatchCfgDat.MatchChannel = 0;
	  PWMMatchCfgDat.ResetOnMatch = ENABLE;
	  PWMMatchCfgDat.StopOnMatch = DISABLE;
	  PWM_ConfigMatch(LPC_PWM1, &PWMMatchCfgDat);

	  configurePWMpin(3);
	  configurePWMpin(4);
	  configurePWMpin(5);

	  /* Reset and Start counter */
	  PWM_ResetCounter(LPC_PWM1);
	  PWM_CounterCmd(LPC_PWM1, ENABLE);
	  PWM_Cmd(LPC_PWM1, ENABLE);

	// DECLARACION como salida el P2.0, P2.1, P2.2, P2.3
//	LPC_GPIO2->FIODIR |=  (1 << 2) | (1 << 3); //1 = salida, 0 = entrada
	LPC_GPIO0->FIODIR |=  (1 << 10) | (1 << 5); //1 = salida, 0 = entrada

	// CONFIGURACION DEL FUNCIONAMIENTO DE LOS PINES.
		PINSEL_CFG_Type PinCfg;
			PinCfg.Funcnum = PINSEL_FUNC_1;
			PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
			PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
			PinCfg.Pinnum = PINSEL_PIN_2;
			PinCfg.Portnum = PINSEL_PORT_2;
			PINSEL_ConfigPin(&PinCfg);
			PinCfg.Pinnum = PINSEL_PIN_3;
			PINSEL_ConfigPin(&PinCfg);

			PinCfg.Pinnum = PINSEL_PIN_4;
			PINSEL_ConfigPin(&PinCfg);

			PinCfg.Portnum = PINSEL_PORT_0;
			PinCfg.Funcnum = PINSEL_FUNC_0;
			PinCfg.Pinnum = PINSEL_PIN_10;
			PINSEL_ConfigPin(&PinCfg);
			PinCfg.Funcnum = PINSEL_FUNC_0;   //declaracion de la configuracion de los pines
			PinCfg.Pinnum = PINSEL_PIN_5;
			PINSEL_ConfigPin(&PinCfg);

}

void motorDerecho(int direccion, int PWM)
{

	if(direccion == 0)
	{
		LPC_GPIO0->FIOSET = (1 << 5) ;  // 1 al pin
	}

	if(direccion == 1)
	{
		LPC_GPIO0->FIOCLR = (1 << 5) ; // 0 al pin
	}

	setPWM(3,PWM);
}

void motorIzquierdo(int direccion, int PWM)
{
	if(direccion == 0)
	{
		LPC_GPIO0->FIOSET = (1 << 10) ;  // 1 al pin
	}

	if(direccion == 1)
	{
		LPC_GPIO0->FIOCLR = (1 << 10) ; // 0 al pin
	}

	setPWM(4,PWM);
}

void configureADC(const int channel) {
  ADC_IntConfig( LPC_ADC, channel, DISABLE);
  ADC_ChannelCmd(LPC_ADC, channel, ENABLE);
}

void initADC() {
  // Configure the ADCs
	LPC_GPIO0->FIODIR &= ~((1 << 23)  | (1 << 24) | (1 << 25) | (1 << 26)); // entradas por puertos adc
	ADC_Init(LPC_ADC, 1000);
	configureADC(0);
	configureADC(1);
	configureADC(2);
	configureADC(3);

  // CONFIGURACION DEL FUNCIONAMIENTO DE LOS PINES.
  		PINSEL_CFG_Type PinCfg;
  			PinCfg.Funcnum = PINSEL_FUNC_1;
  			PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
  			PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
  			PinCfg.Portnum = PINSEL_PORT_0;
  			PinCfg.Pinnum = PINSEL_PIN_23;
  			PINSEL_ConfigPin(&PinCfg);
  			PinCfg.Pinnum = PINSEL_PIN_24;
  			PINSEL_ConfigPin(&PinCfg);
  			PinCfg.Pinnum = PINSEL_PIN_25;
  			PINSEL_ConfigPin(&PinCfg);
  			PinCfg.Pinnum = PINSEL_PIN_26;
  			PINSEL_ConfigPin(&PinCfg);

  ADC_StartCmd(LPC_ADC, ADC_START_CONTINUOUS);
  ADC_BurstCmd(LPC_ADC, 1);
}

void readADC(const int channel) {
	int promedio = 0;
	int i;
	int lValueToSend = 0;

	for (i = 0; i < 100; i++)
	  {
		promedio += ADC_ChannelGetData(LPC_ADC, channel);
	  }

	lValueToSend = promedio/100;

	xQueueSendToBack(visionQueue, &lValueToSend, 0 );
}


Bool buscarOponenteDerecha()
{
	int visionlReceivedValue;
	Bool FLAG = FALSE;

	int ADC, ADCMOV, vueltas = 0;

	readADC(0);
	xQueueReceive(visionQueue, &visionlReceivedValue, xTicksToWait );
	ADC = visionlReceivedValue;

	for (;;)
	{
		motorIzquierdo(ADELANTE,100);
		motorDerecho(ATRAS,100);
		vTaskDelay(10/portTICK_RATE_MS);
		motorIzquierdo(ATRAS,0);
		motorDerecho(ADELANTE,0);




		readADC(0);
		xQueueReceive(visionQueue, &visionlReceivedValue, xTicksToWait );
		ADCMOV = visionlReceivedValue;
		vueltas++;
	//	vTaskDelay(10/portTICK_RATE_MS);
		ADCDIFD = ADCMOV-ADC;

if(vueltas>7000)
{
	FLAG = TRUE;
	LPC_GPIO2->FIOSET = (1 << 13);
	break;
}
		if(ADCDIFD >= DIFERENCIA)
		{

			readADC(0); // confirmo la lectura
			xQueueReceive(visionQueue, &visionlReceivedValue, xTicksToWait );
			ADCMOV = visionlReceivedValue;
			ADCDIFD = ADCMOV-ADC;

			if(ADCDIFD > DIFERENCIA + 4)
					{
						FLAG = TRUE;
						LPC_GPIO2->FIOSET = (1 << 13);
						break;
					}
		}
		if(ADCDIFD < 10)
		{
			LPC_GPIO2->FIOCLR = (1 << 13);
		}



	}
		ADC = ADCMOV;
		if(!FLAG)
		{
			motorIzquierdo(ATRAS,100);
			motorDerecho(ADELANTE,100);
			vTaskDelay(300/portTICK_RATE_MS);
			motorIzquierdo(ATRAS,0);
			motorDerecho(ADELANTE,0);
		}
		return FLAG;
}

void vTaskBuscarEncontrar(void *pvParameters)
{
	/*ESTA TAREA SE ENCARGA DE VER EN DOND ESTA EL ENEMIGO, AL FINALIZAR, ESTA TAREA SE DESTRUYE Y CREA LA TAREA DE ATAQUE
	 * SE ANALIZAN LOS DATOS DEL SENSOR IR COMPARANDO UNA MEDICION CON CADA MOVIMIENTO Y ASI SABER EN DONDE ESTA EL ATACANTE
	 */

	//DECLARACION DE VARIABLES
	Bool flag1;

	//INICIA TAREA
	while(1){
	flag1 = buscarOponenteDerecha();


				vTaskDelay(10/portTICK_RATE_MS);

				if (flag1)
				{


					motorDerecho(ADELANTE, 250);
					motorIzquierdo(ADELANTE,250);
					daleReversa = TRUE;
					vTaskSuspend( NULL );


				}
				motorDerecho(ADELANTE, 0);
				motorIzquierdo(ADELANTE,0);
				vTaskDelay(100/portTICK_RATE_MS);
	}
}




int leerSensoresPiso(int pin)
{

	LPC_GPIO2->FIOSET = (1 << 7) ;  /// CADA VEZ QUE SE LEEN LOS SENSORES, SE ENCIENDE EL IR
	unsigned int sensor_value = RES;

	LPC_GPIO2->FIODIR |= (1 << pin); //DECLARACION COMO salida del sensor P        DEL PIN PARA EL BOTON P0.11
	LPC_GPIO2->FIOSET = (1 << pin) ;

	//delay(10000);           // carga el capacitor por 10 us
	delay(100);


	LPC_GPIO2->FIODIR &= ~(1 << pin); // declara como entrada
	LPC_GPIO2->FIOCLR = (1 << pin) ;  // lo pone en estado bajo

	    unsigned long startTime = xTaskGetTickCount();
	    while (xTaskGetTickCount() - startTime < RES)
	    {
	        unsigned int time = xTaskGetTickCount() - startTime;
	            if ( !(LPC_GPIO2->FIOPIN & (1 << pin)) && time < sensor_value)
	            {
	                sensor_value = time;
	            }

	    }
	LPC_GPIO2->FIOCLR = (1 << 7) ; /// CADA VEZ QUE SE LEEN LOS SENSORES, SE APAGA EL IR PARA AHORRAR BATERIA
	    return sensor_value;
}


void vTaskRevisarSensoresPiso(void *pvParameters)
{


	while(1){



	for(x = 10; x < 13; x++)
	{
		MEM = leerSensoresPiso(x);

		if (MEM > 10)
		{

			meSali = FALSE; // ESTE SE PUEDE QUITAR, POR DEFECTO YA ES FALSO
				 LPC_GPIO0->FIOSET = (1 << 9) ;


		}
		if (MEM < 10) /// ESTE DATO TENDRIA QUE CALIBRARSE
		{
			if (daleReversa)
			{
				x = 9;
			}
			meSali = TRUE;

			break; // me salgo y la variable x se queda con el sensor que senso el blanco



		}

	}
		if (meSali)
		{
			/*SI SE CUMPLE CREA LA TAREA DE NO SALIRME DEL CIRCULO CON UNA PRIORIDAD MAYOR
			 */
			LPC_GPIO0->FIOCLR = (1 << 9) ;
			meSali = FALSE;
			xTaskCreate(vTaskQuienSensoPiso, "Quien Senso en el piso", 1000, NULL, 2, NULL );
		}

	}
}

void delay(int us){
    volatile int    i;
        for (i = 0; i < us; i++)
        {
        	mcTicks++ ;    /* Burn cycles. */

        }
}

void vTaskQuienSensoPiso(void *pvParameters)
{
	vTaskSuspend( xTaskRevisarHandle );
	vTaskSuspend(  xTaskBuscarHandle );

	switch (x)   // ver quien senso de ultimo
	{

	case 9:    //sensor izquierda
				motorDerecho(ATRAS,200);
				motorIzquierdo(ATRAS,200);
			//	delay(1000000);
				daleReversa = FALSE;
				vTaskDelay(700/portTICK_RATE_MS);
				vTaskResume( xTaskBuscarHandle );

			break;


	case 10:    //sensor izquierda
			motorDerecho(ATRAS,200);
			motorIzquierdo(ATRAS,200);
		//	delay(1000000);
			vTaskDelay(150/portTICK_RATE_MS);
			motorDerecho(ADELANTE,0);
			motorIzquierdo(ADELANTE,0);
		//	delay(1000000);
			vTaskDelay(100/portTICK_RATE_MS);
			motorDerecho(ATRAS,200);
			motorIzquierdo(ADELANTE,200);
		//	delay(1000000);
			vTaskDelay(100/portTICK_RATE_MS);
		break;

	case 11: // sensor derecha
		motorDerecho(ATRAS,200);
		motorIzquierdo(ATRAS,200);
		vTaskDelay(150/portTICK_RATE_MS);
		motorDerecho(ADELANTE,0);
		motorIzquierdo(ADELANTE,0);
		vTaskDelay(100/portTICK_RATE_MS);
		motorDerecho(ADELANTE,200);
		motorIzquierdo(ATRAS,200);
		vTaskDelay(100/portTICK_RATE_MS);

		break;

	case 12: // sensor enmedio

		motorDerecho(ATRAS,200);
		motorIzquierdo(ATRAS,200);
		vTaskDelay(150/portTICK_RATE_MS);
		motorDerecho(ADELANTE,0);
		motorIzquierdo(ADELANTE,0);
		vTaskDelay(100/portTICK_RATE_MS);
		motorDerecho(ADELANTE,200);
		motorIzquierdo(ATRAS,200);
		vTaskDelay(100/portTICK_RATE_MS);

		break;

	default:
		motorIzquierdo(ATRAS, 0);
		motorDerecho(ATRAS, 0);
		break;

	}

	motorIzquierdo(ATRAS, 0);
	motorDerecho(ATRAS, 0);
	vTaskResume( xTaskRevisarHandle );
	vTaskResume( xTaskBuscarHandle );

	vTaskDelete( NULL ); // se elimina sola Y EMPIEZA OTR VEZ NORMAL

}

void vTaskEsperarBoton(void *pvParameters)
{
	LPC_GPIO2->FIOCLR = (1 << 9) ;
	while(1)
	{

		if(!(LPC_GPIO0->FIOPIN & (1 << 11)))  //AND AL PIN 8 EN EL REGISTRO PARA COMPARARLO Y SABER SI ESTA ENCENDIDO O NO
				{

				  LPC_GPIO0->FIOSET = (1 << 9) ;
				  vTaskDelay(500/portTICK_RATE_MS);
				  LPC_GPIO0->FIOCLR = (1 << 9) ;
				  vTaskDelay(500/portTICK_RATE_MS);
				  LPC_GPIO0->FIOSET = (1 << 9) ;
				  vTaskDelay(500/portTICK_RATE_MS);
				  LPC_GPIO0->FIOCLR = (1 << 9) ;
				  vTaskDelay(500/portTICK_RATE_MS);
				  LPC_GPIO0->FIOSET = (1 << 9) ;
				  vTaskDelay(500/portTICK_RATE_MS);
				  LPC_GPIO0->FIOCLR = (1 << 9) ;
				  vTaskDelay(500/portTICK_RATE_MS);

				  xTaskCreate( vTaskRevisarSensoresPiso, "Revisar Sensores Piso", 1000, NULL, 0, &xTaskRevisarHandle );

				  xTaskCreate( vTaskBuscarEncontrar, "BuscarEncontrar", 1000, NULL, 0, &xTaskBuscarHandle );
				  vTaskDelete( xTask1Handle );
				  vTaskDelete( NULL );
				}
	}
}




/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void vMainConfigureTimerForRunTimeStats( void )
{
	/* How many clocks are there per tenth of a millisecond? */
	ulClocksPer10thOfAMilliSecond = configCPU_CLOCK_HZ / 10000UL;
}
/*-----------------------------------------------------------*/

uint32_t ulMainGetRunTimeCounterValue( void )
{
uint32_t ulSysTickCounts, ulTickCount, ulReturn;
const uint32_t ulSysTickReloadValue = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
volatile uint32_t * const pulCurrentSysTickCount = ( ( volatile uint32_t *) 0xe000e018 );
volatile uint32_t * const pulInterruptCTRLState = ( ( volatile uint32_t *) 0xe000ed04 );
const uint32_t ulSysTickPendingBit = 0x04000000UL;

	/* NOTE: There are potentially race conditions here.  However, it is used
	anyway to keep the examples simple, and to avoid reliance on a separate
	timer peripheral. */


	/* The SysTick is a down counter.  How many clocks have passed since it was
	last reloaded? */
	ulSysTickCounts = ulSysTickReloadValue - *pulCurrentSysTickCount;

	/* How many times has it overflowed? */
	ulTickCount = xTaskGetTickCountFromISR();

	/* Is there a SysTick interrupt pending? */
	if( ( *pulInterruptCTRLState & ulSysTickPendingBit ) != 0UL )
	{
		/* There is a SysTick interrupt pending, so the SysTick has overflowed
		but the tick count not yet incremented. */
		ulTickCount++;

		/* Read the SysTick again, as the overflow might have occurred since
		it was read last. */
		ulSysTickCounts = ulSysTickReloadValue - *pulCurrentSysTickCount;
	}

	/* Convert the tick count into tenths of a millisecond.  THIS ASSUMES
	configTICK_RATE_HZ is 1000! */
	ulReturn = ( ulTickCount * 10UL ) ;

	/* Add on the number of tenths of a millisecond that have passed since the
	tick count last got updated. */
	ulReturn += ( ulSysTickCounts / ulClocksPer10thOfAMilliSecond );

	return ulReturn;
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/
