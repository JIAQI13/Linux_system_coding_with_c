#define _POSIX_C_SOURCE 199309L // sigaction

#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <strings.h>
#include <string.h>
#include <sys/dir.h>
#include <stdint.h>
#include <stdbool.h>
#include "repoutil.h"

#define MAX_BUFF_SIZE 10000
#define MAX_MSG 10000

/********************************************************************************/

int daemonFlag = 1;   // 1 = on, 0 = off
bool thread_on = true;
char clientMessage[MAX_BUFF_SIZE];
char serverMessage[MAX_BUFF_SIZE];

char *XMLrepo[26144]; // Assuming max 2^16 hashes each with 4 name aliases
char repo_dir[100];   // path to .dedup; set in main()

/* SIGNAL HANDLER */
/* Intercepts a SIGTERM in order to write the current repository (XMLrepo) */
/* into an XML .dedup file where the server is started */
void handler(int signum) {
  if (signum == SIGTERM) {
    // do not create dedup XML file if there is nothing to write
    if (get_repo_len(XMLrepo) == 0) {
      daemonFlag = 0;
      thread_on = false;
      return;
    }
    
    xmlTextWriterPtr writer;
    char dedup_dir[200];
    strcpy(dedup_dir, repo_dir);
    strcat(dedup_dir, "/.dedup");

    writer = xmlNewTextWriterFilename(dedup_dir, 0);
    xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
    xmlTextWriterStartElement(writer, BAD_CAST "repository"); // Root

    int repo_itr = 0;
    while(XMLrepo[repo_itr]) {
      if (strlen(XMLrepo[repo_itr]) == 32) {
	// For each hash...
	xmlTextWriterStartElement(writer, BAD_CAST "file");
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "hashname", "%s", XMLrepo[repo_itr]);
	repo_itr++;
	do {
	  // Write all of its alias files
	  xmlTextWriterWriteFormatElement(writer, BAD_CAST "knownas", "%s", XMLrepo[repo_itr]);
	  repo_itr++;
	} while(XMLrepo[repo_itr] && strlen(XMLrepo[repo_itr]) != 32);
	xmlTextWriterEndElement(writer); // Close <file>
      }
    }
    xmlCleanupParser();
    xmlTextWriterEndDocument(writer);
    xmlFreeTextWriter(writer);
    
    daemonFlag = 0; // Turn off infinite loops
    thread_on = false;
  }
}


