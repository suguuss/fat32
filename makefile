CC			:= gcc
CC_FLAGS	:= -Wall -Wextra

BUILD 		:= build
SRC			:= src
LIBRARIES	:=			#Library flags like -lm -lncurses
EXECUTABLE	:= main

all: $(BIN)/$(EXECUTABLE)

run: pre-build all
	./$(BUILD)$(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): $(SRC)/*.c
	$(CC) $(CC_FLAGS) $^ -o $(BUILD)$@ $(LIBRARIES)

pre-build:
	mkdir -p $(BUILD)

clean:
	rm -r $(BUILD)