/*
 * Copyright (C) 2023 The RADclock Project (see AUTHORS file)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdio.h>

#include "verbose.h"

#include "ntp_auth.h"

/*
 * Convert char string into byte array
 */
char *
cstr_2_bytes(char *key_str)
{
	int num_bytes = strlen(key_str) / 2;

	char *new_crypt_data = malloc(sizeof(char)*num_bytes);
	char *pos = key_str;
	for (size_t count = 0; count < num_bytes; count++) {
		sscanf(pos, "%2hhx", &new_crypt_data[count]);
		pos += 2;
	}
	return new_crypt_data;
}


/*
 * Adds authentication key
*/
void
add_auth_key(char ** key_data, char * buff, int is_private_key)
{
	char crypt_type[16];
	char crypt_key[64];
	int key_id = -1;
	int min_key_id = 0;    // DV: seems to be actual min index in C notation
	int max_key_id = PRIVATE_CN_NTP_KEYS - 1;   // DV: seems to be total number, not C notation

	if (is_private_key) {
		min_key_id = PRIVATE_CN_NTP_KEYS;
		max_key_id = MAX_NTP_KEYS;
	}

	if (sscanf( buff, "%d %s %s", &key_id, crypt_type, crypt_key) == 3) {
		// Set the private key ids to start from index PRIVATE_CN_NTP_KEYS
		if (is_private_key)
			key_id += PRIVATE_CN_NTP_KEYS; // want to do if fail test below?

		// Sanity check that private keys start at PRIVATE_CN_NTP_KEYS
		if (is_private_key && key_id < PRIVATE_CN_NTP_KEYS) {
			verbose(LOG_ERR, "Auth file read: Key id %d private key conflict", key_id);
			return;
		}

		// Sanity check that public keys are below PRIVATE_CN_NTP_KEYS
		if (!is_private_key && key_id >= PRIVATE_CN_NTP_KEYS) {
			verbose(LOG_ERR, "Auth file read: Key id %d public key conflict", key_id);
			return;
		}

		if (min_key_id <= key_id && key_id < max_key_id) {
			// Data for this key has already been inserted, warn user
			if (key_data[key_id])
				verbose(LOG_WARNING, "Auth file read: Key id %d already allocated", key_id);

			if (strcmp("SHA1", crypt_type) == 0)
				key_data[key_id] = cstr_2_bytes(crypt_key);
			else
				verbose(LOG_WARNING, "Auth file read: Invalid key type. "
				    "Only SHA1 currently supported, got %s", crypt_type);
		} else
			verbose(LOG_WARNING, "Auth file read: key ids must be in range 1-127");  // but min_key_id = 0  ..
	}
}

/*
 * Read authentication keys from a single file
 */
void read_key_file(char **key_data, char *file_path, int is_private_key)
{
	FILE *fp = fopen(file_path, "r");
	if (fp == 0) {
		verbose(LOG_WARNING, "No authentication keys present within %s", file_path);
		return;
	}
	
	char buff[128];
	int buff_size = 0;
	int ignore_content = 0;

	buff[0] = 0;
	char c;
	while ( !feof(fp) ) {
		fread(&c, 1, 1, fp);
		if (c == '#')
			ignore_content = 1;

		if (! ignore_content && buff_size < 126) {
			buff[buff_size] = c;
			buff_size ++;
		}

		if (c == '\n') {
			buff[buff_size] = 0;
			add_auth_key(key_data, buff, is_private_key);
			buff_size = 0;
			ignore_content = 0;
		}
	}

	// Check if the last line with no '\n' had data. The add_auth_key checks that the contents of the line
	buff[buff_size] = 0;
	add_auth_key(key_data, buff, is_private_key);

	fclose(fp);
}

/*
 * Read authentication keys to validate requests
 * TODO: Could make MAX_NTP_KEYS dynamic in future
 */
char **
read_keys()
{
	char **key_data = malloc(sizeof(char *)*MAX_NTP_KEYS);
	memset(key_data, 0, sizeof(char *)*MAX_NTP_KEYS);

	read_key_file(key_data, PUBLIC_KEY_FILE_PATH, 0);
	read_key_file(key_data, PRIVATE_KEY_FILE_PATH, 1);

	return key_data;
}
