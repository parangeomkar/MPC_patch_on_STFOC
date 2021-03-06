
/**
  ******************************************************************************
  * @file    mc_tasks.c
  * @author  Motor Control SDK Team, ST Microelectronics
  * @brief   This file implements tasks definition
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
//cstat -MISRAC2012-Rule-21.1
#include "main.h"
//cstat +MISRAC2012-Rule-21.1
#include "mc_type.h"
#include "mc_math.h"
#include "motorcontrol.h"
#include "regular_conversion_manager.h"
#include "mc_interface.h"
#include "digital_output.h"
#include "pwm_common.h"

#include "mc_tasks.h"
#include "parameters_conversion.h"
#include "mcp_config.h"
#include "dac_ui.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private define */

/* Private define ------------------------------------------------------------*/
/* Un-Comment this macro define in order to activate the smooth
   braking action on over voltage */
/* #define  MC.SMOOTH_BRAKING_ACTION_ON_OVERVOLTAGE */

  #define CHARGE_BOOT_CAP_MS  ((uint16_t)10)
  #define CHARGE_BOOT_CAP_MS2 ((uint16_t)10)
  #define OFFCALIBRWAIT_MS    ((uint16_t)0)
  #define OFFCALIBRWAIT_MS2   ((uint16_t)0)
  #define STOPPERMANENCY_MS   ((uint16_t)400)
  #define STOPPERMANENCY_MS2  ((uint16_t)400)
  #define CHARGE_BOOT_CAP_TICKS  (uint16_t)((SYS_TICK_FREQUENCY * CHARGE_BOOT_CAP_MS) / ((uint16_t)1000))
  #define CHARGE_BOOT_CAP_TICKS2 (uint16_t)((SYS_TICK_FREQUENCY * CHARGE_BOOT_CAP_MS2)/ ((uint16_t)1000))
  #define OFFCALIBRWAITTICKS     (uint16_t)((SYS_TICK_FREQUENCY * OFFCALIBRWAIT_MS)   / ((uint16_t)1000))
  #define OFFCALIBRWAITTICKS2    (uint16_t)((SYS_TICK_FREQUENCY * OFFCALIBRWAIT_MS2)  / ((uint16_t)1000))
  #define STOPPERMANENCY_TICKS   (uint16_t)((SYS_TICK_FREQUENCY * STOPPERMANENCY_MS)  / ((uint16_t)1000))
  #define STOPPERMANENCY_TICKS2  (uint16_t)((SYS_TICK_FREQUENCY * STOPPERMANENCY_MS2) / ((uint16_t)1000))

/* USER CODE END Private define */
#define VBUS_TEMP_ERR_MASK (MC_OVER_VOLT| MC_UNDER_VOLT| MC_OVER_TEMP)
/* Private variables----------------------------------------------------------*/
static FOCVars_t FOCVars[NBR_OF_MOTORS];

static PWMC_Handle_t *pwmcHandle[NBR_OF_MOTORS];
static CircleLimitation_Handle_t *pCLM[NBR_OF_MOTORS];
//cstat !MISRAC2012-Rule-8.9_a
static RampExtMngr_Handle_t *pREMNG[NBR_OF_MOTORS];   /*!< Ramp manager used to modify the Iq ref
                                                    during the start-up switch over.*/

static uint16_t hMFTaskCounterM1 = 0; //cstat !MISRAC2012-Rule-8.9_a
static volatile uint16_t hBootCapDelayCounterM1 = ((uint16_t)0);
static volatile uint16_t hStopPermanencyCounterM1 = ((uint16_t)0);

static volatile uint8_t bMCBootCompleted = ((uint8_t)0);

/* USER CODE BEGIN Private Variables */

/* USER CODE END Private Variables */

/* Private functions ---------------------------------------------------------*/
void TSK_MediumFrequencyTaskM1(void);
void FOC_Clear(uint8_t bMotor);
void FOC_InitAdditionalMethods(uint8_t bMotor);
void FOC_CalcCurrRef(uint8_t bMotor);
void TSK_MF_StopProcessing(  MCI_Handle_t * pHandle, uint8_t motor);
MCI_Handle_t * GetMCI(uint8_t bMotor);
static uint16_t FOC_CurrControllerM1(void);
void TSK_SetChargeBootCapDelayM1(uint16_t hTickCount);
bool TSK_ChargeBootCapDelayHasElapsedM1(void);
void TSK_SetStopPermanencyTimeM1(uint16_t hTickCount);
bool TSK_StopPermanencyTimeHasElapsedM1(void);
void TSK_SafetyTask_PWMOFF(uint8_t motor);

/* USER CODE BEGIN Private Functions */

int i,j,c1_mpc,c2_mpc;
float IalphaPred,IbetaPred,IqTx;
int16_t Varray[7][3];
volatile int16_t IdPred,IqPred,IdTemp,IqTemp;

uint8_t optimalVector,optimalDuty,Sa,Sb,Sc;
int16_t polePairs,IqRef;




int states[7] = {1,3,2,6,4,5,0};
int switchingEffort[6][6] = {{0,1,2,3,2,1},
							 {1,0,1,2,3,2},
							 {2,1,0,1,2,3},
							 {3,2,1,0,1,2},
							 {2,3,2,1,0,1},
							 {1,2,3,2,1,0}
							};
int oldOptimalVector = 0;
int runMPC = 0;
int hasMPCinit = 0;
alphabeta_t Valbt;

