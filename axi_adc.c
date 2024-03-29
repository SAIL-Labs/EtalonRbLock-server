/*
 * Redpitaya ADC Acquisition and PID controller Server Program for SAIL Photonics Comb Rubidium lock
 * Base on axi_adc.c redpitaya example code by Nils Roos (License attached).
 * 
 * New features include:
 * - TCP/IP comms
 * - Interface with MeCOM API for comunication with PID controller
 * - Startup flags to change quastion size (-a), enable PID contoller (-m), set cilent IP (-i) 
 * 
 * Copyright Chris Betters USYD 2017
 */

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "temp_moniter.h"
#include "configuration.h"
#include "MeComAPI/MeCom.h"
#include "bme280.h"

/* data types */
enum equalizer
{
  EQ_OFF,
  EQ_LV,
  EQ_HV
};
enum trigger
{
  TR_OFF = 0,
  TR_MANUAL,
  TR_CH_A_RISING,
  TR_CH_A_FALLING,
  TR_CH_B_RISING,
  TR_CH_B_FALLING,
  TR_EXT_RISING,
  TR_EXT_FALLING,
  TR_ASG_RISING,
  TR_ASG_FALLING
};
enum decimation
{
  DE_OFF = 0,
  DE_1 = 0x00001,
  DE_8 = 0x00008,
  DE_64 = 0x00040,
  DE_1024 = 0x00400,
  DE_8192 = 0x02000,
  DE_65536 = 0x10000
};

struct queue
{
  pthread_mutex_t mutex;
  pthread_t sender;
  int started;
  unsigned int read_end;
  uint8_t *buf;
  int sock_fd;
};

/* macros and prototypes */
/* note: the circular buffer macros may evaluate each of their arguments once,
 * more
 *       than once or not at all. don't use expressions with side-effects */
/* add offsets within circular buffer */
#define CIRCULAR_ADD(arg1, arg2, size) (((arg1) + (arg2)) % (size))
/* subtract offsets within circular buffer */
#define CIRCULAR_SUB(arg1, arg2, size) \
  ((arg1) >= (arg2) ? (arg1) - (arg2) : (size) + (arg1) - (arg2))
/* calculate distance within circular buffer */
#define CIRCULAR_DIST(argfrom, argto, size) \
  CIRCULAR_SUB((argto), (argfrom), (size))
/* memcpy from circular source to linear target */
#define CIRCULARSRC_MEMCPY(target, src_base, src_offs, src_size, length) \
  do                                                                     \
  {                                                                      \
    if ((src_offs) + (length) <= (src_size))                             \
    {                                                                    \
      memcpy((target), (void *)(src_base) + (src_offs), (length));       \
    }                                                                    \
    else                                                                 \
    {                                                                    \
      unsigned int __len1 = (src_size) - (src_offs);                     \
      memcpy((target), (void *)(src_base) + (src_offs), __len1);         \
      memcpy((void *)(target) + __len1, (src_base), (length)-__len1);    \
    }                                                                    \
  } while (0)

static void scope_reset(void);
static void scope_set_filters(enum equalizer eq, int shaping,
                              volatile uint32_t *base);
static void scope_setup_input_parameters(enum decimation dec,
                                         enum equalizer ch_a_eq,
                                         enum equalizer ch_b_eq,
                                         int ch_a_shaping, int ch_b_shaping);
static void scope_setup_trigger_parameters(int thresh_a, int thresh_b,
                                           int hyst_a, int hyst_b,
                                           int deadtime);
static void scope_setup_axi_recording(void);
static void scope_activate_trigger(enum trigger trigger);
static void ADC_read_worker(struct queue *a, struct queue *b);
static void *TCP_ADC_data_send_worker(void *data);
unsigned long long getMillisecondsSinceEpoch(void);
int flipFibreSwitchs(bool enableSpec);

/* module global variables */
static volatile void
    *scope; /* access to fpga registers must not be optimized */
