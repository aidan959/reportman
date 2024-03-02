#define _POSIX_C_SOURCE 199309L // for POSIX timers
#define OPENSSL_API_COMPAT 10002
#include <openssl/sha.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include "test.h"

#define TRANSACT_SUCCESS 0
#define TRANSACT_ERR_OPEN -1
#define TRANSACT_HASH_FAILED -2

int verify_files(FILE *source, FILE * dest);
int transact_move_file(const char *source_path, const char *dest_path);
void hash_file(unsigned char * sha256_hash, FILE *source); 
void move_directory(const char *source, const char *destination);

/// @brief Moves file from source to destination - hashes the file before removing.
/// @param source_path 
/// @param dest_path 
/// @return Successfully moved file.
int transact_move_file(const char *source_path, const char *dest_path) {
    FILE *source, *dest;
    char buffer[1024];
    size_t bytes;

    source = fopen(source_path, "rb");
    if (source == NULL) {
        syslog(LOG_ERR, "Error opening source file: %s", strerror(errno));
        return TRANSACT_ERR_OPEN;
    }

    dest = fopen(dest_path, "wb");
    if (dest == NULL) {
        syslog(LOG_ERR, "Error opening destination file: %s", strerror(errno));
        fclose(source);
        return TRANSACT_ERR_OPEN;
    }

    while ((bytes = fread(buffer, sizeof(char), sizeof(buffer), source)) > 0) {
        fwrite(buffer, sizeof(char), bytes, dest);
    }
    
    if (verify_files(source, dest) < 0) {
        syslog(LOG_ERR,
            "Destination file (%s) hash did not match source file (%s) hash. File will not be removed from source.",source_path, dest_path);
        fclose(dest);
        fclose(source);
        return TRANSACT_HASH_FAILED;
    }

    fclose(dest);
    fclose(source);

    remove(source_path);
    return TRANSACT_SUCCESS;
}

/// @brief Compares two files SHA256 hash values.
/// @param source
/// @param dest 
int verify_files(FILE *source, FILE * dest){
    unsigned char source_hash[SHA256_DIGEST_LENGTH];
    unsigned char dest_hash[SHA256_DIGEST_LENGTH];

    hash_file(source_hash, source);
    hash_file(dest_hash, dest);

    if (memcmp(source_hash, dest_hash, SHA256_DIGEST_LENGTH)==0){
        return 0;
    }

    return -1;
    

}

/// @brief Hashes source file's bytes.
/// @param sh256_hash returns the hashed version of source into this.
/// @param source will reset seek to 0 and seek to end
void hash_file(unsigned char * sha256_hash, FILE *source){ 
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    unsigned char buffer[1024];
    size_t bytes = 0;
    if (fseek(source, 0, SEEK_SET) < 0) {
        syslog(LOG_ERR, "Error seeking: %s", strerror(errno));
    }

    while ((bytes = fread(buffer, sizeof(char), sizeof(buffer), source))) {
        SHA256_Update(&sha256, buffer, bytes);
    }

    SHA256_Final(sha256_hash, &sha256);
}
void move_directory(const char *source, const char *destination) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char src_path[1024];
    char dest_path[1024];

    // Create the destination directory
    mkdir(destination, 0777);

    if ((dir = opendir(source)) == NULL) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if(snprintf(src_path, sizeof(src_path), "%s/%s", source, entry->d_name) < 0){
            syslog(LOG_CRIT, "Snprintf: %s",  strerror(errno));
        }
        if(snprintf(dest_path, sizeof(dest_path), "%s/%s", destination, entry->d_name)){
            syslog(LOG_CRIT, "Snprintf: %s",  strerror(errno));
        }

        if (stat(src_path, &statbuf) == -1) {
            syslog(LOG_CRIT,"Stat: %s", strerror(errno));
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            move_directory(src_path, dest_path);
            continue;
        }
        syslog(LOG_NOTICE, "Backing up %s to %s\n", src_path, dest_path);
        if(transact_move_file(src_path, dest_path) != TRANSACT_SUCCESS){
            syslog(LOG_ERR, "Backing up %s to %s FAILED. TRANSACT CANCELLED.\n", src_path, dest_path);

        }
        
    }
    closedir(dir);
}


int main(void) {
    move_directory("sample/source", "sample/destination");
    return 0;
}