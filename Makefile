AST_ROOT=	../asterisk-1.8.14.1
SERVAL_ROOT=	../batphone/jni/serval-dna

NAME=	app_servaldna

SRCS=	app_servaldna.c \
	chan_vomp.c

HDRS=	app.h

OBJS=	$(SRCS:.c=.o)

include $(AST_ROOT)/makeopts

CC=	cc
RM=	rm

# Voodoo from Asterisk 1.8.14.1 Makefile for OSX
ifneq ($(findstring darwin,$(OSARCH)),)
  CFLAGS+=-D__Darwin__
  LDFLAGS+=-bundle -Xlinker -macosx_version_min -Xlinker 10.4 -Xlinker -undefined -Xlinker dynamic_lookup -force_flat_namespace
  ifeq ($(shell if test `/usr/bin/sw_vers -productVersion | cut -c4` -gt 5; then echo 6; else echo 0; fi),6)
    LDFLAGS+=/usr/lib/bundle1.o
  endif
  LDFLAGS+=-L/usr/local/lib
else
# Everyone else
  $(warning Building for someone else)
  LDFLAGS+=-shared
endif

CFLAGS+=	-DAST_MODULE=\"$(NAME)\" -I$(AST_ROOT)/include -I$(SERVAL_ROOT) -fPIC $(PTHREAD_CFLAGS) -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -g
CFLAGS+=	-DLOW_MEMORY
LDFLAGS+= $(SERVAL_ROOT)/libmonitorclient.a

%.o:	%.c $(HDRS)
	$(CC) $(DEFS) $(CFLAGS) -c $<

$(NAME).so: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) -f $(OBJS)

