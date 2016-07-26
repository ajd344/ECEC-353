/* Reference code that searches files for a user-specified string. 
 * 
 * Author: Naga Kandasamy kandasamy
 * Date: 7/19/2015
 *
 * Compile the code as follows: gcc -o mini_grep min_grep.c queue_utils.c -std=c99 -lpthread - Wall
 *
*/

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include "queue.h"

int serialSearch(char **);
int parallelSearchStatic(char **);
int parallelSearchDynamic(char **);

int 
main(int argc, char** argv)
{
          if(argc < 5){
                     printf("\n %s <search string> <path> <num threads> static (or)\
                                          %s <search string> <path> <num threads> dynamic \n", argv[0], argv[0]);
                     exit(EXIT_SUCCESS);
          }

         
          int num_occurrences;

          struct timeval start, stop;                                           /* Start the timer. */
          gettimeofday(&start, NULL);

          num_occurrences = serialSearch(argv);     /* Perform a serial search of the file system. */
          printf("\n The string %s was found %d times within the file system.", argv[1], num_occurrences);

          gettimeofday(&stop, NULL);
          printf("\n Overall execution time = %fs.", \
                                (float)(stop.tv_sec - start.tv_sec + (stop.tv_usec - start.tv_usec)/(float)1000000));


          /* Perform a multi-threaded search of the file system. */
          if(strcmp(argv[4], "static") == 0){
                     printf("\n Performing multi-threaded search using static load balancing.");
                     num_occurrences = parallelSearchStatic(argv);
                     printf("\n The string %s was found %d times within the file system.", argv[1], num_occurrences);

          }
          else if(strcmp(argv[4], "dynamic") == 0){
                     printf("\n Performing multi-threaded search using dynamic load balancing.");
                     num_occurrences = parallelSearchDynamic(argv);
                     printf("\n The string %s was found %d times within the file system.", argv[1], num_occurrences);

          }
          else{
                     printf("\n Unknown load balancing option provided. Defaulting to static load balancing.");
                     num_occurrences = parallelSearchStatic(argv);
                     printf("\n The string %s was found %d times within the \file system.", argv[1], num_occurrences);

          }
 

          printf("\n");
          exit(EXIT_SUCCESS);
}


/**
 * Function for handling a directory.
 *
 * Returns:
 *  0 for success
 *  -1 on unable to open directory
 */
int handle_dir(queue_element_t* element, queue_t* queue, struct dirent* entry){
    int status;
    struct dirent* result = NULL;
    queue_element_t* new_element;
    printf("%s is a directory. \n", element->path_name);
    DIR* directory = opendir(element->path_name);
    if(directory == NULL){
        printf("Unable to open directory %s \n", element->path_name);
        return -1;
    }
    while(1){
        status = readdir_r(directory, entry, &result); /* Read directory entry. */
        if(status != 0){
        printf("Unable to read directory %s \n", element->path_name);
        break;
        }
        if(result == NULL)                /* End of directory. */
        break;                                           
              
        if(strcmp(entry->d_name, ".") == 0)   /* Ignore the "." and ".." entries. */
        continue;
        if(strcmp(entry->d_name, "..") == 0)
        continue;

        /* Insert this directory entry in the queue. */
        new_element = (queue_element_t *)malloc(sizeof(queue_element_t));
        if(new_element == NULL){
        printf("Error allocating memory. Exiting. \n");
        exit(-1);
        }
        /* Construct the full path name for the directory item stored in entry. */
        strcpy(new_element->path_name, element->path_name);
        strcat(new_element->path_name, "/");
        strcat(new_element->path_name, entry->d_name);
        insertElement(queue, new_element);
    }
    closedir(directory);
    return 0;
}

/**
 * Function for handling a file.
 *
 * Returns:
 *  Number of occurances found (non-negative)
 *  -1 on unable to open file
 */
int handle_file(queue_element_t* element, char* search_string){
    printf("%s is a regular file. \n", element->path_name);
    FILE *file_to_search;
    char buffer[MAX_LENGTH];
    char *bufptr, *searchptr, *tokenptr;
    int num_occurrences = 0;

    /* Search the file for the search string provided as the command-line argument. */ 
    file_to_search = fopen(element->path_name, "r");
    if(file_to_search == NULL){
      printf("Unable to open file %s \n", element->path_name);
      return -1;
    } 
    else{
      while(1){
                 bufptr = fgets(buffer, sizeof(buffer), file_to_search);        /* Read in a line from the file. */
                 if(bufptr == NULL){
                            if(feof(file_to_search)) break;
                            if(ferror(file_to_search)){
                                      printf("Error reading file %s \n", element->path_name);
                                      break;
                            }
                 }
                 
                 /* Break up line into tokens and search each token. */ 
                 tokenptr = strtok(buffer, " ,.-");
                 while(tokenptr != NULL){
                            searchptr = strstr(tokenptr, search_string);
                            if(searchptr != NULL){
                                      printf("Found string %s within \
                                                 file %s. \n", search_string, element->path_name);
                                      num_occurrences ++;
                            }
                            
                            tokenptr = strtok(NULL, " ,.-");                        /* Get next token from the line. */
                 }
      }
    }
    fclose(file_to_search);
    return num_occurrences;
}

