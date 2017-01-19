int initMeCom(int MECOM_ADDRESS, int MECOM_INST, int USE_BUILT_IN_PID);
int setTECVandC(int MECOM_ADDRESS, int MECOM_INST, float Voltage, float Current);
int getTECVandC(int MECOM_ADDRESS, int MECOM_INST, float *Voltage, float *Current);
float getTECTemp(int MECOM_ADDRESS, int MECOM_INST);