/*
Created by: John Denbutter
November 7th, 2023
*/
#define  _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_WORD_LENGTH 100 //change for dictionary word length
#define DEFAULT_THREAD_COUNT 16 //amount of concurrent threads prepared for at compile-time

/*
Global variables
*/
static char terminationFlag = 0;
static pthread_mutex_t mutexPrintControl; //which thread has the right to print
static pthread_mutex_t mutexMenu; //to allow menu to wait for option from user
static pthread_cond_t menuCondition;
static pthread_t menuThread;
static int menuInput;
//variables that track threads
static int currentThreadsCount;
static char* activeThreads;

/*
Structs
*/
typedef struct{
    char fileName[MAX_WORD_LENGTH];
    char dictionaryName[MAX_WORD_LENGTH];
    char topMistakes[5][MAX_WORD_LENGTH];
    char topCorrection[5][MAX_WORD_LENGTH];
    int topMistakesFrequency[5];
    int mistakesCount;
    int threadIndex;

}threadArgs;

typedef struct{
    char word[MAX_WORD_LENGTH];
    int frequency;
}dictionaryWords;

/*
Function definitions
*/
void *spellCheck( void* argPtr );
int existsInDictionary( dictionaryWords* Dictionary, int dictionaryCount, char* newWord );
int evaluateLevenshtein( dictionaryWords* Dictionary, int dictionaryCount, char* newWord );
int LevenshteinDistance( char* dictWord, char* newWord );
int minimum(int a, int b, int c);
void printControl( threadArgs *fArgs );
void *getMenuInput();

/*
Functions
*/

