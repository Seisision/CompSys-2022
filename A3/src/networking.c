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

int c;

struct response {
    unsigned int data_length;
    unsigned int status_code;
    unsigned int block_number;
    unsigned int block_count;
    char* block_hash;
    char* total_hash;
    char* message_body;
};

unsigned int bytes_to_int(char* bytes) {
    return htonl(*((unsigned int*)bytes));
}

void response_init(struct response *p, char *byte_reponse) {
    p->block_hash = malloc(32);
    p->total_hash = malloc(32);

    p->data_length = bytes_to_int(byte_reponse);
    p->status_code = bytes_to_int(byte_reponse+4);
    p->block_number = bytes_to_int(byte_reponse+8);
    p->block_count = bytes_to_int(byte_reponse+12);

    memcpy(p->block_hash, byte_reponse+16, 32);
    memcpy(p->total_hash, byte_reponse+48, 32);

    if (p->data_length >= 0) {
        p->message_body = malloc(p->data_length);
        memcpy(p->message_body,byte_reponse+80, p->data_length);
    } else {
      p->message_body = NULL;
    }
}

void response_init_h(struct response *p, char *byte_reponse) {
    p->block_hash = malloc(32);
    p->total_hash = malloc(32);

    p->data_length = bytes_to_int(byte_reponse);
    p->status_code = bytes_to_int(byte_reponse+4);
    p->block_number = bytes_to_int(byte_reponse+8);
    p->block_count = bytes_to_int(byte_reponse+12);

    memcpy(p->block_hash, byte_reponse+16, 32);
    memcpy(p->total_hash, byte_reponse+48, 32);

    p->message_body = NULL;
}

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
    size_t hash_len = strlen(password) + strlen(salt);
    char str_to_hash[hash_len];

    memcpy(str_to_hash, password, strlen(password));
    memcpy(str_to_hash+strlen(password), salt, strlen(salt));
    get_data_sha(str_to_hash, hash, hash_len, SHA256_HASH_SIZE);
}

char* build_message(char* username, char* signature, char* msg, unsigned int msg_length)
{
    char* msg_data = malloc(52+msg_length);
    memcpy(msg_data, username, strlen(username));

    int u_size = strlen(username);
    int s_size = 32;

    // pad header size
    for (int i = u_size; i < 16; i++) {
        msg_data[i] = 0;
    }

    memcpy(msg_data+16, signature, s_size);

    void* msg_length_p = &msg_length;
    memcpy(msg_data+48,msg_length_p+3,1);
    memcpy(msg_data+49,msg_length_p+2,1);
    memcpy(msg_data+50,msg_length_p+1,1);
    memcpy(msg_data+51,msg_length_p,1);
    printf("length in byte array: %d%d%d%d\n", msg_data[48], msg_data[49], msg_data[50], msg_data[51]);

    if (msg_length > 0) {
        memcpy(msg_data+52, msg, msg_length);
        printf("passed message length: %d\n", msg_length);
        printf("message: %s\n", msg);
        printf("actual message length: %d\n", strlen(msg));
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
    get_signature(password, salt, signature);

    rio_t rio;
    int server = Open_clientfd("127.0.0.1", "23457");

    char* data = build_message(username, signature, 0, 0);
    Rio_writen(server, data, 52);
    Rio_readinitb(&rio, server);

    char buffer[MAX_MSG_LEN];
    struct response r;
    Rio_readn(server, buffer, MAX_MSG_LEN);

    response_init(&r, buffer);
    printf("feedback: %s\n", r.message_body);

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
    // Your code here. This function has been added as a guide, but feel free 
    // to add more, or work in other parts of the code
    hashdata_t signature;
    get_signature(password, salt, signature);

    rio_t rio;
    int server = Open_clientfd("127.0.0.1", "23457");

    char* data = build_message(username, signature, to_get, strlen(to_get));
    Rio_writen(server, data, 52+strlen(to_get));
    Rio_readinitb(&rio, server);

    char buffer[80+MAX_MSG_LEN];
    struct response* pr;
    printf("fetching response\n");
    size_t read_bytes = 0;
    int counter = 0;

    size_t total_message_length = 0;

    struct response** response_buffer;
    while (1)
    {
      if (Rio_readn(server, buffer, 80) != 80) break;
      pr = malloc(sizeof(struct response));
      response_init_h(pr, buffer);
      if (pr->data_length > 0) {
          pr->message_body = malloc(pr->data_length);
          Rio_readn(server, pr->message_body, pr->data_length);
      }
      unsigned int block_number = pr->block_number;
      unsigned int block_count = pr->block_count;

      if (counter == 0) {
        response_buffer = malloc(block_count*sizeof(struct response*));
      }
      response_buffer[block_number] = pr;
      printf("buffer respponse %u\n", response_buffer[counter]->block_number);
      counter++;
      total_message_length += pr->data_length;
      if(counter > block_count) break;
    }

    char* combined_message = malloc(total_message_length+1);
    size_t progress = 0;

    for (int i = 0; i < counter; ++i) {
      memcpy(combined_message+progress,response_buffer[i]->message_body, response_buffer[i]->data_length);
      progress += response_buffer[i]->data_length;
      cleanup_response(response_buffer[i]);
      free(response_buffer[i]);
    }
    free(response_buffer);

    combined_message[total_message_length] = 0;
    printf("%s", combined_message);

    Close(server);
}

int main(int argc, char **argv)
{
    // Users should call this script with a single argument describing what 
    // config to use
    if (argc != 2)
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

    // Register the given user
    register_user(username, password, user_salt);

    printf("\n");
    printf("------------------------NEW CALL get file tiny-------------------");
    printf("\n");

    // Retrieve the smaller file, that doesn't not require support for blocks
    get_file(username, password, user_salt, "tiny.txt");

    printf("\n");
    printf("------------------------NEW CALL get file big-------------------");
    printf("\n");

    // Retrieve the larger file, that requires support for blocked messages
    get_file(username, password, user_salt, "hamlet.txt");

    exit(EXIT_SUCCESS);
}