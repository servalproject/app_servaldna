NAME=	app_servaldna

SRCS=	$(NAME).c
OBJS=	$(SRCS:.c=.o)

AST_ROOT=	/Users/darius/projects/serval/asterisk-1.8.14.1
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

CFLAGS+=	-DAST_MODULE=\"$(NAME)\" -I$(AST_ROOT)/include -fPIC $(PTHREAD_CFLAGS) -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -g
LDFLAGS+=

%.o:	%.c
	$(CC) $(DEFS) $(CFLAGS) -c $<

$(NAME).so: $(OBJS)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	$(RM) -f $(OBJS)

