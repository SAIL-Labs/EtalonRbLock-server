#Cross compiler definition
CC = $(CROSS_COMPILE)gcc
RM      = rm -rf

CFLAGS  = -g -std=gnu99 -Wall
#-Werror
#CFLAGS += -I../../api/include
#CFLAGS += -L ../../api/lib -lm -lpthread -lrp
CFLAGS += -I/opt/redpitaya/include 
CFLAGS += -L/opt/redpitaya/lib -lm -lpthread -lrp

LDFLAGS = -L/opt/redpitaya/lib -lm -lpthread -lrp

MECOMSRC = MeComAPI/MePort_Linux.c \
      MeComAPI/private/MeCom.c MeComAPI/private/MeCRC16.c \
      MeComAPI/private/MeFrame.c MeComAPI/private/MeInt.c \
      MeComAPI/private/MeVarConv.c MeComAPI/ComPort/ComPort_Linux.c

SRCS=temp_moniter.c axi_adc.c
SRCS+=$(MECOMSRC)
OBJ = $(SRCS:%.c=%.o)

# All Target
all: UDPStreamer

UDPStreamer: $(OBJ)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	$(CC) -o "UDPStreamer" $(OBJ) $(CFLAGS)
	@echo 'Finished building target: $@'
	@echo ' '

%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) $(CFLAGS) -c $< -o $@
	@echo 'Finished building: $<'
	@echo ' '

clean:
	-$(RM) $(OBJ) UDPStreamer
	
update:
	clear
	-$(RM) UDPStreamer axi_adc.o
	rsync -arP --exclude '.vscode*' --exclude '.git*' chrisbetters@chris-delphi.sail-laboratories.com:Dropbox/github/postdoc_code/red/RpRbDAQ ~/
	$(MAKE) UDPStreamer