/*
Description: The main spellchecker function piloted by threads. Saves all words from Dictionary and checks
the Levenshtein distance for each word in the file.
Input: Takes argPtr that is converted into the threadArgs struct. (Holds names of files, saves found data from Levenshtein algorithm)
Output: Does not directly output, but calls a printing function once mutex allows.
*/
void *spellCheck( void* argPtr ){
    sleep(50);
    //initialize variables for function
    threadArgs *fArgs = (threadArgs*) argPtr;
    FILE * fp;
    dictionaryWords* Dictionary;

    //variables for creating dictionary
    int currentWords = 0;
    int maxWords = 64;
    char* dictBuffer;
    size_t wordLen = 0;
    size_t maxWordLen = MAX_WORD_LENGTH;

    //variables for copying file
    char *fileData;
	long int fileSize = -1;

    //variables for tokenizing file data
    char newWord[MAX_WORD_LENGTH];
    char* fileDataWords;
    char* savePtr;

    Dictionary = (dictionaryWords*) malloc(sizeof(dictionaryWords) * maxWords); //initializes memory for 64 dictionary words of length Max_Word_Length
    dictBuffer = malloc(MAX_WORD_LENGTH);
    //populate Dictionary from dictionaryName
    if ((fp = fopen(fArgs->dictionaryName, "r")) != NULL){
        while((wordLen = getline(&dictBuffer, &maxWordLen, fp)) != -1){
            if(terminationFlag){ //in case of early termination
                //free Dictionary & close fp
                fclose(fp);
                free(Dictionary);
                free(dictBuffer);
                pthread_exit(NULL);
            }
            //printf("dictBuffer: %s", dictBuffer);
            if(currentWords + 1 == maxWords){ //make sure the dictionary has space, or double dictionary size
                //printf("Have: %d, Need: %d\n", currentWords, maxWords);
                maxWords *= 2;
                //printf("New alloc size: %d\n", maxWords);
                dictionaryWords* temp = realloc(Dictionary, (sizeof(dictionaryWords) * maxWords));
                if (temp != NULL){
                    Dictionary = temp;
                }
                else{
                    write(STDOUT_FILENO, "Error: Could not realloc Dictionary.\n", 37);
                    //free Dictionary & close fp
                    fclose(fp);
                    free(Dictionary);
                    free(dictBuffer);
                    pthread_exit(NULL);
                }
            }
            strncpy(Dictionary[currentWords].word, dictBuffer, wordLen - 1); //copy word into dictionary struct, minus '\n'
            Dictionary[currentWords].frequency = 0;
            //printf("%s, size: %ld, index: %d\n", Dictionary[currentWords].word, wordLen - 1, currentWords);
            currentWords++;
        }
        if(fp){
            fclose(fp);
            fp = NULL;
        }
        if(dictBuffer){
            free(dictBuffer);
        }
    }
    else{
        //mutex for printing
        pthread_mutex_lock(&mutexPrintControl);
        printf("Error: Could not open dictionary: %s.\nTerminating thread.\n\n", fArgs->dictionaryName);
        pthread_mutex_unlock(&mutexPrintControl);

        //free Dictionary
        free(Dictionary);
        free(dictBuffer);

        //update thread tracking variables
        currentThreadsCount--;
        activeThreads[fArgs->threadIndex] = 0;

        pthread_exit(NULL);
    }

    //copy contents of file to variable
    if ((fp = fopen(fArgs->fileName, "r")) != NULL){
        fseek(fp, 0L, SEEK_END);
		fileSize = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		fileData = malloc(fileSize + 1);
        if(!fileData){ //check successful malloc
            //mutex for printing
            pthread_mutex_lock(&mutexPrintControl);
			printf("Error: Malloc for %s unsuccessful.\nTerminating thread.\n\n", fArgs->fileName);
            pthread_mutex_unlock(&mutexPrintControl);
            
            //free Dictionary & close fp
            fclose(fp);
            free(Dictionary);
            free(fileData);

            //update thread tracking variables
            currentThreadsCount--;
            activeThreads[fArgs->threadIndex] = 0;

			pthread_exit(NULL);
		}
		if(fread(fileData, fileSize, 1, fp) < 1){ //check successful copy
            //mutex for printing
            pthread_mutex_lock(&mutexPrintControl);
			printf("Error: Could not read from file: %s.\nTerminating thread.\n\n", fArgs->fileName);
            pthread_mutex_unlock(&mutexPrintControl);

            //free Dictionary & close fp
            fclose(fp);
            free(Dictionary);
            free(fileData);

            //update thread tracking variables
            currentThreadsCount--;
            activeThreads[fArgs->threadIndex] = 0;

			pthread_exit(NULL);
		}
        if(fp){
            fclose(fp);
            fp = NULL;
        }
    }

    //tokenize & act on each word
    fileDataWords = strtok_r(fileData, " ,.-!?\n\r", &savePtr);
    while(fileDataWords){
        if(terminationFlag){ //in case of early termination
            //free Dictionary & close fp
            free(Dictionary);
            free(fileData);
            pthread_exit(NULL);
        }
        //printf("%s word\n", fileDataWords);
        strcpy(newWord, fileDataWords);
        newWord[0] = tolower(newWord[0]);
        if(!existsInDictionary(Dictionary, currentWords, newWord)){ //if new word from stream is not found in Dictionary
            int closestWordIndex = evaluateLevenshtein(Dictionary, currentWords, newWord);
            if(closestWordIndex){
                Dictionary[closestWordIndex].frequency += 1;
                //printf("works: %s, %d\n", Dictionary[closestWordIndex].word, Dictionary[closestWordIndex].frequency);
                for(int i = 0; i < 5; i++){
                    if (fArgs->topMistakesFrequency[i] < Dictionary[closestWordIndex].frequency){ //check if this is in top 5, and update
                        char tempWord1[MAX_WORD_LENGTH];
                        strcpy(tempWord1, newWord);
                        //printf("1.75: %s\n", tempWord1);
                        char tempWord2[MAX_WORD_LENGTH];
                        int tempFrequency1 = Dictionary[closestWordIndex].frequency;
                        //printf("1.9: %d\n", tempFrequency1);
                        int tempFrequency2;
                        //printf("2: %s, %d\n", fArgs->topMistakes[i], fArgs->topMistakesFrequency[i]);
                        char tempCorrection1[MAX_WORD_LENGTH];
                        strcpy(tempCorrection1, Dictionary[closestWordIndex].word);
                        char tempCorrection2[MAX_WORD_LENGTH];

                        for(int j = i; j < 5; j++){
                            if(!strcmp(newWord, fArgs->topMistakes[j])){//if newWord is found in top 5, go no further
                                fArgs->topMistakesFrequency[j] = tempFrequency1;
                                break;
                            }
                            //printf("2.5: %s, %d\n", fArgs->topMistakes[j], fArgs->topMistakesFrequency[j]);
                            strcpy(tempWord2, fArgs->topMistakes[j]); //put topMistake into temp
                            strcpy(tempCorrection2, fArgs->topCorrection[j]);
                            tempFrequency2 = fArgs->topMistakesFrequency[j];
                            //printf("3: upcoming: %s, %d\n", tempWord1, tempFrequency1);

                            strcpy(fArgs->topMistakes[j], tempWord1); //add new entry to topMistakes
                            strcpy(fArgs->topCorrection[j], tempCorrection1);
                            //printf("3.25\n");
                            fArgs->topMistakesFrequency[j] = tempFrequency1;
                            //printf("3.5: %s, %d\n", fArgs->topMistakes[j], fArgs->topMistakesFrequency[j]);

                            strcpy(tempWord1, tempWord2); //prepare temp for entry into topMistakes at place below
                            strcpy(tempCorrection1, tempCorrection2);
                            tempFrequency1 = tempFrequency2;
                            //printf("4\n");
                        }
                        break;
                    }
                }
            }
            else{
                //mutex for printing
                pthread_mutex_lock(&mutexPrintControl);
                printf("Error: Could not determine Levenshtein distance for: %s.\n", newWord);
                pthread_mutex_unlock(&mutexPrintControl);
                
                //free Dictionary
                free(Dictionary);
                free(fileData);

                //update thread tracking variables
                currentThreadsCount--;
                activeThreads[fArgs->threadIndex] = 0;

                pthread_exit(NULL);
            }
        }
        fileDataWords = strtok_r(NULL, " ,.-!?\n\r", &savePtr);
    }

    //mutex for printing
    pthread_mutex_lock(&mutexPrintControl);
    printControl(fArgs);
    pthread_mutex_unlock(&mutexPrintControl);

    //free Dictionary
    free(Dictionary);
    free(fileData);

    //update thread tracking variables
    currentThreadsCount--;
    activeThreads[fArgs->threadIndex] = 0;

    return NULL;
}

