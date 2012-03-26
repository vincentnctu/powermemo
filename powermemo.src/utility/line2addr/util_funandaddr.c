/* vi: set sw=4 ts=4: */
/*
 * addr2line.c,
 *
 * Copyright (C) 2011 by Vincent Chang <changyihsin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#define MAX_MSG_LEN	2000
#define MAX_CMD_LEN 128

extern int optind;
extern char *optarg;
char file_name[256];
int is_file_name=0;
char func_name[256];
int is_func_name=0;
char bin_name[256];
int is_bin_name=0;
char asm_name[256];
int is_asm_name;
int line_num=0;


int run_shell(char * buf, int size, const char * format, ...)
{
    FILE * fp;
    int i, c;
    char cmd[MAX_CMD_LEN];
    va_list marker;

    va_start(marker, format);
    vsnprintf(cmd, sizeof(cmd), format, marker);
    va_end(marker);

    fp = popen(cmd, "r");
    if (fp)
    {
        for (i = 0; i < size-1; i++)
        {
            c = fgetc(fp);
            if (c == EOF) break;
            buf[i] = (char)c;
        }
        buf[i] = '\0';
        pclose(fp);

        /* remove the last '\n' */
        i = strlen(buf);
        if (buf[i-1] == '\n') buf[i-1] = 0;
        return 0;
    }
    buf[0] = 0;
    return -1;
}

/* 
 * string library 
 */

int isEmpty(const char *s)
{
	if(s == NULL)
		return 1;
	if(*s == 0)
		return 1;
	return 0;
}

char *ltrim(char *str)
{
	if(str == NULL)
		return NULL;
	while(*str == ' ' || *str == '\t')
		str++;
	return str;
}

char *rtrim(char *str)
{
	char *index;
	
	if(str == NULL)
		return NULL;
	if(*str == 0)
		return str;
	index = str + strlen(str) - 1;
	while(*index == ' ' || *index == '\t')
	{
		*index = 0;
		if(index > str)
			index--;
	}
	return str;
}
char *trim(char *str)
{
	return(ltrim(rtrim(str)));
}
char *strsep(char **stringp, const char *delim)
{
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}
}

/* Extract token from string and remove leading and trailing blanks */
char *strsep_t(char **stringp, const char *delim)
{
	return trim(strsep(stringp, delim));
}

char *get_line(char **stringp)
{
	char *line;

	if(*stringp == NULL)
		return NULL;
	line = strsep(stringp, "\n");
	if(line == NULL)
		return NULL;
	line = strsep(&line, "\r");
	
	return trim(line);
}
int isHexDigit(char x)
{
    if ((x >= 0x30) && (x <= 0x39))
        return 1;
    else if ((x >= 0x41) && (x <=0x46))
        return 1;
    else if ((x >= 0x61) && (x <= 0x66))
        return 1;
    
    return 0;
}
int isAddress(char *str)
{
	char *ch=NULL;

	ch=str;
	while (ch && *ch != 0)
		if (isHexDigit(*ch++) == 0)
			return 0;
	return 1;
}

int parse_hex(char *str, unsigned long *value)
{
    char *buf_ptr;
    char c;

    buf_ptr = str ;
    *value = 0 ;

    while ((c = *buf_ptr++) != '\0')
    {
        /* check if input char is hex symbol? */
        if(isxdigit(c))
        {
            /* convert char symbol to upper case */
            c = (char) toupper((int)c );
            /* 'A' ~ 'F' */
            if ((c -= '0') > 9 )
                c -= ('A'-'9'-1); /* convert the hex symbol to hex number */
            *value = 16 * (*value) + c;
        }
        else
        {
            if ( c != '\0' )
                return 0;
            break ;
        } /* end of if-else */
    }
    return 1 ;
}  /* End of parse_hex()*/

char *get_FuncName(char **str, char *start)
{
	char *tmp_str;

	tmp_str=strsep(str, "<");
	if (tmp_str == NULL)
		return NULL;
	
	strcpy(start, tmp_str);
	//printf("function addr:%s\n", start);
	tmp_str=strsep(str, ">");
	//printf("FuncName=%s\n", tmp_str);
	return tmp_str;
}
static void show_usage(int exit_code)
{
    printf("Usage: line2addr [OPTIONS]\n"
            "  -h                	show this help message.\n"
            "  -s  [file name]      source code file name\n"
            "  -f  [function name]	source code function name\n"
			"  -l  [line number]	line number in the source code\n"
			"  -a  [assembly code]  assembly file generated by (arch)-objdump\n"
			"  -e  [binary name]	executable file name, vmlinux, userspace application\n");			
    exit(exit_code);
}

