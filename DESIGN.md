Design Report
    1. What is the internal data structure you used to map file names to file contents?
	For the internal data structure, we use an array which contains hashes and filenames to map the hashes and file contents. 
	Since the md5 hash is base on the content of the file, the hash works as a signature of each unique file. So the mapping will be between hash and the file names.
Every time there is a new file coming in, the program will first check if the hash exist already in the array. If it is a new file we calculate the hash with the md5hash function and append the hash with the file name into the list. It will keep adding the filename until there is a new file which should be able to provide a new hash.
	If we want to delete a file name, we not only delete it from the array we also check that if the hash still has a file name.If it doesn’t have a file name we delete the hash too. 

     2. How do you ensure that the server is responding efficiently and quickly to client requests?
	To make sure that the server is responding correctly we strictly follow the protocol of the assignment. We have the flag 0xXX which implies what kind of command we want to run. Every time the client or the server wants to send messages, they will create a buffer(which is called as rbuff) in the code. It begins with the flag (0xXX) and following with the content we want to send.
	If the server received anything that is not in the protocol it will give the client an error message to ensure that they won’t undermine the protocol.

Ex.
List all the file under current directory.

client -> server
+--------+
| 0x00 |
+--------+
server -> client
+------+-------+---------+-----------+--+-----------+--+-----------+-----------+--+
| 0x01 | N | filename1 |\0| filename2 |\0| ... | filenameN |\0|
+------+-------+---------+-----------+--+-----------+--+-----------+-----------+--+

Upload file (and response)

client -> server
+--------+-----------+--+-----+----+----+----+-----------------------------------------+
| 0x02 | filename |\0| size | contents of the file (size bytes long) |
+--------+-----------+--+-----+----+----+----+-----------------------------------------+
server -> client
+------+
| 0x03 |
+------+

Delete file (and response)
client -> server
+--------+-----------+--+
| 0x04 | filename |\0|
+--------+-----------+--+
server -> client
+------+
| 0x05 |
+------+

Download file (and response)
client->server
+--------+-----------+--+
| 0x06 | filename |\0|
+--------+-----------+--+
server->client
+------+-----+----+----+----+-----------------------------------------+
| 0x07 | size | contents of the file (size bytes long) |
+------+-----+----+----+----+-----------------------------------------+

Quit (and response)
client->server
+--------+
| 0x08 |
+--------+
server->client
+------+
| 0x09 |
+------+

Error message
server->client
+--------+-----------+--+
| 0xFF | reason |\0|
+--------+-----------+--+

 3. How do you ensure changes to your internal data structures are consistent when multiple threads try to perform such changes?
    Multiple threads are changing the same Array of strings (the internal data) and so if thread A modifies the repository, then thread B will also see the changes.

4. How are the contents of a file which is being uploaded stored before you can determine whether it matches the hash of already stored file(s)?
	Since the client will send a buffer containing the file content to the server, the server will read the file content and directly calculate the hash with the md5hash function. Then the server compares the hash it gets with the hashes which had previously stored in the mapping array. So it is in the receive buffer all the way until the server finds out if it has been already stored or not.

5.How do you deal with clients that fail or appear to be buggy without harming the consistency of your server?
	There will occur a problem if the client is buggy and failed to send any message to the server, then the client and the server will be both waiting for a message. We use the goto statement to check that if the client failed to send a message to the server, it won’t be waiting for a message from the server, but it will waiting for the user to prompt another command.

6.How do you handle the graceful termination of the server when it receives SIGTERM?
	Once the server received the final termination message, it will first build the XML file which contains a list of hashes and file name. It is just built from the mapping array data structure we created in question1. Then it will send the quit command the all the client that is currently linked to the server to ensure that all the client is terminated.

7.What sanity checks do you perform when the server starts on a given directory?
	If the server starts on a new directory, it will check if there is an XML file called .dedup already exist in the given directory. If the file exists, it will read in the mapping array into its memory to ensure that all the file have the correct map between the file name and the hashes. If the file does not exist, it will create a new empty internal mapping array and ignore any other file under the server directory.

8. How do you deal with zero sized files?
       Zero sized files were tested by using the touch command and giving the argument as a file that does not exist. This creates a completely empty file. This file was uploaded to the daemon server from a client and could be listed and downloaded as an empty file. It appears that empty files could also be hashed by the function given in eClass (char* md5Hash).