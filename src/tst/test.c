#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* readLsofOutput(void) {
    FILE *fp;
    char *data = NULL;
    size_t dataAllocated = 0;
    size_t dataUsed = 0;
    char buffer[1024];

    // Execute lsof to list open files
    fp = popen("lsof -i", "r");
    if (fp == NULL) {
        perror("Failed to run command");
        exit(1);
    }

    // Read the output a line at a time
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t bufferLen = strlen(buffer);

        // Ensure there's enough space in the data buffer
        if (dataUsed + bufferLen + 1 > dataAllocated) {
            size_t newAllocated = dataAllocated == 0 ? bufferLen * 2 : dataAllocated * 2;
            char *newData = realloc(data, newAllocated);
            if (!newData) {
                perror("Failed to allocate memory");
                free(data);
                pclose(fp);
                exit(1);
            }
            data = newData;
            dataAllocated = newAllocated;
        }

        // Append the new data
        memcpy(data + dataUsed, buffer, bufferLen);
        dataUsed += bufferLen;
        data[dataUsed] = '\0';  // Ensure the string is null-terminated
    }

    // Close the pipe
    pclose(fp);

    return data;
}

int main(void) {
    char *lsofOutput = readLsofOutput();
    if (lsofOutput != NULL) {
        printf("lsof output:\n%s", lsofOutput);
        free(lsofOutput);  // Remember to free the allocated memory
    }
    return 0;
}