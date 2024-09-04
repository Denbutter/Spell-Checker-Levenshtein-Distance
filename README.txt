CIS 3050
Assignment 3
John Denbutter (1056466)

makefile instructions:
	make - creates the executable file A3Checker
	make clean - asks for permission to remove the executable file A3Checker. reply 'y', or 'n'.
*note: the makefile does also include functionality for creating "LongestWord" and "DictionaryTest", these were additional programs written to help test the dictionary "american-english" for edge cases that the main program would have to deal with.



A3Checker instructions:
	Description: A program that tests spelling mistakes of given files to given dictionaries concurrently through threading.

	Notes:  On termination from main menu, any threads that are looping through: their given dictionary, or their given file
			will stop after completing their current word, and terminate.
		Any threads that have completed, but are still waiting to print, will be allowed to terminate normally: after printing.
		Threads that terminated normally will only display their content when on the main menu,
			and they will restart the main menu after printing. This will cause the termination results to move upward.
		The top 5 mistakes hold (N/A, 0) by default.
		Does on occasion have difficulty with one letter words in american-english. (specifically 'a', not 'I').

	Considerations: The maximum allowed word size (and filename size) is 100.
				This can be modified by adjusting the constant MAX_WORD_LENGTH
			The program was designed to take a flexible amount of concurrent threads (held in an array that could be realloc'd)
				This does not always execute smoothly, therefore it is given the initial size of 16 threads. (should be plenty).
				This can be changed however by modifying the constant DEFAULT_THREAD_COUNT

	Arguments: No command-line arguments are needed.
		   The main function is responsible for creating new threads using input given at run-time.
	
	Sample output for successful run:
		$./A3Checker
		1. Start a new spellchecking task
		2. Exit

		1
		Please enter the name of the file:
		test.txt
		Please enter the name of the dictionary:
		american-english
		Are you sure you want to check: test.txt with the dictionary american-english?
		(y/n)?
		y
		1. Start a new spellchecking task
		2. Exit

		Thread completed successfully for file: test.txt, dictionary: american-english
		Top 5 mistakes:
		neccessary: necessary, 3
		zigotez: bigoted, 2
		bein: bean, 2
		heppiness: happiness, 1
		h3ppy: happy, 1
		1. Start a new spellchecking task
		2. Exit

		2
		Are you sure you want to exit the program before all tasks have been completed?
		(y/n)?
		y
		closing threads.
		Have a good day!
