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
#include <sys/epoll.h>
#define datalen 56
#define	BUFSIZE	1500

typedef struct pinginfo {
	char* target_ip;
	int sockfd;
	double rtt[3];
	int count;
	struct addrinfo* ai;
} PI;

void tv_sub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
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
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}
	if (nleft == 1)
	{
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
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

	len = 8 + datalen;

	sendto(sockfd, sendbuf, len, 0, sasend, salen);
}

double rtt_from_resp6(char* ptr, int len, struct timeval* tvrecv){
	int	hlen1, icmp6len;
	double rtt;
	struct ip6_hdr *ip6;
	struct icmp6_hdr *icmp6;
	struct timeval *tvsend;

	ip6 = (struct ip6_hdr *) ptr;
	hlen1 = sizeof(struct ip6_hdr);
	if (ip6->ip6_nxt != IPPROTO_ICMPV6)
		exit_protocol("header (not IPPROTO_ICMPV6)");

	icmp6 = (struct icmp6_hdr *) (ptr + hlen1);
	if ( (icmp6len = len - hlen1) < 8)
		exit_protocol("icmp6 response length (<8)");

	if (icmp6->icmp6_type == ICMP6_ECHO_REPLY) {
		if (icmp6->icmp6_id != getpid())
			return -1;
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

	len = 8 + datalen;
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

	ip = (struct ip *) ptr;	
	hlen1 = ip->ip_hl << 2;	
	icmp = (struct icmp *) (ptr + hlen1);
	if ( (icmplen = len - hlen1) < 8)
		exit_protocol("icmp response length (less than 8)");

	if (icmp->icmp_type == ICMP_ECHOREPLY) {
		if (icmp->icmp_id != getpid())
			return -1;
		if (icmplen < 16)
			exit_protocol("icmp response length (less than 16)");

		tvsendptr = (struct timeval *) icmp->icmp_data;
		tv_sub(tvrecv, tvsendptr);
		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;
		return rtt;
	}
	return -1;
}

int getsocket(struct addrinfo * ai){
	int sockfd;
	if (ai->ai_family == AF_INET){
		sockfd = socket(ai->ai_family, SOCK_RAW, IPPROTO_ICMP);
	}
	else if(ai->ai_family == AF_INET6){
		sockfd = socket(ai->ai_family, SOCK_RAW, IPPROTO_ICMPV6);
	}
	else{
		exit_protocol("address family not recognised");
	}
	if (sockfd < 0){
		perror ("socket");
		printf ("Please ensure that you are using sudo\n");
		exit_protocol ("socket()");
	}
	setuid(getuid());
	int size = 120 * 1024;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == -1){
		exit_protocol("connect");
	}
	return sockfd;
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
	int efd = epoll_create(20000);
	struct epoll_event ev;


	PI *pis = malloc(sizeof(PI) * 100);
	int pis_size = 100;
	char* line = NULL;
    size_t bytes_read;
    ssize_t n = 0;
	int count = 0;
    while ((bytes_read = getline(&line, &n, fips)) != -1) {        
        

		if (line[bytes_read-1] == '\n')
            line[bytes_read-1] = '\0';
		
		int n;
		struct addrinfo *res;
		if ( (n = getaddrinfo(line, NULL, 0, &res)) != 0)
			exit_protocol("getaddrinfo");
		pis[count].target_ip = line;
		pis[count].ai = res;
		pis[count].count = 0;
		int sockfd = getsocket(res);
		pis[count].sockfd = sockfd;
		char sendbuf[BUFSIZE];
		int seq = 0;

		if(res->ai_family == AF_INET){ 
			send_ipv4(sendbuf, seq++, sockfd, res->ai_addr, res->ai_addrlen);
			send_ipv4(sendbuf, seq++, sockfd, res->ai_addr, res->ai_addrlen);
			send_ipv4(sendbuf, seq, sockfd, res->ai_addr, res->ai_addrlen);
			// printf("sent 3\n");
		}
		else if(res->ai_family == AF_INET6){ 
			send_ipv6(sendbuf, seq++, sockfd, res->ai_addr, res->ai_addrlen);
			send_ipv6(sendbuf, seq++, sockfd, res->ai_addr, res->ai_addrlen);
			send_ipv6(sendbuf, seq, sockfd, res->ai_addr, res->ai_addrlen);
		}
		
		ev.data.fd = sockfd;
		ev.data.u32 = count;
        ev.events = EPOLLIN;
		if (epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &ev) == -1)
            exit_protocol("epoll_ctl");
		// printf("added %s: at idx %d, sockfd %d\n", pis[count].target_ip, count, sockfd);

		count++;
		if (pis_size <= count){
			pis = realloc (pis, pis_size*sizeof(PI)*2);
			if (pis == NULL)
				exit_protocol("memory allocation");
			pis_size = pis_size*2;
		}
		line = NULL;
	}
	
	struct epoll_event evlist[count];
	int num_evs = 0;

	
	for (;;){
		
		num_evs = epoll_wait(efd, evlist, count, -1);
        if (num_evs == -1){
            if (errno == EINTR)
                continue;
            else
                exit_protocol("epoll_wait");
		}
        else{ // wait successfully returns
			// printf("new %d polls\n", num_evs);
			for (int i = 0; i < num_evs; i++){
				int idx = evlist[i].data.u32;
				// printf("%s inquiry\n", pis[idx].target_ip);
				for ( ; ; ) {
					
					if (pis[idx].ai->ai_family == AF_INET){
						int len = pis[idx].ai->ai_addrlen;
						struct sockaddr_in src_addr;
						char recvbuf[BUFSIZE];
						int n = recvfrom(pis[idx].sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&src_addr, &len);
						if (n < 0) {
							if (errno == EINTR)
								continue;
							else{
								perror("recvfrom");
								exit_protocol("recvfrom");
							}
						}
						struct timeval tvrecv;
						gettimeofday(&tvrecv, NULL);
						
						if ((pis[idx].rtt[pis[idx].count] = rtt_from_resp4(recvbuf, n, &tvrecv)) < 0){
							continue;
						}
					}
					else if (pis[idx].ai->ai_family == AF_INET6){
						int len = pis[idx].ai->ai_addrlen;
						struct sockaddr_in6 src_addr;
						char recvbuf[BUFSIZE];
						int n = recvfrom(pis[idx].sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&src_addr, &len);
						if (n < 0) {
							if (errno == EINTR)
								continue;
							else{
								perror("recvfrom");
								exit_protocol("recvfrom");
							}
						}
						struct timeval tvrecv;
						gettimeofday(&tvrecv, NULL);
						
						if ((pis[idx].rtt[pis[idx].count] = rtt_from_resp6(recvbuf, n, &tvrecv)) < 0){
							continue;
						}
					}
					
					pis[idx].count++;
					if (pis[idx].count == 3){
						// print rtts and remove from active list
						printf("%s - %.3lf ms %.3lf ms %.3lf ms\n", pis[idx].target_ip, pis[idx].rtt[0], pis[idx].rtt[1], pis[idx].rtt[2]);
						if (epoll_ctl(efd, EPOLL_CTL_DEL, pis[idx].sockfd, &evlist[i]) == -1)
            				exit_protocol("epoll_ctl");
						count--;
					}
					break;
				}
			
			}
		
			if (count == 0){
				break;
			}
		}      
	}

	// close the fds here
    return 0;
}