static void parse_args(int argc, char * argv[])
{
    int opt, i;

	//for (i = 0; i < argc; i++)
	//	printf("%d %s\n", i, argv[i]);

    while ((opt = getopt(argc, argv, "ha:s:f:l:e:")) > 0)
    {
        switch (opt)
        {
        	case 'h':   show_usage(0);                  break;
        	case 's': 
				strcpy(file_name, optarg);
				is_file_name++;
				//printf("source file name: %s\n", file_name); 				                         
				break;						
        	case 'f':   
				strcpy(func_name, optarg);   
				is_func_name++;
				//printf("function name: %s\n", func_name);
				break;
        	case 'l':   
				line_num = atoi(optarg);     
				//printf("line number: %d\n", line_num);    
				break;
            case 'a':
                strcpy(asm_name, optarg);
				is_asm_name++;
                //printf("assembly file name: %s\n", asm_name);
                break;
			case 'e':		
				strcpy(bin_name, optarg);
				is_bin_name++;
				//printf("binary: %s\n", optarg);
				break;
        	default:    show_usage(-1);                 break;
        }
    }
}

char black_fun_list[][32] =
	{ "_init",
	  ".plt",
	  "_start",
	  "call_gmon_start",
	  "__do_global_dtors_aux",
	  "call___do_global_dtors_aux",
	  "frame_dummy",
	  "call_frame_dummy",
	  "__libc_csu_init",
	  "__libc_csu_fini",
	  "__do_global_ctors_aux",
	  "call___do_global_ctors_aux",
	  "_fini",
	  "NULL"
	};
struct fun_list
{
	char funname[32];
	char addr[12];
};

int total_number = 0;
struct fun_list Fun_Addr_List[50000];

/*
 *	main function for this program
 */
void main(int argc, char *argv[])
{
	int i = 0, fdsrc, numbytes, len, got_function = 0;
	char fname[] = "vmlinux.asm";
	char *str=NULL, *line=NULL, *fun=NULL;	
	char *header=NULL, *cmd=NULL;
	char *buf, ch, *cmd_buf;
	char addrcmd[256];
	struct stat filestat;
	char start_addr[32];
	char end_addr[32];
	char pre_addr[32];

	parse_args(argc, argv);	
	/* read the assembly file, it must have function name and code and its address */
	if (is_asm_name == 0) {
		printf("You should specify binary or assembly file name\n");
		exit(-1);
	}

	if((fdsrc = open(asm_name, O_RDONLY)) < 0) {
		perror("open fdsrc");
		exit(EXIT_FAILURE);
	}
	
	fstat(fdsrc, &filestat);	
	buf=malloc(filestat.st_size);

	if (buf == NULL){
		printf("allocate memory fail\n");
		exit(EXIT_FAILURE);
	}
	cmd_buf=malloc(1024);

	if (cmd_buf == NULL) {
		printf("allocate command output fail\n");
		exit(EXIT_FAILURE);
	}

	/* read up to 10 bytes, write up to 10 bytes */
	while ((numbytes = read(fdsrc, buf, filestat.st_size)) != 0) {
		str=buf;

		while(1) {
		
			line=get_line(&str);
				
			if (line == 0)
				break;

			if (line && *line == 0)
				continue;
					
			header = strsep_t(&line, ":");
			cmd = strsep_t(&line, "\n");	
			
			/* try to find start of function */
			if (isAddress(header) == 0){
				
				if (strcmp(header, "...") == 0)
					continue;

				fun=get_FuncName(&header, start_addr);
				if (fun != NULL) {
					int i = 0;
					int black_fun = 0;
					
					/* skip the function we don't interest */
					for (i = 0; (strcmp("NULL", black_fun_list[i]) != 0); i++)
					{
						//printf("black:%s\n", black_fun_list[i]);
						if (strcmp(fun, black_fun_list[i]) == 0)
							black_fun = 1; 
					}
					if (black_fun == 0 && total_number < 50000)
					{
						//printf("%s,0x%s\n", fun, start_addr);	
						strcpy(Fun_Addr_List[total_number].funname, fun);
						strcpy(Fun_Addr_List[total_number].addr, trim(start_addr));						
						total_number++;
					}

				}	
				continue; 
			}
			
		}
	}
	//printf("Total:%d\n", total_number);
	for (i = 0; i < total_number; i++)
		printf("%s,0x%s\n", Fun_Addr_List[i].funname, Fun_Addr_List[i].addr);
	//for (i = 0; i < total_number; i++)
	//	printf("0x%s,",Fun_Addr_List[i].addr);
	
	//printf("\n");
	/* close our files and exit */
	close(fdsrc);
	exit(EXIT_SUCCESS);
}
