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
    printf("\n%ld", saltLen);
    return EXIT_SUCCESS;

}

char *encrypt(char arr[])
{
    char *retArray;
    retArray = malloc(strlen(arr) + saltLen + 1);
    retArray[strlen(arr) + saltLen + 1] = 0;
    int length = strlen(arr);
    int hLength = length / 2;

    for (int i = 0; i < hLength + saltLen; i++)
    {
        if(i>(saltLen-1)*2 || i%2==1)
            retArray[hLength + saltLen - i - 1] = arr[i];
        else
            retArray[hLength + saltLen - i - 1] = SALT[i];
    }

    for (int i = hLength + saltLen; i < length + saltLen; i++)
    {
        retArray[length + saltLen - i - 1 + hLength] = arr[i];
    }

    for (int i = 0; i < strlen(retArray);i++){
        retArray[i] += i % 4;
    }

    return retArray;
}