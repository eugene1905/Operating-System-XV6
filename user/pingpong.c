#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char ch;
    pipe(p2c);
    pipe(c2p);
    if(fork() == 0) // child process
    {
        read(p2c[0], &ch, 1);
        printf("%d: received ping\n", getpid());
        write(c2p[1], &ch, 1);
    }
    else
    {
    	write(p2c[1], &ch, 1);
    	read(c2p[0], &ch, 1);
    	printf("%d: received pong\n",getpid());
    }
    exit(0);
}
