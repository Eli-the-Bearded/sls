/*
 * "Super" ls program.  Allows complete specification of output contents:
 * field order, width, format, and sort order.
 *
 * Options:
 *   -a		show all files (includes '.' files)
 *   -d		show directories in arg list as files
 *   -l		long listing (uses $SLS_LONGFMT if set, or ls style)
 *   -L		follow symbolic links (show stat of pointed-to file)
 *   -p "ostr"	output key string
 *   -R		recursively list subdirectories
 *   -s "sstr"	sort key string
 *   -u		use ls-style date formatting (<6 mos. vs. >6 mos.)
 *
 * Uses environment vbl SLS_DATEFMT as default format string for dates, if
 * supplied.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>

#define ISEXEC(m)	(m & (S_IEXEC|(S_IEXEC>>3)|(S_IEXEC>>6)))
#define MAXUIDS		200	/* max #of user names from passwd file */
#define MAXGIDS		100	/* max #of group names from group file */

    /* defines for sort/print option strings (preceded by a '%'); keys can
     * have field widths; leading '-' indicates left-justify (string fields)
     * or reverse order if it is a sort key; leading '0' indicates zero-fill
     * (numeric fields) */
#define F_ADATE		'a'	/* access date (optional fmtdate string) */
#define F_BLOCKS	'b'	/* #allocated (512-byte) blocks (+ m,k,c|b) */
#define F_CDATE		'c'	/* inode change date */
#define F_DEV		'd'	/* device number inode resides on */
#define F_GROUPA	'g'	/* ascii group name (from group file) */
#define F_GROUPN	'G'	/* numeric group id */
#define F_INODE		'i'	/* inode number */
#define F_BLKSIZE	'k'	/* optimal file system block size */
#define F_NLINKS	'l'	/* number of hard links */
#define F_MDATE		'm'	/* modify date (optional fmtdate string) */
#define F_NAME		'n'	/* file name (+ b=basename,a=ascii,s=suffix) */
#define F_NAMEL		'N'	/* file name with symbolic links shown */
#define F_PERMA		'p'	/* ascii permissions */
#define F_PERMO		'P'	/* octal permissions */
#define F_RDEV		'r'	/* device number file resides on */
#define F_SIZE		's'	/* file size in bytes (mods: m,k,c|b) */
#define F_TYPE		't'	/* file type (coded like 'ls') */
#define F_USERA		'u'	/* ascii user name (from passwd file) */
#define F_USERN		'U'	/* numeric user id */

typedef struct {		/* for saving name only */
    char        *f_name;
    } FILESTAT;

typedef struct {		/* for saving name and stat info */
    char        *f_name;
    struct stat  f_stat;
    } SFILESTAT;

FILESTAT *Pfiles;	/* list of files (no stat buf) to sort/print */
int       Npfiles;	/* number of files in Pfiles */

SFILESTAT *Psfiles;	/* list of files to sort/print */
int       Npsfiles;	/* number of files in Psfiles */

    /* global options */
int    Follow_links;	/* follow symbolic links */
int    Recursive;	/* follow subdirectories */
int    Stat_dirs;	/* show directories as files */
int    All_files;	/* include '.' files in listing */
int    Use_less;	/* use ls-style date format */
int    Dostat;		/* =0 if stat(2) system call not needed */
time_t Sixmosago;	/* system date for 6 mos. ago (for default dates) */
char  *Defdatefmt;	/* default format for dates */
char  *Sortopt;
char  *Printopt;

static char  *fmtdate(), *aperms(), *getfmtstr(), *showname(), *copystr();
static char  *getpwname(), *getgrname();
static int    sortsfile(), sortfile();
static time_t getsixmosago();		/* for emulating 'ls' date silliness */

extern char  *getenv(), *malloc(), *realloc();
extern int    errno;
extern time_t time();

/******************************************************************************/

