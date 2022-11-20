#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#ifdef __APPLE__
#include "./endian.h"
#else
#include <endian.h>
#endif

#include "./networking.h"
#include <netinet/in.h>
#include "./sha256.h"

char server_ip[IP_LEN];
char server_port[PORT_LEN];
char my_ip[IP_LEN];
char my_port[PORT_LEN];

// header format constants
const size_t HEADER_FIELD_OFFSET = 4;
const size_t USERNAME_HEADER_OFFSET = 16;
const size_t CLIENT_HEADER_OFFSET = 52;
const size_t SERVER_HEADER_OFFSET = 80;

int c;

// struct for storing server response
struct response {
    unsigned int data_length;
    unsigned int status_code;
    unsigned int block_number;
    unsigned int block_count;
    char* block_hash;
    char* total_hash;
    char* message_body;
};

// convert byte order (switch endianness)
// as this operation is symmetrical this method can be used for both
// host to network and network to host.
unsigned int bytes_to_int(char* bytes) {
    return htonl(*((unsigned int*)bytes));
}

// initialize a response.
void response_init(struct response *p, char *byte_reponse) {
    // allocate memory for hashes
    p->block_hash = malloc(SHA256_HASH_SIZE);
    p->total_hash = malloc(SHA256_HASH_SIZE);

    // begin reading header one field at a time (and switching endianness)
    p->data_length = bytes_to_int(byte_reponse);
    size_t offset = HEADER_FIELD_OFFSET;

    p->status_code = bytes_to_int(byte_reponse+offset);
    offset += HEADER_FIELD_OFFSET;

    p->block_number = bytes_to_int(byte_reponse+offset);
    offset += HEADER_FIELD_OFFSET;

    p->block_count = bytes_to_int(byte_reponse+offset);
    offset += HEADER_FIELD_OFFSET;

    // copy hash into allocated memory
    memcpy(p->block_hash, byte_reponse+offset, SHA256_HASH_SIZE);
    offset += SHA256_HASH_SIZE;

    memcpy(p->total_hash, byte_reponse+offset, SHA256_HASH_SIZE);

    // only allocate space for message body if response data is present
    if (p->data_length > 0) {
        p->message_body = malloc(p->data_length);
    } else {
      p->message_body = NULL;
    }
}

// free memory allocated in response
void cleanup_response(struct response *p) {
  free(p->block_hash);
  free(p->total_hash);
  free(p->message_body);
}

/*
 * Gets a sha256 hash of specified data, sourcedata. The hash itself is
 * placed into the given variable 'hash'. Any size can be created, but a
 * a normal size for the hash would be given by the global variable
 * 'SHA256_HASH_SIZE', that has been defined in sha256.h
 */
void get_data_sha(const char* sourcedata, hashdata_t hash, uint32_t data_size, 
    int hash_size)
{
  SHA256_CTX shactx;
  unsigned char shabuffer[hash_size];
  sha256_init(&shactx);
  sha256_update(&shactx, sourcedata, data_size);
  sha256_final(&shactx, shabuffer);

  for (int i=0; i<hash_size; i++)
  {
    hash[i] = shabuffer[i];
  }
}

// takes data, hashes it and compares it to a given hash. 
// Returns 0 if the hash of the data does not match the
// passed hash and returns 1 if it does match.
int verify_block_checksum(char* block_hash, char* data, uint32_t data_size) {
    hashdata_t data_hash;

    get_data_sha(data, data_hash, data_size, SHA256_HASH_SIZE);

    for (int i = 0; i < SHA256_HASH_SIZE; ++i) {
        if (data_hash[i] != (uint8_t)block_hash[i]) {
          return 0;
        }
    }
    return 1;
}

/*
 * Gets a sha256 hash of specified data file, sourcefile. The hash itself is
 * placed into the given variable 'hash'. Any size can be created, but a
 * a normal size for the hash would be given by the global variable
 * 'SHA256_HASH_SIZE', that has been defined in sha256.h
 */
