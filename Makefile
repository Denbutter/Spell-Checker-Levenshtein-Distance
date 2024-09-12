CC = gcc
CFLAGS = -Wall -std=c11 -g

all: spellChecker

spellChecker: spellChecker.c
	$(CC) $(CFLAGS) spellChecker.c -o spellChecker

LongestWord: LongestWord.c
	$(CC) $(CFLAGS) LongestWord.c -o LongestWord

DictionaryTest: DictionaryTest.c
	$(CC) $(CFLAGS) DictionaryTest.c -o DictionaryTest

clean:
	rm -i spellChecker LongestWord DictionaryTest *.o