/*
Description: A helper function for threads to check if the word in file is in the saved dictionary using a binary search.
Input: The saved dictionary, the number of terms in the dictionary, and the word to check.
Output: Returns a success or failure to thread.
*/
int existsInDictionary( dictionaryWords* Dictionary, int dictionaryCount, char* newWord ){
    int bottom = 0;
    int mid;
    int top = dictionaryCount - 1;

    while(bottom <= top){
        mid = (bottom + top) / 2;
        int success = strcmp(Dictionary[mid].word, newWord);
        if(success == 0){
            return 1;
        }
        else if (success > 0){
            top = mid - 1;
        }
        else if (success < 0){
            bottom = mid + 1;
        }
    }

    return 0;
}

/*
Description: A helper function for threads to check the Levenshtein Distance for the misspelled word by calling a helper function.
Input: The saved dictionary, the number of terms in the dictionary, and the word to check.
Output: Returns the index of the closest correct word.
*/
int evaluateLevenshtein( dictionaryWords* Dictionary, int dictionaryCount, char* newWord ){
    int lowestLevenshtein = 128;
    int lowestIndex = -1;
    for(int i = 0; i < dictionaryCount; i++){
        //printf("checking: %s to %s: Levenshtein: ", Dictionary[i].word, newWord);
        int newLevenshtein = LevenshteinDistance(Dictionary[i].word, newWord);
        //printf("%d\n", newLevenshtein);
        if(lowestLevenshtein > newLevenshtein){
            lowestLevenshtein = newLevenshtein;
            lowestIndex = i;
        }
    }

    //printf("Match for %s: Lowest Levenshtein: %s, index: %d, value: %d\n", newWord, Dictionary[lowestIndex].word, lowestIndex, lowestLevenshtein);

    return lowestIndex;
}

