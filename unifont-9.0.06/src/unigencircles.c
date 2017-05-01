/*
   unigencircles.c -- program to superimpose dashed combining circles
                      on combining glyphs.

   Author: Paul Hardy, 2013.

   LICENSE:

      This program is free software: you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation, either version 2 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXSTRING	256


int
main (int argc, char **argv)
{

   char teststring[MAXSTRING];  /* current input line                       */
   int  loc;                    /* Unicode code point of current input line */
   char *gstart;                /* glyph start, pointing into teststring    */

   char combining[0x110000];

   void add_single_circle(char *); /* add a single-width dashed circle */
   void add_double_circle(char *); /* add a double-width dashed circle */

   FILE *infilefp;

   /*
      if (argc != 3) {
         fprintf (stderr,
                "\n\nUsage: %s combining.txt nonprinting.hex < unifont.hex > unifontfull.hex\n\n");
         exit (EXIT_FAILURE);
      }
   */

   /*
      Read the combining characters list.
   */
   /* Start with no combining code points flagged */
   memset (combining, 0, 0x110000 * sizeof (char));

   if ((infilefp = fopen (argv[1],"r")) == NULL) {
      fprintf (stderr,"ERROR - combining characters file %s not found.\n\n",
              argv[1]);
      exit (EXIT_FAILURE);
   }

   /* Flag list of combining characters to add a dashed circle. */
   while (fscanf (infilefp, "%X", &loc) != EOF) combining[loc] = 1;

   fclose (infilefp); /* all done reading combining.txt */

   /* Now read the non-printing glyphs; they never have dashed circles */
   if ((infilefp = fopen (argv[2],"r")) == NULL) {
      fprintf (stderr,"ERROR - nonprinting characters file %s not found.\n\n",
              argv[1]);
      exit (EXIT_FAILURE);
   }

   /* Reset list of nonprinting characters to avoid adding a dashed circle. */
   while (fscanf (infilefp, "%X:%*s", &loc) != EOF) combining[loc] = 0;

   fclose (infilefp); /* all done reading nonprinting.hex */

   /*
      Read the hex glyphs.
   */
   teststring[MAXSTRING - 1] = '\0';   /* so there's no chance we leave array  */
   while (fgets (teststring, MAXSTRING-1, stdin) != NULL) {
      sscanf (teststring, "%X", &loc);     /* loc == the Uniocde code point    */
      gstart = index (teststring,':') + 1; /* start of glyph bitmap            */
      if (combining[loc]) {                /* if a combining character         */
         if (strlen (gstart) < 35) add_single_circle (gstart); /* single-width */
         else add_double_circle (gstart);                      /* double-width */
      }
      printf ("%s", teststring); /* output the new character .hex string */
   }

   exit (EXIT_SUCCESS);
}


/*
   add_single_circle - superimpose a single-width dashed combining circle.
*/
void
add_single_circle (char *glyphstring)
{

   char newstring[256];
   /* Circle hex string pattern is "00000008000024004200240000000000" */
   char circle[32]={0x0,0x0,  /* row  1 */
                    0x0,0x0,  /* row  2 */
                    0x0,0x0,  /* row  3 */
                    0x0,0x0,  /* row  4 */
                    0x0,0x0,  /* row  5 */
                    0x0,0x0,  /* row  6 */
                    0x2,0x4,  /* row  7 */
                    0x0,0x0,  /* row  8 */
                    0x4,0x2,  /* row  9 */
                    0x0,0x0,  /* row 10 */
                    0x2,0x4,  /* row 11 */
                    0x0,0x0,  /* row 12 */
                    0x0,0x0,  /* row 13 */
                    0x0,0x0,  /* row 14 */
                    0x0,0x0,  /* row 15 */
                    0x0,0x0}; /* row 16 */

   int digit1, digit2; /* corresponding digits in each string */

   int i; /* index variables */

   /* for each character position, OR the corresponding circle glyph value */
   for (i = 0; i < 32; i++) {
      glyphstring[i] = toupper (glyphstring[i]);

      /* Convert ASCII character to a hexadecimal integer */
      digit1 = (glyphstring[i] <= '9') ?
               (glyphstring[i] - '0') : (glyphstring[i] - 'A' + 0xA);

      /* Superimpose dashed circle */
      digit2 = digit1 | circle[i];

      /* Convert hexadecimal integer to an ASCII character */
      newstring[i] = (digit2 <= 9) ?
                     ('0' + digit2) : ('A' + digit2 - 0xA);
   }

   /* Terminate string for output */
   newstring[i++] = '\n';
   newstring[i++] = '\0';

   strncpy (glyphstring, newstring, i);

   return;
}


/*
   add_double_circle - superimpose a single-width dashed combining circle.
*/
void
add_double_circle (char *glyphstring)
{

   char newstring[256];
   /* Circle hex string pattern is "00000008000024004200240000000000" */
   char circle[64]={0x0,0x0,0x0,0x0,  /* row  1 */
                    0x0,0x0,0x0,0x0,  /* row  2 */
                    0x0,0x0,0x0,0x0,  /* row  3 */
                    0x0,0x0,0x0,0x0,  /* row  4 */
                    0x0,0x0,0x0,0x0,  /* row  5 */
                    0x0,0x0,0x0,0x0,  /* row  6 */
                    0x0,0x2,0x4,0x0,  /* row  7 */
                    0x0,0x0,0x0,0x0,  /* row  8 */
                    0x0,0x4,0x2,0x0,  /* row  9 */
                    0x0,0x0,0x0,0x0,  /* row 10 */
                    0x0,0x2,0x4,0x0,  /* row 11 */
                    0x0,0x0,0x0,0x0,  /* row 12 */
                    0x0,0x0,0x0,0x0,  /* row 13 */
                    0x0,0x0,0x0,0x0,  /* row 14 */
                    0x0,0x0,0x0,0x0,  /* row 15 */
                    0x0,0x0,0x0,0x0}; /* row 16 */

   int digit1, digit2; /* corresponding digits in each string */

   int i; /* index variables */

   /* for each character position, OR the corresponding circle glyph value */
   for (i = 0; i < 64; i++) {
      glyphstring[i] = toupper (glyphstring[i]);

      /* Convert ASCII character to a hexadecimal integer */
      digit1 = (glyphstring[i] <= '9') ?
               (glyphstring[i] - '0') : (glyphstring[i] - 'A' + 0xA);

      /* Superimpose dashed circle */
      digit2 = digit1 | circle[i];

      /* Convert hexadecimal integer to an ASCII character */
      newstring[i] = (digit2 <= 9) ?
                     ('0' + digit2) : ('A' + digit2 - 0xA);
   }

   /* Terminate string for output */
   newstring[i++] = '\n';
   newstring[i++] = '\0';

   strncpy (glyphstring, newstring, i);

   return;
}