int16_t Vphase = 8;
int16_t sinTable[361] = {0,572,1144,1715,2286,2856,3425,3993,4560,5126,5690,6252,6813,7371,7927,8481,9032,9580,10126,10668,11207,11743,12275,12803,13328,13848,14364,14876,15383,15886,16384,16876,17364,17846,18323,18794,19260,19720,20173,20621,21062,21497,21925,22347,22762,23170,23571,23964,24351,24730,25101,25465,25821,26169,26509,26841,27165,27481,27788,28087,28377,28659,28932,29196,29451,29697,29934,30162,30381,30591,30791,30982,31163,31335,31498,31650,31794,31927,32051,32165,32269,32364,32448,32523,32587,32642,32687,32722,32747,32762,32767,32762,32747,32722,32687,32642,32587,32523,32448,32364,32269,32165,32051,31927,31794,31650,31498,31335,31163,30982,30791,30591,30381,30162,29934,29697,29451,29196,28932,28659,28377,28087,27788,27481,27165,26841,26509,26169,25821,25465,25101,24730,24351,23964,23571,23170,22762,22347,21925,21497,21062,20621,20173,19720,19260,18794,18323,17846,17364,16876,16384,15886,15383,14876,14364,13848,13328,12803,12275,11743,11207,10668,10126,9580,9032,8481,7927,7371,6813,6252,5690,5126,4560,3993,3425,2856,2286,1715,1144,572,	0,-572,-1144,-1715,-2286,-2856,-3425,-3993,-4560,-5126,-5690,-6252,-6813,-7371,-7927,-8481,-9032,-9580,-10126,-10668,-11207,-11743,-12275,-12803,-13328,-13848,-14364,-14876,-15383,-15886,-16384,-16876,-17364,-17846,-18323,-18794,-19260,-19720,-20173,-20621,-21062,-21497,-21925,-22347,-22762,-23170,-23571,-23964,-24351,-24730,-25101,-25465,-25821,-26169,-26509,-26841,-27165,-27481,-27788,-28087,-28377,-28659,-28932,-29196,-29451,-29697,-29934,-30162,-30381,-30591,-30791,-30982,-31163,-31335,-31498,-31650,-31794,-31927,-32051,-32165,-32269,-32364,-32448,-32523,-32587,-32642,-32687,-32722,-32747,-32762,-32767,-32762,-32747,-32722,-32687,-32642,-32587,-32523,-32448,-32364,-32269,-32165,-32051,-31927,-31794,-31650,-31498,-31335,-31163,-30982,-30791,-30591,-30381,-30162,-29934,-29697,-29451,-29196,-28932,-28659,-28377,-28087,-27788,-27481,-27165,-26841,-26509,-26169,-25821,-25465,-25101,-24730,-24351,-23964,-23571,-23170,-22762,-22347,-21925,-21497,-21062,-20621,-20173,-19720,-19260,-18794,-18323,-17846,-17364,-16876,-16384,-15886,-15383,-14876,-14364,-13848,-13328,-12803,-12275,-11743,-11207,-10668,-10126,-9580,-9032,-8481,-7927,-7371,-6813,-6252,-5690,-5126,-4560,-3993,-3425,-2856,-2286,-1715,-1144,-572,	0};



void initModelPredictiveControl(){
	ab_t Vab;
	alphabeta_t Valphabeta;

	c1_mpc = (int)(10000*(1 - (0.65/(20000*0.0007))));
	c2_mpc = (int)(10000*(1/(20000*0.0007)));

	for(i=0;i<7;i++){
		Sa = states[i] & 0x01;
		Sb = (states[i]>>1) & 0x01;
		Sc = (states[i]>>2) & 0x01;

		Vab.a = 4*((2*Sa-Sb-Sc))*32767/Vphase;
		Vab.b = 4*((2*Sb-Sa-Sc))*32767/Vphase;

		Valphabeta = MCM_Clarke(Vab);

		Varray[i][0] = Valphabeta.alpha;
		Varray[i][1] = Valphabeta.beta;
	}
}

int16_t sinMPC(int16_t thetaElec){
	if(thetaElec < 0){
		thetaElec = ((360+thetaElec) - 360*(1+(thetaElec/360)));
	} else {
		thetaElec = (thetaElec - 360*(thetaElec/360));
	}

  if(thetaElec <= 90){
    return sinTable[thetaElec];
  } else if(thetaElec > 90 && thetaElec <=180){
    return sinTable[180 - thetaElec];
  } else if(thetaElec > 180 && thetaElec <= 270){
    return -sinTable[thetaElec - 180];
  } else {
    return -sinTable[360 - thetaElec];
  }
}

/* USER CODE END Private Functions */
/**
  * @brief  It initializes the whole MC core according to user defined
  *         parameters.
  * @param  pMCIList pointer to the vector of MCInterface objects that will be
  *         created and initialized. The vector must have length equal to the
  *         number of motor drives.
  * @retval None
  */
__weak void MCboot( MCI_Handle_t* pMCIList[NBR_OF_MOTORS] )
{
  /* USER CODE BEGIN MCboot 0 */

  /* USER CODE END MCboot 0 */

  if (MC_NULL == pMCIList)
  {
    /* Nothing to do */
  }
  else
  {

    bMCBootCompleted = (uint8_t )0;
    pCLM[M1] = &CircleLimitationM1;

    /**********************************************************/
    /*    PWM and current sensing component initialization    */
    /**********************************************************/
    pwmcHandle[M1] = &PWM_Handle_M1._Super;
    R3_2_Init(&PWM_Handle_M1);
    ASPEP_start(&aspepOverUartA);

    /* USER CODE BEGIN MCboot 1 */

    /* USER CODE END MCboot 1 */

    /**************************************/
    /*    Start timers synchronously      */
    /**************************************/
    startTimers();

    /******************************************************/
    /*   PID component initialization: speed regulation   */
    /******************************************************/
    PID_HandleInit(&PIDSpeedHandle_M1);

    /******************************************************/
    /*   Main speed sensor component initialization       */
    /******************************************************/
    STO_PLL_Init (&STO_PLL_M1);

    /******************************************************/
    /*   Speed & torque component initialization          */
    /******************************************************/
    STC_Init(pSTC[M1],&PIDSpeedHandle_M1, &STO_PLL_M1._Super);

    /****************************************************/
    /*   Virtual speed sensor component initialization  */
    /****************************************************/
    VSS_Init(&VirtualSpeedSensorM1);

    /**************************************/
    /*   Rev-up component initialization  */
    /**************************************/
    RUC_Init(&RevUpControlM1, pSTC[M1], &VirtualSpeedSensorM1, &STO_M1, pwmcHandle[M1]);

    /********************************************************/
    /*   PID component initialization: current regulation   */
    /********************************************************/
    PID_HandleInit(&PIDIqHandle_M1);
    PID_HandleInit(&PIDIdHandle_M1);

    /********************************************************/
    /*   Bus voltage sensor component initialization        */
    /********************************************************/
    RVBS_Init(&BusVoltageSensor_M1);

    /*************************************************/
    /*   Power measurement component initialization  */
    /*************************************************/
    pMPM[M1]->pVBS = &(BusVoltageSensor_M1._Super);
    pMPM[M1]->pFOCVars = &FOCVars[M1];

    /*******************************************************/
    /*   Temperature measurement component initialization  */
    /*******************************************************/
    NTC_Init(&TempSensor_M1);

    pREMNG[M1] = &RampExtMngrHFParamsM1;
    REMNG_Init(pREMNG[M1]);

    FOC_Clear(M1);
    FOCVars[M1].bDriveInput = EXTERNAL;
    FOCVars[M1].Iqdref = STC_GetDefaultIqdref(pSTC[M1]);
    FOCVars[M1].UserIdref = STC_GetDefaultIqdref(pSTC[M1]).d;
    MCI_Init(&Mci[M1], pSTC[M1], &FOCVars[M1],pwmcHandle[M1] );
    MCI_ExecSpeedRamp(&Mci[M1],
    STC_GetMecSpeedRefUnitDefault(pSTC[M1]),0); /*First command to STC*/

    pMCIList[M1] = &Mci[M1];

    DAC_Init(&DAC_Handle);

    /*************************************************/
    /*   STSPIN32G4 driver component initialization  */
    /*************************************************/
    STSPIN32G4_init( &HdlSTSPING4 );
    STSPIN32G4_reset( &HdlSTSPING4 );
    STSPIN32G4_setVCC( &HdlSTSPING4, (STSPIN32G4_confVCC){ .voltage = _12V,
                                                           .useNFAULT = true,
                                                           .useREADY = false } );
    STSPIN32G4_setVDSP( &HdlSTSPING4, (STSPIN32G4_confVDSP){ .deglitchTime = _4us,
                                                             .useNFAULT = true } );
    STSPIN32G4_clearFaults( &HdlSTSPING4 );

    /* USER CODE BEGIN MCboot 2 */

    /* USER CODE END MCboot 2 */

    bMCBootCompleted = 1U;
  }
}

