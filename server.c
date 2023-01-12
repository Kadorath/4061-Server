#include "server.h"
#define PERM 0644

//Global Variables [Values Set in main()]
int queue_len           = INVALID;                              //Global integer to indicate the length of the queue
int cache_len           = INVALID;                              //Global integer to indicate the length or # of entries in the cache        
int num_worker          = INVALID;                              //Global integer to indicate the number of worker threads
int num_dispatcher      = INVALID;                              //Global integer to indicate the number of dispatcher threads      
FILE *logfile;                                                  //Global file pointer for writing to log file in worker


/* ************************ Global Hints **********************************/

int cache_to_evict = 0;                         //[Cache]           --> When using cache, how will you track which cache entry to evict from array?
int workerIndex = 0;                            //[worker()]        --> How will you track which index in the request queue to remove next?
int dispatcherIndex = 0;                        //[dispatcher()]    --> How will you know where to insert the next request received into the request queue?
int curequest= 0;                               //[multiple funct]  --> How will you update and utilize the current number of requests in the request queue?


pthread_t worker_thread[MAX_THREADS];           //[multiple funct]  --> How will you track the p_thread's that you create for workers?
pthread_t dispatcher_thread[MAX_THREADS];       //[multiple funct]  --> How will you track the p_thread's that you create for dispatchers?
int threadID[MAX_THREADS];                      //[multiple funct]  --> Might be helpful to track the ID's of your threads in a global array


pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;        //What kind of locks will you need to make everything thread safe? [Hint you need multiple]
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t some_content = PTHREAD_COND_INITIALIZER;  //What kind of CVs will you need  (i.e. queue full, queue empty) [Hint you need multiple]
pthread_cond_t free_space = PTHREAD_COND_INITIALIZER;
request_t req_entries[MAX_QUEUE_LEN];                    //How will you track the requests globally between threads? How will you ensure this is thread safe?
int req_free_ind = 0;
int req_first_ind = 0;

cache_entry_t* cache;                                    // Cache

/**********************************************************************************/


/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGGESTION. FEEL FREE TO MODIFY AS NEEDED
*/


/* ******************************** Cache Code  ***********************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  for(int i = 0; i < cache_len; i ++){
    if(strcmp(cache[i].request, request) == 0){
      return i;
    }
  }
  return INVALID;
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory , int memory_size){
  cache[cache_to_evict].len = memory_size;

  free(cache[cache_to_evict].content);

  cache[cache_to_evict].content = malloc(sizeof(char) * memory_size);
  memcpy(cache[cache_to_evict].content, memory, memory_size);
  
  free(cache[cache_to_evict].request);
  cache[cache_to_evict].request = malloc(sizeof(char) * (strlen(mybuf)+1));
  strcpy(cache[cache_to_evict].request, mybuf);
  cache_to_evict ++;
  cache_to_evict %= cache_len;
}

// Function to clear the memory allocated to the cache
void deleteCache(){
  for(int i = 0; i < cache_len; i++){
    free(&cache[i].request);
    free(&cache[i].content);
    free(&cache[i]);
  }

  free(cache);
}

// Function to initialize the cache
void initCache(){
  cache = (cache_entry_t *)malloc(sizeof(cache_entry_t) * cache_len);
 
  for(int i = 0; i < cache_len; i ++){
    cache[i].content = (char *)malloc(1);
    memcpy(cache[i].content, "\0", 1);

    cache[i].request = (char *)malloc(1);
    memcpy(cache[i].request, "\0", 1);
  }
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char *mybuf) {
  char *dot_ind = strchr(mybuf, '.');
  if(dot_ind == NULL){
    fprintf(stderr, "'.' not found in file path: %s", mybuf);
    return NULL;
  }

  char extension[6];
  strcpy(extension, dot_ind);
  if(strcmp(extension, ".html") == 0 || strcmp(extension, ".htm") == 0){
    return "text/html";
  }
  else if(strcmp(extension, ".jpg") == 0){
    return "image/jpg";
  }
  else if(strcmp(extension, ".gif") == 0){
    return "image/gif";
  }
  else{
    return "text/plain";
  }

  //If we get here, I have no idea what happened.
  return NULL;
}

// Function to open and read the file from the disk into the memory. Add necessary arguments as needed
// Hint: caller must malloc the memory space
int readFromDisk(int fd, char *mybuf, void **memory) {
  // Description: Try and open requested file, return INVALID if you cannot meaning error
  FILE *fp;
  if((fp = fopen(mybuf, "r")) == NULL){
    return INVALID;
  }

  struct stat file_stat;
  if(fstat(fileno(fp), &file_stat) == -1){
    perror("fstat failed\n");
    return INVALID;
  }
  off_t file_size = file_stat.st_size;

  *memory = malloc(file_size);
  fread(*memory, file_size, 1, fp);

  fclose(fp);

  return file_size;
}

/**********************************************************************************/

