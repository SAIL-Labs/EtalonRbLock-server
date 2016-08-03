#include "MeComAPI/ComPort/ComPort.h"
#include "MeComAPI/MeCom.h"

#include "temp_moniter.h"
#include "MeComAPI/ComPort/ComPort.h"
#include "MeComAPI/MeCom.h"
#include "configuration.h"

int initMeCom(void) {
  MeParLongFields lFields;

  /*MeCom port open*/
  ComPort_Open(0, 57600);

  if (USE_BUILT_IN_PID) {
    lFields.Value = 2; // Temperature Controller
    MeCom_TEC_Ope_OutputStageInputSelection(MECOM_ADDRESS, MECOM_INST, &lFields,
                                            MeSet);
  } else {
    lFields.Value = 1; // Live Current/Voltage
    MeCom_TEC_Ope_OutputStageInputSelection(MECOM_ADDRESS, MECOM_INST, &lFields,
                                            MeSet);
    setTECVandC(0, 0);
  }
  return 0;
}

int setTECVandC(float Voltage, float Current) {
  MeParFloatFields fFields;
  int err;
  fFields.Value = 0;
  err =
      MeCom_TEC_Oth_LiveSetCurrent(MECOM_ADDRESS, MECOM_INST, &fFields, MeSet);
  if (err==0) {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }

  fFields.Value = 0;
  err =
      MeCom_TEC_Oth_LiveSetVoltage(MECOM_ADDRESS, MECOM_INST, &fFields, MeSet);
  if (err==0) {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }
  return 0;
}

int getTECVandC(float *Voltage, float *Current) {
  MeParFloatFields fFields;
  int err;

  err = MeCom_TEC_Mon_ActualOutputVoltage(MECOM_ADDRESS, MECOM_INST, &fFields,
                                          MeGet);
  if (err==0) {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }
  *Voltage = fFields.Value;

  err = MeCom_TEC_Mon_ActualOutputCurrent(MECOM_ADDRESS, MECOM_INST, &fFields,
                                          MeGet);
  if (err==0) {
    fprintf(stderr, "LiveSetCurrent failed: Error %d", err);
    return err;
  }
  *Current = fFields.Value;
  return 0;
}

float getTECTemp(void) {
  MeParFloatFields fFields;
  MeCom_TEC_Mon_ObjectTemperature(MECOM_ADDRESS, MECOM_INST, &fFields, MeGet);
  return fFields.Value;
}