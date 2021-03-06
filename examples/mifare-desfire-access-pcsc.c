/*-
 * Copyright (C) 2010, Romain Tartiere.
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 * 
 * $Id$
 */

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <freefare_pcsc.h>

uint8_t key_data_null[8]  = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

int
main(int argc, char *argv[])
{
    int error = EXIT_SUCCESS;
    char *str = NULL;
    char *reader = NULL;
    LONG err;
    MifareTag *tags = NULL;

    if (argc > 1)
	errx (EXIT_FAILURE, "usage: %s", argv[0]);

    struct pcsc_context *context;
    pcsc_init(&context);
    if (!context)
    {
	fprintf(stderr, "unable to init pcsc context\n");
	exit(EXIT_FAILURE);
    }

    err = pcsc_list_devices(context, &str);
    if (err != SCARD_S_SUCCESS)
    {
	fprintf(stderr, "no readers found\n");
	exit(EXIT_FAILURE);
    }
	
    size_t device_count = 0;
    reader = str;
    while(*reader != '\0')
    {
	printf("Reader %s found\n", reader);
	device_count++;
	reader += strlen(reader) + 1;
    }
    
    reader = str;

    while(*reader != '\0')
    {
	tags = freefare_get_tags_pcsc (context, reader);
	if (!tags) {
	    pcsc_exit (context);
	    errx (EXIT_FAILURE, "Error listing tags.");
	}

	for (int i = 0; (!error) && tags[i]; i++) {
	    if (MIFARE_DESFIRE != freefare_get_tag_type (tags[i]))
		continue;

	    int res;
	    char *tag_uid = freefare_get_tag_uid (tags[i]);

	    res = mifare_desfire_connect (tags[i]);
	    if (res < 0) {
		warnx ("Can't connect to Mifare DESFire target.");
		error = EXIT_FAILURE;
		break;
	    }


	    MifareDESFireKey key = mifare_desfire_des_key_new_with_version (key_data_null);
	    res = mifare_desfire_authenticate (tags[i], 0, key);
	    if (res < 0)
		errx (EXIT_FAILURE, "Authentication on master application failed");

	    MadAid mad_aid = { 0x12, 0x34 };
	    MifareDESFireAID aid = mifare_desfire_aid_new_with_mad_aid (mad_aid, 0x5);
	    res = mifare_desfire_create_application (tags[i], aid, 0xFF, 0x1);
	    if (res < 0)
		errx (EXIT_FAILURE, "Application creation failed");

	    res = mifare_desfire_select_application (tags[i], aid);
	    if (res < 0)
		errx (EXIT_FAILURE, "Application selection failed");

	    res = mifare_desfire_authenticate (tags[i], 0, key);
	    if (res < 0)
		errx (EXIT_FAILURE, "Authentication on application failed");

	    res = mifare_desfire_create_std_data_file (tags[i], 1, MDCM_ENCIPHERED, 0x0000, 20);
	    if (res < 0)
		errx (EXIT_FAILURE, "File creation failed");

	    const char *s= "Hello World";
	    res = mifare_desfire_write_data (tags[i], 1, 0, strlen (s), s);
	    if (res < 0)
		errx (EXIT_FAILURE, "File write failed");

	    char buffer[20];
	    res = mifare_desfire_read_data (tags[i], 1, 0, 0, buffer);
	    if (res < 0)
		errx (EXIT_FAILURE, "File read failed");

	    res = mifare_desfire_select_application (tags[i], NULL);
	    if (res < 0)
		errx (EXIT_FAILURE, "Master application selection failed");

	    res = mifare_desfire_authenticate (tags[i], 0, key);
	    if (res < 0)
		errx (EXIT_FAILURE, "Authentication on master application failed");

	    res = mifare_desfire_format_picc (tags[i]);
	    if (res < 0)
		errx (EXIT_FAILURE, "PICC format failed");

	    mifare_desfire_key_free (key);
	    free (tag_uid);
	    free (aid);

	    mifare_desfire_disconnect (tags[i]);
	}
	reader += strlen(reader) + 1;

	freefare_free_tags (tags);
    }
    pcsc_exit(context);
    exit (error);
} /* main() */

