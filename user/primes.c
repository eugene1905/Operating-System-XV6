#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void solve(int fd){
    int p[2];
    int prime;
    int ret = read(fd, &prime, sizeof(prime));
    if (ret == 0){
    	close(fd);
    	return;
    }
    /* prime number */
    printf("prime %d\n", prime);
    pipe(p);
    ret = fork();
    if(ret == 0){ // child process
        close(fd);
        close(p[1]);
        solve(p[0]);
        close(p[0]);
    }
    else
    {
        close(p[0]);
        do
        {
    	    int n;
	    ret = read(fd, &n, sizeof(n));
	    if(ret == 0) break;
	    /* pass to right neighbour*/
	    if (n % prime) 
	    {
	        write(p[1], &n, sizeof(n));    
	    } 
        }while(ret);
        close(p[1]);
        close(fd);
        wait(0);
    }
}

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    for(int i = 2; i <= 31; i++)
    {
        write(p[1], &i, sizeof(i));
    }
    close(p[1]);
    solve(p[0]);
    exit(0);
}