/**
 * @brief Runs all the Tasks of the Motor Control cockpit
 *
 * This function is to be called periodically at least at the Medium Frequency task
 * rate (It is typically called on the Systick interrupt). Exact invokation rate is
 * the Speed regulator execution rate set in the Motor Contorl Workbench.
 *
 * The following tasks are executed in this order:
 *
 * - Medium Frequency Tasks of each motors
 * - Safety Task
 * - Power Factor Correction Task (if enabled)
 * - User Interface task.
 */
__weak void MC_RunMotorControlTasks(void)
{
  if (0U == bMCBootCompleted)
  {
    /* Nothing to do */
  }
  else
  {
    /* ** Medium Frequency Tasks ** */
    MC_Scheduler();

    /* Safety task is run after Medium Frequency task so that
     * it can overcome actions they initiated if needed. */
    TSK_SafetyTask();

  }
}

/**
 * @brief Performs stop process and update the state machine.This function
 *        shall be called only during medium frequency task
 */
void TSK_MF_StopProcessing(  MCI_Handle_t * pHandle, uint8_t motor)
{
  R3_2_SwitchOffPWM(pwmcHandle[motor]);

  FOC_Clear(motor);
  MPM_Clear((MotorPowMeas_Handle_t*) pMPM[motor]);
  TSK_SetStopPermanencyTimeM1(STOPPERMANENCY_TICKS);
  Mci[motor].State = STOP;
  return;
}

/**
 * @brief  Executes the Medium Frequency Task functions for each drive instance.
 *
 * It is to be clocked at the Systick frequency.
 */
__weak void MC_Scheduler(void)
{
/* USER CODE BEGIN MC_Scheduler 0 */

/* USER CODE END MC_Scheduler 0 */

  if (((uint8_t)1) == bMCBootCompleted)
  {
    if(hMFTaskCounterM1 > 0u)
    {
      hMFTaskCounterM1--;
    }
    else
    {
      TSK_MediumFrequencyTaskM1();

      MCP_Over_UartA.rxBuffer = MCP_Over_UartA.pTransportLayer->fRXPacketProcess(MCP_Over_UartA.pTransportLayer,
                                                                                &MCP_Over_UartA.rxLength);
      if ( 0U == MCP_Over_UartA.rxBuffer)
      {
        /* Nothing to do */
      }
      else
      {
        /* Synchronous answer */
        if (0U == MCP_Over_UartA.pTransportLayer->fGetBuffer(MCP_Over_UartA.pTransportLayer,
                                                     (void **) &MCP_Over_UartA.txBuffer, //cstat !MISRAC2012-Rule-11.3
                                                     MCTL_SYNC))
        {
          /* no buffer available to build the answer ... should not occur */
        }
        else
        {
          MCP_ReceivedPacket(&MCP_Over_UartA);
          MCP_Over_UartA.pTransportLayer->fSendPacket(MCP_Over_UartA.pTransportLayer, MCP_Over_UartA.txBuffer,
                                                      MCP_Over_UartA.txLength, MCTL_SYNC);
          /* no buffer available to build the answer ... should not occur */
        }
      }

      /* USER CODE BEGIN MC_Scheduler 1 */

      /* USER CODE END MC_Scheduler 1 */
      hMFTaskCounterM1 = (uint16_t)MF_TASK_OCCURENCE_TICKS;
    }
    if(hBootCapDelayCounterM1 > 0U)
    {
      hBootCapDelayCounterM1--;
    }
    if(hStopPermanencyCounterM1 > 0U)
    {
      hStopPermanencyCounterM1--;
    }
  }
  else
  {
    /* Nothing to do */
  }
  /* USER CODE BEGIN MC_Scheduler 2 */

  /* USER CODE END MC_Scheduler 2 */
}

/**
  * @brief Executes medium frequency periodic Motor Control tasks
  *
  * This function performs some of the control duties on Motor 1 according to the
  * present state of its state machine. In particular, duties requiring a periodic
  * execution at a medium frequency rate (such as the speed controller for instance)
  * are executed here.
  */
