/*
 * PrBoomX: PrBoomX is a fork of PrBoom-Plus with quality-of-play upgrades. 
 *
 * Copyright (C) 2023  JadingTsunami
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __C_CVAR__
#define __C_CVAR__

#include "doomtype.h"
#include <stdio.h>

typedef enum cvartype_e {
    CVAR_TYPE_INT,
    CVAR_TYPE_FLOAT,
    CVAR_TYPE_STRING,
    CVAR_TYPE_MAX,
    CVAR_TYPE_INVALID
} cvartype_t;

typedef enum cvarstatus_e {
    CVAR_STATUS_OK,
    CVAR_STATUS_INVALID_KEY,
    CVAR_STATUS_INVALID_TYPE,
    CVAR_STATUS_INVALID_VALUE,
    CVAR_STATUS_WRONG_TYPE,
    CVAR_STATUS_KEY_NOT_FOUND,
    CVAR_STATUS_ALREADY_EXISTS,
    CVAR_STATUS_MAX
} cvarstatus_t;

typedef enum cvarflags_e {
    CVAR_FLAG_INIT = 0, /* initialized and not set */
    CVAR_FLAG_ARCHIVE = 1, /* write to config file upon exit */
} cvarflags_t;

void C_CvarInit();

dboolean C_CvarExists(const char* key);
cvarstatus_t C_CvarOverwriteFlags(const char* key, cvarflags_t flags);
cvarstatus_t C_CvarSetFlags(const char* key, cvarflags_t flags);

cvartype_t C_CvarGetType(const char* key, cvarstatus_t* status);

/* export all archive-flagged cvars to a console-executable file */
void C_CvarExportToFile(FILE* f);

/* returns true if:
 *  key exists
 *   and
 *  is a "nonzero"/non-null value, or is no-value type
 */
dboolean C_CvarIsSet(const char* key);

/* Set/Clear a CVAR (boolean flag), create if it does not exist */
cvarstatus_t C_CvarSet(const char* key);
cvarstatus_t C_CvarClear(const char* key);

int C_CvarGetAsInt(const char* key, cvarstatus_t* status);
float C_CvarGetAsFloat(const char* key, cvarstatus_t* status);
char* C_CvarGetAsString(const char* key, cvarstatus_t* status);

/* type is changed if it's something else */
cvarstatus_t C_CvarCreateOrOverwrite(const char* key, const char* value, cvarflags_t flags);

/* does NOT overwrite if exists; returns failure status */
cvarstatus_t C_CvarCreate(const char* key, const char* value, cvarflags_t flags);
cvarstatus_t C_CvarDelete(const char* key);

const char* C_CvarComplete(const char* partial);

const char* C_CvarErrorToString(cvarstatus_t status);

#endif
