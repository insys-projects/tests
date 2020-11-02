
PHONY = clean
TARGET_NAME = mem_test

include $(BARDYDIR)/make/macros.mk

DIRS := $(GIPCYINC) $(BARDYDIR)/BRDINC $(BARDYDIR)/BRDINC/ctrladmpro $(BARDYDIR)/IcrInc \
$(BARDYDIR)/BRDLIBS
INC := $(addprefix  -I, $(DIRS))

#CFLAGS += -Wc++0x-compat $(INC)
CFLAGS +=  -std=c++11 $(INC)

SRC := $(wildcard *.cpp)
SOURCE += $(SRC)
OBJ_FILES := $(SOURCE:.cpp=.o)

all: $(TARGET_NAME)
	cp $(TARGET_NAME) $(BARDYDIR)/bin

include $(BARDYDIR)/make/rules.mk
include $(BARDYDIR)/make/clean.mk
include $(BARDYDIR)/make/app.mk