/* Receives a client message in the appropriate protocol and manipulates */
/* internal data to accomplish the requested task */
void task(char *recv, int newSocket) {
  char taskCode = recv[0];     // Byte 1 is always the task ID
  char rbuff[MAX_BUFF_SIZE];   // Construct a receiving buffer
  int rbuffsize = 0;           // Housekeeping of the rbuff
  memset(rbuff, '\0', MAX_BUFF_SIZE);
  
  if (taskCode == 0x00) {
    /***************************************************************/
    // LIST
    /***************************************************************/
    rbuff[0] = 0x01;
    rbuffsize = 3; // Minimum rbuffsize is 3
    uint16_t numFiles = 0;
    int concat_offset = 0;
    
    for (int i=0; XMLrepo[i]; i++ ) {
      if (strlen(XMLrepo[i]) > 31) // Skip file contents (MD5 hashes)
	continue;
      // For each alias...
      numFiles++;
      strcat(rbuff+3+concat_offset, XMLrepo[i]); // +3 to skip 0x01 and N
      concat_offset += (strlen(XMLrepo[i])+1);   // +1 for '\0'
      rbuffsize += strlen(XMLrepo[i])+1;         // Increment rbuff size
    }
    // Set bytes 1 and 2
    rbuff[1] = (numFiles>>8) & 0xff;
    rbuff[2] = numFiles & 0xff;

    
  } else if (taskCode == 0x02) {
    /***************************************************************/
    // UPLOAD
    /***************************************************************/
    rbuff[0] = 0x03;
    rbuffsize = 1; // rbuff is always size one byte

    // Send error message if uploading a file that already exists
    int repo_itr = 0;
    while (XMLrepo[repo_itr]) {
      if (strcmp(XMLrepo[repo_itr++], recv+1) == 0) {
	rbuff[0] = 0xFF;
	char* error_msg = "SERROR file already exists";
	strcpy(rbuff+1, error_msg);
	rbuffsize += strlen(error_msg) + 1;
	goto end_server_send; // Jump to end -- do not upload the file
      }
    }

    // Get file length, file contents, and hash
    char* filename;
    filename = recv+1; // ptr to the first \0 terminated string

    // Construct a file_length integer from a char[4] array
    int offset = 2+strlen(filename); // bytes offset to the size of upload
    int file_length = 0;
    for (int i=0; i<4; i++)
      file_length |= (int)recv[offset+i] << (24-i*8);

    // Get the MD5 hash of the file contents
    char hash[33];
    strcpy(hash, md5hash(recv+offset+4, file_length));

    // Check if file contents (hash) already exist on the server
    repo_itr = 0;
    bool contents_exist = false;
    while (XMLrepo[repo_itr]) {
      if (strcmp(XMLrepo[repo_itr++], hash) == 0) {
	// If hash exists, insert upload as an alias
	contents_exist = true;
	insert_alias(XMLrepo, hash, filename);
	break;
      }
    }

    // If hash does not exist, append it to the end of .dedup
    // As well as the first alias of that hash
    if (!contents_exist) {
      char* file_contents;
      file_contents = recv+6+strlen(filename);
      char hash_file_loc[200];
      strcpy(hash_file_loc, repo_dir);
      strcat(hash_file_loc, "/");
      strcat(hash_file_loc, hash);
      insert_new_file_contents(XMLrepo, file_contents, file_length, filename);
      FILE *fp = fopen(hash_file_loc, "w+");
      fwrite(file_contents, 1, file_length, fp);
      fflush(fp);
      fclose(fp);
    }

  } else if (taskCode == 0x04) {
    /***************************************************************/
    // REMOVE
    /***************************************************************/
    rbuff[0] = 0x05;
    rbuffsize = 1;
    int repo_itr = 0;
    char filename[33];
    strcpy(filename, recv+1);

    // Find the file to delete
    while(XMLrepo[repo_itr]) {
      if (strcmp(XMLrepo[repo_itr], filename) == 0)
	break;
      repo_itr += 1;
    }

    // Prompt an error if deleting a file that doesn't exist
    if (repo_itr == get_repo_len(XMLrepo)) {
      rbuff[0] = 0xFF;
      char* error_msg = "SERROR file not found";
      strcpy(rbuff+1, error_msg);
      rbuffsize += strlen(error_msg) + 1;
      goto end_server_send;
    }
    
    // Find the index of the found alias' hash
    repo_itr -= 1;
    while(strlen(XMLrepo[repo_itr]) != 32) {
      repo_itr--;
    }

    // Get the directory of the hash file in the server repository
    char hash_dir[200];
    strcpy(hash_dir, repo_dir);
    strcat(hash_dir, "/");
    strcat(hash_dir, XMLrepo[repo_itr]);

    // Delete alias
    int unique = delete_file_alias(XMLrepo, filename);

    // If unique alias, delete also the hash file
    if (unique)
      remove(hash_dir);
	
  } else if (taskCode == 0x06) {
    /***************************************************************/
    // DOWNLOAD
    /***************************************************************/
    rbuff[0] = 0x07;
    rbuffsize = 1;
    int repo_itr = 0;
    char filename[33];
    strcpy(filename, recv+1);

    // Find the file to download
    while(XMLrepo[repo_itr]) {
      if (strcmp(XMLrepo[repo_itr], filename) == 0)
	break;
      repo_itr += 1;
    }

    // Prompt an error if downloading a file that doesn't exist
    if (repo_itr == get_repo_len(XMLrepo)) {
      rbuff[0] = 0xFF;
      char* error_msg = "SERROR file not found";
      strcpy(rbuff+1, error_msg);
      rbuffsize += strlen(error_msg) + 1;
      goto end_server_send;
    }

    // Find the index of the found alias' hash
    repo_itr -= 1;
    while(strlen(XMLrepo[repo_itr]) != 32) {
      repo_itr--;
    }

    char hash_file[200];
    strcpy(hash_file, repo_dir);
    strcat(hash_file, "/");
    strcat(hash_file, XMLrepo[repo_itr]);

    // Collect size and contents of the file from the server repository
    FILE *hfp = fopen(hash_file, "r");
    fseek(hfp, 0, SEEK_END);
    int size = ftell(hfp);
    fseek(hfp, 0, SEEK_SET);
    char* file_contents = 0;
    file_contents = malloc(size);
    if (file_contents)
      fread(file_contents, 1, size, hfp);

    // Build size array of chars
    char size_chars[4];
    for (int i=0; i<4; i++)
      rbuff[1+i] = (size>>24-i*8) & 0xFF;
    
    strcat(rbuff, size_chars);
    strcat(rbuff+5, file_contents);
    
    fclose(hfp);
    free(file_contents);
    rbuffsize += 4 + size;
    
  } else if (taskCode == 0x08) {
    /***************************************************************/
    // QUIT
    /***************************************************************/
    rbuff[0] = 0x09;
    rbuffsize=1;
  }

 end_server_send:
  // Send rbuff with its size and memset rbuff
  send(newSocket, rbuff, rbuffsize, 0);
  memset(rbuff, 0x00, MAX_BUFF_SIZE);

  // Close communication with client if quit
  if (taskCode == 0x08)
    thread_on = false;
}

