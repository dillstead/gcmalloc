CC        := gcc
TST       := gcmalloc_test
SLIB      := libgcmalloc.a
SRC       := gcmalloc.c getregs.S
SOBJ      := $(SRC:%.c=%.o)
SOBJ      += $(SRC:%.S=%.o)
TST_SRC   := gcmalloc_test.c
TST_OBJ   := gcmalloc_test.o
CFLAGS    := -Wall -Werror -Wno-format -pedantic -std=c99 -g3 
ifdef NDEBUG
CFLAGS    += -DNDEBUG
endif
LDFLAGS := -L.

.PHONY: all clean

all: $(SLIB) $(TST)

$(SLIB): $(SOBJ)
	$(AR) $(ARFLAGS) $@ $^

$(TST): LDLIBS += -l:$(SLIB)
$(TST): $(TST_OBJ) $(SLIB)

clean:
	$(RM) -r $(TST) *.o *.a

