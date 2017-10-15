Super LS
========

In 1989 this program was released to the world in the form of being posted
to the Usenet group comp.sources.unix. It was in volume 18 of the archives,
which probably exist out on some dusty ftp server still. (But a quick
search did not find such an ftp server in October 2017. The links run cold
at the Internet Systems Consortium's `sources.isc.org`.) 

Many of the features of `sls` were new at the time, but are less radical
now.  And the growth of interpreted languages for system admin use.
Languages like Perl (described as *a "replacement" for awk and sed*, when
posted to c.s.u, and archived in volume 13) and Python provide native
access to `stat`(2). And now there's a straight-out `stat`(1) in Gnu
`coreutils`. 

I found this program in the archives somewhere around 1993 and immediately
took a liking to it, and patched it many times for my own needs. Today I
barely use it, `sls` having fallen to the wayside due to several factors:
non-standard interface (the `printf` style output control isn't really
`printf`(3)), the rampant buffer overrun problems in the code, more
powerful all-in-one output features available with Perl. 

Original README (markdown-ified)
--------------------------------

This program provides a new shell interface to the `stat`(2) information
normally provided by the Unix `ls`(1) program.  This archive contains the
following files:

*   `README`	- this file
*   `sls.c`	- source
*   `sls.1`	- man page
*   `Makefile`	- for `make`(1)

Type `make -f Makefile sls` to try it out; define $(BIN), $(MAN) and
type `make -f Makefile install` to install it.  WARNING: this has been
compiled and tested only on a Sun 3 under SunOS 3.5 and on a Sun 386i
under SunOS 4.0.  It has been run through lint and cleaned up.  The
rest of this file is part of the man page (to stimulate your curiosity).

NAME
----

sls - list information about file(s) and directories

SYNOPSIS
--------

`sls [ -adlpsuLR ] filename ...`

DESCRIPTION
-----------

`Sls` is a program designed to overcome the limitations of the
standard  UNIX  `ls`(1)  program,  providing a more consistent
interface to file inode  information.   It  is  particularly
designed for use by shell scripts to make obtaining information
about files easier.   It  uses  `printf`(3)-style  format
strings  to  control the sorting and output of file information.

Advantages of `sls` over `ls`:

*  Allows complete specification of the  output  contents  -
   field (column) order, field widths, right/left justification,
    and zero-fill.

*  Allows complete specification of the sort order  independently
   of  the  output options - output can be sorted on
   one or more fields.

*  Has consistent, user-definable file date formats  -  `ls`'s
   are inconsistent and hard to parse (the seconds are never
   displayed, the year is shown  instead  of  the  time  for
   files more than 6 months old, etc.).

*  Has **normalized** output (no summary lines  or  changing
   formats).

*  Allows specification of delimiter char(s) -  the  characters
   between  fields  - which makes the output easier to
   parse in shell scripts.

*  Won't `stat` files if it's not necessary (e.g., `sls <dir>`);
   in the trivial (but common) case of calling `sls`
   on a directory (or list of directories) with no  options,
   it  will  simply  read the directory file and display the
   file names, sorted alphabetically.  For very large directories,
   this is *much* faster than `ls`, and gets around command line
   limitations of the various  login  shells  when using `echo`(1).

EXAMPLES
--------

To produce the same output as `ls -l` (differs slightly from
`sls -l`, in date format and filename display):

```
	 sls -u -p '%t%p %2l %-u %s %m %N'
```

To list the size (in kbytes), access and  modify  dates  (no
times),  and file names (no pathname), sorted by size (largest
first):

```
	 sls -s %-s -p '%sk %a"%h %d 19%y" %m"%h %d 19%y" %nb' /u/mydir
```

How a shell script might get the last-modify date on a  file
with `sls`, vs. `ls` (assume that `SLS_DATEFMT="%h %d %H:%M"`; remember
that you have no control over the time vs. year field with `ls`):

```
	 FILEDATE=`ls -l file | awk '{print $5,$6,$7}'`
	 FILEDATE=`sls -p %m file`
```

Feel free to send me bug and portability fixes, comments, and enhancements
(but watch out for "creeping featurism").  No flames, please - "You get
what you pay for."

Enjoy!

Rich Baughman		  
rich@cfi.com OR ima!cfisun!rich
Price Waterhouse/CFI      
Waltham, MA  617-899-6500