/*
Levenshtein Distance algorithm - Iterative with full matrix similar to wiki
Description: A helper function for evaluateLevenshtein, that checks the Levenshtein Distance for the misspelled word on one Dictionary word.
Input: The saved dictionary, the number of terms in the dictionary, and the word to check.
Output: Returns their levenshtein distance.
*/
int LevenshteinDistance( char* dictWord, char* newWord ){
    int x = strlen(dictWord);
    int y = strlen(newWord);
    int array[x + 1][y + 1];
    for(int i = 1; i < x; i++){
        for(int j = 1; j < y; j++){
            array[i][j] = 0;
        }
    }

    for(int i = 0; i <= x; i++){
        array[i][0] = i;
    }
    for(int j = 0; j <= y; j++){
        array[0][j] = j;
    }
    //printf("Array %s:\n", dictWord); //for debugging: printing the array

    int substitutionCost = -1;
    for(int i = 1; i <= x; i++){
        for(int j = 1; j <= y; j++){
            if(dictWord[i - 1] == newWord[j - 1]){
                substitutionCost = 0;
            }
            else{
                substitutionCost = 1;
            }
            array[i][j] = minimum(array[i - 1][j] + 1, array[i][j - 1] + 1, array[i - 1][j - 1] + substitutionCost);
            //printf("%d ", array[i][j]); //for debugging: printing the array
        }
        //printf("\n"); //for debugging: printing the array
    }

    return array[x][y];
}

/*
Description: A simple helper function used by LevenshteinDistance that returns the lesser of three values.
Input: Three integers
Output: Returns the greater integer
*/
int minimum(int a, int b, int c){
    if(a <= b && a <= c){
        return a;
    }
    else if(b <= a && b <= c){
        return b;
    }
    else /*(c <= a && c <= b)*/{
        return c;
    }
}

/*
Description: A simple helper function used by main that returns the lowest available thread space in the array.
Input: The active thread array, and its size.
Output: Returns the index, or -1 if there are none left.
*/
int findLowestIndex( char* activeThreads, int max ){
    for(int i = 0; i < max; i++){
        if(activeThreads[i] < 1){
            return i;
        }
    }
    return -1;
}

/*
Description: A simple helper function used by main, that doubles the size of the thread variables to allow for more threads.
Input: A pointer to: the thread argument array, thread array, active thread check array, and the max size.
Output: Modifies values directly, does not return anything.
*/
void doubleThreadsSize( threadArgs *ThreadArgs, pthread_t *threadsArray, char *activeThreads, int *maxThreadCount ){
    *maxThreadCount *= 2;

    threadArgs* tempArgs = realloc(ThreadArgs, (sizeof(threadArgs) * *maxThreadCount));
    if (tempArgs != NULL){
        ThreadArgs = tempArgs;
    }
    else{
        printf("Error: Could not realloc ThreadArgs.\n");
        exit(1);
    }

    pthread_t* tempThreads = realloc(threadsArray, (sizeof(pthread_t) * *maxThreadCount));
    if (tempThreads != NULL){
        threadsArray = tempThreads;
    }
    else{
        printf("Error: Could not realloc threadsArray.\n");
        exit(1);
    }

    char* tempActive = realloc(activeThreads, (*maxThreadCount));
    if (tempActive != NULL){
        activeThreads = tempActive;
        for(int i = (*maxThreadCount / 2); i < *maxThreadCount; i++){
            activeThreads[i] = 0;
        }
    }
    else{
        printf("Error: Could not realloc activeThreads.\n");
        exit(1);
    }
}

/*
Description: A simple helper function used by threads that overrides the main menu thread and prints the thread output.
Input: The arguments of the terminating thread.
Output: Outputs directly to console, does not return anything.
*/
void printControl( threadArgs *fArgs ){
    pthread_cancel(menuThread); //cancel menu

    printf("Thread completed successfully for file: %s, dictionary: %s\nTop 5 mistakes:\n", fArgs->fileName, fArgs->dictionaryName);
    for(int i = 0; i < 5; i++){
        printf("%s: %s, %d\n", fArgs->topMistakes[i], fArgs->topCorrection[i], fArgs->topMistakesFrequency[i]);
    }

    pthread_create(&menuThread, NULL, getMenuInput, NULL); //restart menu
}

