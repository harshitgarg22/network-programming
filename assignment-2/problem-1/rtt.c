#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <signal.h>
#include <sys/time.h>

#define datalen 56
#define	BUFSIZE	1500


void tv_sub(struct timeval *out, struct timeval *in)
{
	if ( (out->tv_usec -= in->tv_usec) < 0) {	/* out -= in */
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

void exit_protocol(char* reason){
    printf("Problem encountered in %s. Exiting\n", reason);
    exit(1);
}

uint16_t in_cksum(uint16_t *addr, int len)
{
	int nleft = len;
	uint32_t sum = 0;
	uint16_t *w = addr;
	uint16_t answer = 0;
	/*
	* Our algorithm is simple, using a 32 bit accumulator (sum), we add
	* sequential 16 bit words to it, and at the end, fold back all the
	* carry bits from the top 16 bits into the lower 16 bits.
	*/
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}
	/* mop up an odd byte, if necessary */
	if (nleft == 1)
	{
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}
	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
	sum += (sum >> 16);					/* add carry */
	answer = ~sum;						/* truncate to 16 bits */
	return (answer);
}

void send_ipv6(char* sendbuf, int seq, int sockfd, struct sockaddr* sasend, socklen_t salen){
	int	len;
	struct icmp6_hdr *icmp6;

	icmp6 = (struct icmp6_hdr *) sendbuf;
	icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
	icmp6->icmp6_code = 0;
	icmp6->icmp6_id = getpid();
	icmp6->icmp6_seq = seq;
	gettimeofday((struct timeval *) (icmp6 + 1), NULL);

	len = 8 + datalen;		/* 8-byte ICMPv6 header */

	sendto(sockfd, sendbuf, len, 0, sasend, salen);
		/* kernel calculates and stores checksum for us */
}

double rtt_from_resp6(char* ptr, int len, struct timeval* tvrecv){
	int	hlen1, icmp6len;
	double rtt;
	struct ip6_hdr *ip6;
	struct icmp6_hdr *icmp6;
	struct timeval *tvsend;

	ip6 = (struct ip6_hdr *) ptr;		/* start of IPv6 header */
	hlen1 = sizeof(struct ip6_hdr);
	if (ip6->ip6_nxt != IPPROTO_ICMPV6)
		exit_protocol("header (not IPPROTO_ICMPV6)");

	icmp6 = (struct icmp6_hdr *) (ptr + hlen1);
	if ( (icmp6len = len - hlen1) < 8)
		exit_protocol("icmp6 response length (<8)");

	if (icmp6->icmp6_type == ICMP6_ECHO_REPLY) {
		if (icmp6->icmp6_id != getpid())
			return -1;			/* not a response to our ECHO_REQUEST */
		if (icmp6len < 16)
			exit_protocol("icmp6 response length (<16)");

		tvsend = (struct timeval *) (icmp6 + 1);
		tv_sub(tvrecv, tvsend);
		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;
		return rtt;
	}
	return -1;
}

void send_ipv4(char* sendbuf, int seq, int sockfd, struct sockaddr* sasend, socklen_t salen){
	int len;
	struct icmp	*icmp;

	icmp = (struct icmp *) sendbuf;
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_id = getpid();
	icmp->icmp_seq = 0;
	gettimeofday((struct timeval *) icmp->icmp_data, NULL);

	len = 8 + datalen;		/* checksum ICMP header and data */
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum((u_short *) icmp, len);

	sendto(sockfd, sendbuf, len, 0, sasend, salen);
}

double rtt_from_resp4(char* ptr, int len, struct timeval* tvrecv){
	int	hlen1, icmplen;
	double rtt;
	struct ip *ip;
	struct icmp	*icmp;
	struct timeval *tvsendptr;

	ip = (struct ip *) ptr;		/* start of IP header */
	hlen1 = ip->ip_hl << 2;		/* length of IP header */
	icmp = (struct icmp *) (ptr + hlen1);	/* start of ICMP header */
	if ( (icmplen = len - hlen1) < 8)
		exit_protocol("icmp response length (less than 8)");

	if (icmp->icmp_type == ICMP_ECHOREPLY) {
		if (icmp->icmp_id != getpid())
			return -1;		/* not a response to our ECHO_REQUEST */
		if (icmplen < 16)
			exit_protocol("icmp response length (less than 16)");

		tvsendptr = (struct timeval *) icmp->icmp_data;
		tv_sub(tvrecv, tvsendptr);
		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;
		return rtt;
	}
	return -1;
}

void print_rtts(char* target_ip, struct addrinfo *ai){
	char sendbuf[BUFSIZE];
	int pid  = getpid();
	double rtt1 = 0, rtt2 = 0, rtt3 = 0;
	if (ai->ai_family == AF_INET){
		
		int sockfd = socket(ai->ai_family, SOCK_RAW, IPPROTO_ICMP);
		if (sockfd < 0){
			perror ("socket");
			printf ("Please ensure that you are using sudo\n");
			exit_protocol ("socket()");
		}
		setuid(getuid());
		int size = 60 * 1024;		/* OK if setsockopt fails */
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
		
		int seq = 0;

		// RTT 1
		seq = 0;
		send_ipv4(sendbuf, seq, sockfd, ai->ai_addr, ai->ai_addrlen);

		struct timeval tvrecv;
		for ( ; ; ) {
			int len = ai->ai_addrlen;
			struct sockaddr_in src_addr;
			char recvbuf[BUFSIZE];
			int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&src_addr, &len);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				else{
					perror("recvfrom");
					exit_protocol("recvfrom");
				}
			}
			gettimeofday(&tvrecv, NULL);
			
			if ((rtt1 = rtt_from_resp4(recvbuf, n, &tvrecv)) > 0)
				break;
		}
		// RTT 2
		seq = 0;
		send_ipv4(sendbuf, seq, sockfd, ai->ai_addr, ai->ai_addrlen);

		for ( ; ; ) {
			int len = ai->ai_addrlen;
			struct sockaddr_in src_addr;
			char recvbuf[BUFSIZE];
			int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&src_addr, &len);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				else{
					perror("recvfrom");
					exit_protocol("recvfrom");
				}
			}
			gettimeofday(&tvrecv, NULL);
			
			if ((rtt2 = rtt_from_resp4(recvbuf, n, &tvrecv)) > 0)
				break;
		}
	
		// RTT 3
		seq = 0;
		send_ipv4(sendbuf, seq, sockfd, ai->ai_addr, ai->ai_addrlen);

		for ( ; ; ) {
			int len = ai->ai_addrlen;
			struct sockaddr_in src_addr;
			char recvbuf[BUFSIZE];
			int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&src_addr, &len);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				else{
					perror("recvfrom");
					exit_protocol("recvfrom");
				}
			}
			gettimeofday(&tvrecv, NULL);
			
			if ((rtt3 = rtt_from_resp4(recvbuf, n, &tvrecv)) > 0)
				break;
		}

		printf ("%s %lf ms %lf ms %lf ms \n", target_ip, rtt1, rtt2, rtt3);
	}	
	else if (ai->ai_family == AF_INET6){
		pid  = getpid();
		// datalen = 56;
	
		int sockfd = socket(ai->ai_family, SOCK_RAW, IPPROTO_ICMPV6);
		if (sockfd < 0){
			perror ("socket");
			printf ("Please ensure that you are using sudo\n");
			exit_protocol ("socket()");
		}
		setuid(getuid());
		int size = 60 * 1024;		/* OK if setsockopt fails */
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

		int seq;

		// send here
		seq = 0;
		send_ipv6(sendbuf, seq, sockfd, ai->ai_addr, ai->ai_addrlen);

		// recv here
		for ( ; ; ) {
			int len = ai->ai_addrlen;
			struct sockaddr_in6* sarecv;
			char recvbuf[BUFSIZE];
			int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*) sarecv, &len);
			if (n < 0) {
				exit_protocol("recvfrom");
			}
			struct timeval tvalrecv;
			gettimeofday(&tvalrecv, NULL);
			if ((rtt1 = rtt_from_resp6(recvbuf, n, &tvalrecv)) > 0)
				break;
		}

		// send here
		seq = 1;
		send_ipv6(sendbuf, seq, sockfd, ai->ai_addr, ai->ai_addrlen);

		// recv here
		for ( ; ; ) {
			int len = ai->ai_addrlen;
			struct sockaddr_in6* sarecv;
			char recvbuf[BUFSIZE];
			int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*) sarecv, &len);
			if (n < 0) {
				exit_protocol("recvfrom");
			}
			struct timeval tvalrecv;
			gettimeofday(&tvalrecv, NULL);
			if ((rtt1 = rtt_from_resp6(recvbuf, n, &tvalrecv)) > 0)
				break;
		}

		// send here
		seq = 2;
		send_ipv6(sendbuf, seq, sockfd, ai->ai_addr, ai->ai_addrlen);

		// recv here
		for ( ; ; ) {
			int len = ai->ai_addrlen;
			struct sockaddr_in6* sarecv;
			char recvbuf[BUFSIZE];
			int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*) sarecv, &len);
			if (n < 0) {
				exit_protocol("recvfrom");
			}
			struct timeval tvalrecv;
			gettimeofday(&tvalrecv, NULL);
			if ((rtt1 = rtt_from_resp6(recvbuf, n, &tvalrecv)) > 0)
				break;
		}
	}
	else{
		printf("unknown address family. Exiting\n");
		exit(1);
	}
}

int main(int argc, char* argv[]){
    if (argc!=2){
        printf("usage: ./rtt filename\n");
        exit(1);
    }

	FILE* fips = fopen (argv[1],"r");
    if (fips == NULL){
        printf ("Error opening file with list of IP addresses\n");
        exit (1);
    }

	char* line = NULL;
    size_t bytes_read;
    ssize_t n = 0;
    while ((bytes_read = getline(&line, &n, fips)) != -1) {        
        if (line[bytes_read-1] == '\n')
            line[bytes_read-1] = '\0';
		int pid = fork();
		if (pid == 0){
			int n;
			struct addrinfo hints, *res;
			bzero (&hints, sizeof (struct addrinfo));
			if ( (n = getaddrinfo(line, NULL, &hints, &res)) != 0)
				exit_protocol("getaddrinfo");
			print_rtts (line, res);
			free(line);
			exit(1);
		}
		else if (pid < 0) {
			exit_protocol("fork()");
		}
        free(line);
    }

    return 0;
}
