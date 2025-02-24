#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  int fd;

  if (argc != 3) {
    syslog(LOG_ERR,"This program needs 2 arguments\n");
    exit(1);
  }

  // open FILE
  fd = open(argv[1], O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);

  if(fd == -1) {
    syslog(LOG_ERR,"Cannot open file\n");
    exit(1);
  }

  syslog(LOG_DEBUG,"Writing %s to %s\n", argv[2], argv[1]);


  int nr = write(fd, argv[2], strlen(argv[2]));
  if(nr == -1) {
    syslog(LOG_ERR, "cannot write to file");
    exit(1);
  }
  close(fd);


  return EXIT_SUCCESS;
}
