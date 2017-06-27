#include <stdio.h> // printf, perror ...
#include <stdlib.h> // EXIT_*
#include <stdint.h> // uint*
#include <string.h> // strlen, strcmp, memset
#include <netinet/in.h> // struct sock*
#include <arpa/inet.h> // inet_addr
#include <ifaddrs.h> // getifaddrs
#include <unistd.h> // close
/* Libs for socket */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/* METHODS SIGNATURES */

// Prints network infos from parameter
void print_network_interface(struct ifaddrs *);

// Returns first non lo network interface
const struct sockaddr *get_network_interfaces(void);

/*
  Compilation: gcc -g -O3 ssdp.c -o ssdp
  Documentation: https://en.wikipedia.org/wiki/Simple_Service_Discovery_Protocol
*/
int main(void) {
  // Constants
  const uint16_t SSDP_PORT = 1900;
  const char *SSDP_IP_MULTICAST = "239.255.255.250";

  // Variables
  int status = EXIT_FAILURE,
    udp_socket = 0,
    err = 0;
  const struct sockaddr *src_sock = get_network_interfaces();
  struct sockaddr_in multicast_sock_in,
    src_sock_in,
    *temporary_sock_in;
  struct in_addr src_interface;
  struct ip_mreq multicast_group;

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
  src_interface = temporary_sock_in->sin_addr; // <- VALGRIND ERROR 

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
    exit(status);
  }
		   
  // Closes UDP socket
  err = close(udp_socket);

  if (err == -1) {
    perror("Error: close(socket).\n");
    exit(status);
  }
  
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
const struct sockaddr *get_network_interfaces(void) {
  // Constants
  const char *LO = "lo";
  
  // Variables
  struct ifaddrs *interface_addrs = NULL, *iterate_ptr = NULL;
  struct sockaddr *result_ptr = NULL;
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
      result_ptr = iterate_ptr->ifa_addr;
      break;
    }
    iterate_ptr = iterate_ptr->ifa_next;
  }
  
  // Free interfaces_addrs
  freeifaddrs(interface_addrs);

  // Checks if result_ptr is empty if so then error
  if (result_ptr == NULL) {
    perror("Error: get_network_interfaces(...).\n");
    exit(EXIT_FAILURE);
  }

  // Returns value in result_ptr
  return result_ptr;
}