__weak void TSK_MediumFrequencyTaskM1(void)
{
  /* USER CODE BEGIN MediumFrequencyTask M1 0 */

  /* USER CODE END MediumFrequencyTask M1 0 */

  int16_t wAux = 0;

  bool IsSpeedReliable = STO_PLL_CalcAvrgMecSpeedUnit(&STO_PLL_M1, &wAux);
  PQD_CalcElMotorPower(pMPM[M1]);

  if (MCI_GetCurrentFaults(&Mci[M1]) == MC_NO_FAULTS)
  {
    if (MCI_GetOccurredFaults(&Mci[M1]) == MC_NO_FAULTS)
    {
      switch (Mci[M1].State)
      {
        case IDLE:
        {
          if ((MCI_START == Mci[M1].DirectCommand) || (MCI_MEASURE_OFFSETS == Mci[M1].DirectCommand))
          {
            RUC_Clear(&RevUpControlM1, MCI_GetImposedMotorDirection(&Mci[M1]));

           if (pwmcHandle[M1]->offsetCalibStatus == false)
           {
             PWMC_CurrentReadingCalibr(pwmcHandle[M1], CRC_START);
             Mci[M1].State = OFFSET_CALIB;
           }
           else
           {
             /* calibration already done. Enables only TIM channels */
             pwmcHandle[M1]->OffCalibrWaitTimeCounter = 1u;
             PWMC_CurrentReadingCalibr(pwmcHandle[M1], CRC_EXEC);
             R3_2_TurnOnLowSides(pwmcHandle[M1]);
             TSK_SetChargeBootCapDelayM1(CHARGE_BOOT_CAP_TICKS);
             Mci[M1].State = CHARGE_BOOT_CAP;

           }

          }
          else
          {
            /* nothing to be done, FW stays in IDLE state */
          }
          break;
        }

        case OFFSET_CALIB:
          {
            if (MCI_STOP == Mci[M1].DirectCommand)
            {
              TSK_MF_StopProcessing(&Mci[M1], M1);
            }
            else
            {
              if (PWMC_CurrentReadingCalibr(pwmcHandle[M1], CRC_EXEC))
              {
                if (MCI_MEASURE_OFFSETS == Mci[M1].DirectCommand)
                {
                  FOC_Clear(M1);
                  MPM_Clear((MotorPowMeas_Handle_t*) pMPM[M1]);
                  Mci[M1].DirectCommand = MCI_NO_COMMAND;
                  Mci[M1].State = IDLE;
                }
                else
                {
                  R3_2_TurnOnLowSides(pwmcHandle[M1]);
                  TSK_SetChargeBootCapDelayM1(CHARGE_BOOT_CAP_TICKS);
                  Mci[M1].State = CHARGE_BOOT_CAP;

                }
              }
              else
              {
                /* nothing to be done, FW waits for offset calibration to finish */
              }
            }
            break;
          }

        case CHARGE_BOOT_CAP:
        {
          if (MCI_STOP == Mci[M1].DirectCommand)
          {
            TSK_MF_StopProcessing(&Mci[M1], M1);
          }
          else
          {
            if (TSK_ChargeBootCapDelayHasElapsedM1())
            {
              R3_2_SwitchOffPWM(pwmcHandle[M1]);
             FOCVars[M1].bDriveInput = EXTERNAL;
             STC_SetSpeedSensor( pSTC[M1], &VirtualSpeedSensorM1._Super );
              STO_PLL_Clear(&STO_PLL_M1);
              FOC_Clear( M1 );

              Mci[M1].State = START;

              PWMC_SwitchOnPWM(pwmcHandle[M1]);
            }
            else
            {
              /* nothing to be done, FW waits for bootstrap capacitor to charge */
            }
          }
          break;
        }

        case START:
        {
          if (MCI_STOP == Mci[M1].DirectCommand)
          {
            TSK_MF_StopProcessing(&Mci[M1], M1);
          }
          else
          {
            /* Mechanical speed as imposed by the Virtual Speed Sensor during the Rev Up phase. */
            int16_t hForcedMecSpeedUnit;
            qd_t IqdRef;
            bool ObserverConverged = false;

            /* Execute the Rev Up procedure */
            if(! RUC_Exec(&RevUpControlM1))

            {
            /* The time allowed for the startup sequence has expired */
              MCI_FaultProcessing(&Mci[M1], MC_START_UP, 0);

           }
           else
           {
             /* Execute the torque open loop current start-up ramp:
              * Compute the Iq reference current as configured in the Rev Up sequence */
             IqdRef.q = STC_CalcTorqueReference( pSTC[M1] );
             IqdRef.d = FOCVars[M1].UserIdref;
             /* Iqd reference current used by the High Frequency Loop to generate the PWM output */
             FOCVars[M1].Iqdref = IqdRef;
           }

           (void) VSS_CalcAvrgMecSpeedUnit(&VirtualSpeedSensorM1, &hForcedMecSpeedUnit);

           /* check that startup stage where the observer has to be used has been reached */
           if (true == RUC_FirstAccelerationStageReached(&RevUpControlM1))

            {
             ObserverConverged = STO_PLL_IsObserverConverged(&STO_PLL_M1, &hForcedMecSpeedUnit);
             STO_SetDirection(&STO_PLL_M1, (int8_t)MCI_GetImposedMotorDirection(&Mci[M1]));

              (void)VSS_SetStartTransition(&VirtualSpeedSensorM1, ObserverConverged);
            }

            if (ObserverConverged)
            {
              qd_t StatorCurrent = MCM_Park(FOCVars[M1].Ialphabeta, SPD_GetElAngle(&STO_PLL_M1._Super));

              /* Start switch over ramp. This ramp will transition from the revup to the closed loop FOC. */
              REMNG_Init(pREMNG[M1]);
              (void)REMNG_ExecRamp(pREMNG[M1], FOCVars[M1].Iqdref.q, 0);
              (void)REMNG_ExecRamp(pREMNG[M1], StatorCurrent.q, TRANSITION_DURATION);

              Mci[M1].State = SWITCH_OVER;
            }
          }
          break;
        }

        case SWITCH_OVER:
        {
          if (MCI_STOP == Mci[M1].DirectCommand)
          {
            TSK_MF_StopProcessing(&Mci[M1], M1);
          }
          else
          {
            bool LoopClosed;
            int16_t hForcedMecSpeedUnit;

            if(! RUC_Exec(&RevUpControlM1))

            {
              /* The time allowed for the startup sequence has expired */
              MCI_FaultProcessing(&Mci[M1], MC_START_UP, 0);

            }
            else

            {
              /* Compute the virtual speed and positions of the rotor.
                 The function returns true if the virtual speed is in the reliability range */
              LoopClosed = VSS_CalcAvrgMecSpeedUnit(&VirtualSpeedSensorM1, &hForcedMecSpeedUnit);
              /* Check if the transition ramp has completed. */
              bool tempBool;
              tempBool = VSS_TransitionEnded(&VirtualSpeedSensorM1);
              LoopClosed = LoopClosed || tempBool;

              /* If any of the above conditions is true, the loop is considered closed.
                 The state machine transitions to the START_RUN state. */
              if (true ==  LoopClosed)
              {
                #if ( PID_SPEED_INTEGRAL_INIT_DIV == 0 )
                PID_SetIntegralTerm(&PIDSpeedHandle_M1, 0);
                #else
                PID_SetIntegralTerm(&PIDSpeedHandle_M1,
                                    (((int32_t)FOCVars[M1].Iqdref.q * (int16_t)PID_GetKIDivisor(&PIDSpeedHandle_M1))
                                    / PID_SPEED_INTEGRAL_INIT_DIV));
                #endif

                /* USER CODE BEGIN MediumFrequencyTask M1 1 */

                /* USER CODE END MediumFrequencyTask M1 1 */
                STC_SetSpeedSensor(pSTC[M1], &STO_PLL_M1._Super); /*Observer has converged*/
                FOC_InitAdditionalMethods(M1);
                FOC_CalcCurrRef( M1 );
                STC_ForceSpeedReferenceToCurrentSpeed(pSTC[M1]); /* Init the reference speed to current speed */
                MCI_ExecBufferedCommands(&Mci[M1]); /* Exec the speed ramp after changing of the speed sensor */
                Mci[M1].State = RUN;
              }
            }
          }
          break;
        }

        case RUN:
        {
          if (MCI_STOP == Mci[M1].DirectCommand)
          {
            TSK_MF_StopProcessing(&Mci[M1], M1);
          }
          else
          {
            /* USER CODE BEGIN MediumFrequencyTask M1 2 */

            /* USER CODE END MediumFrequencyTask M1 2 */

            MCI_ExecBufferedCommands(&Mci[M1]);
            FOC_CalcCurrRef(M1);

            if(!IsSpeedReliable)
            {
              MCI_FaultProcessing(&Mci[M1], MC_SPEED_FDBK, 0);
            }

          }
          break;
        }

        case STOP:
        {
          if (TSK_StopPermanencyTimeHasElapsedM1())
          {

            STC_SetSpeedSensor(pSTC[M1], &VirtualSpeedSensorM1._Super);  	/*  sensor-less */
            VSS_Clear(&VirtualSpeedSensorM1); /* Reset measured speed in IDLE */

            /* USER CODE BEGIN MediumFrequencyTask M1 5 */

            /* USER CODE END MediumFrequencyTask M1 5 */
            Mci[M1].DirectCommand = MCI_NO_COMMAND;
            Mci[M1].State = IDLE;

          }
          else
          {
            /* nothing to do, FW waits for to stop */
          }
          break;
        }

        case FAULT_OVER:
        {
          if (MCI_ACK_FAULTS == Mci[M1].DirectCommand)
          {
            Mci[M1].DirectCommand = MCI_NO_COMMAND;
            Mci[M1].State = IDLE;

          }
          else
          {
            /* nothing to do, FW stays in FAULT_OVER state until acknowledgement */
          }
        }
        break;

        case FAULT_NOW:
        {
          Mci[M1].State = FAULT_OVER;
        }

        default:
          break;
       }
    }
    else
    {
      Mci[M1].State = FAULT_OVER;
    }
  }
  else
  {
    Mci[M1].State = FAULT_NOW;
  }

  /* USER CODE BEGIN MediumFrequencyTask M1 6 */

  /* USER CODE END MediumFrequencyTask M1 6 */
}

