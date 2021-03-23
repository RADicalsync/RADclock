/*
 * Copyright (C) 2006-2012, Julien Ridoux and Darryl Veitch
 * Copyright (C) 2013-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
 * All rights reserved.
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

#ifndef NTP_AUTH
#define NTP_AUTH


#define PUBLIC_KEY_FILE_PATH "/etc/radclock_public.keys"
#define PRIVATE_KEY_FILE_PATH "/etc/radclock_private.keys"
#define MAX_NTP_KEYS  128
#define PRIVATE_CN_NTP_KEYS  MAX_NTP_KEYS / 2


#define IS_PRIVATE_KEY(x) (x>=PRIVATE_CN_NTP_KEYS)
#define IS_PUBLIC_KEY(x) (x<PRIVATE_CN_NTP_KEYS)

/*
 * Convert char string into byte array
*/
char *
cstr_2_bytes(char * key_str);


/*
 * Adds authentication key
*/
void
add_auth_key(char ** key_data, char * buff, int is_private_key);

/*
 * Read authentication keys to validate requests
 */
char** 
read_keys();

#endif // #ifndef NTP_AUTH