main (ac, av)
    int            ac;
    char          *av[];
{
    register char *pav, *ptmp, *longfmt;
    register int   i, basename, gotfilearg;

    gotfilearg = 0;
    Sortopt = "%n";
    Printopt = "%n";
    Defdatefmt = "%h %d 19%y %H:%M";
    if ((pav = getenv("SLS_DATEFMT")) != NULL && strlen(pav) > 0)
	Defdatefmt = copystr (pav);
    longfmt = "%t%p %2l %-u %s %m %n";
    if ((pav = getenv("SLS_LONGFMT")) != NULL && strlen(pav) > 0)
	longfmt = copystr (pav);
    Use_less = Follow_links = Recursive = Stat_dirs = All_files = 0;
    Dostat = 0;
    Sixmosago = getsixmosago ();	/* in case Use_less==1 */

    while (--ac > 0)
    {
	pav = *++av;
	if (*pav == '-')
	{
	    while (*++pav) switch (*pav)
	    {
	    case 'a':
		All_files = 1;
		break;
	    case 'd':
		Stat_dirs = 1;
		Dostat = 1;
		break;
	    case 'l':
		Printopt = longfmt;
		if (strcmp(Printopt,"%n") != 0 && strcmp(Printopt,"%nb") != 0)
		    Dostat = 1;
		break;
	    case 'L':
		Follow_links = 1;
		Dostat = 1;
		break;
	    case 'p':
		if (--ac == 0)
		{
		    fprintf (stderr, "? -p requires an argument\n");
		    exit (1);
		}
		Printopt = *++av;
		pav = "-";	/* so while() quits */
		if (strcmp(Printopt,"%n") != 0 && strcmp(Printopt,"%nb") != 0)
		    Dostat = 1;
		break;
	    case 'R':
		Recursive = 1;
		Dostat = 1;
		break;
	    case 's':
		if (--ac == 0)
		{
		    fprintf (stderr, "? -s requires an argument\n");
		    exit (1);
		}
		Sortopt = *++av;
		pav = "-";	/* so while() quits */
		Dostat = 1;
		break;
	    case 'u':		/* the "use-l(es)s format" option */
		Use_less = 1;
		break;
	    default:
		fprintf (stderr, "?unknown option '%s'\n", pav);
		exit (1);
	    }
	}
	else
	{
	    dofile (pav, 1);
	    ++gotfilearg;
	}
    }

    if (!gotfilearg)
	dofile ("", 1);

    if (!Dostat)
    {
	qsort ((char *)Pfiles, Npfiles, sizeof(*Pfiles), sortfile);
	basename = (strcmp(Printopt,"%nb") == 0);
	for (i=0; i<Npfiles; ++i)
	{
	    if (basename)
	    {
		ptmp = (Pfiles+i)->f_name;
		if ((pav = strrchr (ptmp, '/')) == NULL)
		    pav = ptmp;
		else
		    ++pav;
		puts (pav);
	    }
	    else
		puts ((Pfiles+i)->f_name);
	}
	exit (0);
    }

    if (strlen(Sortopt) > 0)
	qsort ((char *)Psfiles, Npsfiles, sizeof(*Psfiles), sortsfile);

    for (i=0; i<Npsfiles; ++i)
	display (Psfiles+i);
    
    exit (0);

}  /* main */

/******************************************************************************/

dofile (fname, cmdarg)
    register char  *fname;	/* name of file to stat and list */
    int             cmdarg;	/* =1 if fname was command line arg */
{  /* check if file should be displayed (read contents if a directory) */
    struct stat     sbuf;
    register char  *pbase;	/* ptr to basename part of fname */

    if ((pbase = strrchr (fname, '/')) == (char *)NULL)
	pbase = fname;
    else
	++pbase;

	/* skip files that start with '.' unless -a */
    if (*pbase == '.' && !All_files && !cmdarg)
	return;
    
    if (!Dostat && !cmdarg)
    {
	selectf (fname);	/* short version - file name only */
	return;
    }

    if ((!Follow_links && lstat (fname, &sbuf) != 0)
	|| (Follow_links && stat (fname, &sbuf) != 0))
    {
	fprintf (stderr, "?unable to stat file '%s' (errno=%d)\n",
	    fname, errno);
	return;
    }

    if ((sbuf.st_mode&S_IFMT) == S_IFDIR)	/* it's a directory */
    {
	if (!cmdarg
	    && (strcmp (pbase, ".") == 0 || strcmp (pbase, "..") == 0))
	{  /* don't show these unless -a given */
	    if (All_files)
		selectfs (fname, &sbuf);
	    return;
	}

	if (Stat_dirs || (!cmdarg && !Recursive))
	    selectfs (fname, &sbuf);
	else
	    dirread (fname);
	return;
    }

    if (Dostat)
	selectfs (fname, &sbuf);
    else
	selectf (fname);

}  /* dofile */

/******************************************************************************/

dirread (dirname)
    char           *dirname;
{  /* read a directory and everything under it that's on the same device */
    register DIR   *dirp;		/* ptr to directory list */
    register struct direct  *dentp;	/* ptr to directory entry */
    register char  *pfname;
    register int    len;
    char            fname[512];		/* maximum pathname length */

	/* open and read a directory */
    if ((dirp = opendir(dirname)) == NULL)
    {
	fprintf (stderr, "?unable to open directory '%s' (errno=%d)\n",
	    dirname, errno);
	return;
    }
    strcpy (fname, dirname);
    len = strlen(fname);
    pfname = &fname[len];		/* point to start of filename part */
    if (len > 0 && *(pfname-1) != '/')
    {  /* doesn't end with slash, so add it */
	strcat (fname, "/");
	++pfname;
	++len;
    }

	/* loop through directory entries */
    for (dentp = readdir(dirp); dentp != NULL; dentp = readdir(dirp))
    {
	if (len + dentp->d_namlen >= sizeof(fname))
	{
	    *pfname = '\0';	/* so we don't see previous filename part */
	    fprintf (stderr, "?file name too long: '%s%s'\n",
		fname, dentp->d_name);
	    continue;
	}
	    /* add this file name to the directory path */
	strcpy (pfname, dentp->d_name);
	dofile (fname, 0);
    }

    closedir (dirp);

}  /* dirread */

/******************************************************************************/