/**
  * @brief  It re-initializes the current and voltage variables. Moreover
  *         it clears qd currents PI controllers, voltage sensor and SpeednTorque
  *         controller. It must be called before each motor restart.
  *         It does not clear speed sensor.
  * @param  bMotor related motor it can be M1 or M2
  * @retval none
  */
__weak void FOC_Clear(uint8_t bMotor)
{
  /* USER CODE BEGIN FOC_Clear 0 */

  /* USER CODE END FOC_Clear 0 */
  ab_t NULL_ab = {((int16_t)0), ((int16_t)0)};
  qd_t NULL_qd = {((int16_t)0), ((int16_t)0)};
  alphabeta_t NULL_alphabeta = {((int16_t)0), ((int16_t)0)};

  FOCVars[bMotor].Iab = NULL_ab;
  FOCVars[bMotor].Ialphabeta = NULL_alphabeta;
  FOCVars[bMotor].Iqd = NULL_qd;
  FOCVars[bMotor].Iqdref = NULL_qd;
  FOCVars[bMotor].hTeref = (int16_t)0;
  FOCVars[bMotor].Vqd = NULL_qd;
  FOCVars[bMotor].Valphabeta = NULL_alphabeta;
  FOCVars[bMotor].hElAngle = (int16_t)0;

  PID_SetIntegralTerm(pPIDIq[bMotor], ((int32_t)0));
  PID_SetIntegralTerm(pPIDId[bMotor], ((int32_t)0));

  STC_Clear(pSTC[bMotor]);

  PWMC_SwitchOffPWM(pwmcHandle[bMotor]);

  /* USER CODE BEGIN FOC_Clear 1 */

  /* USER CODE END FOC_Clear 1 */
}

/**
  * @brief  Use this method to initialize additional methods (if any) in
  *         START_TO_RUN state
  * @param  bMotor related motor it can be M1 or M2
  * @retval none
  */
__weak void FOC_InitAdditionalMethods(uint8_t bMotor) //cstat !RED-func-no-effect
{
    if (M_NONE == bMotor)
    {
      /* Nothing to do */
    }
    else
    {
  /* USER CODE BEGIN FOC_InitAdditionalMethods 0 */

  /* USER CODE END FOC_InitAdditionalMethods 0 */
    }
}

