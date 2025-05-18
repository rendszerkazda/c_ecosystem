CC = gcc
CFLAGS = -Wall -Wextra -g -fopenmp
LDFLAGS = -fopenmp -lncurses  -ltinfo

# Forrásfájlok
SRCS = main.c world_utils.c entity_actions.c

# Tárgyfájlok (automatikus generálás SRCS alapján)
OBJS = $(SRCS:.c=.o)

# Futtatható állomány neve
TARGET = ecosystem_simulator

# Alapértelmezett cél: a futtatható állomány létrehozása
all: $(TARGET)

# A futtatható állomány linkelése a tárgyfájlokból
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Általános szabály .c fájlokból .o fájlok fordítására
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# "make clean" parancs a generált fájlok törléséhez
clean:
	rm -f $(TARGET) $(OBJS)

# "make run" parancs a program futtatásához (opcionális argumentummal)
run: $(TARGET)
	./$(TARGET) $(ARGS) 2> timings.log

.PHONY: all clean run 
