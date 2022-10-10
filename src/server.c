#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../includes/optparser.h"
#include "../includes/hash.h"

#define MAXPENDING 99

int main(int argc, char* argv[]){
    if(argc != 5 && argc != 3) {
        printf("Wrong number of arguments.\n");
        return 1;
    }
    struct server_arguments args = server_parseopt(argc,argv);

    /* Create server socket */
    int serverSock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (serverSock < 0) {
        perror("sock() failed.\n");
        return 1;
    }

    /* server address, binding, listening*/
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(args.port);
    if (bind(serverSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        perror("bind() failed.\n");
        return 1;
    }
    if (listen(serverSock,MAXPENDING) < 0) {
        perror("listen() failed.\n");
        return 1;
    }

    /* build hash context */
    struct checksum_ctx* ctx;
    if (argc == 3) {
        ctx = checksum_create(NULL,0);
    }
    else {
        ctx = checksum_create((const uint8_t *)args.salt,args.salt_len);
    }
    if(!ctx) {
        printf("checksum_create() failed.\n");
        return 1;
    }

    /* real meat */
    while (1) {
        struct sockaddr_in clntAddr;
        socklen_t clntAddrLen = sizeof(clntAddr);
        int clntsock = accept(serverSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
        if (clntsock < 0){
            perror("accept() failed.\n");
            continue;
        }

        char header[8];
        int msg[2];
        size_t rcvd = 0;
        ssize_t numBytes;

        /* get initialization */
        while (rcvd < 8) {
            numBytes = recv(clntsock, header+rcvd, sizeof(header)-rcvd, 0);
            if (numBytes < 0){
                perror("recv() failed.\n");
                break;
            }
            if (numBytes == 0) {
                perror("No bytes receive for initialization.\n");
                break;
            }
            rcvd += numBytes;
        }
        if (numBytes <= 0) { // check validity
            close(clntsock);
            continue;
        }
        if (htonl(((int *)header)[0]) != 1) {
            close(clntsock);
            continue;
        }

        /* construct and send acknowledgement */
        msg[0] = htonl(2);
        msg[1] = htonl(40 * htonl(((int *)header)[1]));
        numBytes = send(clntsock, msg, sizeof(msg), 0);
        if (numBytes < 0) {
            perror("send() failed.\n");
            close(clntsock);
            continue;
        }
        else if (numBytes != sizeof(msg)) {
            perror("send() failed.\n");
            close(clntsock);
            continue;
        }

        int n = htonl(((int *)header)[1]);
        char *payload;
        uint8_t checksum[32];
        numBytes = 0;
        int flag = 0; // for error detection
        /* receive hash request*/
        for (int i = 0; i < n; i++) {
            /* get type and length of payload */
            rcvd = 0;
            while (rcvd < sizeof(header)){ 
                numBytes = recv(clntsock,header,sizeof(header),0);
                if(numBytes < 0){
                    perror("recv() failed.\n");
                    break;
                }
                if(numBytes == 0){
                    perror("No bytes received for request.\n");
                    break;
                }
                rcvd += numBytes;
            }
            if(htonl(((int *)header)[0]) != 3 || htonl(((int *)header)[1]) < 1) {
                break;
            }
            size_t len = htonl(((int *)header)[1]);
            rcvd = 0;
            payload = (char *)malloc(len);
            while (rcvd < len) {
                numBytes = recv(clntsock,payload+rcvd,len-rcvd,0);
                if(numBytes < 0){
                    flag = 1;
                    perror("recv() failed.\n");
                    break;
                }
                if(numBytes == 0){
                    flag = 1;
                    perror("No bytes received for request.\n");
                    break;
                }
                rcvd += numBytes;
            }
            if (flag) {
                break;
            }
            checksum_finish(ctx,(const uint8_t *)payload,len,checksum);
            checksum_reset(ctx);
            free(payload);
            msg[0] = htonl(4);
            msg[1] = htonl(i);
            numBytes = send(clntsock, msg, sizeof(msg), 0);
            if (numBytes < 0) {
                perror("send() failed.\n");
                close(clntsock);
                continue;
            }
            else if (numBytes != sizeof(msg)) {
                perror("send() failed.\n");
                close(clntsock);
                continue;
            }
            numBytes = send(clntsock, checksum, sizeof(checksum), 0);
            if (numBytes < 0) {
                perror("send() failed.\n");
                close(clntsock);
                continue;
            }
            else if (numBytes != sizeof(checksum)) {
                perror("send() failed.\n");
                close(clntsock);
                continue;
            }
        } // for loop for handling hash request
        close(clntsock);
        flag = 0;
    } // never ending while loop

    checksum_destroy(ctx);
    free(args.salt);
}