OBJDIR=build
C_FILES=$(wildcard src/*.c)
OBJ_FILES=$(patsubst src/%.c,$(OBJDIR)/%.o,$(C_FILES))
LIBS=-lm
CC=gcc
LD=gcc
CFLAGS=-Iinclude -I. -Wall -Wpedantic -Werror -std=c99 -MMD
DEBUG=1
PROFILE=0

ifeq ($(PROFILE), 1)
	CFLAGS+=-O2 -pg -DNDEBUG
	LDFLAGS+=-pg
else
	ifeq ($(DEBUG), 1)
		CFLAGS+=-O0 -g -D_DEBUG
	else
		CFLAGS+=-O2 -DNDEBUG
	endif
endif

all: $(OBJDIR)/mal tags

clean:
	rm -f $(OBJDIR)/*.[oda] $(OBJDIR)/builtins/*.[oda]
	rm -f step1_read_print

$(OBJDIR)/%.o:src/%.c
	${CC} ${CFLAGS} -c -o $@ $<  

$(OBJDIR)/mal: ${OBJ_FILES} ${LIB_BUILTINS}
	${LD} ${LDFLAGS} -Xlinker -Map=$(patsubst $(OBJDIR)/%,$(OBJDIR)/%.map,$@) \
		-o $@ ${OBJ_FILES} ${LIB_BUILTINS} ${LIBS}

step1_read_print: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step2_eval: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step3_env: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step4_if_fn_do: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step5_tco: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step6_file: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step7_quote: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step8_macros: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

step9_try: $(OBJDIR)/mal
	@rm -f $@
	@ln -s $< $@

.PHONY: all clean tags

ifneq ($(wildcard $(OBJDIR)/*.d), )
include $(wildcard $(OBJDIR)/*.d)
endif

ifneq ($(wildcard $(OBJDIR)/builtins/*.d), )
include $(wildcard $(OBJDIR)/builtins/*.d)
endif