/*
Description: The menu display and check used by main that can be overruled by the threads.
Input: Does not take any input.
Output: Modifies global variable and throws a condition signal
*/
void *getMenuInput(){
    char threadBuffer[MAX_WORD_LENGTH];
    write(STDOUT_FILENO, "1. Start a new spellchecking task\n2. Exit\n\n", 43);
    /*for(int i = 0; i < 4; i++){
        printf("%c ", activeThreads[i] + '0');
    }
    printf("\n");*/
    printf("Active Threads: %d\n", currentThreadsCount);
    read(STDIN_FILENO, threadBuffer, MAX_WORD_LENGTH);
    menuInput = threadBuffer[0] - '0'; //gets number value of input character
    pthread_cond_signal(&menuCondition);

    return NULL;
}

/*
Description: The main driver. Holds backend of menu.
Input: N/A
Output: N/A
*/
int main( void ){
    //overhead: malloc data for threads & keeping track of threads. malloc'd to allow for variable size.
    threadArgs *ThreadArgs = malloc(sizeof(threadArgs) * DEFAULT_THREAD_COUNT);
    pthread_t *threadsArray = malloc(sizeof(pthread_t) * DEFAULT_THREAD_COUNT);
    activeThreads = malloc(DEFAULT_THREAD_COUNT);
    for(int i = 0; i < DEFAULT_THREAD_COUNT; i++){
        activeThreads[i] = 0;
    }

    if(pthread_mutex_init(&mutexPrintControl, NULL) != 0){ //check for failure of mutex
        write(STDOUT_FILENO, "Error: Mutex for threads failed. Terminating program.\n", 54);
        //free menu mallocs
        free(ThreadArgs);
        free(threadsArray);
        free(activeThreads);

        exit(1);
    }
    pthread_mutex_lock(&mutexPrintControl); //give menu control of printing. Halt other threads

    if(pthread_mutex_init(&mutexMenu, NULL) != 0){ //check for failure of mutex
        write(STDOUT_FILENO, "Error: Mutex for menu failed. Terminating program.\n", 51);
        //free menu mallocs
        free(ThreadArgs);
        free(threadsArray);
        free(activeThreads);

        exit(1);
    }
    pthread_mutex_lock(&mutexMenu); //give menu control of printing. Halt other threads

    if(pthread_cond_init(&menuCondition, NULL) != 0){ //check for failure of condition
        write(STDOUT_FILENO, "Error: Condition failed. Terminating program.\n", 46);
        //free menu mallocs
        free(ThreadArgs);
        free(threadsArray);
        free(activeThreads);

        exit(1);
    }

    //initialize menu variables
    currentThreadsCount = 0;
    int maxThreadCount = DEFAULT_THREAD_COUNT;
    int lowestFreeThreadIndex = 0;
    char menuBuffer[MAX_WORD_LENGTH];
    char exitConfirmationChar = 'a';
    menuInput = 0;

    //main menu loop
    do{
        pthread_create(&menuThread, NULL, getMenuInput, NULL); //create main menu thread
        pthread_mutex_unlock(&mutexPrintControl); //allow other threads to take over printing
        pthread_cond_wait(&menuCondition, &mutexMenu); //wait for menu input
        pthread_mutex_lock(&mutexPrintControl); //stop other threads from printing

        switch (menuInput){
            case 1:
            {
                //setup args
                int newThreadArgsIndex = lowestFreeThreadIndex; //just in case lowestFreeThreadIndex changes while starting a new thread
                ThreadArgs[newThreadArgsIndex].threadIndex = newThreadArgsIndex;
                int inputLength = -1;

                //file name
                write(STDOUT_FILENO, "Please enter the name of the file:\n", 35);
                inputLength = read(STDIN_FILENO, menuBuffer, MAX_WORD_LENGTH);
                strncpy(ThreadArgs[newThreadArgsIndex].fileName, menuBuffer, inputLength - 1); //exclude \n
                strcat(ThreadArgs[newThreadArgsIndex].fileName, "\0"); //add string terminator
                //printf("read: %s, %d\n", ThreadArgs[newThreadArgsIndex].fileName, inputLength - 1);

                //dictionary name
                write(STDOUT_FILENO, "Please enter the name of the dictionary:\n", 41);
                inputLength = read(STDIN_FILENO, menuBuffer, MAX_WORD_LENGTH);
                strncpy(ThreadArgs[newThreadArgsIndex].dictionaryName, menuBuffer, inputLength - 1); //exclude \n
                strcat(ThreadArgs[newThreadArgsIndex].dictionaryName, "\0"); //add string terminator
                //printf("read: %s, %d\n", ThreadArgs[newThreadArgsIndex].dictionaryName, inputLength - 1);

                ThreadArgs[newThreadArgsIndex].mistakesCount = 0;
                for(int i = 0; i < 5; i++){
                    strcpy(ThreadArgs[newThreadArgsIndex].topMistakes[i], "N/A");
                    ThreadArgs[newThreadArgsIndex].topMistakesFrequency[i] = -1; //max int value
                    //printf("%s, %d\n", ThreadArgs[newThreadArgsIndex].topMistakes[i], ThreadArgs[newThreadArgsIndex].topMistakesFrequency[i]);
                }

                //check for consent
                while(1){
                    write(STDOUT_FILENO, "Are you sure you want to check: ", 32);
                    write(STDOUT_FILENO, ThreadArgs[newThreadArgsIndex].fileName, strlen(ThreadArgs[newThreadArgsIndex].fileName));
                    write(STDOUT_FILENO, " with the dictionary: ", 22);
                    write(STDOUT_FILENO, ThreadArgs[newThreadArgsIndex].dictionaryName, strlen(ThreadArgs[newThreadArgsIndex].dictionaryName));
                    write(STDOUT_FILENO, "?\n(y/n)?\n", 9);
                    read(STDIN_FILENO, menuBuffer, MAX_WORD_LENGTH);
                    exitConfirmationChar = menuBuffer[0];
                    exitConfirmationChar = tolower(exitConfirmationChar);

                    if(exitConfirmationChar == 'y'){ //create new thread
                        currentThreadsCount++;
                        activeThreads[newThreadArgsIndex] = 1;
                        lowestFreeThreadIndex = findLowestIndex(activeThreads, maxThreadCount);
                        if(lowestFreeThreadIndex == -1){ //if all are full, double the size and try again.
                            doubleThreadsSize(ThreadArgs, threadsArray, activeThreads, &maxThreadCount);
                            lowestFreeThreadIndex = findLowestIndex(activeThreads, maxThreadCount);
                        }
                        pthread_create(&threadsArray[newThreadArgsIndex], NULL, spellCheck, (void*)&ThreadArgs[newThreadArgsIndex]);
                        break;
                    }
                    else if(exitConfirmationChar == 'n'){ //abort
                        write(STDOUT_FILENO, "Returning to menu.\n", 19);
                        break;
                    }
                    else{
                        write(STDOUT_FILENO, "Please indicate a character for either yes or no: y/n\n", 54);
                    }
                }

                break;
            }

            case 2:
            {
                while(1){
                    write(STDOUT_FILENO, "Are you sure you want to exit the program before all tasks have been completed?\n(y/n)?\n", 87);
                    read(STDIN_FILENO, menuBuffer, MAX_WORD_LENGTH);
                    exitConfirmationChar = menuBuffer[0];
                    exitConfirmationChar = tolower(exitConfirmationChar);

                    if(exitConfirmationChar == 'y'){
                        terminationFlag = 1; //signal threads to close
                        write(STDOUT_FILENO, "closing threads.\n", 17);
                        pthread_mutex_unlock(&mutexPrintControl); //allow other threads to finish their print

                        //wait for threads to finish
                        for(int i = 0; i < currentThreadsCount; i++){
                            if(activeThreads[i] == 1){
                                pthread_join(threadsArray[i], NULL);
                            }
                        }

                        //free menu mallocs
                        free(ThreadArgs);
                        free(threadsArray);
                        free(activeThreads);
                        
                        //destroy mutex & condition
                        pthread_mutex_destroy(&mutexPrintControl);
                        pthread_cond_destroy(&menuCondition);

                        write(STDOUT_FILENO, "Have a good day!\n", 17);
                        break;
                    }
                    else if(exitConfirmationChar == 'n'){
                        write(STDOUT_FILENO, "Returning to menu.\n", 19);
                        menuInput = 0;
                        break;
                    }
                    else{
                        write(STDOUT_FILENO, "Please indicate a character for either yes or no: y/n\n", 54);
                    }
                }
                break;
            }

            default:
            {
                write(STDOUT_FILENO, "Please enter a number corresponsing to an existing option:\n", 59);
                break;
            }
        }
    } while(menuInput != 2);
}
