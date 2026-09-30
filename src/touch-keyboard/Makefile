CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wno-unknown-pragmas -std=c++17
INCLUDES = -I.

SRCS = main.cc evdevsource.cc fakekeyboard.cc faketouchpad.cc uinputdevice.cc \
       haptic/ff_driver.cc haptic/touch_ff_manager.cc \
       statemachine/eventkey.cc statemachine/slot.cc statemachine/statemachine.cc \
       logging.cc

OBJS = $(SRCS:.cc=.o)
TARGET = touch_keyboard_handler

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
