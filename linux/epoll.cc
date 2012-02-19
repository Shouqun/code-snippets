#include <sys/epoll.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

//C++ headers
#include <map>
#include <string>
#define MAX_EVENTS 10

typedef struct client {
  int fd;
  char *buffer;
  int buffer_len;
  int data_len;
}client;

std::map<int, client*> clients_map;


int set_nonblocking(int fd)
{
  int flags, ret;
  //set non-blocking
  if ( (flags = fcntl(fd, F_GETFL, 0)) < 0) {
    perror("Error get fd flags\n");
    return -1;
  }

  flags |= O_NONBLOCK;
  if ( (ret = fcntl(fd, F_SETFL, 0)) <0 ) {
    perror("Error set fd flags\n");
    return -1;
  }
  
  return 0;
}

int create_listen_fd()
{
  int fd = -1;
  struct sockaddr_in  local_addr, peer_addr;
  int flags, ret;
  int set = 1;

  fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0 ) {
    perror("Error create socket\n");
    return fd;
  }

  //set the socket to reuse addr
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) <0 ) {
    perror("Error setsockopt\n");
    close(fd);
    return -1;
  }

  memset(&local_addr, 0, sizeof(local_addr));
  local_addr.sin_family = AF_INET;
  local_addr.sin_addr.s_addr = INADDR_ANY;
  local_addr.sin_port = htons(8000);

  if (bind(fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) <0 ) {
    perror("Error bind to local port\n");
    close(fd);
    return -1;
  }
 
  if (set_nonblocking(fd) < 0) {
    perror("Error setNonblocking");
    close(fd);
    return -1;
  }
  return fd;
}

void check_and_exit(const char *err, ...)
{
  va_list args;
  
  printf("%s:", err);
  perror("");

  va_start(args, err);
  
  int arg_count = va_arg(args, int);
  for (int i=0; i<arg_count; i++) {
    int fd = va_arg(args, int);
    if (fd > 0)
      close(fd);
  }
  va_end(args);

  exit(1);
}

void handle_event(int epollfd, epoll_event event)
{
  client* pclient = NULL;
  
  if ( clients_map.find(event.data.fd) == clients_map.end())
    return;

  pclient = clients_map[event.data.fd];

  if(pclient->buffer == NULL) {
    pclient->buffer = new char[1024];
    pclient->buffer_len = 1024;
    pclient->data_len = 0;
  }
  
  if (event.events & EPOLLIN) {
    printf("EPOLLIN event: %d\n", event.data.fd);
    
    int count = read(event.data.fd, pclient->buffer, pclient->buffer_len);
    if (count == -1) {
      if (errno != EAGAIN) {
        perror("Error read\n");
      }
      printf("Error\n");
    } else if (count == 0) {
      //peer closed
      printf("Peer closed\n");
      if (epoll_ctl(epollfd, EPOLL_CTL_DEL, event.data.fd, NULL) < 0) {
        perror("Error delete event fd\n");
      }
      close(event.data.fd);
      
      if (clients_map.find(event.data.fd) != clients_map.end()) {
        delete[] pclient->buffer;
        delete pclient;
        clients_map.erase(event.data.fd);
      }
      return;
    } else {
      //success read
      pclient->data_len = count; 
    }

    // watch peer's write status
    event.events |= EPOLLOUT;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, event.data.fd, &event) < 0) {
      perror("Error add EPOLLOUT\n");
    }

  } else if (event.events & EPOLLOUT) {
    printf("EPOLLOUT events received\n");
    
    if (pclient->data_len > 0) {
      int ret = write(event.data.fd, pclient->buffer, pclient->data_len);
      if (ret < 0) {
        perror("Error write\n");
      }
      
      //reset the fd to receive data
      event.events = EPOLLIN;
      if (epoll_ctl(epollfd, EPOLL_CTL_MOD, event.data.fd, &event) < 0) {
        perror("Error del EPOLLOUT\n");
      }

    }
  }
  return;
}

int main(int argc, char **argv)
{
  struct epoll_event ev, events[MAX_EVENTS];
  int sockfd, epollfd;
  int nfds;
  int ret;

  sockfd = create_listen_fd();
  if (sockfd < 0) {
    return 1;
  }

  ret = listen(sockfd, SOMAXCONN);
  if (ret<0) {
    check_and_exit("Error listen on fd\n", 1, sockfd);
  }
  
  epollfd = epoll_create(10);
  if ( epollfd <0 ) {
    check_and_exit("Error epoll_create\n",1, sockfd);
  }

  ev.events = EPOLLIN;
  ev.data.fd = sockfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) <0) {
    check_and_exit("Error epoll_ctl\n", 2, sockfd, epollfd);
  }
  
  for( ; ;) {
    printf("Start polling...\n");
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds <0 && errno != EWOULDBLOCK) {
      // check if the error state is EWOULDBLOCK
      check_and_exit("Error epoll_wait\n", 2, sockfd, epollfd);
    }

    for (int i=0; i<nfds; i++) {
      if (events[i].data.fd == sockfd) {
        //handle listen event
        struct sockaddr peer;
        socklen_t addrlen;
        int peerfd = accept(sockfd, &peer, &addrlen);
        if (peerfd <0) {
          perror("Error accept\n");
          continue;
        }
       
        printf("Connection established\n");
        set_nonblocking(peerfd);

        //add peerfd to epoll queue, Edge trigger
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = peerfd;
        if ( epoll_ctl(epollfd, EPOLL_CTL_ADD, peerfd, &ev) <0) {
          perror("Error cpoll_ctl on peerfd\n");
          continue;
        }

        //add fd and client to the global map
        client *pclient = new client;
        pclient->fd = ev.data.fd;
        pclient->buffer = NULL;
        pclient->buffer_len = 0;
        
        clients_map.insert(std::make_pair(peerfd, pclient));

      } else {
        handle_event(epollfd, events[i]);
      }
    }
  }

  return 0;
}