static void *buf_a = MAP_FAILED;
static void *buf_b = MAP_FAILED;
static struct queue queue_a = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .started = 0,
    .read_end = 0,
    .buf = NULL,
    .sock_fd = -1,
};
static struct queue queue_b = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .started = 0,
    .read_end = 0,
    .buf = NULL,
    .sock_fd = -1,
};

static struct queue queue_tecpid = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .started = 0,
    .read_end = 0,
    .buf = NULL,
    .sock_fd = -1,
};

int AckSock_fd;

char CLIENT_IP_ADDR[] = "10.66.101.131";
int ACQUISITION_LENGTH = 20000;
int USE_BUILT_IN_PID;

// int bmefd;
// bme280_calib_data bmecal;

/* functions */
/*
 * main without paramater evaluation - all configuration is done through
 * constants for the purposes of this example.
 */
int main(int argc, char **argv)
{
  int rc;
  int mem_fd;
  void *smap = MAP_FAILED;
  struct sockaddr_in srv_addr;
  int c;

  while ((c = getopt(argc, argv, "a:m:i:")) != -1)
    switch (c)
    {
    case 'a':
      ACQUISITION_LENGTH = atoi(optarg);
      break;
    case 'i':
      strcpy(CLIENT_IP_ADDR, optarg);
      break;
    case 'm':
      USE_BUILT_IN_PID = atoi(optarg);
      break;
    case '?':
      if (optopt == 'c')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr,
                "Unknown option character `\\x%x'.\n",
                optopt);
      return 1;
    default:
      abort();
    }
  fprintf(stderr, "IP of Moniter %s\n", CLIENT_IP_ADDR);
  // if (rp_Init() != RP_OK) {
  //   fprintf(stderr, "Red Pitaya API init failed!\n");
  //   return EXIT_FAILURE;
  // }

  if (ENABLE_MECOM)
  {
    if (initMeCom(0, 1, USE_BUILT_IN_PID))
    {
      fprintf(stderr, "MeCom Failed.");
      goto main_exit;
    }
  }

  // if (ENABLE_BME280)
  // {
  //   if (setupBME280())
  //   {
  //     fprintf(stderr, "BME280 Failed.");
  //     goto main_exit;
  //   }
  // }

  /* acquire pointers to mapped bus regions of fpga and dma ram */
  mem_fd = open("/dev/mem", O_RDWR);
  if (mem_fd < 0)
  {
    fprintf(stderr, "open /dev/mem failed, %s\n", strerror(errno));
    rc = -1;
    goto main_exit;
  }

  smap = mmap(NULL, 0x00100000UL, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd,
              0x40100000UL);
  buf_a = mmap(NULL, RAM_A_SIZE, PROT_READ, MAP_SHARED, mem_fd, RAM_A_ADDRESS);
  buf_b = mmap(NULL, RAM_B_SIZE, PROT_READ, MAP_SHARED, mem_fd, RAM_B_ADDRESS);
  if (smap == MAP_FAILED || buf_a == MAP_FAILED || buf_b == MAP_FAILED)
  {
    fprintf(stderr, "mmap failed, %s - scope %p buf_a %p buf_b %p\n",
            strerror(errno), smap, buf_a, buf_b);
    rc = -2;
    goto main_exit;
  }
  scope = smap;

  /* allocate cacheable buffers */
  queue_a.buf = malloc(ACQUISITION_LENGTH * 2);
  queue_b.buf = malloc(ACQUISITION_LENGTH * 2);
  if (queue_a.buf == NULL || queue_b.buf == NULL)
  {
    fprintf(stderr, "malloc failed, %s - buf a %p buf b %p\n", strerror(errno),
            queue_a.buf, queue_b.buf);
    rc = -3;
    goto main_exit;
  }

  /* setup tcp sockets */
  queue_a.sock_fd = socket(PF_INET, SOCK_STREAM, 0);
  queue_b.sock_fd = socket(PF_INET, SOCK_STREAM, 0);
  AckSock_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (queue_a.sock_fd < 0 || queue_b.sock_fd < 0 || AckSock_fd < 0)
  {
    fprintf(
        stderr,
        "create socket failed, %s - sock_fd a %d sock_fd b %d sock_fd ack %d\n",
        strerror(errno), queue_a.sock_fd, queue_b.sock_fd, AckSock_fd);
    rc = -4;
    goto main_exit;
  }

  int reuse = 1;
  if (setsockopt(queue_a.sock_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
    fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(queue_b.sock_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
    fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(AckSock_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
    fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");

  memset(&srv_addr, 0, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  srv_addr.sin_port = htons(CLIENT_IP_PORT_A);

  if (bind(queue_a.sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) <
      0)
  {
    fprintf(stderr, "bind A failed, %s\n", strerror(errno));
    rc = -5;
    goto main_exit;
  }

  srv_addr.sin_port = htons(CLIENT_IP_PORT_B);

  if (bind(queue_b.sock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) <
      0)
  {
    fprintf(stderr, "bind B failed, %s\n", strerror(errno));
    rc = -5;
    goto main_exit;
  }

  /* setup ack socket */
  memset(&srv_addr, 0, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  srv_addr.sin_port = htons(CLIENT_IP_PORT_ACK);
  memset(srv_addr.sin_zero, '\0', sizeof srv_addr.sin_zero); // optional

  if (bind(AckSock_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
  {
    fprintf(stderr, "bind Ack failed, %s\n", strerror(errno));
    rc = -5;
    goto main_exit;
  }

  /* initialize scope */
  scope_reset();
  scope_setup_input_parameters(DECIMATION, EQ_LV, EQ_HV, 1, 1);
  scope_setup_trigger_parameters(TRIGGER_THRESHOLD, TRIGGER_THRESHOLD, 50, 50,
                                 1250);
  scope_setup_axi_recording();

  /* start socket senders */
  rc = pthread_create(&queue_a.sender, NULL, TCP_ADC_data_send_worker, &queue_a);
  if (rc != 0)
  {
    fprintf(stderr, "start sender A failed, %s\n", strerror(rc));
    rc = -6;
    goto main_exit;
  }
  queue_a.started = 1;

  rc = pthread_create(&queue_b.sender, NULL, TCP_ADC_data_send_worker, &queue_b);
  if (rc != 0)
  {
    fprintf(stderr, "start sender B failed, %s\n", strerror(rc));
    rc = -6;
    goto main_exit;
  }
  queue_b.started = 1;

  /* start reader in main-thread */
  fprintf(stderr, "ADC_read_worker starting...\n");
  ADC_read_worker(&queue_a, &queue_b);

main_exit:
  fprintf(stderr, "exiting...\n");
  /* cleanup */
  if (queue_a.started)
  {
    pthread_cancel(queue_a.sender);
    pthread_join(queue_a.sender, NULL);
  }
  if (queue_tecpid.started)
  {
    pthread_cancel(queue_b.sender);
    pthread_join(queue_b.sender, NULL);
  }
  if (queue_tecpid.started)
  {
    pthread_cancel(queue_b.sender);
    pthread_join(queue_b.sender, NULL);
  }
  if (smap != MAP_FAILED)
    munmap(smap, 0x00100000UL);
  if (buf_a != MAP_FAILED)
    munmap(buf_a, RAM_A_SIZE);
  if (buf_b != MAP_FAILED)
    munmap(buf_b, RAM_B_SIZE);
  if (queue_a.buf)
    free(queue_a.buf);
  if (queue_b.buf)
    free(queue_b.buf);
  if (mem_fd >= 0)
    close(mem_fd);
  if (queue_a.sock_fd >= 0)
    close(queue_a.sock_fd);
  if (queue_b.sock_fd >= 0)
    close(queue_b.sock_fd);
  if (AckSock_fd >= 0)
    close(AckSock_fd);

  return rc;
}

static void scope_reset(void)
{
  *(uint32_t *)(scope + 0x00000) = 2; /* reset scope */
}

static void scope_set_filters(enum equalizer eq, int shaping,
                              volatile uint32_t *base)
{
  /* equalization filter */
  switch (eq)
  {
  case EQ_HV:
    *(base + 0) = 0x4c5f; /* filter coeff aa */
    *(base + 1) = 0x2f38b; /* filter coeff bb */
    break;
  case EQ_LV:
    *(base + 0) = 0x7d93; /* filter coeff aa */
    *(base + 1) = 0x437c7; /* filter coeff bb */
    break;
  case EQ_OFF:
    *(base + 0) = 0x0; /* filter coeff aa */
    *(base + 1) = 0x0; /* filter coeff bb */
    break;
  }

  /* shaping filter */
  if (shaping)
  {
    *(base + 2) = 0xd9999a; /* filter coeff kk */
    *(base + 3) = 0x2666; /* filter coeff pp */
  }
  else
  {
    *(base + 2) = 0xffffff; /* filter coeff kk */
    *(base + 3) = 0x0; /* filter coeff pp */
  }
}

static void scope_setup_input_parameters(enum decimation dec,
                                         enum equalizer ch_a_eq,
                                         enum equalizer ch_b_eq,
                                         int ch_a_shaping, int ch_b_shaping)
{
  *(uint32_t *)(scope + 0x00014) = dec; /* decimation */
  *(uint32_t *)(scope + 0x00028) =
      (dec != DE_OFF) ? 1 : 0; /* enable averaging */

  scope_set_filters(
      ch_a_eq, ch_a_shaping,
      (uint32_t *)(scope + 0x00030)); /* filter coeff base channel a */
  scope_set_filters(
      ch_b_eq, ch_b_shaping,
      (uint32_t *)(scope + 0x00040)); /* filter coeff base channel b */
}

static void scope_setup_trigger_parameters(int thresh_a, int thresh_b,
                                           int hyst_a, int hyst_b,
                                           int deadtime)
{
  *(uint32_t *)(scope + 0x00008) = thresh_a; /* channel a trigger threshold */
  *(uint32_t *)(scope + 0x0000c) = thresh_b; /* channel b trigger threshold */
  /* the legacy recording logic controls when the trigger mode will be reset. we
   * want
   * that to happen as soon as possible (because that's the signal that a
   * trigger event
   * occured, and the pre-trigger samples are already waiting for transmission),
   * so set
   * some small value > 0 here */
  *(uint32_t *)(scope + 0x00010) = 10; /* legacy post trigger samples */
  *(uint32_t *)(scope + 0x00020) = hyst_a; /* channel a trigger hysteresis */
  *(uint32_t *)(scope + 0x00024) = hyst_b; /* channel b trigger hysteresis */
  *(uint32_t *)(scope + 0x00090) = deadtime; /* trigger deadtime */
}

static void scope_setup_axi_recording(void)
{
  *(uint32_t *)(scope + 0x00050) = RAM_A_ADDRESS; /* buffer a start */
  *(uint32_t *)(scope + 0x00054) =
      RAM_A_ADDRESS + RAM_A_SIZE; /* buffer a stop */
  *(uint32_t *)(scope + 0x00058) = ACQUISITION_LENGTH - PRE_TRIGGER_LENGTH +
                                   64; /* channel a post trigger samples */
  *(uint32_t *)(scope + 0x00070) = RAM_B_ADDRESS; /* buffer b start */
  *(uint32_t *)(scope + 0x00074) =
      RAM_B_ADDRESS + RAM_B_SIZE; /* buffer b stop */
  *(uint32_t *)(scope + 0x00078) = ACQUISITION_LENGTH - PRE_TRIGGER_LENGTH +
                                   64; /* channel b post trigger samples */

  *(uint32_t *)(scope + 0x0005c) = 1; /* enable channel a axi */
  *(uint32_t *)(scope + 0x0007c) = 1; /* enable channel b axi */
}

static void scope_activate_trigger(enum trigger trigger)
{
  /* TODO maybe use the 'keep armed' flag without reset, to have better
   * pre-trigger data when a trigger immediately follows the previous recording
   */
  *(uint32_t *)(scope + 0x00000) = 3; /* reset and arm scope */
  *(uint32_t *)(scope + 0x00000) = 0; /* armed for trigger */
  *(uint32_t *)(scope + 0x00004) = trigger; /* trigger source */
}

/*
 * arms the scope and waits for trigger. once a trigger occurs, it reads samples
 * from dma ram and puts them on the channel queues. advances each queue's
 * queue->read_end for each block that was copied. rinse and repeat. access to
 * read_end is protected by queue->mutex.
 */
static void ADC_read_worker(struct queue *a, struct queue *b)
{
  unsigned int start_pos_a, start_pos_b;
  unsigned int curr_pos_a, curr_pos_b;
  unsigned int read_pos_a, read_pos_b;
  size_t length_a, length_b;
  int a_first, a_ready, b_first, b_ready;
  int did_something;

  char Ackbuf[100];
  char ackstr[3];

  float settempcur;
  float prev_settempcur;
  float currentTemp;
  int psd;
  float t, p, h;

  MeParFloatFields Fields;

  /*wait for ack to start*/
  fprintf(stderr, "Waiting for Ack to Continue! (1st)\n");
  listen(AckSock_fd, 10);
  psd = accept(AckSock_fd, 0, 0);
  recv(psd, Ackbuf, sizeof(Ackbuf), 0);
  close(psd);

  sscanf(Ackbuf, "%s %f", ackstr, &settempcur);
  fprintf(stderr, "Received: %s and Temp set %f\n", ackstr, settempcur);

  prev_settempcur = settempcur + 0.1; // force different for first test

  if (strcmp("END", ackstr) == 0)
    goto ADC_read_worker_exit;

  do
  {

    a_first = b_first = 1;
    a_ready = b_ready = 0;

    do
    {
      /* wait for send to finish */
      /* get buffer positions */
      if (pthread_mutex_lock(&a->mutex) != 0)
        goto ADC_read_worker_exit;
      read_pos_a = a->read_end;
      if (pthread_mutex_unlock(&a->mutex) != 0)
        goto ADC_read_worker_exit;

      if (pthread_mutex_lock(&b->mutex) != 0)
        goto ADC_read_worker_exit;
      read_pos_b = b->read_end;
      if (pthread_mutex_unlock(&b->mutex) != 0)
        goto ADC_read_worker_exit;
      usleep(5);
    } while (read_pos_a != 0 || read_pos_b != 0);

    scope_activate_trigger(TRIGGER_MODE);
    /* wait for trigger */
    while (*(uint32_t *)(scope + 0x00004))
      usleep(5);

    //rp_DpinSetState(RP_LED4, RP_HIGH);

    unsigned long long millisecondsSinceEpoch = getMillisecondsSinceEpoch();
    fprintf(stderr, "Triggered at %lld.\n", millisecondsSinceEpoch);

    if (ENABLE_MECOM)
      currentTemp = getTECTemp(0, 1);
    else
      currentTemp = 0;
    if (ENABLE_BME280)
    {
      connectAndGetBMEData(&t, &p, &h);
      //fprintf(stderr, "Sent - Time: %f, Tec Temp: %f, Ext Temp: %f, Pressure: %f, Humidity: %f\n", millisecondsSinceEpoch / 1000.0, currentTemp, t, p, h);
    }

    start_pos_a =
        *(uint32_t *)(scope + 0x00060); /* channel a trigger pointer */
    start_pos_b =
        *(uint32_t *)(scope + 0x00080); /* channel b trigger pointer */

    start_pos_a = CIRCULAR_SUB(start_pos_a - RAM_A_ADDRESS,
                               PRE_TRIGGER_LENGTH * 2, RAM_A_SIZE);
    start_pos_b = CIRCULAR_SUB(start_pos_b - RAM_B_ADDRESS,
                               PRE_TRIGGER_LENGTH * 2, RAM_B_SIZE);

    did_something = 1;
    // fprintf(stderr,"did_something\n");
    do
    {
      if (!did_something)
        usleep(5);
      did_something = 0;

      /* get buffer positions */
      if (pthread_mutex_lock(&a->mutex) != 0)
        goto ADC_read_worker_exit;
      read_pos_a = a->read_end;
      if (pthread_mutex_unlock(&a->mutex) != 0)
        goto ADC_read_worker_exit;

      if (pthread_mutex_lock(&b->mutex) != 0)
        goto ADC_read_worker_exit;
      read_pos_b = b->read_end;
      if (pthread_mutex_unlock(&b->mutex) != 0)
        goto ADC_read_worker_exit;

      /* before starting, test if senders are ready */
      if (a_first && read_pos_a == 0)
      {
        a_first = 0;
        a_ready = 1;
        // fprintf(stderr,"a_ready\n");
      }
      if (b_first && read_pos_b == 0)
      {
        b_first = 0;
        b_ready = 1;
        // fprintf(stderr,"b_ready\n");
      }

      /* get current recording positions */
      curr_pos_a =
          *(uint32_t *)(scope + 0x00064); /* channel a current write pointer */
      curr_pos_b =
          *(uint32_t *)(scope + 0x00084); /* channel b current write pointer */
      curr_pos_a -= RAM_A_ADDRESS;
      curr_pos_b -= RAM_B_ADDRESS;

      /* calculate block sizes */
      if (read_pos_a + READ_BLOCK_SIZE <= ACQUISITION_LENGTH * 2)
        length_a = READ_BLOCK_SIZE;
      else
        length_a = ACQUISITION_LENGTH * 2 - read_pos_a;
      if (read_pos_b + READ_BLOCK_SIZE <= ACQUISITION_LENGTH * 2)
        length_b = READ_BLOCK_SIZE;
      else
        length_b = ACQUISITION_LENGTH * 2 - read_pos_b;

      /* copy if sender is ready and a full block is available in the dma ram */
      if (a_ready &&
          CIRCULAR_DIST(start_pos_a, curr_pos_a, RAM_A_SIZE) >= length_a)
      {
        CIRCULARSRC_MEMCPY(a->buf + read_pos_a, buf_a, start_pos_a, RAM_A_SIZE,
                           length_a);
        start_pos_a = CIRCULAR_ADD(start_pos_a, length_a, RAM_A_SIZE);

        if (read_pos_a + length_a >= ACQUISITION_LENGTH * 2)
          a_ready = 0; /* stop if all samples were copied */

        if (pthread_mutex_lock(&a->mutex) != 0)
          goto ADC_read_worker_exit;
        if (a->read_end == read_pos_a)
          a->read_end += length_a;
        else
          a_ready = 0; /* stop if sender resetted read_end */
        if (pthread_mutex_unlock(&a->mutex) != 0)
          goto ADC_read_worker_exit;

        did_something = 1;
      }
      if (b_ready &&
          CIRCULAR_DIST(start_pos_b, curr_pos_b, RAM_B_SIZE) > length_b)
      {
        CIRCULARSRC_MEMCPY(b->buf + read_pos_b, buf_b, start_pos_b, RAM_B_SIZE,
                           length_b);
        start_pos_b = CIRCULAR_ADD(start_pos_b, length_b, RAM_B_SIZE);

        if (read_pos_b + length_b >= ACQUISITION_LENGTH * 2)
          b_ready = 0; /* stop if all samples were copied */

        if (pthread_mutex_lock(&b->mutex) != 0)
          goto ADC_read_worker_exit;
        if (b->read_end == read_pos_b)
          b->read_end += length_b;
        else
          b_ready = 0; /* stop if sender resetted read_end */
        if (pthread_mutex_unlock(&b->mutex) != 0)
          goto ADC_read_worker_exit;

        did_something = 1;
      }
    } while (a_first || a_ready || b_first || b_ready);

    listen(AckSock_fd, 10);
    psd = accept(AckSock_fd, 0, 0);
    fprintf(stderr, "Waiting to send temp and timestamp!\n");
    send(psd, &millisecondsSinceEpoch, sizeof(unsigned long long), 0);
    send(psd, &currentTemp, sizeof(float), 0);
    send(psd, &t, sizeof(float), 0);
    send(psd, &p, sizeof(float), 0);
    send(psd, &h, sizeof(float), 0);
    close(psd);

    //rp_DpinSetState(RP_LED4, RP_LOW);

    /*wait for ack to cont*/

    listen(AckSock_fd, 10);
    psd = accept(AckSock_fd, 0, 0);
    fprintf(stderr, "Waiting for Ack to Continue!\n");
    recv(psd, Ackbuf, sizeof(Ackbuf), 0);
    close(psd);
    sscanf(Ackbuf, "%s %f", ackstr, &settempcur);

    fprintf(stderr, "Received: %s and Temp/Vol set %f\n", ackstr, settempcur);

    if (strcmp("END", ackstr) == 0)
      goto ADC_read_worker_exit;

    if (prev_settempcur != settempcur) // only set if value changes.
    {
      if (USE_BUILT_IN_PID && ENABLE_MECOM)
      {
        if (MeCom_TEC_Tem_TargetObjectTemp(0, 1, &Fields, MeGetLimits))
        {
          Fields.Value = settempcur;
          if (MeCom_TEC_Tem_TargetObjectTemp(0, 1, &Fields, MeSet))
            fprintf(stderr, "TEC Object Temperature: New Value: %f\n",
                    Fields.Value);
        }
      }
      else
      {
        setTECVandC(0, 1, 3, settempcur);
        fprintf(stderr, "TEC Current: New Value: %f\n", settempcur);
      }
    }

    usleep(DELAYFORLOOP);
  } while (1);

ADC_read_worker_exit:
  fprintf(stderr, "ADC_read_worker_exit\n");
  return;
}

/*
 * sends samples from a struct queue. synchronisation with the queue is done via
 * queue->read_end. TCP_ADC_data_send_worker will send data from 0 to read_end and will reset
 * read_end to 0 once ACQUISITION_LENGTH samples have been transmitted. then it
 * will wait until read_end advances from 0 and start all over. access to
 * read_end
 * is protected by queue->mutex.
 */
static void *TCP_ADC_data_send_worker(void *data)
{
  struct queue *q = (struct queue *)data;
  int psd = 0;
  unsigned int send_pos = 0;
  ssize_t sent;
  size_t length;

  do
  {
    if (pthread_mutex_lock(&q->mutex) != 0)
      goto TCP_ADC_data_send_worker_exit;
    if (q->read_end >= ACQUISITION_LENGTH * 2 &&
        send_pos >= ACQUISITION_LENGTH * 2)
    {
      send_pos = 0;
      q->read_end = 0;
      close(psd);
      psd = 0;
    }
    length = q->read_end - send_pos;
    if (pthread_mutex_unlock(&q->mutex) != 0)
      goto TCP_ADC_data_send_worker_exit;

    if (length > 0)
    {
      if (!psd)
      {
        //fprintf(stderr, "listening\n");
        listen(q->sock_fd, 10);
        psd = accept(q->sock_fd, NULL, NULL);
        //fprintf(stderr, "accepted\n");
      }

      do
      {
        if (length > SEND_BLOCK_SIZE)
          sent = send(psd, q->buf + send_pos, SEND_BLOCK_SIZE, 0);
        else
          sent = send(psd, q->buf + send_pos, length, 0);
        if (sent > 0)
        {
          send_pos += sent;
          length -= sent;
        }
      } while (sent >= 0 && length > 0);

      // sent = send(q->sock_fd, "\n", 1, 0);
      if (sent < 0)
        goto TCP_ADC_data_send_worker_exit;
    }
    else
    {
      usleep(5);
    }
  } while (1);

TCP_ADC_data_send_worker_exit:
  return NULL;
}

unsigned long long getMillisecondsSinceEpoch(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  unsigned long long millisecondsSinceEpoch =
      (unsigned long long)(tv.tv_sec) * 1000 +
      (unsigned long long)(tv.tv_usec) / 1000;
  return millisecondsSinceEpoch;
}

float actual_error, error_previous, P, I, D;

float PID_Controller(float set_point, float measured_value)
{
  error_previous = actual_error; // error_previous holds the previous error
  actual_error = set_point - measured_value;
  // PID
  P = actual_error; // Current error
  I += error_previous; // Sum of previous errors
  D = actual_error - error_previous; // Difference with previous error

  return Kp * P + Ki * I + Kd * D; // adjust Kp, Ki, Kd empirically or by using
      // online method such as ZN
}