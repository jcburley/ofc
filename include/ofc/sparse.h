/* Copyright 2015 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ofc_sparse_h__
#define __ofc_sparse_h__

#include <stdbool.h>
#include "file.h"
#include "label_table.h"

typedef struct ofc_sparse_s ofc_sparse_t;

ofc_sparse_t* ofc_sparse_create_file(ofc_file_t* file);
ofc_sparse_t* ofc_sparse_create_child(ofc_sparse_t* parent);
bool      ofc_sparse_reference(ofc_sparse_t* sparse);
void      ofc_sparse_delete(ofc_sparse_t* sparse);

unsigned ofc_sparse_len(const ofc_sparse_t* sparse);

bool ofc_sparse_append_strn(ofc_sparse_t* sparse, const char* src, unsigned len);

/* No modifications are allowed after this call. */
void ofc_sparse_lock(ofc_sparse_t* sparse);

/* This will return NULL unless the sparse is locked. */
const char* ofc_sparse_strz(const ofc_sparse_t* sparse);

bool ofc_sparse_label_add(
	ofc_sparse_t* sparse, unsigned number);
bool ofc_sparse_label_find(
	const ofc_sparse_t* sparse, const char* ptr, unsigned* number);

bool ofc_sparse_sequential(
	const ofc_sparse_t* sparse, const char* ptr, unsigned size);

const char* ofc_sparse_parent_pointer(
	const ofc_sparse_t* sparse, const char* ptr);

ofc_lang_opts_t ofc_sparse_lang_opts(const ofc_sparse_t* sparse);

const char* ofc_sparse_get_include(
	const ofc_sparse_t* sparse);
char* ofc_sparse_include_path(
	const ofc_sparse_t* sparse, const char* path);


#include <stdarg.h>

void ofc_sparse_error_va(
	const ofc_sparse_t* sparse, const char* ptr,
	const char* format, va_list args);
void ofc_sparse_warning_va(
	const ofc_sparse_t* sparse, const char* ptr,
	const char* format, va_list args);

void ofc_sparse_error(
	const ofc_sparse_t* sparse, const char* ptr,
	const char* format, ...)
	__attribute__ ((format (printf, 3, 4)));
void ofc_sparse_warning(
	const ofc_sparse_t* sparse, const char* ptr,
	const char* format, ...)
	__attribute__ ((format (printf, 3, 4)));

#endif
