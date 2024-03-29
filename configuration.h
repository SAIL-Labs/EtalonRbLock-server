#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
//#include "redpitaya/rp.h"

/* configuration constants */
//#define SERVER_IP_ADDR "10.66.101.133"//"10.66.101.133"
//#define SERVER_IP_ADDR "10.66.100.101"
#define CLIENT_IP_PORT_A 12345
#define CLIENT_IP_PORT_B 12346
#define CLIENT_IP_PORT_ACK 12347
//#define ACQUISITION_LENGTH 150000    /* samples */
#define PRE_TRIGGER_LENGTH 0        /* samples */
#define DECIMATION DE_64            /* one of enum decimation */
#define TRIGGER_MODE TR_EXT_FALLING /* one of enum trigger */
#define TRIGGER_THRESHOLD 350       // 2048   750         /* ADC counts, 2048 ≃ +0.25V */
#define DELAYFORLOOP 5              // 66000

/* internal constants */
#define READ_BLOCK_SIZE 20000
#define SEND_BLOCK_SIZE 20000
#define RAM_A_ADDRESS 0x1e000000UL
#define RAM_A_SIZE 0x01000000UL
#define RAM_B_ADDRESS 0x1f000000UL
#define RAM_B_SIZE 0x01000000UL

#define ENABLE_MECOM 1
#define ENABLE_BME280 1

#define Kp 0.01
#define Ki 0
#define Kd 0

#define SW1TRIGPIN RP_DIO0_N
#define SW1STATUSPIN RP_DIO1_N

#define SW2TRIGPIN RP_DIO2_N
#define SW2STATUSPIN RP_DIO3_N