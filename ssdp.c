#include <stdio.h> // printf, perror ...
#include <stdlib.h> // EXIT_*
#include <stdint.h> // uint*
#include <string.h> // strlen, strcmp, memset
#include <netinet/in.h> // struct sock*
#include <arpa/inet.h> // inet_addr
#include <ifaddrs.h> // getifaddrs
#include <unistd.h> // close
#include <time.h> // time_t
#include <math.h> // pow()
#include <stdbool.h> // bool
/* Libs for socket */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/* METHODS SIGNATURES */

// Prints network infos from parameter
void print_network_interface(struct ifaddrs *);

// Returns first non lo network interface
struct ifaddrs *get_network_interface(void);

// Returns the maximum number of possible interfaces with the given struct ifaddrs in parameter
const uint32_t get_number_of_interfaces_in(const struct ifaddrs *);

// Returns true if the two struct sockaddr_in given in parameter are equal
bool sockaddr_in_equal(const struct sockaddr_in, const struct sockaddr_in);

// Returns true if the struct sockaddr_in and struct in_addr given in parameter are equal
bool in_addr_equal(const struct in_addr, const struct in_addr);

// Filter the struct sockaddr_in given parameter (i.e del replicate, remove src addr ...)
int sockaddr_in_filter(struct sockaddr_in **, const int, const struct in_addr *);

// Returns M-SEARCH request
const char *get_msearch_request(void);

