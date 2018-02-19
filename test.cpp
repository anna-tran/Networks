#include <stdio.h>
#include <stdlib.h>

int main() {
	char octolegs[8][3];	
	octolegs[0][0] = 1<<0;
	octolegs[0][1] = 1<<5;
	printf("%d\n",octolegs[0][0]);
	printf("%d\n",octolegs[0][1]);	

	return 0;
}