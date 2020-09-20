#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <strings.h>

#define MAX_BUFF_SIZE 10000

/********************************************************************************/

char downloading_file[100] = {0}; // Keep the name of the file being downloaded

void* connection_handler(void *socket_desc);

// Builds the client->server protocol and sends it
int send_client_protocol(char* sbuff, int sock_desc) {
  unsigned char client_action = sbuff[0];
  char pbuff[MAX_BUFF_SIZE];
  int pbuff_size = 0;

  if (client_action == 'l') {
    /********************************************************************/
    // LIST
    /********************************************************************/
    pbuff_size = 1;
    pbuff[0] = 0x00;
    
  } else if (client_action == 'u') {
    /********************************************************************/
    // UPLOAD
    /********************************************************************/

    // Disallow files of name length of 32 because length is the
    // identifying characteristic of the internal array data structure
    pbuff[0] = 0x02;
    char* filename = sbuff+2;
    if (strlen(filename) >= 32) {
      printf("CERROR filename can't be 32 or more characters\n");
      memset(pbuff, 0x00, MAX_BUFF_SIZE);
      return -1;
    }
    
    strcat(pbuff, filename);
    
    FILE *fp;
    if(!(fp = fopen(filename, "r"))) {
      printf("CERROR file not found in local directory\n");
      memset(pbuff, 0x00, MAX_BUFF_SIZE);
      return -1;
    }

    // Get contents of the file
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek (fp, 0, SEEK_SET);
    char* file_contents = 0;
    file_contents = malloc(size);
    if (file_contents)
      fread(file_contents, 1, size, fp);

    // Build a char[4] array to be decoded into an int
    char size_chars[4];
    for (int i=0; i<4; i++)
      pbuff[2+strlen(filename)+i] = (size>>24-i*8) & 0xFF;

    strcat(pbuff+strlen(filename)+6, file_contents);
    free(file_contents);
    pbuff_size = 6+strlen(filename)+size;

  } else if (client_action == 'r') {
    /********************************************************************/
    // DELETE
    /********************************************************************/
    pbuff[0] = 0x04;
    char* filename = sbuff+2;
    strcat(pbuff, filename);
    pbuff_size = 2+strlen(filename);
    
  } else if (client_action == 'd') {
    /********************************************************************/
    // DOWNLOAD
    /********************************************************************/
    pbuff[0] = 0x06;
    char* filename = sbuff+2;
    strcpy(downloading_file, filename);
    strcat(pbuff, filename);
    pbuff_size = 2+strlen(filename);
        
  } else if (client_action == 'q') {
    pbuff_size = 1;
    pbuff[0] = 0x08;
  }
  
  send(sock_desc, pbuff, pbuff_size, 0);
  memset(pbuff, 0x00, MAX_BUFF_SIZE);
  return 0; // Successful send
}

int receive_client_protocol(char *rbuff, int sock_desc) {
  if (recv(sock_desc, rbuff, MAX_BUFF_SIZE, 0) == 0) {
    printf("Error receiving"); // -d flag?
    return -1;
  }

  unsigned char return_code = rbuff[0];
  
  // Error code 0xFF:
  if (return_code == 0xff) {
    printf("%s\n", rbuff+1);
    
  } else if (return_code == 0x01) {
    uint16_t files = ((uint16_t)rbuff[1]<<8) | (uint16_t)rbuff[2];

    int char_itr = 3;
    for (int file=0; file<files; file++) {
      if (rbuff[char_itr] == '\n') continue;
      printf("OK+ ");

      while(rbuff[char_itr] != '\0')
	printf("%c", rbuff[char_itr++]);
      char_itr++;
      printf("\n");
    }
  } else if (return_code == 0x07) {
    // Download the file received. The name of the file was
    // (downloading_file) was set in the sending function
    FILE *dfp;
    dfp = fopen(downloading_file, "w+");
    int size=0;
    for (int i=0; i<4; i++)
      size |= (int)rbuff[1+i] << (24-i*8);
    fwrite(rbuff+5, 1, size, dfp);
    fclose(dfp);
    memset(downloading_file, 0x00, 100);
    
  } else if (return_code == 0x09) { // Quit
    printf("OK\n");
    exit(0);
  }

  memset(rbuff, 0x00, MAX_BUFF_SIZE);
  switch(return_code) case 1: case 3: case 5: case 7: printf("OK\n");
  
}

int main(int argc, char** argv) {
  // Socket connecting code from eClass under Assignment 2
  int sock_desc;
  int write_fd;
  struct sockaddr_in serv_addr;
  char sbuff[MAX_BUFF_SIZE], rbuff[MAX_BUFF_SIZE];

  if((sock_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    printf("Failed creating socket\n");

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  int port = atoi(argv[2]);
  serv_addr.sin_port = htons(port);

  if (connect(sock_desc, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) < 0) {
    printf("Failed to connect to server\n");
  }

  while(1) {
    // Fetch a command and if the sending protocol can't be built
    // because of a CERROR, retry.
  getmessage:
    fgets(sbuff, MAX_BUFF_SIZE , stdin);
    sbuff[strcspn(sbuff, "\n")] = 0;
    if(send_client_protocol(sbuff,sock_desc) != 0)
      goto getmessage;
    receive_client_protocol(rbuff,sock_desc);
  }
  return 0;
}
