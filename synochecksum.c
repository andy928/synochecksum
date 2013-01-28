/*

  $Id: synochecksum.c,v 1.7 2008/06/03 20:06:05 flip Exp flip $

  synochecksum.c -- calculates a checksum.syno file

  Written 2008 by flipflip and Emmanuel

  CRC32 routines based on:
    efone - Distributed internet phone system.
    (c) 1999,2000 Krzysztof Dabrowski
    (c) 1999,2000 ElysiuM deeZine


  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version
  2 of the License, or (at your option) any later version.


  All trademarks mentioned in this file or any other file or program
  contained in this package are the property of their respective owners.



  Background:
  -----------

  Synology's (http://www.synology.com) NAS devices (DiskStation, CubeStation
  and RackStation series) use tar(1) for "firmware patch (.pat) files". See
  http://oinkzwurgl.org/diskstation_patchfiles for details. The patch file must
  contain a checksum.syno file with some checksums.

  The checksum.syno file has the following format. Each file in the .pat
  archive is listed on a separate line which consists of five fields. They are:
  crc2 (decimal), filesize (in bytes), filename, x and y. While the first three
  fields are trivial to determine, the numbers a and b are a bit more
  complicated (see synock() routine below).

  This programme outputs a valid checksum.syno file on the standard output for
  all files given on the command line (see -h for usage instructions).


*/


/* we need some funky functions */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <stdint.h>
#include <dirent.h>


/* programme meta data */

#define PROG_NAME      "synochecksum"
#define PROG_INFO      "Calculates checksum.syno files."
#define PROG_VERSION   "1.0"
#define PROG_AUTHOR    "Written 2008 by flipflip and Emmanuel"
#define PROG_REV       "\
$Id: synochecksum.c,v 1.7 2008/06/03 20:06:05 flip Exp flip $"


/* constants */

#define BUFSIZE   0x7FFF       /* 32k file read buffer (configurable) */
#define CRC32INIT 0xFFFFFFFFL  /* for crc32() */
#define CRC32POLY 0xEDB88320L  /* for crc32gentab() */
#define RAND1INIT 1804289383   /* result of random() with prior srandom(1) */


/* lookup table for the CRC32 routine */

u_int32_t crc_tab[256];


/* routine to initialise the CRC32 table */

void crc32gentab()
{
  int i, j;
  for (i = 0; i < 256; i++)
    {
      crc_tab[i] = i;
      for (j = 8; j > 0; j--)
        {
          if (crc_tab[i] & 1)
            {
              crc_tab[i] = (crc_tab[i] >> 1) ^ CRC32POLY;
            }
          else
            {
              crc_tab[i] >>= 1;
            }
        }
    }
}


/* routine to calculate the CRC32 for a block of a file */

u_int32_t crc32(int fd)
{
  unsigned char buf[BUFSIZE], *tmp;
  u_int32_t crc = CRC32INIT;
  ssize_t  bytes_read;
  while ((bytes_read = read(fd, buf, BUFSIZE)) > 0)
    {
      tmp = buf;
      while (bytes_read--)
        {
          crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *tmp++) & 0xFF];
        }
    }
  return crc ^ CRC32INIT;
}


/* routine to get the size of a file */

size_t filesize(int fd)
{
  struct stat buf;
  if ((fstat(fd, &buf)) == -1)
    err(EXIT_FAILURE, "Cannot determine file size.");
  return buf.st_size;
}


/* routine to determine Syno's magic "checksums" of a file*/

void synock(int fd, size_t filesize, int *x, int *y)
{
#ifndef SYNOCKVARIANT
#define SYNOCKVARIANT 1
#endif



#if SYNOCKVARIANT == 1
#warning "Using flipflip's synock() variant #1"

  off_t offset;
  unsigned char ybuf[4];

  /* determine y (offset of the magic bytes and sum them up) */
  offset = RAND1INIT % filesize;
  if (filesize - offset < 4) offset = filesize - 5;
  if (offset == 0)           offset = 1;
  if (offset < 0)            offset = 0;
  lseek(fd, offset, SEEK_SET);
  bzero(ybuf, sizeof(ybuf));
  read(fd, ybuf, sizeof(ybuf));
  *y = ybuf[0] + ybuf[1] + ybuf[2] + ybuf[3];

  /* the x magic number */
  *x = offset + filesize;




#elif SYNOCKVARIANT == 2
#warning "Using Emmanuel's synock() variant #1"

  off_t offset;
  unsigned char ybuf[4];
  int r, bytes_to_read;

  srand(1);
  r = random();
  offset = r % filesize;
  if (offset + 4 > filesize)
    offset = (filesize > 3) ? filesize - 4 : 0;
  lseek(fd, offset, SEEK_SET);
  *x = offset + filesize;
  bzero(ybuf, 4);
  bytes_to_read = filesize > 3 ? 4 : filesize;
  read(fd, ybuf, bytes_to_read);
  *y = ybuf[0] + ybuf[1] + ybuf[2] + ybuf[3];


#else
#error "Illegal value of SYNOCKVARIANT"
#endif

}