/**
 * Function for handling an element form the queue.
 *
 * Returns:
 *  Number of occurances found.
 *  Returns 0 on some error (int this case, occurances could not be found).
 */
int handle_element(queue_element_t* element, queue_t* queue, struct dirent* entry, char* search_string){
    struct stat file_stats;
    int num_occurrences = 0;
    int status = lstat(element->path_name, &file_stats); /* Obtain information about the file. */
    if(status == -1){
        printf("Error obtaining stats for %s \n", element->path_name);
        free((void *)element);
        return -1;
    }

    if(S_ISLNK(file_stats.st_mode)){ /* Ignore symbolic links. */
    } 
    else if(S_ISDIR(file_stats.st_mode)){ /* If directory, descend in and post work to queue. */
        status = handle_dir(element, queue, entry);
        if (status == -1){
            return 0;
        }
    } 
    else if(S_ISREG(file_stats.st_mode)){      /* Directory entry is a regular file. */
        int additional_occurances = handle_file(element, search_string);
        if (additional_occurances == -1){
            return 0;
        }
        num_occurrences += additional_occurances;
    }
    else{
        printf("%s is of type other. \n", element->path_name);
    }
    free((void *)element);
    return num_occurrences;
}


int                         /* Serial search of the file system starting from the specified path name. */
serialSearch(char **argv)           
{
          int num_occurrences = 0;
          queue_element_t *element;
          struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH);

          queue_t *queue = createQueue();               /* Create and initialize the queue data structure. */
          element = (queue_element_t *)malloc(sizeof(queue_element_t));
          if(element == NULL){
                     perror("malloc");
                     exit(EXIT_FAILURE);
          }
          strcpy(element->path_name, argv[2]);
          element->next = NULL;
          insertElement(queue, element);                            /* Insert the initial path name into the queue. */

          while(queue->head != NULL){                                   /* While there is work in the queue, process it. */
                     queue_element_t *element = removeElement(queue);
                     num_occurrences += handle_element(element, queue, entry, argv[1]);
          }

          return num_occurrences;
}

int                             /* Parallel search with static load balancing accross threads. */
parallelSearchStatic(char **argv)
{
          int num_occurrences = 0;

          return num_occurrences;
}


/**
 * Checks if any threads are running in the array of thread statuses.
 */
int thread_is_working(int num_threads, int* thread_status){
    for (int i = 0; i < num_threads; i++){
        if (thread_status[i]){
            return 1;
        }
    }
    return 0;
}

/**
 * Returns the index of the first available thread.
 * Returns -1 if none are available.
 */
int first_available_thread(int num_threads, int* thread_status){
    for (int i = 0; i < num_threads; i++){
        if (!thread_status[i]){
            return i;
        }
    }
    return -1;
}


int                             /* Parallel search with dynamic load balancing. */
parallelSearchDynamic(char **argv)
{
          int num_occurrences = 0;

          // Get the number of threads
          int num_threads = atoi(argv[3]);

          // Keep track of which threads are done
          pthread_t threads[num_threads];
          int thread_status[num_threads];  // 0: not running, 1: running
          for (int i = 0; i < num_threads; i++){
              thread_status[i] = 0;
          }

          // Create the queue
          queue_element_t *element;
          struct dirent *entry = (struct dirent *)malloc(sizeof(struct dirent) + MAX_LENGTH);

          queue_t *queue = createQueue();               /* Create and initialize the queue data structure. */
          element = (queue_element_t *)malloc(sizeof(queue_element_t));
          if(element == NULL){
                     perror("malloc");
                     exit(EXIT_FAILURE);
          }
          strcpy(element->path_name, argv[2]);
          element->next = NULL;
          insertElement(queue, element);                            /* Insert the initial path name into the queue. */


          while(queue->head != NULL || thread_is_working(num_threads, thread_status)){                                   /* While there is work in the queue, process it. */
                     queue_element_t *element = removeElement(queue);

                     // Create new thread
                     int open_thread = first_available_thread(num_threads, thread_status);
                     if (open_thread == -1){
                         // No available threads
                         continue;
                     }

                     // Create new thread
                     
                     //pthread_create(&threads[i], NULL, , );
                     //thread_status[open_thread] = 1;
          }

          return num_occurrences;
}

