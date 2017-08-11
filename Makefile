#Cross compiler definition
CC = $(CROSS_COMPILE)gcc
RM      = rm -rf

SOURCEDIR=Dropbox/github/postdoc_code/PhotonicComb/EtalonRbLock-server

CFLAGS  = -g -std=gnu99 -Wall
#-Werror
#CFLAGS += -I../../api/include
#CFLAGS += -L ../../api/lib -lm -lpthread -lrp
CFLAGS += -I/opt/redpitaya/include 
CFLAGS += -L/opt/redpitaya/lib -lm -lpthread -lrp
CFLAGS += -lwiringPi

LDFLAGS = -L/opt/redpitaya/lib -lm -lpthread -lrp

MECOMSRC = MeComAPI/MePort_Linux.c \
      MeComAPI/private/MeCom.c MeComAPI/private/MeCRC16.c \
      MeComAPI/private/MeFrame.c MeComAPI/private/MeInt.c \
      MeComAPI/private/MeVarConv.c MeComAPI/ComPort/ComPort_Linux.c

SRCS=temp_moniter.c axi_adc.c bme280.c
SRCS+=$(MECOMSRC)
OBJ = $(SRCS:%.c=%.o)

# All Target
all: EtalonRbLock-server

EtalonRbLock-server: $(OBJ)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	$(CC) -o "EtalonRbLock-server" $(OBJ) $(CFLAGS)
	@echo 'Finished building target: $@'
	@echo ' '

%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) $(CFLAGS) -c $< -o $@
	@echo 'Finished building: $<'
	@echo ' '

clean:
	-$(RM) $(OBJ) EtalonRbLock-server
	
update:
	clear
	-$(RM) EtalonRbLock-server axi_adc.o
	rsync -arP --exclude '.vscode*' --exclude '.git*' chrisbetters@chris-delphi.sail-laboratories.com:$(SOURCEDIR) ~/
	$(MAKE) EtalonRbLock-server
