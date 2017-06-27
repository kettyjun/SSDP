#include <stdio.h> // printf, perror ...
#include <stdlib.h> // EXIT_*
#include <stdint.h> // uint*
#include <string.h> // strlen, strcmp
#include <netinet/in.h> // struct sock*
#include <arpa/inet.h> // inet_addr
#include <ifaddrs.h> // getifaddrs
/* Libs for socket */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/* METHODS SIGNATURES */

// Prints network infos from parameter
void print_network_interface(struct ifaddrs *);

// Returns first non lo interface
const struct sockaddr get_network_interfaces(void);

/*
  Compilation: gcc -g -O3 ssdp.c -o ssdp
  Documentation: https://en.wikipedia.org/wiki/Simple_Service_Discovery_Protocol
*/
int main(void) {
  // Constants
  const uint16_t SSDP_PORT = 1900;
  const char *SSDP_IP_MULTICAST = "239.255.255.250";

  // Variables
  int res = EXIT_FAILURE;
  const struct sockaddr src_addr = get_network_interfaces();
  
  // End
  res = EXIT_SUCCESS;
  return res;
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
  printf("\tInterface's name: %s\n", interface->ifa_name);
  printf("\tInterface's IP address: %s\n", address);
  printf("\tInterface's netmask: %s\n", netmask);
}

// Returns first non lo interface or null ptr if error
const struct sockaddr get_network_interfaces(void) {
  // Constants
  const char *LO = "lo";
  
  // Variables
  struct ifaddrs *interface_addrs = NULL, *iterate_ptr = NULL;
  struct sockaddr result_ptr;
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
      result_ptr = *iterate_ptr->ifa_addr;
      break;
    }
    iterate_ptr = iterate_ptr->ifa_next;
  }
  
  // Free interfaces_addrs
  freeifaddrs(interface_addrs);

  // Checks if result_ptr is empty if so then error
  char empty_sa_data[14];
  if ((result_ptr.sa_family == 0)
      && (strcmp(result_ptr.sa_data, empty_sa_data) == 0)) {
    perror("Error: get_network_interfaces(...).\n");
    exit(EXIT_FAILURE);
  }

  // Returns value in result_ptr
  return result_ptr;
}