/*
  Compilation: gcc -g -O3 ssdp.c -o ssdp -lm
  Documentation: https://en.wikipedia.org/wiki/Simple_Service_Discovery_Protocol
*/
int main(void) {
  // Constants
  const uint16_t SSDP_PORT = 1900;
  const char *SSDP_IP_MULTICAST = "239.255.255.250";
  const char *CLEAN_CONSOLE = "\e[1;1H\e[2J";
  const long int MAX_TIME = 10; // 10 seconds
  const long int MAX_SIZE = (1 << (10)); // 1024 bytes

  // Variables
  int status = EXIT_FAILURE,
    udp_socket = 0,
    err = 0,
    counter = 0;
  const char *request = NULL;
  time_t start_t = 0, end_t = 0;

  struct ifaddrs *interface = get_network_interface();
  const uint32_t max_interface = get_number_of_interfaces_in(interface);
  char buffer[max_interface][MAX_SIZE];
  struct sockaddr *src_sock = interface->ifa_addr;
  struct sockaddr_in multicast_sock_in,
    src_sock_in,
    *temporary_sock_in = NULL,
    *responds_sock_in = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in) * max_interface);
  struct in_addr src_interface;
  struct ip_mreq multicast_group;
  socklen_t size_sockaddr_in = sizeof(struct sockaddr_in);

  //////////////////////////////////////////////////////////////////////

  // Creates UDP socket
  udp_socket = socket(AF_INET, // IPv4
		      SOCK_DGRAM, // UDP
		      IPPROTO_UDP); // Protocol to use

  // Enables SO_REUSEADDR using setsockopt
  err = setsockopt(udp_socket, // manipulates socket udp_socket
		   SOL_SOCKET, // sockets API level
		   SO_REUSEADDR, // option name
		   &(int){ 1 }, // set option value
		   sizeof(int)); // sizeof option value

  if (err == -1) {
    perror("Error: setsockopt(..., SO_REUSEADDR, ...).\n");
    close(udp_socket);
    exit(status);
  }

  // Disables loopback (won't receive our own datagram)
  err = setsockopt(udp_socket, // manipulates socket udp_socket
		   IPPROTO_IP, // option interpreted by IP
		   IP_MULTICAST_LOOP, // option name
		   &(int){ 0 }, // set option value
		   sizeof(int)); // sizeof option value

  if (err == -1) {
    perror("Error: setsockopt(..., IP_MULTICAST_LOOP, ...).\n");
    close(udp_socket);
    exit(status);
  }

  // Initializes and sets multicast_sock
  memset(&multicast_sock_in, 0x0, sizeof(struct sockaddr_in));
  multicast_sock_in.sin_family = AF_INET; // IPv4
  multicast_sock_in.sin_port = htons(SSDP_PORT); // no port
  multicast_sock_in.sin_addr.s_addr = inet_addr(SSDP_IP_MULTICAST); // IPv4 multicast addr

  // Sets interface multicast datagrams sould be sent from. ( -> source addr)
  memset(&src_interface, 0x0, sizeof(struct in_addr));
  
  // Casts struct sockaddr * to struct sockaddr_in * then sets src_interface
  temporary_sock_in = (struct sockaddr_in *)src_sock;
  src_interface.s_addr = temporary_sock_in->sin_addr.s_addr;

  // Sets socket's option IP_MULTICAST_IF (specifies default interface multicast datagrams sould be sent from)
  err = setsockopt(udp_socket, // manipulates socket udp_socket
		   IPPROTO_IP, // option interpreted by IP
		   IP_MULTICAST_IF, // option name
		   &src_interface, // set option value
		   sizeof(src_interface)); // sizeof option value

  if (err == -1) {
    perror("Error: setsockopt(..., IP_MULTICAST_IF, ...).\n");
    close(udp_socket);
    exit(status);
  }

  // Set and join multicast group using socket's option IP_ADD_MEMBERSHIP
  memset(&multicast_group, 0x0, sizeof(struct ip_mreq));
  multicast_group.imr_multiaddr.s_addr = inet_addr(SSDP_IP_MULTICAST);
  multicast_group.imr_interface.s_addr = src_interface.s_addr;
  
  err = setsockopt(udp_socket, // manipulates socket udp_socket
		   IPPROTO_IP, // option interpreted by IP
		   IP_ADD_MEMBERSHIP, // option name
		   &multicast_group, // set option value
		   sizeof(multicast_group)); // sizeof option value

  if (err == -1) {
    perror("Error: setsockopt(..., IP_ADD_MEMBERSHIP, ...).\n");
    close(udp_socket);
    exit(status);
  }

  // Sets src_sock_in then bind udp_socket
  memset(&src_sock_in, 0x0, sizeof(struct sockaddr_in));
  src_sock_in.sin_family = AF_INET; // IPv4
  src_sock_in.sin_port = htons(SSDP_PORT); // sets no_port
  src_sock_in.sin_addr.s_addr = INADDR_ANY; // ~0

  // Assigns address specified by src_sock_in to socket udp_socket
  err = bind(udp_socket,
	     (struct sockaddr *)&src_sock_in,
	     sizeof(src_sock_in));

  if (err == -1) {
    perror("Error: bind(socket).\n");
    close(udp_socket);
    free(src_sock);
    free(interface->ifa_netmask);
    freeifaddrs(interface);
    exit(status);
  }
  
  // Sends request to the multicast group
  request = get_msearch_request();
  err = sendto(udp_socket, // sending socket
	       request, // ~ 'message'
	       (strlen(request) + 1), // length of request
	       0, // flag
	       (struct sockaddr *)&multicast_sock_in, // destination
	       sizeof(multicast_sock_in));

  if (err == -1) {
    perror("Error: sendto(socket).\n");
    close(udp_socket);
    free(src_sock);
    free(interface->ifa_netmask);
    freeifaddrs(interface);
    exit(status);
  }

  // Prints request sent
  printf("SSDP request sent successfully!\n\n");

  // Set the program on 'server mod', waits for answers during MAX_TIME seconds then continue
  time(&start_t);
  end_t = start_t + MAX_TIME;
  
  printf("Start 'SERVER MOD' for %lus.\n", MAX_TIME);
  printf("Number maximum of answers: %d.\n", max_interface);
  printf("Waiting for answers...\n");

  /*
    / ! \ combo recvfrom + time, not accurate enough
  */
  while ((start_t < end_t) && (counter < max_interface)) {
    err = recvfrom(udp_socket,
		   buffer[counter],
		   MAX_SIZE,
		   0,
		   (struct sockaddr *)&responds_sock_in[counter],
		   &size_sockaddr_in);
    
    if (err == -1) {
      perror("Error: recvfrom(socket).\n");
      close(udp_socket);
      exit(status);
    } else {
      printf("Received 1 answer from %s.\n", inet_ntoa(responds_sock_in[counter].sin_addr));
      memset(&buffer[counter], 0x0, MAX_SIZE);
      counter++;
    }
    time(&start_t);
  }
  printf("Stop 'SERVER MOD'.\n");
  printf("%d/%d answer(s) received!\n", counter, max_interface);

  //wait 1s before cleaning sdtout
  sleep(1);
  err = write(STDOUT_FILENO, CLEAN_CONSOLE, (strlen(CLEAN_CONSOLE) + 1));

  if (err == -1) {
    perror("Error: write(..., CLEAN_CONSOLE, ...).\n");
    close(udp_socket);
    free(src_sock);
    free(interface->ifa_netmask);
    freeifaddrs(interface);
    free(responds_sock_in);
    exit(status);
  }

  // Filters the list responds_sock_in and returns the new size
  counter = sockaddr_in_filter(&responds_sock_in, counter, &src_interface);

  // Displays address
  printf("Found %d different(s) adress:\n", counter);
  for(int i = 0; i < counter; i++) printf("\t%s\n",inet_ntoa(responds_sock_in[i].sin_addr));
  
  // Closes UDP socket
  err = close(udp_socket);

  if (err == -1) {
    perror("Error: close(socket).\n");
    free(src_sock);
    free(interface->ifa_netmask);
    freeifaddrs(interface);
    free(responds_sock_in);
    exit(status);
  }

  // Free(s)
  free(src_sock);
  free(interface->ifa_netmask);
  freeifaddrs(interface);
  free(responds_sock_in);
  
  // End
  status = EXIT_SUCCESS;
  return status;
}

