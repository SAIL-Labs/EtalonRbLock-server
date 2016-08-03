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

/* configuration constants */
#define SERVER_IP_ADDR "10.66.101.133"
//#define SERVER_IP_ADDR          "10.66.100.99"
#define SERVER_IP_PORT_A 12345
#define SERVER_IP_PORT_B 12346
#define SERVER_IP_PORT_ACK 12347
#define ACQUISITION_LENGTH 250000    /* samples */
#define PRE_TRIGGER_LENGTH 2000      /* samples */
#define DECIMATION DE_8              /* one of enum decimation */
#define TRIGGER_MODE TR_CH_B_FALLING /* one of enum trigger */
#define TRIGGER_THRESHOLD 750 // 2048            /* ADC counts, 2048 ≃ +0.25V */
#define DELAYFORLOOP 5        // 66000

/* internal constants */
#define READ_BLOCK_SIZE 16384
#define SEND_BLOCK_SIZE 16384
#define RAM_A_ADDRESS 0x1e000000UL
#define RAM_A_SIZE 0x01000000UL
#define RAM_B_ADDRESS 0x1f000000UL
#define RAM_B_SIZE 0x01000000UL

/* MeCom API parameters */
#define MECOM_ADDRESS 0
#define MECOM_INST 1
#define USE_BUILT_IN_PID true

#define Kp 0.01
#define Ki 0
#define Kd 0