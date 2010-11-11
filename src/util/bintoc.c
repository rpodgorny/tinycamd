
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define MAXSYM 256

int main( int argc, char **argv)
{
    int i;
    FILE *out = tmpfile();;

    if ( !out) {
	fprintf(stderr,"Failed to open temporary output file.\n");
	exit(1);
    }

    for ( i = 1; i < argc; i++) {
	char sym[1024];
	char fname[1024];
	FILE *f;
	int pos;
	int j;
	int indent;

	if ( sscanf( argv[i], "%1023[^=]=%1023s", sym, fname) != 2) {
	    fprintf(stderr,"Argument '%s' is too mysterious to process.\n", argv[i]);
	    exit(1);
	}

	f = fopen( fname, "r");
	if ( !f) {
	    fprintf(stderr,"Failed to open `%s' for reading: %s\n", fname, strerror(errno));
	    exit(1);
	}

	fprintf( out, "const char %s[] = \"", sym);
	indent = 15 + strlen(sym);

	pos = 0;
	for (;;) {
	    int ch = fgetc(f);
	    if ( ch == EOF) {
		if ( ferror(f)) {
		    fprintf(stderr,"Error reading %s: %s\n", fname, strerror(errno));
		    exit(1);
		}
		break;
	    }
	    switch(ch) {
	      case '\n':
		fprintf(out,"\\n");
		pos += 2;
		break;
	      case '\r':
		fprintf(out,"\\r");
		pos += 2;
		break;
	      case '\t':
		fprintf(out,"\\t");
		pos += 2;
		break;
	      case '\\':
		fprintf(out,"\\\\");
		pos += 2;
		break;
	      case '"':
		fprintf(out,"\\\"");
		pos += 2;
		break;
	      default:
		if ( isgraph(ch) || ch == ' ') {
		    fputc(ch,out);
		    pos += 1;
		} else {
		    fprintf(out, "\\%03o", ch);
		    pos += 3;
		}
	    }
	    if ( pos >= 64) {
		fputc('"', out);
		fputc('\n', out);
		for ( j = 0; j < indent; j++) fputc(' ', out);
		fputc('"', out);
		pos = 0;
	    }
	}
	fputc('"', out);
	fputc(';', out);
	fputc('\n', out);
	fprintf( out, "const int %s_size = sizeof(%s)-1;\n", sym, sym);
	fputc('\n', out);
    }

    if ( ferror( out)) {
	fprintf(stderr,"Error writing temporary output file: %s\n", strerror(errno));
	exit(1);
    }

    rewind(out);

    for (;;) {
	int ch = fgetc(out);
	if ( ch == EOF) {
	    if ( feof(out)) break;
	    else {
		fprintf(stderr,"Failed rereading temporary file: %s\n", strerror(errno));
		exit(1);
	    }
	}
	if ( fputc( ch, stdout) == EOF) {
	    fprintf(stderr,"Failed writing output: %s\n", strerror(errno));
	    exit(1);
	}
    }
    fclose(out);

    return 0;
}