// Function to receive the path)request from the client and add to the queue
void * dispatch(void *arg) {

  int id = *(int *)arg;
  printf("Dispatcher                     [  %d] Started\n", id);

  // Dispatcher thread main loop
  while (1) {
    request_t tempreq;
    tempreq.request = (char *)malloc(BUFF_SIZE);

    tempreq.fd = accept_connection();
    
    get_request(tempreq.fd, tempreq.request);

    //fprintf(stderr, "Dispatcher Received Request: fd[%d] request[%s]\n", tempreq.fd, tempreq.request);

        //(1) Copy the filename from get_request into allocated memory to put on request queue
        
        //(2) Request thread safe access to the request queue
        pthread_mutex_lock(&lock);
        //(3) Check for a full queue... wait for an empty one which is signaled from req_queue_notfull
        while(curequest >= queue_len){
          printf("Queue full\n");
          pthread_cond_wait(&free_space, &lock);
        }

        //(4) Insert the request into the queue
        req_entries[req_free_ind] = tempreq;
        //(5) Update the queue index in a circular fashion
        curequest ++;
        req_free_ind ++;
        req_free_ind %= queue_len;
        //(6) Release the lock on the request queue and signal that the queue is not empty anymore
        pthread_mutex_unlock(&lock);
        pthread_cond_signal(&some_content);
  }
  return NULL;
}

/**********************************************************************************/
// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {


  int num_request = 0;                                    //Integer for tracking each request for printing into the log
  bool cache_hit  = false;                                //Boolean flag for tracking cache hits or misses if doing 
  int filesize    = 0;                                    //Integer for holding the file size returned from readFromDisk or the cache
  void *memory    = NULL;                                 //memory pointer where contents being requested are read and stored
  int fd          = INVALID;                              //Integer to hold the file descriptor of incoming request
  char mybuf[BUFF_SIZE];                                  //String to hold the file path from the request

  int id = *(int *)arg;
  printf("Worker                         [  %d] Started\n", id);

  // Worker thread main loop
  while (1) {
          //(1) Request thread safe access to the request queue by getting the req_queue_mutex lock
          pthread_mutex_lock(&lock);
          //(2) While the request queue is empty conditionally wait for the request queue lock once the not empty signal is raised

          while(curequest == 0){
            pthread_cond_wait(&some_content, &lock);
          }

          num_request ++;
          //(3) Now that you have the lock AND the queue is not empty, read from the request queue
          fd = req_entries[req_first_ind].fd;
          for(int i = 0; i < BUFF_SIZE; i ++){
            mybuf[i] = req_entries[req_first_ind].request[i];
            if(req_entries[req_first_ind].request[i] == '\0'){
              break;
            }
          }
          free(req_entries[req_first_ind].request);

          //(4) Update the request queue remove index in a circular fashion
          curequest --;
          req_first_ind ++;
          req_first_ind %= queue_len;  
          //(5) Check for a path with only a "/" if that is the case add index.html to it
          if(mybuf[0] == '/' && mybuf[1] == '\0'){
            strcpy(mybuf, "/index.html");
          }

          //(6) Fire the request queue not full signal to indicate the queue has a slot opened up and release the request queue lock
          pthread_cond_signal(&free_space);
          pthread_mutex_unlock(&lock);

    pthread_mutex_lock(&cache_lock);
    int cacheInd = getCacheIndex(mybuf);

    if(cacheInd != INVALID){ 
      cache_hit = true;
      filesize = cache[cacheInd].len;
      memory = malloc(filesize);

      memcpy(memory, cache[cacheInd].content, filesize);
    }
    else{
      cache_hit = false;
      //Prepending a '.' to the requested file path
      char filepath[BUFF_SIZE+1];
      filepath[0] = '.';
      strcpy(&filepath[1], mybuf);

      filesize = readFromDisk(fd, filepath, &memory);
      if(filesize == INVALID){
        return_error(fd, "404 Not Found\n");
        LogPrettyPrint(NULL, id, num_request, fd, mybuf, filesize, cache_hit);

        pthread_mutex_lock(&log_lock);
        LogPrettyPrint(logfile, id, num_request, fd, mybuf, filesize, cache_hit);
        pthread_mutex_unlock(&log_lock);
        
        continue;
      }
      addIntoCache(mybuf, memory, filesize);
    }
    pthread_mutex_unlock(&cache_lock);

    LogPrettyPrint(NULL, id, num_request, fd, mybuf, filesize, cache_hit);
    
    pthread_mutex_lock(&log_lock);
    LogPrettyPrint(logfile, id, num_request, fd, mybuf, filesize, cache_hit);
    pthread_mutex_unlock(&log_lock);

    return_result(fd, getContentType(mybuf), memory, filesize);

    free(memory);
  }




  return NULL;

}

