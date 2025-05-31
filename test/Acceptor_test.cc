#include <iostream>
#include <unistd.h>

#include "InetAddress.h"
#include "Acceptor.h"
#include "EventLoop.h"

void newConnection(int sockfd, const InetAddress& peerAddr)
{
    printf("newConnection(): accepted a new connection from %s\n", peerAddr.toIpPort().c_str());
    ::write(sockfd, "How are you?\n", 13);
}

int main()
{
    printf("main(): pid = %d\n", getpid());

    InetAddress listenAddr(9981);
    EventLoop loop;

    Acceptor acceptor(&loop, listenAddr, true);
    acceptor.setNewConnectionCallback(newConnection);
    acceptor.listen();

    loop.loop();
}