void get_file_sha(const char* sourcefile, hashdata_t hash, int size)
{
    int casc_file_size;

    FILE* fp = Fopen(sourcefile, "rb");
    if (fp == 0)
    {
        printf("Failed to open source: %s\n", sourcefile);
        return;
    }

    fseek(fp, 0L, SEEK_END);
    casc_file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    char buffer[casc_file_size];
    Fread(buffer, casc_file_size, 1, fp);
    Fclose(fp);

    get_data_sha(buffer, hash, casc_file_size, size);
}

/*
 * Combine a password and salt together and hash the result to form the 
 * 'signature'. The result should be written to the 'hash' variable. Note that 
 * as handed out, this function is never called. You will need to decide where 
 * it is sensible to do so.
 */
void get_signature(char* password, char* salt, hashdata_t* hash)
{
    // concatenate password and salt, then hash the result
    size_t hash_len = strlen(password) + strlen(salt);
    char str_to_hash[hash_len];
    memcpy(str_to_hash, password, strlen(password));
    memcpy(str_to_hash+strlen(password), salt, strlen(salt));
    get_data_sha(str_to_hash, *hash, hash_len, SHA256_HASH_SIZE);
}

// build a message to send to the server
char* build_message(char* username, hashdata_t* signature, char* msg, unsigned int msg_length)
{
    int u_size = strlen(username);

    // message to be sent to the server
    char* msg_data = malloc(CLIENT_HEADER_OFFSET+msg_length);

    // copy username to the header
    memcpy(msg_data, username, strlen(username));

    // pad username part of header if size too small 
    for (size_t i = u_size; i < USERNAME_HEADER_OFFSET; i++) {
        msg_data[i] = 0;
    }

    size_t offset = USERNAME_HEADER_OFFSET; 

    // copy signature to the header (after the username)
    memcpy(msg_data+offset, signature, SHA256_HASH_SIZE);
    offset += SHA256_HASH_SIZE;

    // translate endianness to network byte order and copy length to header
    unsigned int msg_length_flipped = htonl(msg_length);
    memcpy(msg_data+offset, &msg_length_flipped, HEADER_FIELD_OFFSET);
    
    // if any request data exists it is added to the message (after the header)
    if (msg_length > 0) {
        memcpy(msg_data+CLIENT_HEADER_OFFSET, msg, msg_length);
    }
    return msg_data;
}

/*
 * Register a new user with a server by sending the username and signature to 
 * the server
 */
void register_user(char* username, char* password, char* salt)
{
    hashdata_t signature;
    get_signature(password, salt, &signature);

    // open server using ip and port
    int server = Open_clientfd(server_ip, server_port);

    // build registration message (no data)
    char* data = build_message(username, &signature, 0, 0);
    
    // send registration message 
    Rio_writen(server, data, CLIENT_HEADER_OFFSET);

    // read response header into buffer
    char buffer[SERVER_HEADER_OFFSET];
    Rio_readn(server, buffer, SERVER_HEADER_OFFSET);

    // populate response struct from buffer
    struct response r;
    response_init(&r, buffer);

    // read remaining part of response if present
    if (r.data_length > 0) {
          Rio_readn(server, r.message_body, r.data_length);
          printf("%s\n", r.message_body);
    }

    free(data);
    cleanup_response(&r);

    Close(server);
}

/*
 * Get a file from the server by sending the username and signature, along with
 * a file path. Note that this function should be able to deal with both small 
 * and large files. 
 */
