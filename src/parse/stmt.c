#include "parse.h"


unsigned parse_stmt(
	const sparse_t* src, const char* ptr,
	const unsigned* label,
	parse_stmt_t* stmt)
{
	unsigned len = 0;
	stmt->type  = PARSE_STMT_EMPTY;
	stmt->label = label;

	if (len == 0) len = parse_stmt_continue(src, ptr, stmt);
	if (len == 0) len = parse_stmt_stop_pause(src, ptr, stmt);
	if (len == 0) len = parse_stmt_go_to(src, ptr, stmt);
	if (len == 0) len = parse_stmt_assign(src, ptr, stmt);
	if (len == 0) len = parse_stmt_if_computed(src, ptr, stmt);
	if (len == 0) len = parse_stmt_write(src, ptr, stmt);
	if (len == 0) len = parse_stmt_format(src, ptr, stmt);

	if ((ptr[len] == '\r')
		|| (ptr[len] == '\n')
		|| (ptr[len] == ';'))
	{
		len += 1;
	}
	else
	{
		if (len == 0)
			return 0;

		sparse_warning(src, ptr,
			"Expected newline or semicolon after statement");
	}

	return len;
}

void parse_stmt_cleanup(
	parse_stmt_t stmt)
{
	unsigned i;
	switch (stmt.type)
	{
		case PARSE_STMT_ASSIGN:
			parse_expr_cleanup(stmt.assign.rhs);
			break;
		case PARSE_STMT_STOP:
		case PARSE_STMT_PAUSE:
			if (stmt.stop_pause.has_code)
				parse_expr_cleanup(stmt.stop_pause.code);
			break;
		case PARSE_STMT_IF_COMPUTED:
			free(stmt.if_comp.label);
			parse_expr_cleanup(stmt.if_comp.cond);
			break;
		case PARSE_STMT_WRITE:
			for (i = 0; i < stmt.write.elem_count; i++)
				parse_expr_cleanup(stmt.write.elem[i]);
			free(stmt.write.elem);
			parse_expr_cleanup(stmt.write.file);
			break;
		case PARSE_STMT_FORMAT:
			for (i = 0; i < stmt.format.desc_count; i++)
				parse_format_desc_cleanup(stmt.format.desc[i]);
			free(stmt.format.desc);
			break;
		default:
			break;
	}
}