/* METHODS DECLARATIONS */

// Prints network infos from parameter interface
void print_network_interface(struct ifaddrs *interface) {
  // Variables
  int status = 0;
  char address[NI_MAXHOST], netmask[NI_MAXHOST];

  // address-to-name translation
  status = getnameinfo(interface->ifa_addr,
		       sizeof(struct sockaddr_in),
		       address,
		       NI_MAXHOST,
		       NULL,
		       0,
		       NI_NUMERICHOST // Numeric form of the hostname is returned
		       );

  // checks if getnameinfo worked
  if (status != 0) {
    perror("Error: getnameinfo(ifa_addr, ...).\n");
    exit(EXIT_FAILURE);
  }

  status = getnameinfo(interface->ifa_netmask,
		       sizeof(struct sockaddr_in),
		       netmask,
		       NI_MAXHOST,
		       NULL,
		       0,
		       NI_NUMERICHOST // Numeric form of the hostname is returned
		       );

  // checks if getnameinfo worked
  if (status != 0) {
    perror("Error: getnameinfo(ifa_netmask, ...).\n");
    exit(EXIT_FAILURE);
  }
    
  printf("Interface's informations:\n");
  printf("\t-Name: %s\n", interface->ifa_name);
  printf("\t-IP address: %s\n", address);
  printf("\t-Netmask: %s\n", netmask);
}

// Returns first non lo network interface or null ptr if error
struct ifaddrs *get_network_interface(void) {
  // Constants
  const char *LO = "lo";
  
  // Variables
  struct ifaddrs *interface_addrs = NULL, *iterate_ptr = NULL;
  struct ifaddrs *result_ptr = NULL;
  int status = 0;

  status = getifaddrs(&interface_addrs);

  // Checks if getifaddrs worked correctly
  if (status == -1) {
    perror("Error: getifaddrs(...).\n");
    exit(EXIT_FAILURE);
  }

  iterate_ptr = interface_addrs;
  // Iterates through interface_addrs
  while (iterate_ptr != NULL) {

    // Checks if interface_addrs->ifa_addr is different from constant LO
    if ((strcmp(iterate_ptr->ifa_name, LO) != 0)
	&& (iterate_ptr->ifa_addr->sa_family == AF_INET)) {

      // Prints interface's informations
      print_network_interface(iterate_ptr);
      
      // Sets resul_ptr
      result_ptr = (struct ifaddrs *)malloc(sizeof(struct ifaddrs));
      result_ptr->ifa_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
      result_ptr->ifa_netmask = (struct sockaddr *)malloc(sizeof(struct sockaddr));
      
      memcpy(result_ptr->ifa_addr, iterate_ptr->ifa_addr, sizeof(struct sockaddr));
      memcpy(result_ptr->ifa_netmask, iterate_ptr->ifa_netmask, sizeof(struct sockaddr));
      result_ptr->ifa_next = NULL;
      break;
    }
    iterate_ptr = iterate_ptr->ifa_next;
  }

  // Checks if result_ptr is empty if so then error
  if (result_ptr == NULL) {
    perror("Error: get_network_interfaces(...).\n");
    exit(EXIT_FAILURE);
  }

  // Free
  freeifaddrs(interface_addrs);
  