// Each thread runs an infinite loop of receiving tasks
// and communicating with the server repository
void* socketThread(void *arg) {
  struct stat stat_buf;
  int read_fd;
  int newSocket = *((int *)arg);
  while(thread_on) {
    if(recv(newSocket, clientMessage, 2000, 0) == 0)
      printf("Error");
    else
      task(clientMessage, newSocket);
  }
  thread_on = true;
  close(newSocket);
  pthread_exit(NULL);
}


void constructSocketAndListen(char** argv) {
  // Source: eClass files for Assignment 2
  int serverSocket, newSocket;
  struct sockaddr_in serverAddr;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size;

  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  serverAddr.sin_family = AF_INET;

  // Set port as int(argument 2)
  int port = atoi(argv[2]);
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Localhost
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
  bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

  if(listen(serverSocket, 50) == 0)
    printf("Listening\n");
  else
    printf("Error\n");

  pthread_t tid[60];
  int i = 0;
  while(daemonFlag != 0) {
    addr_size = sizeof serverStorage;
    newSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);

    if (newSocket >= 0) {
      if(pthread_create(&tid[i], NULL, socketThread, &newSocket) != 0)
	printf("Failed to create thread\n");
      i++;
    }
    else if(newSocket < 0)
      printf("Failed to connect");
    
    if (i >= 50) {
      i = 0;
      while (i < 50)
	pthread_join(tid[i++],NULL);
      i = 0;
    }
  }
  close(serverSocket);
}

// Fills up the internal data structure of an existing .dedup file
// Arguments: String that represents the directory to .dedup
void populate_internal_repo(char* dedup_file_loc) {
  /* xmlDoc *dedupxml; */
  xmlTextReaderPtr reader;
  int ret;
  int repo_itr = 0;
  
  /* dedupxml = xmlReadFile(dedup_file_loc, NULL, 0); */
  reader = xmlReaderForFile(dedup_file_loc, NULL,
			    XML_PARSE_DTDATTR |  // default DID attributes
			    XML_PARSE_NOENT |    // substitue entities
			    XML_PARSE_NOBLANKS); // trim whitespace
  if (reader != NULL) {
    ret = xmlTextReaderRead(reader);
    while (ret == 1) {
      const xmlChar *value;
      value = xmlTextReaderConstValue(reader);
      if (value) {
	// Create a new string of max length 32 + \0
	XMLrepo[repo_itr] = (char*) malloc(33);
	strcpy(XMLrepo[repo_itr++], value);
      }
      ret = xmlTextReaderRead(reader);
    }
  }
}

int main(int argc, char** argv) {
  // Setup daemon process
  pid_t process_id = 0;
  pid_t sid = 0;

  process_id = fork();
  if (process_id < 0) {
    printf("fork failed!\n");
    exit(1);
  }

  if (process_id > 0) {
    printf("process_id of child process %d \n", process_id);
    exit(0);
  }
  umask(0);
  sid = setsid();
  if(sid < 0)
    exit(1);

  chdir("./"); // Redundant, but preserves given code from eClass
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  
  // Initilize sighandler as well as internal repository structure
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  // Set repository directory as argument 1
  FILE *dedupfp;
  char dedup_dir[150] = {0};
  strcpy(repo_dir, argv[1]);
  strcat(dedup_dir, repo_dir);
  strcat(dedup_dir, "/.dedup");
  if (dedupfp = fopen(dedup_dir, "r")) {
    populate_internal_repo(dedup_dir);
    fclose(dedupfp);
  }
  
  constructSocketAndListen(argv);
  return 0;
}
