#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

void help(){
   fprintf(stderr, "Usage: ./user structure_id\n "
                   "Supported structures id: \n "
                   "1 - info about tcp\n "
                   "2 - info about unix sockets \n");
}

int main(int argc, char *argv[]){
   if (argc != 2) help();
   if (argc < 2){
       fprintf(stderr, "Not enough arguments \n" );
       return 0;
   }
   if (argc > 2){
       fprintf(stderr, "Too many arguments \n" );
       return 0;
   }

   char *p;
   errno = 0;
   long structure_ID = strtol(argv[1], &p, 10);
   if (*p != '\0' || errno != 0){
       fprintf(stderr, "Provided structure_ID must be number.\n");
       help();
       return 0;
   }

   if (structure_ID !=1 && structure_ID !=2){
       fprintf(stderr, "Provided structure ID is not supported.\n");
       help();
       return 0;
   }

   char inbuf[4096];
   char outbuf[4096];
   int fd = open("/proc/my_procfs", O_RDWR);
   sprintf(inbuf, "%s", argv[1]);

   write(fd, inbuf, 17);
   lseek(fd, 0, SEEK_SET);
   read(fd, outbuf, 4096);

   if (structure_ID == 1){
       printf("info about tcp: \n\n");
   } else {
       printf("info about unix sockets: \n\n");
   }
   puts(outbuf);
   return 0;
}
