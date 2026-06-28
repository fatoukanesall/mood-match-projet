CC = gcc

# Tu peux surcharger ces variables:
# make GEMINI_API_KEY=... DB_PATH=... PORT=8080
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L -I./src -I/usr/include/postgresql
LDFLAGS =
LDLIBS = -lmicrohttpd -lpq -lcurl -lcjson

TARGET = moodmatch_backend
TEST_TARGET = moodmatch_tests

APP_SRC = src/main.c src/server.c src/db.c src/ai_client.c src/models.c
APP_OBJ = $(APP_SRC:.c=.o)

TEST_SRC = tests/test_runner.c tests/test_db.c tests/test_ai_parser.c src/db.c src/ai_client.c src/models.c
TEST_OBJ = $(TEST_SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(APP_OBJ)
	$(CC) $(APP_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_TARGET): $(TEST_OBJ)
	$(CC) $(TEST_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(APP_OBJ) $(TEST_OBJ) $(TARGET) $(TEST_TARGET)

.PHONY: all clean test run