selectf (fname)
    register char *fname;
{  /* allocate space for this file name for sorting */
    FILESTAT     *pf;
    static int     maxpfiles=0;
    unsigned int   size;

	/* check if need more space for FILESTAT ptrs */
    if (Npfiles == maxpfiles)
    {
	maxpfiles += 500;
	size = maxpfiles * sizeof(FILESTAT);
	if (Pfiles == (FILESTAT *) NULL)
	    Pfiles = (FILESTAT *) malloc (size);
	else
	    Pfiles = (FILESTAT *) realloc ((char *)Pfiles, size);
	if (Pfiles == (FILESTAT *) NULL)
	{
	    fprintf (stderr, "?malloc failed (errno=%d) on stat of '%s'\n",
		errno, fname);
	    exit (1);
	}
    }

    pf = Pfiles + Npfiles;
    pf->f_name = copystr (fname);
    ++Npfiles;

}  /* selectf */

/******************************************************************************/

static int sortfile (pf1, pf2)
    register FILESTAT     *pf1, *pf2;
{  /* qsort comparison routine */

    return (strcmp (pf1->f_name, pf2->f_name));

}  /* sortfile */

/******************************************************************************/

selectfs (fname, psbuf)
    register char *fname;
    register struct stat  *psbuf;
{  /* allocate a struct for this file for sorting */
    SFILESTAT      *pf;
    static int     maxpfiles=0;
    unsigned int   size;

	/* check if need more space for SFILESTAT ptrs */
    if (Npsfiles == maxpfiles)
    {
	maxpfiles += 100;
	size = maxpfiles * sizeof(SFILESTAT);
	if (Psfiles == (SFILESTAT *) NULL)
	    Psfiles = (SFILESTAT *) malloc (size);
	else
	    Psfiles = (SFILESTAT *) realloc ((char *)Psfiles, size);
	if (Psfiles == (SFILESTAT *) NULL)
	{
	    fprintf (stderr, "?malloc failed (errno=%d) on stat of '%s'\n",
		errno, fname);
	    exit (1);
	}
    }

    pf = Psfiles + Npsfiles;
    pf->f_name = copystr (fname);
    pf->f_stat = *psbuf;	/* copies entire struct */
    ++Npsfiles;

}  /* selectfs */

/******************************************************************************/