/**********************************************************************************/

int main(int argc, char **argv) {

  if(argc != 7){
    printf("usage: %s port path num_dispatcher num_workers queue_length cache_size\n", argv[0]);
    return -1;
  }


  int port            = -1;
  char path[PATH_MAX] = "no path set\0";
  num_dispatcher      = -1;                               //global variable
  num_worker          = -1;                               //global variable
  queue_len           = -1;                               //global variable
  cache_len           = -1;                               //global variable

  port           = atoi(argv[1]);
  num_dispatcher = atoi(argv[3]);
  num_worker     = atoi(argv[4]);
  queue_len      = atoi(argv[5]);
  cache_len      = atoi(argv[6]);

  for(int i = 0; i < PATH_MAX; i++){
    path[i] = argv[2][i];
    if(argv[2][i] == '\0'){
      break;
    }
  }

  if(port < MIN_PORT || port > MAX_PORT){
    perror("port is out of range\n");
    return -1;
  }

  if(num_dispatcher < 1 || num_dispatcher > MAX_THREADS){
    perror("num_dispatcher is an invalid value\n");
    return -1;
  }
  if(num_worker < 1 || num_worker > MAX_THREADS){
    perror("num_worker is an invalid value\n");
    return -1;
  }
  if(queue_len < 1 || queue_len > MAX_QUEUE_LEN){
    perror("queue_len is an invalid value\n");
    return -1;
  }
  if(cache_len < 1 || cache_len > MAX_CE){
    perror("cache_size is an invalid value\n");
    return -1;
  }

  printf("Arguments Verified:\n\
    Port:           [%d]\n\
    Path:           [%s]\n\
    num_dispatcher: [%d]\n\
    num_workers:    [%d]\n\
    queue_length:   [%d]\n\
    cache_size:     [%d]\n\n", port, path, num_dispatcher, num_worker, queue_len, cache_len);

  logfile = fopen("web_server_log", "w");

  if(chdir(path) == -1){
    perror("Server root directory not found\n");
    return -1;
  };

  initCache();

  init(port);

  for(dispatcherIndex = 0; dispatcherIndex < num_dispatcher; dispatcherIndex++){

    if(pthread_create(&dispatcher_thread[dispatcherIndex],
       NULL,
       dispatch,
       (void *)&threadID[dispatcherIndex+workerIndex]) != 0){
          perror("Failed to create dispatcher thread\n");
          return -1;
    }

    threadID[dispatcherIndex+workerIndex] = dispatcherIndex;
  }

  for(workerIndex = 0; workerIndex < num_worker; workerIndex++){

    if(pthread_create(&worker_thread[workerIndex],
        NULL,
        worker,
        (void *)&threadID[dispatcherIndex+workerIndex]) != 0){
          perror("Failed to create worker thread\n");
          return -1;
    }

    threadID[dispatcherIndex+workerIndex] = workerIndex;
  }

  // Wait for each of the threads to complete their work
  // Threads (if created) will not exit (see while loop), but this keeps main from exiting
  int i;
  for(i = 0; i < num_worker; i++){
    fprintf(stderr, "JOINING WORKER %d \n",i);
    if((pthread_join(worker_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join worker thread %d.\n", i);
    }
  }
  for(i = 0; i < num_dispatcher; i++){
    fprintf(stderr, "JOINING DISPATCHER %d \n",i);
    if((pthread_join(dispatcher_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join dispatcher thread %d.\n", i);
    }
  }
  fprintf(stderr, "SERVER DONE \n");  // will never be reached in SOLUTION
}

