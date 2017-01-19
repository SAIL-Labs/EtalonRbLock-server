#include "MeComAPI/ComPort/ComPort.h"
#include "MeComAPI/MeCom.h"

#include "temp_moniter.h"
#include "MeComAPI/ComPort/ComPort.h"
#include "MeComAPI/MeCom.h"
#include "configuration.h"

int initMeCom(int MECOM_ADDRESS, int MECOM_INST, int USE_BUILT_IN_PID)
{
  MeParLongFields lFields;

  /*MeCom port open*/
  ComPort_Open(0, 57600);

  // if (MeCom_ResetDevice(MECOM_ADDRESS))
  // {
  //   sleep(1);
  //   fprintf(stderr, "Device Reset OK.\n");
  // }

  if (MeCom_TEC_Ope_OutputStageInputSelection(MECOM_ADDRESS, MECOM_INST, &lFields, MeGetLimits))
  {
    if (USE_BUILT_IN_PID)
    {
      fprintf(stderr, "Using Built-in Temperature Controller\n\n");
      lFields.Value = 2; // Temperature Controller
      MeCom_TEC_Ope_OutputStageInputSelection(MECOM_ADDRESS, MECOM_INST, &lFields,
                                              MeSet);
    }
    else
    {
      fprintf(stderr, "Using Live Current/Voltage\n\n");
      lFields.Value = 1; // Live Current/Voltage
      MeCom_TEC_Ope_OutputStageInputSelection(MECOM_ADDRESS, MECOM_INST, &lFields,
                                              MeSet);
      //setTECVandC(MECOM_ADDRESS, MECOM_INST, 2, 0);
    }
    return 0;
  }
  return 1;
}

int setTECVandC(int MECOM_ADDRESS, int MECOM_INST, float Voltage, float Current)
{
  MeParFloatFields fFields;
  int err;
  fFields.Value = Current;
  err =
      MeCom_TEC_Oth_LiveSetCurrent(MECOM_ADDRESS, MECOM_INST, &fFields, MeSet);
  if (err == 0)
  {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }

  fFields.Value = Voltage;
  err =
      MeCom_TEC_Oth_LiveSetVoltage(MECOM_ADDRESS, MECOM_INST, &fFields, MeSet);
  if (err == 0)
  {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }
  return 0;
}

int getTECVandC(int MECOM_ADDRESS, int MECOM_INST, float *Voltage, float *Current)
{
  MeParFloatFields fFields;
  int err;

  err = MeCom_TEC_Mon_ActualOutputVoltage(MECOM_ADDRESS, MECOM_INST, &fFields,
                                          MeGet);
  if (err == 0)
  {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }
  *Voltage = fFields.Value;

  err = MeCom_TEC_Mon_ActualOutputCurrent(MECOM_ADDRESS, MECOM_INST, &fFields,
                                          MeGet);
  if (err == 0)
  {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }
  *Current = fFields.Value;
  return 0;
}

float getTECTemp(int MECOM_ADDRESS, int MECOM_INST)
{
  MeParFloatFields fFields;
  MeCom_TEC_Mon_ObjectTemperature(MECOM_ADDRESS, MECOM_INST, &fFields, MeGet);
  return fFields.Value;
}