static int sortsfile (pf1, pf2)
    register SFILESTAT     *pf1, *pf2;
{  /* qsort comparison routine */
    register char         *ps, *p1, *p2, *ptmp;
    register struct stat  *psbuf1, *psbuf2;
    long                   l1, l2;
    int                    i, reverse, ascii, basen;
    char                   tbuf1[200], tbuf2[200];

    psbuf1 = &(pf1->f_stat);
    psbuf2 = &(pf2->f_stat);
    for (ps=Sortopt; *ps; ++ps)
    {
	if (*ps++ != '%')
	{
	    fprintf (stderr, "?unknown sort key at '%.6s'...\n", ps);
	    exit (1);
	}

	    /* check for '-' (for reverse order sort) */
	reverse = 1;		/* NOT a flag - multiplied by strcmp() result */
	if (*ps == '-')
	{
	    reverse = -1;
	    ++ps;
	}

	l1 = l2 = 0;
	p1 = p2 = (char *)NULL;
	switch (*ps)
	{
	case F_TYPE:
	    switch (psbuf1->st_mode & S_IFMT)
	    {
	    case S_IFREG:	p1 = "-"; break;
	    case S_IFDIR:	p1 = "d"; break;
	    case S_IFCHR:	p1 = "c"; break;
	    case S_IFBLK:	p1 = "b"; break;
	    case S_IFLNK:	p1 = "l"; break;
	    case S_IFSOCK:	p1 = "s"; break;
	    case S_IFIFO:	p1 = "p"; break;
	    default:		p1 = "?"; break;
	    }
	    switch (psbuf2->st_mode & S_IFMT)
	    {
	    case S_IFREG:	p2 = "-"; break;
	    case S_IFDIR:	p2 = "d"; break;
	    case S_IFCHR:	p2 = "c"; break;
	    case S_IFBLK:	p2 = "b"; break;
	    case S_IFLNK:	p2 = "l"; break;
	    case S_IFSOCK:	p2 = "s"; break;
	    case S_IFIFO:	p2 = "p"; break;
	    default:		p2 = "?"; break;
	    }
	    break;
	case F_PERMA:
	    p1 = aperms ((int)psbuf1->st_mode, ps+1, &i);
	    p2 = aperms ((int)psbuf2->st_mode, ps+1, &i);
	    ps += i;
	    break;
	case F_PERMO:
	    l1 = ((psbuf1->st_mode) & (~S_IFMT));
	    l2 = ((psbuf2->st_mode) & (~S_IFMT));
	    break;
	case F_NLINKS:
	    l1 = psbuf1->st_nlink;
	    l2 = psbuf2->st_nlink;
	    break;
	case F_INODE:
	    l1 = psbuf1->st_ino;
	    l2 = psbuf2->st_ino;
	    break;
	case F_DEV:
	    l1 = psbuf1->st_dev;
	    l2 = psbuf2->st_dev;
	    break;
	case F_RDEV:
	    l1 = psbuf1->st_rdev;
	    l2 = psbuf2->st_rdev;
	    break;
	case F_USERA:
	    l1 = psbuf1->st_uid;
	    l2 = psbuf2->st_uid;
	    if ((ptmp = getpwname((int)l1)) == (char *)NULL)
		break;
	    strcpy (tbuf1, ptmp);
	    if ((ptmp = getpwname((int)l2)) == (char *)NULL)
		break;
	    strcpy (tbuf2, ptmp);
	    p1 = tbuf1;
	    p2 = tbuf2;
	    break;
	case F_USERN:
	    l1 = psbuf1->st_uid;
	    l2 = psbuf2->st_uid;
	    break;
	case F_GROUPA:
	    l1 = psbuf1->st_gid;
	    l2 = psbuf2->st_gid;
	    if ((ptmp = getgrname((int)l1)) == (char *)NULL)
		break;
	    strcpy (tbuf1, ptmp);
	    if ((ptmp = getgrname((int)l2)) == (char *)NULL)
		break;
	    strcpy (tbuf2, ptmp);
	    p1 = tbuf1;
	    p2 = tbuf2;
	    break;
	case F_GROUPN:
	    l1 = psbuf1->st_gid;
	    l2 = psbuf2->st_gid;
	    break;
	case F_SIZE:
	    l1 = psbuf1->st_size;
	    l2 = psbuf2->st_size;
	    break;
	case F_BLOCKS:
	    l1 = psbuf1->st_blocks;
	    l2 = psbuf2->st_blocks;
	    break;
	case F_BLKSIZE:
	    l1 = psbuf1->st_blksize;
	    l2 = psbuf2->st_blksize;
	    break;
	case F_MDATE:
	    l1 = psbuf1->st_mtime;
	    l2 = psbuf2->st_mtime;
	    break;
	case F_ADATE:
	    l1 = psbuf1->st_atime;
	    l2 = psbuf2->st_atime;
	    break;
	case F_CDATE:
	    l1 = psbuf1->st_ctime;
	    l2 = psbuf2->st_ctime;
	    break;
	case F_NAME:
	case F_NAMEL:
	    ascii = basen = 0;
	    while (*(ps+1) && strchr("abs",*(ps+1)) != NULL)
	    {
		++ps;
		if (*ps == 'a')
		    ascii = 1;
		else if (*ps == 'b')
		    basen = 1;
		else if (*ps == 's')	/* ignored for sort purposes */
		    ;
	    }
	    if (basen)	/* basename only */
	    {
		if ((p1 = strrchr (pf1->f_name, '/')) == (char *)NULL)
		    p1 = pf1->f_name;
		else
		    ++p1;
		if ((p2 = strrchr (pf2->f_name, '/')) == (char *)NULL)
		    p2 = pf2->f_name;
		else
		    ++p2;
	    }
	    else
	    {
		p1 = pf1->f_name;
		p2 = pf2->f_name;
	    }
	    if (ascii)	/* show non-printing chars */
	    {
		strcpy (tbuf1, showname (p1));
		strcpy (tbuf2, showname (p2));
		p1 = tbuf1;
		p2 = tbuf2;
	    }
	    break;
	default:
	    fprintf (stderr, "?unknown sort option char '%%%c'\n", *ps);
	    continue;
	}  /* switch */

	    /* now check for type of comparison for this field */
	if (p1 != (char *)NULL)
	{  /* string comparison */
	    i = strcmp (p1, p2);
	    if (i == 0)
		continue;	/* need to go to next sort field */
	    else
		return (reverse*i);
	}
	else
	{  /* numeric comparison */
	    if (l1 == l2)
		continue;	/* need to go to next sort field */
	    else if (l1 > l2)
		return (reverse);
	    else		/* l1 < l2 */
		return (-reverse);
	}
    }  /* loop through sort options string */

    return (strcmp (pf1->f_name, pf2->f_name));

}  /* sortsfile */

/******************************************************************************/

