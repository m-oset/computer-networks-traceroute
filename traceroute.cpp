// Marcelina Oset 313940

#include <arpa/inet.h>
#include <cassert>
#include <errno.h>
#include <iostream>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>

void print_as_bytes(unsigned char *buff, ssize_t length) {
    for (ssize_t i = 0; i < length; i++, buff++)
        printf("%.2x ", *buff);
}

u_int16_t compute_icmp_checksum(const void *buff, int length) {
    u_int32_t sum;
    const u_int16_t *ptr = (u_int16_t *)buff;
    assert(length % 2 == 0);
    for (sum = 0; length > 0; length -= 2)
        sum += *ptr++;
    sum = (sum >> 16) + (sum & 0xffff);
    return (u_int16_t)(~(sum + (sum >> 16)));
}

void send(int sockfd, struct sockaddr_in &recipient, int seq_value) {
    struct icmp header;
    header.icmp_type = ICMP_ECHO;
    header.icmp_code = 0;
    header.icmp_hun.ih_idseq.icd_id = getpid() & 0xffff;
    header.icmp_hun.ih_idseq.icd_seq = seq_value;
    header.icmp_cksum = 0;
    header.icmp_cksum = compute_icmp_checksum((u_int16_t *)&header, sizeof(header));

    sendto(sockfd, &header, sizeof(header), 0, (struct sockaddr *)&recipient,
           sizeof(recipient));
}

int recieve(int sockfd, int ttl) {
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(sockfd, &descriptors);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int time_ms = 0;
    int count = 0;
    char sender_ip[3][20];

    bool done = false;

    while (true) {
        int ready = select(sockfd + 1, &descriptors, NULL, NULL, &tv);
        if (ready < 0) {
            fprintf(stderr, "select() error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (ready == 0) {
            if (count == 0) {
                printf("%d. *\n", ttl);
            } else {
                printf("%d. ", ttl);
                printf("%s ", sender_ip[0]);

                if (count > 1 && strcmp(sender_ip[0], sender_ip[1]) != 0) {
                    printf("%s ", sender_ip[1]);
                }

                printf("???\n");
            }

            return 0;
        }

        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);
        u_int8_t buffer[IP_MAXPACKET];

        ssize_t packet_len = recvfrom(sockfd, buffer, IP_MAXPACKET, MSG_DONTWAIT,
                                      (struct sockaddr *)&sender, &sender_len);

        if (packet_len < 0) {
            fprintf(stderr, "recvfrom error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        struct ip *ip_header = (struct ip *)buffer;
        ssize_t ip_header_len = 4 * ip_header->ip_hl;

        u_int8_t *icmp_packet = buffer + 4 * ip_header->ip_hl;
        struct icmp *icmp_header = (struct icmp *)icmp_packet;

        if (icmp_header->icmp_type == ICMP_TIME_EXCEEDED) {
            icmp_header = (struct icmp *)(buffer + 2 * ip_header_len + 8);
        } else {
            done = true;
        }

        if (icmp_header->icmp_hun.ih_idseq.icd_id == (getpid() & 0xFFFF) &&
            icmp_header->icmp_hun.ih_idseq.icd_seq / 3 == ttl) {

            inet_ntop(AF_INET, &(sender.sin_addr), sender_ip[count],
                      sizeof(sender_ip[count]));
            count++;
            time_ms += 1000 - (tv.tv_sec * 1000 + tv.tv_usec / 1000);
        }

        if (count == 3) {
            printf("%d. ", ttl);
            printf("%s ", sender_ip[0]);
            if (strcmp(sender_ip[0], sender_ip[1]) != 0) {
                printf("%s ", sender_ip[1]);
            }

            if ((strcmp(sender_ip[0], sender_ip[2]) != 0) &&
                (strcmp(sender_ip[1], sender_ip[2]) != 0)) {
                printf("%s ", sender_ip[2]);
            }

            printf("%dms\n", time_ms / 3);
            return done;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Invalid usage. Use ./traceroute IP. %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        fprintf(stderr, "socket error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    struct sockaddr_in recipient;
    bzero(&recipient, sizeof(recipient));
    recipient.sin_family = AF_INET;
    int inet_pton_result = inet_pton(AF_INET, argv[1], &recipient.sin_addr);
    if (inet_pton_result < 0) {
        fprintf(stderr, "inet_pton error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    for (int ttl = 1; ttl <= 30; ttl++) {
        setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(int));

        send(sockfd, recipient, ttl * 3);
        send(sockfd, recipient, ttl * 3 + 1);
        send(sockfd, recipient, ttl * 3 + 2);

        if (recieve(sockfd, ttl) == 1) {
            break;
        }
    }
}