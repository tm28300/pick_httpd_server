# uncomment to add many debug messages
#DEBUG_FLAG=-DPHS_DEBUG
EXEC_NAME=pick_httpd_server
OBJS=$(patsubst %.c, %.o, $(wildcard *.c)) $(patsubst %.cpp, %.o, $(wildcard *.cpp))
OPENQM_ROOT=/home/thierry/openqm/openqm.account
INCLUDES=-I$(OPENQM_ROOT)/SYSCOM -I$(OPENQM_ROOT)/gplsrc
CCFLAGS=-Wall -g
LT_LDFLAGS=-L$(OPENQM_ROOT)/bin -lmicrohttpd -lconfig -lpcre -lqmcli64 -lstdc++ -lPocoFoundation
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
