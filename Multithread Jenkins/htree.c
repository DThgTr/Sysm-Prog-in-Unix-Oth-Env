#include <stdio.h>     
#include <stdlib.h>   
#include <stdint.h>  
#include <inttypes.h>  
#include <errno.h>     // for EINTR
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "common.h"

// Struct to hold data across threads
typedef struct ThreadData {
    uint8_t *fileArr;
    int numThread;
    int threadCode;
    uint32_t numBlock;
} ThreadData;

// Print out the usage of the program and exit.
void Usage(char*);
// Jenkin hash
uint32_t jenkins_one_at_a_time_hash(const uint8_t* , uint64_t );
// Binary thread tree
void *hashTree(void *);
// Get current time
double GetTime();

// block size
#define BSIZE 4096

int 
main(int argc, char** argv) 
{
    int32_t fd;
    uint32_t nblocks;

    // input checking 
    if (argc != 3)
    Usage(argv[0]);
    
    // open input file
    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
    perror("open failed");
    exit(EXIT_FAILURE);
    }
    //========================================PROCESS FILE========================================
    //---------------------CALCULATE BLOCKS TO PROCESS---------------------
    // use fstat to get file size
    struct stat st; 
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }
    // calculate nblocks
    nblocks = st.st_size / (BSIZE*atoi(argv[2]));
    
    printf("no. of blocks = %u \n", nblocks);
    //----------------------------HASH BLOCKS-----------------------------
    double start = GetTime();   // Get hash start time
    
    //-------------Start hash------------
    // calculate hash value of the input file
    // Map file to memory
    uint8_t *fileArr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (fileArr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Create root thread
    char *hashString; // Hash string
    ThreadData data = {fileArr, atoi(argv[2]), 0, nblocks};
    pthread_t p1;
	pthread_create(&p1, NULL, hashTree, &data);
	pthread_join(p1, (void **) &hashString);
    //-------------End hash---------------
    
    double end = GetTime();     // Get hash end time
    
    //Print hash value & time
    unsigned int hash = (unsigned int)strtoul(hashString, NULL, 10);
    printf("hash value = %u \n", hash);
    printf("time taken = %f \n", (end - start));

    free(hashString);
    close(fd);
    return EXIT_SUCCESS;
}
//=====================HASH TREE================
void 
*hashTree(void *arg) {
	pthread_t p1, p2;
	
	uint32_t jkHashResult; // holder for jenkins hash result
	ThreadData* data = (ThreadData*) arg; // Get parameter data
    
    // Check if the number of threads is overstepping the thread limit
	if (data->threadCode >= data->numThread) { 
	    // Return an empty string and exit if the thead code passed the maximum number of threads
	    char* retString = (char*)malloc(sizeof(char));
        *retString = '\0';
        pthread_exit((void *) retString);
	}
	//------------------------------------HASH ASSIGNED BLOCK---------------------------------------
    char hashSelf[11];   // Max number of digits of a uint32_t is 10, the extra place is for \0
    //int numBlock = data->nblocks/data->ht;
    uint8_t *addr = data->fileArr;
    jkHashResult = jenkins_one_at_a_time_hash(&addr[(data->threadCode)*(data->numBlock)*BSIZE], (uint64_t)(data->numBlock)*BSIZE);
    sprintf(hashSelf, "%u", jkHashResult);
	
	//------------------------------------BINARY TREE OF THREADS---------------------------------------
	// Binary tree of threads creation
	if (data->threadCode >= (data->numThread/2) || (data->numThread == 1)) {
	    //++++++LEAF THREAD++++++
	    // Allocate enough memory to hold hashSelf
	    char *hashString = malloc(sizeof(char) * (strlen(hashSelf) + 1));
        strcpy(hashString, hashSelf);
        
        // Print processes
        printf("Leaf Thread==========================\n");
        printf("tnum %d hash computed %u\n", data->threadCode, jkHashResult);
        printf("tnum %d hash sent to parent %s\n", data->threadCode, hashString);
        // Return hash string and exit thread
        pthread_exit((void *) hashString);
	}
	else {
	    //++++++INTERIOR THREAD++++++
	    //-------------Thread creations and post creations--------------
	    ThreadData childDataLeft = {data->fileArr, data->numThread, 2*data->threadCode + 1, data->numBlock};
	    ThreadData childDataRight = {data->fileArr, data->numThread, 2*data->threadCode + 2, data->numBlock};

	    // Creating threads
		pthread_create(&p1, NULL, hashTree, &childDataLeft);
		pthread_create(&p2, NULL, hashTree, &childDataRight);
		
		// Wait for child threads to finish and get return strings
	    char* retStrLeft;
	    char* retStrRight;
    	pthread_join(p1, (void **) &retStrLeft);
    	pthread_join(p2, (void **) &retStrRight);
    	
        //--------------Process return strings from threads--------------	
    	// Allocate enough memory to hold return strings from left, right child and hashSelf
        char hashString[strlen(hashSelf) + strlen(retStrLeft) + strlen(retStrRight) + 1];
    	strcat(hashString, hashSelf); // Concate hashSelf
        strcat(hashString, retStrLeft); // Concate return string from left child thread
        strcat(hashString, retStrRight);    // Concate return string from right child thread
        
        jkHashResult = jenkins_one_at_a_time_hash((uint8_t*) hashString, strlen(hashString));
        
        char *retHash = malloc(sizeof(char) * 11);
        sprintf(retHash, "%u", jkHashResult);
        // Print processes
        printf("Parent Thread==========================\n");
        printf("tnum %d hash computed: %s\n", data->threadCode, hashSelf);  // Self hash
        
        printf("tnum %d hash from left child %s\n", data->threadCode, retStrLeft);  // Left hash
        if (strcmp(retStrRight, "") != 0)
            printf("tnum %d hash from right child %s\n", data->threadCode, retStrRight);    // Right hash

        printf("tnum %d concat string %s\n", data->threadCode, hashString);
        printf("tnum %d hash sent to parent %s\n", data->threadCode, retHash);

        //Free allocated memory from child threads
        free(retStrLeft);  
        free(retStrRight);
        // Return hash string and exit thread
        pthread_exit((void *) retHash);
	}
    return NULL;
}

//=====================JENKINS======================
uint32_t 
jenkins_one_at_a_time_hash(const uint8_t* key, uint64_t length) 
{
    uint64_t i = 0;
    uint32_t hash = 0;
    
    while (i != length) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}
//====================ERROR MESSAGE===============
void 
Usage(char* s) 
{
    fprintf(stderr, "Usage: %s filename num_threads \n", s);
    exit(EXIT_FAILURE);
}