void get_file(char* username, char* password, char* salt, char* to_get)
{
    // flag for tracking success (assume success set to 0 if anything wrong happens)
    int success = 1;

    hashdata_t signature;
    get_signature(password, salt, &signature);

    // open server using ip and port
    int server = Open_clientfd(server_ip, server_port);

    // build fetch file message with file path as data
    char* data = build_message(username, &signature, to_get, strlen(to_get));

    // send fetch request to server
    Rio_writen(server, data, CLIENT_HEADER_OFFSET+strlen(to_get));
    printf("fetching response\n");

    // read response into buffer
    char buffer[SERVER_HEADER_OFFSET+MAX_MSG_LEN];

    // variables for tracking and storing multi part messages
    int counter = 0;
    size_t total_message_length = 0;
    char total_hash[SHA256_HASH_SIZE];
    struct response** response_buffer;
    struct response* pr;

    // continue recieving messages until all blocks are read
    while (1)
    {
      // read the server headers (in blocks of 80 bytes)
      size_t net_bytes_read = Rio_readn(server, buffer, SERVER_HEADER_OFFSET);
      
      // verify correct length of header/remaining data
      if (net_bytes_read != SERVER_HEADER_OFFSET) {
          if (net_bytes_read > 0) {
            success = 0;
            printf("unexpected msg size: %lu\n", net_bytes_read);
          }
          break; 
      }

      // allocate and initialize response based on the read header
      pr = malloc(sizeof(struct response));
      response_init(pr, buffer);

      // keep block number of current response and expected total count of blocks
      unsigned int block_number = pr->block_number;
      unsigned int block_count = pr->block_count;

      // allocate buffer space for the total number of blocks expected to be sent
      // we only do this once when recieving the first block
      if (counter == 0) {
        response_buffer = malloc(block_count*sizeof(struct response*));
        // hash of total data sent across all blocks
        memcpy(total_hash, pr->total_hash, SHA256_HASH_SIZE);
      }

      // check if header has response data (payload) 
      // if it does allocate and read the message/payload
      if (pr->data_length > 0) {
          Rio_readn(server, pr->message_body, pr->data_length);
          //verify the block hash of the response data in this message
          // also verify that the total hash (total hash should be consistent across all blocks)
          if (verify_block_checksum(pr->block_hash, pr->message_body, pr->data_length) == 0 &&
              pr->total_hash == total_hash) { 
              success = 0;
              // +1 as blocks are 0 indexed
              printf("block count %u/%u\n", block_number+1, block_count);
              printf("Block hash does not match\n");
          }
      }

      // check header status code
      if (pr->status_code != 1) {
          success = 0;
          printf("Error: %s\n", pr->message_body);
      }

      // add the read response to the buffer array at its correct index in the series
      // this means we dont have to sort the blocks afterwards
      response_buffer[block_number] = pr;

      // increment counter and total_message_length
      counter++;
      total_message_length += pr->data_length;

      // if we have read the total number of blocks to be sent break
      if(counter > (int)block_count) {
          break; 
      }
    }

    // if we failed while trying to get the file, then clean up and return
    if (success == 0) {
        printf("failed to fetch file\n");
        if (response_buffer != NULL) {
            for (int i = 0; i < counter; ++i) {
                cleanup_response(response_buffer[i]);
                free(response_buffer[i]);
            }
            free(response_buffer);
        }
        Close(server);
        return;
    }

    // allocate space for the full message 
    char* combined_message = malloc(total_message_length);

    // current position of combined buffer to write to
    size_t progress = 0;

    // iterate through response buffer and construct the combined_message
    for (int i = 0; i < counter; ++i) {
      // copy the read message to the combined_message
      memcpy(combined_message+progress,response_buffer[i]->message_body, response_buffer[i]->data_length);
      // increment progress by length of newly copied data
      progress += response_buffer[i]->data_length;
      // free any memory relevant to the newly copied response as it is no longer needed
      cleanup_response(response_buffer[i]);
      free(response_buffer[i]);
    }
    free(response_buffer);

    // verify total hash
    if (verify_block_checksum(total_hash, combined_message, total_message_length) == 0) { 
        printf("Total hash does not match\n");
        free(combined_message);
        Close(server);
        return;
    }

    // save recieved file to local file system
    FILE *file;
    file = fopen (to_get,"w");
    fputs(combined_message, file);
    printf("file: %s fetched and copied to /src/\n", to_get);

    // final cleanup
    fclose(file);
    free(combined_message);
    Close(server);
}

