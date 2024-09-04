CC = gcc
CFLAGS = -Wall -std=c11 -g

all: A3Checker

A3Checker: A3Checker.c
	$(CC) $(CFLAGS) A3Checker.c -o A3Checker

LongestWord: LongestWord.c
	$(CC) $(CFLAGS) LongestWord.c -o LongestWord

DictionaryTest: DictionaryTest.c
	$(CC) $(CFLAGS) DictionaryTest.c -o DictionaryTest

clean:
	rm -i A3Checker LongestWord DictionaryTest *.o