display (pf)
    SFILESTAT       *pf;
{  /* display info about a file */
    register struct stat   *psbuf;
    register char  *popt, *pobuf, *fmtstr;
    register int    n, fwid;
    long            l;
    int             i, zerofill, leftjust, ascii, basen, suffix;
    char            obuf[200], lname[200], *ptmp, *pc, *fname, c;

    fname = pf->f_name;
    psbuf = &(pf->f_stat);
    pobuf = obuf;
    for (popt=Printopt; *popt; ++popt)
    {
	if (*popt != '%')
	{
	    *pobuf++ = *popt;
	    continue;
	}
	if (*(popt+1) == '%')
	{
	    *pobuf++ = *popt++;
	    continue;
	}

	    /* check for optional field width and '-' (for left justify) */
	fwid = zerofill = leftjust = 0;
	if (*(popt+1) == '-')
	{
	    leftjust = 1;
	    ++popt;
	}
	if (isdigit (*++popt))
	{
	    zerofill = (*popt == '0');
	    for (fwid=0; isdigit(*popt); ++popt)
		fwid = fwid*10 + *popt - '0';
	}

	c = *popt;
	switch (c)
	{
	case F_TYPE:
	    switch (psbuf->st_mode & S_IFMT)
	    {
	    case S_IFREG:	ptmp = "-"; break;
	    case S_IFDIR:	ptmp = "d"; break;
	    case S_IFCHR:	ptmp = "c"; break;
	    case S_IFBLK:	ptmp = "b"; break;
	    case S_IFLNK:	ptmp = "l"; break;
	    case S_IFSOCK:	ptmp = "s"; break;
	    case S_IFIFO:	ptmp = "p"; break;
	    default:
		ptmp = "?";
		fprintf (stderr, "?unknown file type 0%o for file '%s'\n",
		    psbuf->st_mode & S_IFMT, fname);
		break;
	    }
	    if (fwid == 0)
	    {
		*pobuf++ = *ptmp;
		*pobuf = '\0';	/* so 'while' loop below exits */
	    }
	    else
	    {
		fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr, ptmp);
	    }
	    break;
	case F_PERMA:
	    if (fwid == 0)
		strcpy (pobuf, aperms ((int)psbuf->st_mode, popt+1, &i));
	    else
	    {
		fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr,
		    aperms ((int)psbuf->st_mode, popt+1, &i));
	    }
	    popt += i;
	    break;
	case F_PERMO:
	    if (fwid == 0)
		fmtstr = getfmtstr ('o', 3, 0, 1);
	    else
		fmtstr = getfmtstr ('o', fwid, leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_mode & (~S_IFMT));
	    break;
	case F_NLINKS:
	    fmtstr = getfmtstr ('d', (fwid?fwid:4), leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_nlink);
	    break;
	case F_INODE:
	    fmtstr = getfmtstr ('d', (fwid?fwid:6), leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_ino);
	    break;
	case F_DEV:
	    fmtstr = getfmtstr ('d', (fwid?fwid:6), leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_dev);
	    break;
	case F_RDEV:
	    fmtstr = getfmtstr ('d', (fwid?fwid:6), leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_rdev);
	    break;
	case F_USERA:
	    n = psbuf->st_uid;
	    if ((ptmp = getpwname(n)) == (char *)NULL)
	    {
		fmtstr = getfmtstr ('d', (fwid?fwid:8), leftjust, zerofill);
		sprintf (pobuf, fmtstr, n);
	    }
	    else
	    {
		if (fwid == 0)
		    fmtstr = getfmtstr ('s', 8, leftjust, 0);
		else
		    fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr, ptmp);
	    }
	    break;
	case F_USERN:
	    fmtstr = getfmtstr ('d', (fwid?fwid:4), leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_uid);
	    break;
	case F_GROUPA:
	    n = psbuf->st_gid;
	    if ((ptmp = getgrname(n)) == (char *)NULL)
	    {
		fmtstr = getfmtstr ('d', (fwid?fwid:8), leftjust, zerofill);
		sprintf (pobuf, fmtstr, psbuf->st_uid);
	    }
	    else
	    {
		if (fwid == 0)
		    fmtstr = getfmtstr ('s', 8, leftjust, 0);
		else
		    fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr, ptmp);
	    }
	    break;
	case F_GROUPN:
	    fmtstr = getfmtstr ('d', (fwid?fwid:4), leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_gid);
	    break;
	case F_SIZE:
	    l = (long) psbuf->st_size;
	    if (*(popt+1) == 'm')
	    {
		fmtstr = getfmtstr ('d', (fwid?fwid:3), leftjust, zerofill);
		sprintf (pobuf, fmtstr, (l+1048575)>>20);
		++popt;
	    }
	    else if (*(popt+1) == 'k')
	    {
		fmtstr = getfmtstr ('d', (fwid?fwid:6), leftjust, zerofill);
		sprintf (pobuf, fmtstr, (l+1023)>>10);
		++popt;
	    }
	    else
	    {
		fmtstr = getfmtstr ('d', (fwid?fwid:9), leftjust, zerofill);
		sprintf (pobuf, fmtstr, l);
		if (*(popt+1) == 'c' || *(popt+1) == 'b')
		    ++popt;
	    }
	    break;
	case F_BLOCKS:
	    l = (long) psbuf->st_blocks;	/* 512-byte blocks allocated */
	    if (*(popt+1) == 'm')
	    {
		l = (l+511) >> 11;	/* (* 512 / 1048576) = divide by 2048 */
		fmtstr = getfmtstr ('d', (fwid?fwid:3), leftjust, zerofill);
		sprintf (pobuf, fmtstr, l);
		++popt;
	    }
	    else if (*(popt+1) == 'k')
	    {
		l = (l+1) >> 1;		/* (* 512 / 1024) = divide by 2 */
		fmtstr = getfmtstr ('d', (fwid?fwid:6), leftjust, zerofill);
		sprintf (pobuf, fmtstr, l);
		++popt;
	    }
	    else if (*(popt+1) == 'c' || *(popt+1) == 'b')
	    {
		l <<= 9;		/* multiply by 512 */
		fmtstr = getfmtstr ('d', (fwid?fwid:9), leftjust, zerofill);
		sprintf (pobuf, fmtstr, l);
		++popt;
	    }
	    else
	    {
		fmtstr = getfmtstr ('d', (fwid?fwid:5), leftjust, zerofill);
		sprintf (pobuf, fmtstr, l);
	    }
	    break;
	case F_BLKSIZE:
	    fmtstr = getfmtstr ('d', (fwid?fwid:4), leftjust, zerofill);
	    sprintf (pobuf, fmtstr, psbuf->st_blksize);
	    break;
	case F_MDATE:
	    if (fwid == 0)
		strcpy (pobuf, fmtdate (psbuf->st_mtime, popt+1, &i));
	    else
	    {
		fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr, fmtdate (psbuf->st_mtime, popt+1, &i));
	    }
	    popt += i;
	    break;
	case F_ADATE:
	    if (fwid == 0)
		strcpy (pobuf, fmtdate (psbuf->st_atime, popt+1, &i));
	    else
	    {
		fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr, fmtdate (psbuf->st_atime, popt+1, &i));
	    }
	    popt += i;
	    break;
	case F_CDATE:
	    if (fwid == 0)
		strcpy (pobuf, fmtdate (psbuf->st_ctime, popt+1, &i));
	    else
	    {
		fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr, fmtdate (psbuf->st_ctime, popt+1, &i));
	    }
	    popt += i;
	    break;
	case F_NAME:
	case F_NAMEL:
	    ascii = basen = suffix = 0;
	    while (*(popt+1) && strchr ("abs", *(popt+1)) != NULL)
	    {
		++popt;
		if (*popt == 'a')
		    ascii = 1;
		else if (*popt == 'b')
		    basen = 1;
		else if (*popt == 's')
		    suffix = 1;
	    }
	    if (basen)	/* basename only */
	    {
		if ((ptmp = strrchr (fname, '/')) == (char *)NULL)
		    ptmp = fname;
		else
		    ++ptmp;
	    }
	    else
		ptmp = fname;
	    if (ascii)	/* show non-printing chars */
		ptmp = showname (ptmp);
	    pc = "";
	    if (suffix)
	    {	/* add trailing type char for some files */
		switch (psbuf->st_mode & S_IFMT)
		{
		case S_IFDIR:	pc = "/"; break;
		case S_IFLNK:	pc = "@"; break;
		case S_IFSOCK:	pc = "="; break;
		default:
		    if (ISEXEC(psbuf->st_mode))
			pc = "*";
		    break;
		}
	    }
	    if (fwid == 0)
	    {
		strcpy (pobuf, ptmp);
		if (c == F_NAMEL && !suffix)
		{	/* add linked-to filename if a symlink */
		    if ((psbuf->st_mode & S_IFMT) == S_IFLNK
			&& (n = readlink (fname, lname, sizeof(lname))) >= 0)
		    {
			lname[n] = '\0';
			strcat (pobuf, " -> ");
			strcat (pobuf, lname);
		    }
		}
	    }
	    else
	    {
		fmtstr = getfmtstr ('s', fwid, leftjust, zerofill);
		sprintf (pobuf, fmtstr, ptmp);
	    }
	    if (*pc)
	    {  /* add type-char to end of filename */
		for (ptmp=pobuf; *ptmp; ++ptmp)
		{
		    if (*ptmp == ' ')
		    {
			*ptmp = *pc;
			pc = "";
			break;
		    }
		}
		if (*pc)  /* didn't find a space to add it, so add it to end */
		    strcat (pobuf, pc);
	    }
	    break;
	default:
	    fprintf (stderr, "?unknown print option '%%%c'\n", *popt);
	    continue;
	}

	    /* update output ptr */
	while (*pobuf)
	    ++pobuf;

    }  /* loop through print options string */

    *pobuf = '\0';
    puts (obuf);

}  /* display */