/**
  * @brief  It computes the new values of Iqdref (current references on qd
  *         reference frame) based on the required electrical torque information
  *         provided by oTSC object (internally clocked).
  *         If implemented in the derived class it executes flux weakening and/or
  *         MTPA algorithm(s). It must be called with the periodicity specified
  *         in oTSC parameters
  * @param  bMotor related motor it can be M1 or M2
  * @retval none
  */
__weak void FOC_CalcCurrRef(uint8_t bMotor)
{

  /* USER CODE BEGIN FOC_CalcCurrRef 0 */

  /* USER CODE END FOC_CalcCurrRef 0 */
  if (INTERNAL == FOCVars[bMotor].bDriveInput)
  {
    FOCVars[bMotor].hTeref = STC_CalcTorqueReference(pSTC[bMotor]);
    FOCVars[bMotor].Iqdref.q = FOCVars[bMotor].hTeref;

  }
  else
  {
    /* Nothing to do */
  }
  /* USER CODE BEGIN FOC_CalcCurrRef 1 */

  /* USER CODE END FOC_CalcCurrRef 1 */
}

/**
  * @brief  It set a counter intended to be used for counting the delay required
  *         for drivers boot capacitors charging of motor 1
  * @param  hTickCount number of ticks to be counted
  * @retval void
  */
__weak void TSK_SetChargeBootCapDelayM1(uint16_t hTickCount)
{
   hBootCapDelayCounterM1 = hTickCount;
}

/**
  * @brief  Use this function to know whether the time required to charge boot
  *         capacitors of motor 1 has elapsed
  * @param  none
  * @retval bool true if time has elapsed, false otherwise
  */
__weak bool TSK_ChargeBootCapDelayHasElapsedM1(void)
{
  bool retVal = false;
  if (((uint16_t)0) == hBootCapDelayCounterM1)
  {
    retVal = true;
  }
  return (retVal);
}

/**
  * @brief  It set a counter intended to be used for counting the permanency
  *         time in STOP state of motor 1
  * @param  hTickCount number of ticks to be counted
  * @retval void
  */
__weak void TSK_SetStopPermanencyTimeM1(uint16_t hTickCount)
{
  hStopPermanencyCounterM1 = hTickCount;
}

/**
  * @brief  Use this function to know whether the permanency time in STOP state
  *         of motor 1 has elapsed
  * @param  none
  * @retval bool true if time is elapsed, false otherwise
  */
__weak bool TSK_StopPermanencyTimeHasElapsedM1(void)
{
  bool retVal = false;
  if (((uint16_t)0) == hStopPermanencyCounterM1)
  {
    retVal = true;
  }
  return (retVal);
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM) || defined(__GNUC__)
__attribute__((section (".ccmram")))
#endif
#endif

/**
  * @brief  Executes the Motor Control duties that require a high frequency rate and a precise timing
  *
  *  This is mainly the FOC current control loop. It is executed depending on the state of the Motor Control
  * subsystem (see the state machine(s)).
  *
  * @retval Number of the  motor instance which FOC loop was executed.
  */
__weak uint8_t TSK_HighFrequencyTask(void)
{
  /* USER CODE BEGIN HighFrequencyTask 0 */

  /* USER CODE END HighFrequencyTask 0 */

  uint16_t hFOCreturn;
  uint8_t bMotorNbr = 0;

  Observer_Inputs_t STO_Inputs; /*  only if sensorless main*/

  STO_Inputs.Valfa_beta = FOCVars[M1].Valphabeta;  /* only if sensorless*/
  if (SWITCH_OVER == Mci[M1].State)
  {
    if (!REMNG_RampCompleted(pREMNG[M1]))
    {
      FOCVars[M1].Iqdref.q = (int16_t)REMNG_Calc(pREMNG[M1]);
    }
  }
  /* USER CODE BEGIN HighFrequencyTask SINGLEDRIVE_1 */

  /* USER CODE END HighFrequencyTask SINGLEDRIVE_1 */
  hFOCreturn = FOC_CurrControllerM1();
  /* USER CODE BEGIN HighFrequencyTask SINGLEDRIVE_2 */

  /* USER CODE END HighFrequencyTask SINGLEDRIVE_2 */
  if(hFOCreturn == MC_FOC_DURATION)
  {
    MCI_FaultProcessing(&Mci[M1], MC_FOC_DURATION, 0);
  }
  else
  {
    bool IsAccelerationStageReached = RUC_FirstAccelerationStageReached(&RevUpControlM1);
    STO_Inputs.Ialfa_beta = FOCVars[M1].Ialphabeta; /*  only if sensorless*/
    STO_Inputs.Vbus = VBS_GetAvBusVoltage_d(&(BusVoltageSensor_M1._Super)); /*  only for sensorless*/
    (void)( void )STO_PLL_CalcElAngle(&STO_PLL_M1, &STO_Inputs);
    STO_PLL_CalcAvrgElSpeedDpp(&STO_PLL_M1); /*  Only in case of Sensor-less */
	 if (false == IsAccelerationStageReached)
    {
      STO_ResetPLL(&STO_PLL_M1);
    }
    /*  only for sensor-less*/
    if(((uint16_t)START == Mci[M1].State) || ((uint16_t)SWITCH_OVER == Mci[M1].State))
    {
      int16_t hObsAngle = SPD_GetElAngle(&STO_PLL_M1._Super);
      (void)VSS_CalcElAngle(&VirtualSpeedSensorM1, &hObsAngle);
    }
    /* USER CODE BEGIN HighFrequencyTask SINGLEDRIVE_3 */

    /* USER CODE END HighFrequencyTask SINGLEDRIVE_3 */
  }
  DAC_Exec(&DAC_Handle);
  /* USER CODE BEGIN HighFrequencyTask 1 */

  /* USER CODE END HighFrequencyTask 1 */

  GLOBAL_TIMESTAMP++;
  if (0U == MCPA_UART_A.Mark)
  {
    /* Nothing to do */
  }
  else
  {
    MCPA_dataLog (&MCPA_UART_A);
  }

  return (bMotorNbr);
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM) || defined(__GNUC__)
__attribute__((section (".ccmram")))
#endif
#endif
/**
  * @brief It executes the core of FOC drive that is the controllers for Iqd
  *        currents regulation. Reference frame transformations are carried out
  *        accordingly to the active speed sensor. It must be called periodically
  *        when new motor currents have been converted
  * @param this related object of class CFOC.
  * @retval int16_t It returns MC_NO_FAULTS if the FOC has been ended before
  *         next PWM Update event, MC_FOC_DURATION otherwise
  */