int main(int argc, char **argv)
{
    // Users should call this script with a single argument describing what 
    // config to use
    // we added a potential third argument for testing purposes
    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: %s <config file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Read in configuration options. Should include a client_directory, 
    // client_ip, client_port, server_ip, and server_port
    char buffer[128];
    fprintf(stderr, "Got config path at: %s\n", argv[1]);
    FILE* fp = Fopen(argv[1], "r");
    while (fgets(buffer, 128, fp)) {
        if (starts_with(buffer, CLIENT_IP)) {
            memcpy(my_ip, &buffer[strlen(CLIENT_IP)], 
                strcspn(buffer, "\r\n")-strlen(CLIENT_IP));
            if (!is_valid_ip(my_ip)) {
                fprintf(stderr, ">> Invalid client IP: %s\n", my_ip);
                exit(EXIT_FAILURE);
            }
        }else if (starts_with(buffer, CLIENT_PORT)) {
            memcpy(my_port, &buffer[strlen(CLIENT_PORT)], 
                strcspn(buffer, "\r\n")-strlen(CLIENT_PORT));
            if (!is_valid_port(my_port)) {
                fprintf(stderr, ">> Invalid client port: %s\n", my_port);
                exit(EXIT_FAILURE);
            }
        }else if (starts_with(buffer, SERVER_IP)) {
            memcpy(server_ip, &buffer[strlen(SERVER_IP)], 
                strcspn(buffer, "\r\n")-strlen(SERVER_IP));
            if (!is_valid_ip(server_ip)) {
                fprintf(stderr, ">> Invalid server IP: %s\n", server_ip);
                exit(EXIT_FAILURE);
            }
        }else if (starts_with(buffer, SERVER_PORT)) {
            memcpy(server_port, &buffer[strlen(SERVER_PORT)], 
                strcspn(buffer, "\r\n")-strlen(SERVER_PORT));
            if (!is_valid_port(server_port)) {
                fprintf(stderr, ">> Invalid server port: %s\n", server_port);
                exit(EXIT_FAILURE);
            }
        }        
    }
    fclose(fp);

    fprintf(stdout, "Client at: %s:%s\n", my_ip, my_port);
    fprintf(stdout, "Server at: %s:%s\n", server_ip, server_port);

    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
    char user_salt[SALT_LEN+1];
    
    fprintf(stdout, "Enter a username to proceed: ");
    scanf("%16s", username);
    while ((c = getchar()) != '\n' && c != EOF);
    // Clean up username string as otherwise some extra chars can sneak in.
    for (int i=strlen(username); i<USERNAME_LEN; i++)
    {
        username[i] = '\0';
    }
 
    fprintf(stdout, "Enter your password to proceed: ");
    scanf("%16s", password);
    while ((c = getchar()) != '\n' && c != EOF);
    // Clean up password string as otherwise some extra chars can sneak in.
    for (int i=strlen(password); i<PASSWORD_LEN; i++)
    {
        password[i] = '\0';
    }

    // Note that a random salt should be used, but you may find it easier to
    // repeatedly test the same user credentials by using the hard coded value
    // below instead, and commenting out this randomly generating section.
    for (int i=0; i<SALT_LEN; i++)
    {
        user_salt[i] = 'a' + (random() % 26);
    }
    user_salt[SALT_LEN] = '\0';
    //strncpy(user_salt, 
    //    "0123456789012345678901234567890123456789012345678901234567890123\0", 
    //    SALT_LEN+1);

    fprintf(stdout, "Using salt: %s\n", user_salt);

    // The following function calls have been added as a structure to a 
    // potential solution demonstrating the core functionality. Feel free to 
    // add, remove or otherwise edit. 

    int test_case = 0;
    if (argc == 3) {
      test_case = atoi(argv[2]);
    }

    if (test_case == 1) {
        // Retrieve the a file without registering a user
        get_file(username, password, user_salt, "tiny.txt");

    } else if(test_case == 2) {
        // Register user and try to get a file that doesnt exist
        register_user(username, password, user_salt);
        get_file(username, password, user_salt, "doesnotexist.txt");

    } else if(test_case == 3) {
        // Register user and use a wrong password when retrieving file
        register_user(username, password, user_salt);
        get_file(username, "wrongpassword", user_salt, "hamlet.txt");
        
    } else {
        // Register the given user
        register_user(username, password, user_salt);

        // Retrieve the smaller file, that doesn't not require support for blocks
        get_file(username, password, user_salt, "tiny.txt");

        // Retrieve the larger file, that requires support for blocked messages
        get_file(username, password, user_salt, "hamlet.txt");
    }
    exit(EXIT_SUCCESS);
}