/******************************************************************************/

static char *getfmtstr (ftype, width, leftjust, zerofill)
    register char   ftype;	/* format type */
    register int    width;	/* field width */
    register int    leftjust;	/* =1 if left-justified wanted ('%-3s') */
    register int    zerofill;	/* =1 if zero-fill wanted ('%03d') */
{  /* build (user-specified) format string */
    register char  *pbuf;
    static char     buf[20];

    pbuf = buf;
    *pbuf++ = '%';
    if (ftype == 's' && leftjust)
	*pbuf++ = '-';
    else if (ftype != 's' && zerofill)
	*pbuf++ = '0';

	/* encode width here instead of sprintf - too much overhead */
    if (width > 99)
    {	/* hundred's digit */
	width %= 1000;		/* ensure a single digit for pathologic cases */
	*pbuf++ = width/100 + '0';
	width %= 100;
    }
    if (width > 9)
    {	/* tens digit */
	*pbuf++ = width/10 + '0';
	width %= 10;
    }
    *pbuf++ = width + '0';	/* ones digit */
    *pbuf++ = ftype;		/* the format char itself ('s', 'd', 'o') */

    *pbuf++ = '\0';
    return (buf);

}  /* getfmtstr */

/******************************************************************************/

static char  *aperms (mode, poptstr, pnmod)
    register int   mode;	/* mode word from stat structure */
    char          *poptstr;	/* ptr to format string following %<char> */
    int           *pnmod;	/* #of chars "eaten" from format string */
{  /* return ascii representation of permission bits */
    static char    buf[20];
    register char *pbuf;
    register int   i;

    *pnmod = 0;
    pbuf = buf;
	/* get perm bits for user, group, other */
    for (i=0; i<3; ++i)
    {
	*pbuf++ = ((mode & (S_IREAD>>(i*3))) ? 'r' : '-');
	*pbuf++ = ((mode & (S_IWRITE>>(i*3))) ? 'w' : '-');
	*pbuf = ((mode & (S_IEXEC>>(i*3))) ? 'x' : '-');
	if (i == 0 && (mode & S_ISUID))
	    *pbuf = (*pbuf == 'x' ? 's' : 'S');
	else if (i == 1 && (mode & S_ISGID))
	    *pbuf = (*pbuf == 'x' ? 's' : 'S');
	else if (i == 2 && (mode & S_ISVTX))
	    *pbuf = (*pbuf == 'x' ? 't' : 'T');
	++pbuf;
    }
    *pbuf = '\0';

    if (*poptstr && strchr ("123456789", *poptstr) != NULL)
    {
	buf[0] = buf[*poptstr - '0'];
	buf[1] = '\0';
	*pnmod = 1;
    }

    return (buf);

}  /* aperms */