//int32_t errGAMain;
//int16_t errGA,errGAi,errGAp;
//int16_t KpGA = -1;
//int16_t KiGA = -1;
//uint16_t startGATuning = 0, iGA;
//int16_t setGA = 1500;
//volatile int16_t diffGA;
inline uint16_t FOC_CurrControllerM1(void)
{
	volatile qd_t Iqd, Vqd,VqdTemp;
	volatile int costTemp1,costTemp2,cost;
	ab_t Iab;
	alphabeta_t Ialphabeta, Valphabeta;
	int16_t angleMPC,Vd,Vq;

	int16_t hElAngle;
	uint16_t hCodeError;
	SpeednPosFdbk_Handle_t *speedHandle;

	speedHandle = STC_GetSpeedSensor(pSTC[M1]);
	hElAngle = SPD_GetElAngle(speedHandle);
	hElAngle += SPD_GetInstElSpeedDpp(speedHandle)*PARK_ANGLE_COMPENSATION_FACTOR;
	PWMC_GetPhaseCurrents(pwmcHandle[M1], &Iab);
	Ialphabeta = MCM_Clarke(Iab);
	Iqd = MCM_Park(Ialphabeta, hElAngle);



	/* Omkar code start */

	int speedRPM = SPEED_UNIT_2_RPM(SPD_GetAvrgMecSpeedUnit(speedHandle));
	int16_t wr = SPEED_UNIT_2_RPM(MC_GetMecSpeedAverageMotor1())/9.55;

	if(hElAngle < 0){
		angleMPC = 360+(hElAngle*180/32767);
	} else {
		angleMPC = (hElAngle*180/32767);
	}

	if(!hasMPCinit){
		hasMPCinit = 1;
		initModelPredictiveControl();
	}


	if(speedRPM > 1900 || runMPC){
		runMPC = 1;

		IqTemp = Iqd.q;
		IdTemp = Iqd.d;

		IqRef = FOCVars[M1].Iqdref.q*686/100;

		cost = 2147483628;


		for(i=0; i<6; i++){

//			int m = switchingEffort[oldOptimalVector][i];


//			if(i<6){
//				VqdTemp.d = sinMPC(angleMPC + i*60);
//				VqdTemp.q = sinMPC(angleMPC + 270 + i*60);
//			} else {
//				VqdTemp.d = 0;
//				VqdTemp.q = 0;
//			}

			if(i<6){
				Valphabeta.alpha = Varray[i][0];
				Valphabeta.beta  = Varray[i][1];

				VqdTemp = MCM_Park(Valphabeta, hElAngle);
			} else {
				VqdTemp.d = 0;
				VqdTemp.q = 0;
			}


			IdPred = ((c1_mpc*IdTemp/1456) + (4*wr*IqTemp/(1456*2))  + (c2_mpc*(VqdTemp.d)*Vphase/32767));
			IqPred = ((c1_mpc*IqTemp/1456) - (4*wr*IdTemp/(1456*2)) - (19*wr)  + (c2_mpc*(VqdTemp.q)*Vphase/32767));

			costTemp1 = (IqRef - IqPred);
			costTemp2 = (-IdPred);

			if(costTemp1<0){
				costTemp1 = -costTemp1;
			}

			if(costTemp2<0){
				costTemp2 = -costTemp2;
			}

			costTemp1 = costTemp1*costTemp1;
			costTemp2 = costTemp2*costTemp2;

			if((costTemp1+costTemp2) < cost){
				optimalVector = i;
				cost = costTemp1+costTemp2;
				Vqd.d = VqdTemp.d;
				Vqd.q = VqdTemp.q;
			}

		}
	} else {
		  Vqd.q = PI_Controller(pPIDIq[M1], (int32_t)(FOCVars[M1].Iqdref.q) - Iqd.q);
		  Vqd.d = PI_Controller(pPIDId[M1], (int32_t)(FOCVars[M1].Iqdref.d) - Iqd.d);
	}
	/* Omkar code end */

	Sa = states[optimalVector] & 0x01;
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, Sa);

	Vqd = Circle_Limitation(pCLM[M1], Vqd);
	hElAngle += SPD_GetInstElSpeedDpp(speedHandle)*REV_PARK_ANGLE_COMPENSATION_FACTOR;

	Valphabeta = MCM_Rev_Park(Vqd, hElAngle);
	hCodeError = PWMC_SetPhaseVoltage(pwmcHandle[M1], Valphabeta);

	FOCVars[M1].Vqd = Vqd;
	FOCVars[M1].Iab = Iab;
	FOCVars[M1].Ialphabeta = Ialphabeta;
	FOCVars[M1].Iqd = Iqd;
	FOCVars[M1].Valphabeta = Valphabeta;
	FOCVars[M1].hElAngle = hElAngle;

	return(hCodeError);
}

/**
  * @brief  Executes safety checks (e.g. bus voltage and temperature) for all drive instances.
  *
  * Faults flags are updated here.
  */
__weak void TSK_SafetyTask(void)
{
  /* USER CODE BEGIN TSK_SafetyTask 0 */

  /* USER CODE END TSK_SafetyTask 0 */
  if (1U == bMCBootCompleted)
  {
    TSK_SafetyTask_PWMOFF(M1);
    /* User conversion execution */
    RCM_ExecUserConv();
  /* USER CODE BEGIN TSK_SafetyTask 1 */

  /* USER CODE END TSK_SafetyTask 1 */
  }
}

/**
  * @brief  Safety task implementation if  MC.ON_OVER_VOLTAGE == TURN_OFF_PWM
  * @param  bMotor Motor reference number defined
  *         \link Motors_reference_number here \endlink
  * @retval None
  */
