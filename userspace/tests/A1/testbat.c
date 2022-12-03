#include "unistd.h"
#include "stdio.h"
#include "unistd.h"

int main(){
    printf("Starting battery...\n");
    char* const path = "/usr/canceltest.sweb";
	// exec call
	char* const args[] = {path};
    execv(path, args);
    printf("we should never get back here...\n");
}