/* returns 1 for filtered files, 2 for directories  */

int filter(char *fn)
{
  DIR *d;
  int l;
  l = strlen(fn);
  if (strcmp(fn, "checksum.syno") == 0) return 1;
  if (fn[l-1] == '~') return 1;
  if (fn[l-4] == '.' && fn[l-3] == 'p' && fn[l-2] == 'a' && fn[l-1] == 't')
    return 1;
  if ((d = opendir(fn)) == NULL) return 0;
  closedir(d);
  return 2;
}


/* prints usage information */

void usage()
{
  fprintf(stderr, "\n%s %s -- %s\n\n%s\n\n",
          PROG_NAME, PROG_VERSION, PROG_INFO, PROG_AUTHOR);
  fprintf(stderr, "\
Usage: %s [-q] <file1> [<file2> ...]\n\
\n", PROG_NAME);

  fprintf(stderr, "\
Computes the checksums of the files given on the command line and\n\
outputs a checksum.syno file on stdout.\n\n\
Files matching \"checksum.syno\", \"*.pat\" and \"*~\", as well as\n\
directories, are ignored.\n\n\
Therefore, '%s * > checksum.syno' creates and updates\n\
the checksum file.\n\n\
The -q flag (as the first argument) enables quiet operation.\n\n\
The programme returns %i on success and %i on errors.\n",
          PROG_NAME, EXIT_SUCCESS, EXIT_FAILURE);

  fprintf(stderr, "\n\
The CRC32 routines used in this programme are based on:\n\
    efone - Distributed internet phone system.\n\
    (c) 1999,2000 Krzysztof Dabrowski and ElysiuM deeZine\n\
\n\
This program is free software; you can redistribute it and/or\n\
modify it under the terms of the GNU General Public License\n\
as published by the Free Software Foundation; either version\n\
2 of the License, or (at your option) any later version.\n\n");
  exit(EXIT_FAILURE);
}


/* main programme */

int main(int argc, char **argv)
{
  char      *fn;         /* file name */
  u_int32_t  crc;        /* CRC32 of the file */
  size_t     fs;         /* size of the file */
  int        x, y;       /* Syno's "checksums" */
  size_t     sumfs;      /* [fs] */
  int        sumx, sumy; /* [x], [y] */
  int        fd;         /* file descriptor */
  int        quiet;      /* -q flag */
  int        argp;       /* position of first file name argument */
  int        nfiles;     /* number of files processed */
  //  int r;


  /* check command line arguments */

  if (argc > 1 && strcmp(argv[1], "-q"))
    {
      quiet = 0; argp = 1;
    }
  else
    {
      quiet = 1; argp = 2;
    }

  if (argc > argp && strcmp(argv[argp], "-h") == 0)
    {
      usage();
    }

  if (argc < argp + 1)
    {
      fprintf(stderr, "%s %s -- %s\n",
              PROG_NAME, PROG_VERSION, PROG_AUTHOR);

      fprintf(stderr, "Try '%s -h'.\n", PROG_NAME);
      exit(EXIT_FAILURE);

    }


  /* say hello */

  if (!quiet)
    fprintf(stderr, "%s %s -- %s\n",
            PROG_NAME, PROG_VERSION, PROG_AUTHOR);


  /* initialise CRC32 table */

  crc32gentab();


  /* process all files */

  sumfs = 0; sumx = 0; sumy = 0; nfiles = 0;

  while (argp < argc)
    {

      /* next file name */

      fn = argv[argp++];

      if (filter(fn))
        {
          if (!quiet)
            fprintf(stderr, "Ignoring %s.\n", fn);
          continue;
        }


      if (!quiet)
        fprintf(stderr, "Processing %s ..\n", fn);


      /* open file */

      if ((fd = open(fn, O_RDONLY)) == -1)
        err(EXIT_FAILURE, "%s", fn);


      /* calculate checksums */

      fs = filesize(fd);
      if (fs == 0)
        {
          /* special case for filesize == 0 */
          crc = 0;
          x = 0;
          y = 2767;
        }
      else
        {
          crc = crc32(fd);
          synock(fd, fs, &x, &y);
        }

      printf("%ju %ju %s %i %i\n", (intmax_t)crc, (intmax_t)fs, fn, x, y);


      /* sum up */

      sumfs += fs;
      sumx += x;
      sumy += y;
      nfiles++;


      /* close file */

      if ((fd = close(fd)) == -1)
        warn("%s", fn);

    }


  /* print sums */

  printf("#Synocksum %ju %i %i\n", (intmax_t)sumfs, sumx, sumy);



  /* goodbye */

  if (!quiet)
    fprintf(stderr, "%i files processed.\n", nfiles);

  exit(EXIT_SUCCESS);

}



/* eof */
