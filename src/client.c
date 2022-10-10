#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../includes/optparser.h"

int main(int argc, char * argv[]) {
    if (argc != 13 && argc != 11) {
        fprintf(stdout, "Wrong number of args.\n");
        return 1;
    }

    /* declaration */
    struct client_arguments args = client_parseopt(argc, argv);
    in_port_t port;
    uint32_t smin, smax, max = 16777216;
    unsigned int n;
    char* filename, ip[strlen(args.ip_address)+1];

    /* check */
    if (args.hashnum < 0) {
        fprintf(stdout, "n must be greater than 0.\n");
        return 1;
    }
    else if (args.smin < 1) {
        fprintf(stdout, "smin must be greater than 0.\n");
        return 1;
    }
    else if (args.smax < args.smin) {
        fprintf(stdout, "smax must be less than or equal to smin.\n");
        return 1;
    }
    else if ((uint32_t) args.smax > max) {
        args.smax = max;
    }
    
    /* move values and free mem */
    filename = (char *) malloc(strlen(args.filename) + 1);
    strcpy(filename, args.filename);
    strcpy(ip, args.ip_address);
    smin = args.smin;
    smax = args.smax;
    n = args.hashnum;
    port = args.port;
    free(args.filename);

    /* file stuff */
    FILE* fp = fopen(filename, "r");
    if(!fp) {
        fprintf(stdout, "Cannot find specified file.\n");
        return 1;
    }

    /* socket stuff */
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        fprintf(stdout, "sock() failed.\n");
        return 1;
    }
    
    /* server ip */
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    int rtnval = inet_pton(AF_INET, ip, &servAddr.sin_addr.s_addr);
    if  (rtnval == 0 || rtnval < 0) {
        fprintf(stdout, "Invalid IP address.\n");
        return 1;
    }
    servAddr.sin_port = htons(port);

    /* connect */
    if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        perror("Connect failed.");
        return 1;
    }

    /* initialization construction */
    int msg[2];
    msg[0] = htonl(1);
    msg[1] = htonl(n);

    /* send initialization*/
    ssize_t numBytes = send(sock, msg, sizeof(msg), 0);
    if (numBytes < 0) {
        fprintf(stdout, "send() failed.\n");
        return 1;
    }
    else if (numBytes != sizeof(msg)) {
        fprintf(stdout, "send() sended unexpected number of bytes.\n");
        return 1;
    }
    unsigned int rcvd = 0;
    char buffer[sizeof(msg)]; 
    while (rcvd < sizeof(msg)) { //get acknowledgement
        numBytes = recv(sock, buffer+rcvd, sizeof(msg)-rcvd, 0);
        if (numBytes < 0) {
            fprintf(stdout, "recv() failed.\n");
            return 1;
        }
        else if (numBytes == 0){
            perror("No acknowledgement received.\n");
            return 1;
        }
        rcvd += numBytes;
    }

    int *ack = (int *)buffer;
    if (htonl(ack[0]) != 2 || htonl(ack[1]) != n*40){
        printf("Wrong acknowledgement: %d %d",htonl(ack[0]), htonl(ack[1]));
        return 1;
    }

    /* Hash request */
    char hash[smax];
    for (uint32_t i = 0; i < n; i++){
        int l = (random() % (smax - smin + 1)) + smin;
        numBytes = fread(hash, 1, l, fp);
        /* Hash request construction */
        msg[0] = htonl(3);
        msg[1] = htonl(numBytes);
        int sent = numBytes;
        /* send request */
        numBytes = send(sock, msg, sizeof(msg), 0);
        if (numBytes < 0) {
            fprintf(stdout, "send() failed.\n");
            return 1;
        }
        else if (numBytes != sizeof(msg)) {
            fprintf(stdout, "send() sended unexpected number of bytes.\n");
            return 1;
        }
        numBytes = send(sock, hash, sent, 0);
        if (numBytes < 0) {
            fprintf(stdout, "send() failed.\n");
            return 1;
        }
        else if (numBytes != sent) {
            fprintf(stdout, "send() sended unexpected number of bytes.\n");
            return 1;
        }

        rcvd = 0;
        char response[40];
        /* receive response */
        while(rcvd < sizeof(response)) {
            numBytes = recv(sock, response+rcvd,sizeof(response)-rcvd,0);
            if (numBytes < 0) {
                printf("recv() failed.\n");
                return 1;
            }
            else if (numBytes == 0){
                perror("No response received.\n");
                return 1;
            }
            rcvd += numBytes;
        }
        printf("%d: 0x", htonl(((uint32_t *)response)[1])+1);
        for (int j = 2; j < 10; j++) {
            printf("%08x", htonl(((uint32_t *)response)[j]));
        }
        printf("\n");
    }
    free(filename);
    fclose(fp);
    close(sock);
    return 0;
}