/******************************************************************************/

static char *showname (fname)
    register char  *fname;
{  /* show non-printing chars in a filename as \ddd */
    static char     newname[200];
    register char  *pnew;

    for (pnew=newname; *fname; ++fname,++pnew)
    {
	if (isprint(*pnew = *fname))
	    continue;
	sprintf (pnew, "\\%03o", *fname);
	pnew += strlen(pnew) - 1;
    }
    *pnew = '\0';

    return (newname);

}  /* showname */

/******************************************************************************/

static char  *Wdays[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char  *FWdays[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday" };
static char  *Months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December" };

static char *fmtdate (ldate, poptstr, pnmod)
    time_t          ldate;	/* system date to be formatted */
    register char  *poptstr;	/* ptr to format string following %<char> */
    int            *pnmod;	/* #of chars "eaten" from format string */
{  /* format ldate according to string in pfmt, returning ptr to static area */
    struct tm      *localtime();
    register struct tm  *ptime;
    register char   c, *padate, *pfmt, endc;
    int             i;
    static char     adate[100];

    *pnmod = 0;

	/* get components of the system date */
    ptime = localtime (&ldate);

	/* check for format string */
    pfmt = poptstr;
    endc = '\0';
    if (*pfmt == '\'' || *pfmt == '\"')
	endc = *pfmt++;
    else if (Use_less)
    {
	if (ldate < Sixmosago)
	    pfmt = "%h %d  19%y";	/* more than 6 mos. old (roughly) */
	else
	    pfmt = "%h %d %H:%M";	/* less than 6 mos. old */
    }
    else  /* use default date format (or environment vbl) */
	pfmt = Defdatefmt;

	/* parse format string */
    padate = adate;
    while ((c = *pfmt++) != endc && c != '\0')
    {
	if (c != '%')
	{
	    *padate++ = c;
	    continue;
	}
	switch (c = *pfmt++)
	{
	  case '%':	/* "%%" - print a percent sign */
	    *padate++ = '%';
	    break;

	  case 'n':	/* "%n" - print a newline */
	    *padate++ = '\n';
	    break;

	  case 't':	/* "%t" - print a tab */
	    *padate++ = '\t';
	    break;

	  case 'm':	/* "%m" - print month of year (01 to 12) */
	    sprintf (padate, "%02d", ptime->tm_mon+1);
	    padate += 2;
	    break;

	  case 'd':	/* "%d" - print day of month (01 to 31) */
	    sprintf (padate, "%02d", ptime->tm_mday);
	    padate += 2;
	    break;

	  case 'y':	/* "%y" - print last 2 digits of year (00 to 99) */
	    sprintf (padate, "%02d", ptime->tm_year);
	    padate += 2;
	    break;

	  case 'D':	/* "%D" - print date as mm/dd/yy */
	    sprintf (padate, "%02d/%02d/%02d",
		ptime->tm_mon+1, ptime->tm_mday, ptime->tm_year);
	    padate += 8;
	    break;

	  case 'H':	/* "%H" - print hour (00 to 23) */
	    sprintf (padate, "%02d", ptime->tm_hour);
	    padate += 2;
	    break;

	  case 'M':	/* "%M" - print minute (00 to 59) */
	    sprintf (padate, "%02d", ptime->tm_min);
	    padate += 2;
	    break;

	  case 'S':	/* "%S" - print second (00 to 59) */
	    sprintf (padate, "%02d", ptime->tm_sec);
	    padate += 2;
	    break;

	  case 'T':	/* "%T" - print time as HH:MM:SS */
	    sprintf (padate, "%02d:%02d:%02d",
		ptime->tm_hour, ptime->tm_min, ptime->tm_sec);
	    padate += 8;
	    break;

	  case 'j':	/* "%j" - print julian date (001 to 366) */
	    sprintf (padate, "%03d", ptime->tm_yday+1);
	    padate += 3;
	    break;

	  case 'w':	/* "%w" - print day of week (0 to 6) (0=Sunday) */
	    sprintf (padate, "%1d", ptime->tm_wday);
	    padate += 1;
	    break;

	  case 'a':	/* "%a" - print abbreviated weekday (Sun to Sat) */
	    sprintf (padate, "%3.3s", Wdays[ptime->tm_wday]);
	    padate += 3;
	    break;

	  case 'W':	/* "%W" - print full weekday (Sunday to Saturday) */
	    sprintf (padate, "%s", FWdays[ptime->tm_wday]);
	    padate += strlen(FWdays[ptime->tm_wday]);

	  case 'h':	/* "%h" - print abbreviated month (Jan to Dec) */
	    sprintf (padate, "%3.3s", Months[ptime->tm_mon]);
	    padate += 3;
	    break;

	  case 'F':	/* "%F" - print full month (January to December) */
	    sprintf (padate, "%s", Months[ptime->tm_mon]);
	    padate += strlen(Months[ptime->tm_mon]);
	    break;

	  case 'r':	/* "%r" - print time in AM/PM notation */
	    i = ptime->tm_hour;
	    if (i > 12)
		i -= 12;
	    sprintf (padate, "%02d:%02d:%02d %2.2s",
		i, ptime->tm_min, ptime->tm_sec,
		ptime->tm_hour >= 12 ? "PM" : "AM");
	    padate += 11;
	    break;

	  case 'E':	/* "%E" - print day of month with no padding (CFI) */
	    sprintf (padate, "%d", ptime->tm_mday);
	    padate += (ptime->tm_mday > 9 ? 2 : 1);
	    break;

	  case 'x':	/* "%x" - print date in system format (#seconds) */
	    sprintf (padate, "%ld", (long)ldate);
	    padate += strlen (padate);
	    break;

	  case 'X':	/* "%X" - print date in system format, days only */
	    sprintf (padate, "%ld", (long)ldate/(60*60*24));	/* secs/day */
	    padate += strlen (padate);
	    break;

	  default:
	    *padate++ = '%';
	    *padate++ = c;
	    *padate++ = '?';
	    break;
	}
    }

    if (c != endc)
	fprintf (stderr, "?missing close quote for date format string\n");

    *padate = '\0';
    if (endc != '\0')		/* need to return #chars in format string */
	*pnmod = pfmt - poptstr;
    return (&adate[0]);

}  /* fmtdate */

/******************************************************************************/

static char *getpwname (uid)
    register int  uid;
{  /* get user name from passwd file */
    static struct  uids {
	int   u_id;
	char *u_name;
	} uids[MAXUIDS];
    static int  nuids=0;
    register int  i;
    struct passwd  *pw, *getpwuid();

    for (i=0; i<nuids; ++i)
	if (uids[i].u_id == uid)
	    return (uids[i].u_name);

    if (nuids == MAXUIDS)
    {
	fprintf (stderr, "?too many user names (max=%d)\n", MAXUIDS);
	return ((char *)NULL);
    }

	/* add new user name */
    uids[nuids].u_id = uid;
    if ((pw = getpwuid(uid)) == (struct passwd *)NULL)
	uids[nuids].u_name = (char *)NULL;
    else
	uids[nuids].u_name = copystr (pw->pw_name);
    return (uids[nuids++].u_name);

}  /* getpwname */

/******************************************************************************/

static char *getgrname (gid)
    register int  gid;
{  /* get user group name from group file */
    static struct gids {
	int   g_id;
	char *g_name;
	} gids[MAXGIDS];
    static int    ngids=0;
    register int  i;
    struct group *gr, *getgrgid();

    for (i=0; i<ngids; ++i)
	if (gids[i].g_id == gid)
	    return (gids[i].g_name);

    if (ngids == MAXGIDS)
    {
	fprintf (stderr, "?too many group names (max=%d)\n", MAXGIDS);
	return ((char *)NULL);
    }

	/* add new group name */
    gids[ngids].g_id = gid;
    if ((gr = getgrgid(gid)) == (struct group *)NULL)
	gids[ngids].g_name = (char *)NULL;
    else
	gids[ngids].g_name = copystr (gr->gr_name);
    return (gids[ngids++].g_name);

}  /* getgrname */

/******************************************************************************/

static char *copystr (str)
    char  *str;
{  /* Allocate space for a string, copy it, and return ptr to new space */
    char  *p;
    int    i;
    
    i = strlen(str) + 1;
    p = (char *) malloc ((unsigned) i);
    if (p != (char *)NULL)
	strcpy (p, str);
    return (p);

}  /* copystr */

/******************************************************************************/

static time_t getsixmosago ()
{  /* Calculate the system date for 6 months ago */
    register int   i, ndays, month, year;
    time_t         today;
    struct tm     *now;
    
    today = time ((time_t *)NULL);
    now = localtime (&today);
    month = now->tm_mon + 1;	/* tm_mon is 0-11 */
    year = now->tm_year;
    ndays = 0;
    for (i=1; i<=6; ++i)
    {
	if (--month < 1)	/* starts with last month */
	{
	    month = 12;
	    year--;
	}
	switch (month)
	{
	case 1:		/* Jan */
	case 3:		/* Mar */
	case 5:		/* May */
	case 7:		/* Jul */
	case 8:		/* Aug */
	case 10:	/* Oct */
	case 12:	/* Dec */
	    ndays += 31;
	    break;
	case 4:		/* Apr */
	case 6:		/* Jun */
	case 9:		/* Sep */
	case 11:	/* Nov */
	    ndays += 30;
	    break;
	case 2:		/* Feb */
	    if ((year%4) == 0 && (year%100) != 0)
		ndays += 29;
	    else
		ndays += 28;
	    break;
	}
    }

    return (today - (60*60*24*ndays));

}  /* getsixmosago */
