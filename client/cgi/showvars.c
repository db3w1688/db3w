/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/showvars.c
 *
 *  Copyright (C) 1987-2019 Charles Chou <cchou@db3w.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Copyright (C) 1997-2019 Kimberlite Software <info@kimberlitesoftware.com> version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Copyright (C) 1997-2019 Kimberlite Software <info@kimberlitesoftware.com> for more details.
 *
 *  You should have received a copy of the GNU General Public Copyright (C) 1997-2019 Kimberlite Software <info@kimberlitesoftware.com> along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <string.h>

// Global storage
char InputBuffer[1024];       // generic input buffer

extern char **_environ;

// SwapChar:  This routine swaps one character for another
void SwapChar(char * pOriginal, char cBad, char cGood) {
    int i;    // generic counter variable

    // Loop through the input string (cOriginal), character by
    // character, replacing each instance of cBad with cGood

    i = 0;
    while (pOriginal[i]) {
        if (pOriginal[i] == cBad) pOriginal[i] = cGood;
        i++;
    }
}

// IntFromHex:  A subroutine to unescape escaped characters.
static int IntFromHex(char *pChars) {
    int Hi;        // holds high byte
    int Lo;        // holds low byte
    int Result;    // holds result

    // Get the value of the first byte to Hi

    Hi = pChars[0];
    if ('0' <= Hi && Hi <= '9') {
        Hi -= '0';
    } else
    if ('a' <= Hi && Hi <= 'f') {
        Hi -= ('a'-10);
    } else
    if ('A' <= Hi && Hi <= 'F') {
        Hi -= ('A'-10);
    }

    // Get the value of the second byte to Lo

    Lo = pChars[1];
    if ('0' <= Lo && Lo <= '9') {
        Lo -= '0';
    } else
    if ('a' <= Lo && Lo <= 'f') {
        Lo -= ('a'-10);
    } else
    if ('A' <= Lo && Lo <= 'F') {
        Lo -= ('A'-10);
    }
    Result = Lo + (16 * Hi);
    return (Result);
}

// And now, the main URLDecode() routine.  The routine loops through
// the string pEncoded, and decodes it in place.  It checks for
// escaped values, and changes all plus signs to spaces.  The result
// is a normalized string.  It calls the two subroutines directly
// above in this listing.

void URLDecode(unsigned char *pEncoded) {
    char *pDecoded;          // generic pointer

    // First, change those pesky plusses to spaces
    SwapChar (pEncoded, '+', ' ');

    // Now, loop through looking for escapes
    pDecoded = pEncoded;
    while (*pEncoded) {
        if (*pEncoded=='%') {
            // A percent sign followed by two hex digits means
            // that the digits represent an escaped character.  We
            // must decode it.

            pEncoded++;
            if (isxdigit(pEncoded[0]) && isxdigit(pEncoded[1])) {
                *pDecoded++ = (char) IntFromHex(pEncoded);
                pEncoded += 2;
            }
        } else {
            *pDecoded ++ = *pEncoded++;
        }
    }
    *pDecoded = '\0';
}

// GetPOSTData:  Read in data from POST operation
void GetPOSTData() {
    char * pContentLength;  // pointer to CONTENT_LENGTH
    int  ContentLength;     // value of CONTENT_LENGTH string
    int  i;                 // local counter
    int  x;                 // generic char holder

    // Retrieve a pointer to the CONTENT_LENGTH variable
    pContentLength = getenv("CONTENT_LENGTH");

    // If the variable exists, convert its value to an integer
    // with atoi()
    if (pContentLength != NULL) {
    	ContentLength = atoi(pContentLength);
    }
    else
    {
    	ContentLength = 0;
    }

    // Make sure specified length isn't greater than the size
    // of our statically-allocated buffer
    if (ContentLength > sizeof(InputBuffer)-1) {
    	ContentLength = sizeof(InputBuffer)-1;
    }

    // Now read ContentLength bytes from STDIN
    i = 0;
    while (i < ContentLength) {
    	x = fgetc(stdin);
    	if (x==EOF) break;
    	InputBuffer[i++] = x;
    }

    // Terminate the string with a zero
    InputBuffer[i] = '\0';

    // And update ContentLength
    ContentLength = i;
}

