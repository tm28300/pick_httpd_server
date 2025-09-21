# uncomment to add many debug messages
DEBUG_FLAG=-DPHS_DEBUG
EXEC_NAME=pick_httpd_server
OBJS=$(patsubst %.c, %.o, $(wildcard *.c)) $(patsubst %.cpp, %.o, $(wildcard *.cpp))
ifeq ($(wildcard /home/thierry/openqm/openqm.account/),)
OPENQM_ROOT=/usr/qmsys
QMCLILIB=qmcli
else
OPENQM_ROOT=/home/thierry/openqm/openqm.account
QMCLILIB=qmcli64
endif
INCLUDES=-I$(OPENQM_ROOT)/SYSCOM
CCFLAGS=-Wall -g
LT_LDFLAGS=-L$(OPENQM_ROOT)/bin -lmicrohttpd -lconfig -lpcre2-8 -l$(QMCLILIB) -lstdc++ -lPocoFoundation -lPocoEncodings
DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

all: $(EXEC_NAME)

$(EXEC_NAME): $(OBJS)
	gcc -o $@ $(OBJS) $(LT_LDFLAGS)

%.o : %.c
%.o : %.c $(DEPDIR)/%.d | $(DEPDIR)
	gcc $(CCFLAGS) $(DEPFLAGS) $(INCLUDES) $(DEBUG_FLAG) -c $< -o $@

%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d | $(DEPDIR)
	g++ $(CCFLAGS) $(DEPFLAGS) $(INCLUDES) $(DEBUG_FLAG) -c $< -o $@

$(DEPDIR): ; @mkdir -p $@

DEPFILES := $(OBJS:%.o=$(DEPDIR)/%.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))

clean:
	-rm -f $(OBJS) openqm_httpd_server
