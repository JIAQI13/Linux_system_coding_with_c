#include "repoutil.h"
#include <openssl/md5.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void print_repo(char** repo) {
  int repo_itr = 0;
  while (repo[repo_itr++]) printf("%s\n", repo[repo_itr-1]);
}

int get_repo_len(char** repo) {
  int length = 0;
  while (repo[length++]);
  return length-1;
}

char* md5hash(const char* file_contents, int file_len) {
  unsigned char c[MD5_DIGEST_LENGTH];
  int i;
  MD5_CTX mdContext;
  int bytes;
  unsigned char data[1024];
  MD5_Init (&mdContext);
  for (int i=0; i<file_len-1; i++)
    MD5_Update (&mdContext,&file_contents[i],1);
  
  MD5_Final (c,&mdContext);
  static char answer[MD5_DIGEST_LENGTH] = {0};
  for (i=0; i<MD5_DIGEST_LENGTH; i++)
    answer[i] = '\0';
  unsigned char element[1];
  for(i = 0; i < MD5_DIGEST_LENGTH; i++){
    sprintf(element,"%02x",c[i]);
    strcat(answer,element);
  }
  return answer;
}

void insert_alias(char** repo, char* hashname, char* alias) {
  // Allocate space at the end of the repo for an insertion
  int len = get_repo_len(repo) - 1;
  repo[len+1] = (char*)malloc(33);

  int repo_itr = 0;
  while (strcmp(repo[repo_itr++], hashname) != 0);
  int hash_index = repo_itr-1;
  for (int i=len+1; i>hash_index+1; i--)
    strcpy(repo[i], repo[i-1]);
  
  strcpy(repo[hash_index+1], alias);
}

void insert_new_file_contents(char** repo, const char* file_contents,
			      int file_len, char* first_alias) {
  int len = get_repo_len(repo) -1;
  repo[len+1] = (char*)malloc(33);
  repo[len+2] = (char*)malloc(33);
  strcpy(repo[len+1], md5hash(file_contents, file_len));
  strcpy(repo[len+2], first_alias);
}

int delete_file_alias(char** repo, char* alias) {
  int len = get_repo_len(repo) - 1;
  
  int repo_itr = 0;
  while (strcmp(repo[repo_itr++], alias) != 0);
  int alias_index = repo_itr-1;

  bool unique_alias = true;
  if (strlen(repo[alias_index-1]) != 32)
    unique_alias = false;
  if (len != alias_index && strlen(repo[alias_index+1]) != 32)
    unique_alias = false;

  if (!unique_alias) {
    for (int i=alias_index; i<len; i++)
      strcpy(repo[i], repo[i+1]);
    memset(repo+len, 0x00, 33);
    free(repo[len]);
  } else {
    for (int i=alias_index-1; i<len-1; i++)
      strcpy(repo[i], repo[i+2]);
    memset(repo+len-1, 0x00, 66);
    free(repo[len]);
    free(repo[len-1]);
  }
  return unique_alias;
}
