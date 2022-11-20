#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SALT "tempword"

const size_t saltLen = strlen(SALT);

char *encrypt(char arr[]);

int main()
{

    printf("%s", encrypt("sula"));
    printf("\n%ld\n", saltLen);
    return EXIT_SUCCESS;
}

char *encrypt(char arr[])
{
    char *retArray;
    retArray = malloc(strlen(arr) + saltLen + 1);
    retArray[strlen(arr) + saltLen + 1] = '\0';
    int length = strlen(arr);
    int hLength = length / 2;
    int hSaltlen = saltLen / 2;

    for (int i = 0; i < saltLen / 2; i++)
    {
        retArray[i] = SALT[i];
    }

    for (int i = 0; i < hLength; i++)
    {
        retArray[hSaltlen + i] = arr[hLength - i - 1];
    }

    for (int i = hLength; i < length; i++)
    {
        retArray[hSaltlen + i] = arr[length - i - 1 + hLength];
    }

    for (int i = hSaltlen; i < saltLen; i++)
    {
        retArray[i + length] = SALT[i];
    }

    for (int i = 0; i < strlen(retArray); i++)
    {
        retArray[i] += i % 4;
    }

    return retArray;
}