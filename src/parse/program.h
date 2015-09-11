#ifndef __parse_program_h__
#define __parse_program_h__

typedef struct
{
	str_ref_t        name;
	parse_implicit_t implicit;

	unsigned      decl_count;
	parse_decl_t* decl;

	unsigned      stmt_count;
	parse_stmt_t* stmt;
} parse_program_t;


unsigned parse_program(
	const sparse_t* src, const char* ptr,
	parse_program_t* program);

#endif