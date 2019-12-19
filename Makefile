
CFLAGS += -I.
OBJS = $(patsubst %.c,%.o, $(wildcard src/*.c))
OBJS += $(patsubst %.c,%.o, $(wildcard src/arch-x86/*.c))

atrace: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) $(OBJS)
	$(RM) atrace

include $(patsubst %.o,%.d,$(filter %.o,$(OBJS)))

%.d: %.c
	set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$;	\
	sed 's,\($(notdir $*)\)\.o[ :]*,$(dir $*)\1.o $@ : ,g' < $@.$$$$ > $@;	\
	$(RM) -f $@.$$$$