// PrintVars:  Prints out all environment variables
void PrintVars() {
    int i = 0;          // generic counter
    
    // Tell the user what's coming and start an unnumbered list
    printf("<b>Environment Variables</b>\n");
    printf("<ul>\n");
    	
    // For each variable, decode and print
    while (_environ[i]) {
		strcpy(InputBuffer, _environ[i]);
		URLDecode(InputBuffer);
		printf("<li>%s\n",InputBuffer);
        i++;
    }

    // Terminate the unnumbered list
    printf("</ul>\n");
}


// PrintMIMEHeader:  Prints content-type header
void PrintMIMEHeader() {
    printf("Content-type: text/html\n\n");
system("/bin/ls");
}

// PrintHTMLHeader:  Prints HTML page header
void PrintHTMLHeader() {
    printf(
        "<html>\n"
        "<head><title>showvars.c</title></head>\n"
        "<body>\n"
        "<h1>Special Edition: <i>Using CGI</i></h1>\n"
        "<b>showvars.c</b> -- demonstration CGI written "
        "in C to show environment variables and POSTed "
        "data<p>"
        );
    printf("<p><b>Current Directory: </b>%s</p>\n",getcwd(NULL,MAXPATHLEN));
}

// PrintHTMLTrailer:  Prints closing HTML info
void PrintHTMLTrailer() {
    printf(
        "</body>\n"
        "</html>\n"
        );
}
    
// PrintOut:  Prints out a var=val pair
void PrintOut (char * VarVal) {
    char * pEquals;	          // pointer to equals sign
    int  i;                   // generic counter

    pEquals = strchr(VarVal, '=');    // find the equals sign
    if (pEquals != NULL) {
        *pEquals++ = '\0';            // terminate the Var name
        URLDecode(VarVal);            // decode the Var name

        // Convert the Var name to upper case
        i = 0;
        while (VarVal[i]) {
            VarVal[i] = toupper(VarVal[i]);
            i++;
        }  

        // decode the Value associated with this Var name
        URLDecode(pEquals);

        // print out the var=val pair
        printf("<li>%s=%s\n",VarVal,pEquals);
    }
}

// PrintPOSTData:  Prints data from POST input buffer
void PrintPOSTData() {
    char * pToken;			// pointer to token separator

    // Tell the user what's coming & start an unnumbered list
    printf("<b>POST Data</b>\n");
    printf("<ul>\n");

    // Print out each variable
    pToken = strtok(InputBuffer,"&");
    while (pToken != NULL) {       // While any tokens left in string
        PrintOut (pToken);         // Do something with var=val pair
        pToken = strtok(NULL,"&"); // Find the next token
    }

    // Terminate the unnumbered list
    printf("</ul>\n");
}

// The script's entry point
main() {

    char * pRequestMethod;  // pointer to REQUEST_METHOD
    
    // First, set STDOUT to unbuffered
    setvbuf(stdout,NULL,_IONBF,0);

    // Figure out how we were invoked
    pRequestMethod = getenv("REQUEST_METHOD");

    if (pRequestMethod==NULL) {
        // No request method; must have been invoked from
        // command line.  Print a message and terminate.
        printf("This program is designed to run as a CGI script, "
               "not from the command-line.\n");

    }
    else if (strcasecmp(pRequestMethod,"GET")==0) {
        PrintMIMEHeader();      // Print MIME header
        PrintHTMLHeader();      // Print HTML header
        PrintVars();            // Print variables
        PrintHTMLTrailer();     // Print HTML trailer
    }

    else if (strcasecmp(pRequestMethod,"POST")==0) {
        PrintMIMEHeader();      // Print MIME header
        PrintHTMLHeader();      // Print HTML header
        PrintVars();            // Print variables
        GetPOSTData();          // Get POST data to InputBuffer
        PrintPOSTData();        // Print out POST data
        PrintHTMLTrailer();     // Print HTML trailer
    }

    else
    {
        PrintMIMEHeader();      // Print MIME header
        PrintHTMLHeader();      // Print HTML header
        printf("Only GET and POST methods supported.\n");
        PrintHTMLTrailer();     // Print HTML trailer
    }

    // Finally, flush the output
    fflush(stdout);
}

