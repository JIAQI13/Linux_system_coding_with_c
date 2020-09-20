#ifndef _repoutil
#define _repoutil

// Debug function that prints the entire internal data structure
void print_repo(char** repo);

// Returns the length of the internal repository data structure
// (Array of strings, i.e. char**)
// Assumptions: None
int get_repo_len(char** repo);

// Returns the MD5 hash string for the file contents
// Assumptions: None
char* md5hash(const char* file_contents, int file_len);

// Inserts a new file name whose contents already exist in the server
// into the internal repository
// Assumptions: hashname is in repo
void insert_alias(char** repo, char* hashname, char* alias);

// Appends to the repo a hash of the file contents as well as
// the first filename alias
// Assumptions: filename is of length less than 32 characters
void insert_new_file_contents(char** repo, const char* file_contents,
			      int file_len, char* first_alias);

// Deletes a file alias from the internal data structure --
// if it is a unique alias, delete the hash of its contents
// Assumptions: alias exists in repo
// Returns 0 if non-unique hashing alias, 1 if unique
int delete_file_alias(char** repo, char* alias);

#endif