__weak void TSK_SafetyTask_PWMOFF(uint8_t bMotor)
{
  /* USER CODE BEGIN TSK_SafetyTask_PWMOFF 0 */

  /* USER CODE END TSK_SafetyTask_PWMOFF 0 */
  uint16_t CodeReturn = MC_NO_ERROR;
  uint16_t errMask[NBR_OF_MOTORS] = {VBUS_TEMP_ERR_MASK};

  CodeReturn |= errMask[bMotor] & NTC_CalcAvTemp(pTemperatureSensor[bMotor]); /* check for fault if FW protection is activated. It returns MC_OVER_TEMP or MC_NO_ERROR */
  CodeReturn |= PWMC_CheckOverCurrent(pwmcHandle[bMotor]);                    /* check for fault. It return MC_BREAK_IN or MC_NO_FAULTS
                                                                                 (for STM32F30x can return MC_OVER_VOLT in case of HW Overvoltage) */
  if(M1 == bMotor)
  {
    CodeReturn |= errMask[bMotor] & RVBS_CalcAvVbus(&BusVoltageSensor_M1);
  }
  MCI_FaultProcessing(&Mci[bMotor], CodeReturn, ~CodeReturn); /* process faults */
  if (MCI_GetFaultState(&Mci[bMotor]) != (uint32_t)MC_NO_FAULTS)
  {
    PWMC_SwitchOffPWM(pwmcHandle[bMotor]);
    if (MCPA_UART_A.Mark != 0)
    {
      MCPA_flushDataLog (&MCPA_UART_A);
    }
    FOC_Clear(bMotor);
    MPM_Clear((MotorPowMeas_Handle_t*)pMPM[bMotor]); //cstat !MISRAC2012-Rule-11.3
    /* USER CODE BEGIN TSK_SafetyTask_PWMOFF 1 */

    /* USER CODE END TSK_SafetyTask_PWMOFF 1 */
  }
  else
  {
    /* no errors */
  }

  /* USER CODE BEGIN TSK_SafetyTask_PWMOFF 3 */

  /* USER CODE END TSK_SafetyTask_PWMOFF 3 */
}

/**
  * @brief  This function returns the reference of the MCInterface relative to
  *         the selected drive.
  * @param  bMotor Motor reference number defined
  *         \link Motors_reference_number here \endlink
  * @retval MCI_Handle_t * Reference to MCInterface relative to the selected drive.
  *         Note: it can be MC_NULL if MCInterface of selected drive is not
  *         allocated.
  */
__weak MCI_Handle_t * GetMCI(uint8_t bMotor)
{
  MCI_Handle_t * retVal = MC_NULL;
  if (bMotor < (uint8_t)NBR_OF_MOTORS)
  {
    retVal = &Mci[bMotor];
  }
  return (retVal);
}

/**
  * @brief  Puts the Motor Control subsystem in in safety conditions on a Hard Fault
  *
  *  This function is to be executed when a general hardware failure has been detected
  * by the microcontroller and is used to put the system in safety condition.
  */
__weak void TSK_HardwareFaultTask(void)
{
  /* USER CODE BEGIN TSK_HardwareFaultTask 0 */

  /* USER CODE END TSK_HardwareFaultTask 0 */
  R3_2_SwitchOffPWM(pwmcHandle[M1]);
  MCI_FaultProcessing(&Mci[M1], MC_SW_ERROR, 0);
  /* USER CODE BEGIN TSK_HardwareFaultTask 1 */

  /* USER CODE END TSK_HardwareFaultTask 1 */
}

__weak void UI_HandleStartStopButton_cb (void)
{
/* USER CODE BEGIN START_STOP_BTN */
  if (IDLE == MC_GetSTMStateMotor1())
  {
    /* Ramp parameters should be tuned for the actual motor */
    (void)MC_StartMotor1();
  }
  else
  {
    (void)MC_StopMotor1();
  }
/* USER CODE END START_STOP_BTN */
}

 /**
  * @brief  Locks GPIO pins used for Motor Control to prevent accidental reconfiguration
  */
__weak void mc_lock_pins (void)
{
LL_GPIO_LockPin(M1_TEMPERATURE_GPIO_Port, M1_TEMPERATURE_Pin);
LL_GPIO_LockPin(M1_BUS_VOLTAGE_GPIO_Port, M1_BUS_VOLTAGE_Pin);
LL_GPIO_LockPin(M1_CURR_SHUNT_V_GPIO_Port, M1_CURR_SHUNT_V_Pin);
LL_GPIO_LockPin(M1_CURR_SHUNT_U_GPIO_Port, M1_CURR_SHUNT_U_Pin);
LL_GPIO_LockPin(M1_OPAMP1_EXT_GAIN_GPIO_Port, M1_OPAMP1_EXT_GAIN_Pin);
LL_GPIO_LockPin(M1_OPAMP1_OUT_GPIO_Port, M1_OPAMP1_OUT_Pin);
LL_GPIO_LockPin(M1_OPAMP2_OUT_GPIO_Port, M1_OPAMP2_OUT_Pin);
LL_GPIO_LockPin(M1_OPAMP2_EXT_GAIN_GPIO_Port, M1_OPAMP2_EXT_GAIN_Pin);
LL_GPIO_LockPin(M1_CURR_SHUNT_W_GPIO_Port, M1_CURR_SHUNT_W_Pin);
LL_GPIO_LockPin(M1_PWM_UH_GPIO_Port, M1_PWM_UH_Pin);
LL_GPIO_LockPin(M1_PWM_VH_GPIO_Port, M1_PWM_VH_Pin);
LL_GPIO_LockPin(M1_PWM_VL_GPIO_Port, M1_PWM_VL_Pin);
LL_GPIO_LockPin(M1_PWM_WH_GPIO_Port, M1_PWM_WH_Pin);
LL_GPIO_LockPin(M1_PWM_WL_GPIO_Port, M1_PWM_WL_Pin);
LL_GPIO_LockPin(M1_PWM_UL_GPIO_Port, M1_PWM_UL_Pin);
}
/* USER CODE BEGIN mc_task 0 */

/* USER CODE END mc_task 0 */

/******************* (C) COPYRIGHT 2019 STMicroelectronics *****END OF FILE****/
