#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char get_highest_bit(unsigned char ack) {
    char seqnum = 0;
    printf("ack & (1 << %d+1)  =  %u\n", seqnum, ack & (1 << (seqnum+1)));
    while (ack & (1 << (seqnum+1))) {
    	printf("ack & (1 << %d+1)  =  %u\n", seqnum, ack & (1 << (seqnum+1)));
        seqnum++;
    }
    return 1<<seqnum;
}
int main() {
	char octolegs[8];	
	for (int i = 0; i < 8; i++) {
		memset(&octolegs[i],0,1);
		for (int j = 0; j <= i; j++) 
			octolegs[i] |= 1<<j;
		printf("highest bit %u\n", (unsigned char) get_highest_bit((unsigned char) octolegs[i]));
		printf("%u\n\n", (unsigned char) octolegs[i]);

	}

	return 0;
}