  // Returns value in result_ptr
  return result_ptr;
}

// Returns the maximum number of possible interfaces with the given struct ifaddrs in parameter
const uint32_t get_number_of_interfaces_in(const struct ifaddrs *interface) {
  // Variables and constants
  int status = 0;
  char netmask[NI_MAXHOST];
  struct in_addr in_netmask;
  const char *full = "255.255.255.255";
  const uint32_t max_bits = 32, // 32 bits -> IPv4
    bit = 2; // 2 possibilities: 0 or 1
  uint32_t netmask_t = 0;

  status = getnameinfo(interface->ifa_netmask,
		       sizeof(struct sockaddr_in),
		       netmask,
		       NI_MAXHOST,
		       NULL,
		       0,
		       NI_NUMERICHOST // Numeric form of the hostname is returned
		       );

  if (status == -1) {
    perror("Error: getnameinfo(...) in get_number_of_interfaces.\n");
    exit(status);
  }

  status = inet_aton(netmask, &in_netmask);

  if (status == -1) {
    perror("Error: inet_aton(...) in get_number_of_interfaces.\n");
    exit(status);
  }

  // count how many bit at '1' there is in netmask_t
  netmask_t = in_netmask.s_addr;
  netmask_t = netmask_t - ((netmask_t >> 1) & 0x55555555);
  netmask_t = (netmask_t & 0x33333333) + ((netmask_t >> 2) & 0x33333333);
  netmask_t = (((netmask_t + (netmask_t >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
  
  return (uint32_t)(pow((double)bit, (double)(max_bits - netmask_t)) - 2);
}

// Returns true if the two struct sockaddr_in given in parameter are equal
bool sockaddr_in_equal(const struct sockaddr_in sock_addr1, const struct sockaddr_in sock_addr2) {
  if (sock_addr1.sin_family != sock_addr2.sin_family) return false;

  if (sock_addr1.sin_addr.s_addr != sock_addr2.sin_addr.s_addr) return false;

  const char *ip_addr1 = inet_ntoa(sock_addr1.sin_addr);
  const char *ip_addr2 = inet_ntoa(sock_addr2.sin_addr);
  
  if (strcmp(ip_addr1, ip_addr2) != 0) return false;
  
  return true;
}

// Returns true if the struct sockaddr_in and struct in_addr given in parameter are equal
bool in_addr_equal(const struct in_addr in_addr1, const struct in_addr in_addr2) {

  const char *ip_addr1 = inet_ntoa(in_addr1);
  const char *ip_addr2 = inet_ntoa(in_addr2);
  printf("in_addr_equal1 from %s.\n", ip_addr1);
  printf("in_addr_equal2 from %s.\n", ip_addr2);
  int v = strcmp(ip_addr1, ip_addr2);
  printf("strcmp: %d\n", v);
  if (v != 0) {
    return false;
  }

  return true;
}

// Filter the struct sockaddr_in given parameter (i.e del replicate ...)
int sockaddr_in_filter(struct sockaddr_in **list, const int max_size, const struct in_addr * except) {
  struct sockaddr_in *history = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in) *max_size),
    **temp = NULL;
  int index = 0,
    status = EXIT_FAILURE,
    history_index = 1;
  const char *src_ip = inet_ntoa(*except);
  bool exist = false;
  
  if (max_size == 0 || list == NULL) {
    perror("Error: filter null list!\n");
    free(list);
    exit(status);
  }
  
  history[0] = (*list)[0];
  
  for (int x = 1; x < max_size; x++) {
    exist = false;
    for (int c = 0; c < history_index && !exist; c++) {
      if (sockaddr_in_equal(history[c], (*list)[x]) ||
	  strcmp(src_ip, inet_ntoa((*list)[x].sin_addr)) == 0) {
	exist = true;
      }
    }
    if (!exist) {
      history[history_index] = (*list)[x];
      history_index++;
    }
  }

  free(*list);
  *list = malloc(sizeof(struct sockaddr_in) * history_index);
  
  for (int x = 0; x < history_index; x++) (*list)[x] = history[x];
  
  free(history);

  return history_index;
}

// Returns M-SEARCH request
const char *get_msearch_request(void) {
  return "M-SEARCH * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "MAN: \"ssdp:discover\"\r\n" 
    "MX: 2\r\n" // second to delay response
    "ST: ssdp:all\r\n\r\n"; // Search Target
}
