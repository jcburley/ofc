#include <ofc/sema.h>
#include <math.h>
#include <tgmath.h>

#ifdef complex
/* Remove macro from complex.h */
#undef complex
#endif


static ofc_sema_typeval_t* ofc_sema_typeval__alloc(
	const ofc_sema_typeval_t typeval)
{
	ofc_sema_typeval_t* alloc_typeval =
		(ofc_sema_typeval_t*)malloc(sizeof(ofc_sema_typeval_t));

	if (!alloc_typeval) return NULL;

	*alloc_typeval = typeval;

	return alloc_typeval;
}

static bool ofc_sema_typeval__in_range(
	const ofc_sema_typeval_t* typeval)
{
	if (!typeval)
		return false;

	if (!typeval->type)
		return true;

	/* We only range check integers. */
	if (typeval->type->type
		!= OFC_SEMA_TYPE_INTEGER)
		return true;

	unsigned size = ofc_sema_type_size(
		typeval->type);

	if (size >= sizeof(typeval->integer))
		return true;

	int64_t imax = 1LL << ((size * 8) - 1);
	int64_t imin = -imax;

	return ((typeval->integer < imax)
		&& (typeval->integer >= imin));
}

static bool is_base_digit(
	char c, unsigned base, unsigned* value)
{
	if (!isalnum(c))
		return false;

	unsigned v;
	if ((c >= '0') && (c <= '9'))
		v = (c - '0');
	else
		v = 10 + (toupper(c) - 'A');

	if (v >= base)
		return false;

	if (value) *value = v;
	return true;
}

static ofc_sema_typeval_t* ofc_sema_typeval__integer_literal(
	const ofc_parse_literal_t* literal,
	const ofc_sema_type_t* type)
{
	if (!literal)
		return NULL;

	bool is_byte = false;
	if (type)
	{
		switch (type->type)
		{
			case OFC_SEMA_TYPE_INTEGER:
				break;
			case OFC_SEMA_TYPE_BYTE:
				is_byte = true;
				break;
			default:
				return NULL;
		}
	}

	unsigned base = 10;
	switch (literal->type)
	{
		case OFC_PARSE_LITERAL_NUMBER:
			base = 10;
			break;
		case OFC_PARSE_LITERAL_BINARY:
			base = 2;
			break;
		case OFC_PARSE_LITERAL_OCTAL:
			base = 8;
			break;
		case OFC_PARSE_LITERAL_HEX:
			base = 16;
			break;
		default:
			return NULL;
	}

	const char* ptr  = literal->number.base;
	unsigned    size = literal->number.size;

	if (!ptr || (size == 0))
		return NULL;

	unsigned i = 0;
	bool negate = (ptr[i] == '-');
	if (negate || (ptr[i] == '+'))
		i += 1;

	/* TODO - Check could be improved, maybe limit to valid hex alphas and x. */
	if ((i < size) && !isalnum(ptr[i]))
			return NULL;

	ofc_sema_typeval_t typeval
		= { .type = type, .integer = 0 };

	unsigned digit;
	for (; (i < size) && is_base_digit(ptr[i], base, &digit); i++)
	{
		int64_t nvalue = (typeval.integer * base) + digit;
		if (((nvalue / base) != typeval.integer)
			|| ((nvalue % base) != digit))
		{
			/* TODO - Error: Out of range for compiler */
			return NULL;
		}

		typeval.integer = nvalue;

		if (negate)
		{
			typeval.integer = -typeval.integer;
			negate = false;
		}
	}

	unsigned kind = 0;
	if ((i < size) && (ptr[i] == '_'))
	{
		i++;

		if ((i >= size)
			|| (!isdigit(ptr[i])))
			return NULL;

		for (; (i < size) && isdigit(ptr[i]); i++)
		{
			unsigned digit = (ptr[i] - '0');
			unsigned nkind = (kind * 10) + digit;
			if ((nkind / 10) != kind)
			{
				/* TODO - Error: Kind out of range. */
				return NULL;
			}

			kind = nkind;
		}

		if (type && (ofc_sema_type_size(type) != kind))
		{
			/* TODO - Error: Expected kind doesn't match literal kind. */
			return NULL;
		}
	}

	if (i < size)
	{
		/* This should never happen, and represents
		   a mismatch between the parser and semantic rules. */
		return NULL;
	}

	if (is_byte && (kind > 1))
	{
		/* TODO - Error: Byte can never have a KIND above 1. */
		return NULL;
	}

	if (!typeval.type)
	{
		typeval.type = ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_INTEGER, kind, false, false, false);

		if (!typeval.type)
		{
			/* This should never happen. */
			return NULL;
		}
	}

	if (!ofc_sema_typeval__in_range(&typeval))
	{
		/* TODO - Error: Out of range for type */
		return NULL;
	}

	return ofc_sema_typeval__alloc(typeval);
}


static bool ofc_sema_typeval__real(
	ofc_str_ref_t number, unsigned  ikind,
	long double*  value , unsigned* okind)
{
	if (!value)
		return false;

	const char* ptr  = number.base;
	unsigned    size = number.size;

	if (!ptr || (size == 0))
		return false;

	unsigned i = 0;
	bool negate = (ptr[i] == '-');
	if (negate || (ptr[i] == '+'))
		i += 1;

	if ((i < size) && !isdigit(ptr[i]))
		return false;

	*value = 0.0;
	for (; (i < size) && isdigit(ptr[i]); i++)
	{
		unsigned digit = (ptr[i] - '0');
		*value *= 10.0;
		*value += digit;
	}

	if ((i < size) && (ptr[i] == '.'))
	{
		i++;

		long double f;
		for (f = 0.1; (i < size) && isdigit(ptr[i]); i++, f /= 10.0)
		{
			unsigned digit = (ptr[i] - '0');
			*value += (digit * f);
		}
	}

	if (negate)
		*value = -*value;

	unsigned kind = 0;
	if ((i < size) && isalpha(ptr[i]))
	{
		switch (toupper(ptr[i++]))
		{
			case 'E':
				break;
			case 'D':
				kind = 8;
				break;
			case 'Q':
				kind = 16;
				break;
			default:
				return false;
		}

		negate = ((i < size) && (ptr[i] == '-'));
		if (negate || ((i < size) && (ptr[i] == '+')))
			i += 1;

		long double e = 0.0;
		for (; (i < size) && isdigit(ptr[i]); i++)
		{
			unsigned digit = (ptr[i] - '0');
			e *= 10.0;
			e += digit;
		}

		if ((i < size) && (ptr[i] == '.'))
		{
			i++;

			long double f;
			for (f = 0.1; (i < size) && isdigit(ptr[i]); i++, f /= 10.0)
			{
				unsigned digit = (ptr[i] - '0');
				e += (digit * f);
			}
		}

		if (negate)
			e = -e;

		*value *= powl(10.0, e);
	}

	if ((i < size) && (ptr[i] == '_'))
	{
		i++;

		if ((i >= size)
			|| (!isdigit(ptr[i])))
			return false;

		unsigned ukind = 0;
		for (; (i < size) && isdigit(ptr[i]); i++)
		{
			unsigned digit = (ptr[i] - '0');
			unsigned nkind = (ukind * 10) + digit;
			if ((nkind / 10) != ukind)
			{
				/* TODO - Error: Kind out of range. */
				return false;
			}

			ukind = nkind;
		}

		if ((kind != 0)
			&& (kind != ukind))
		{
			/* TODO - Error: Kinds specified in exponent and F90 style don't agree. */
			return false;
		}

		kind = ukind;
	}

	if ((ikind != 0) && (ikind != kind))
	{
		/* TODO - Error: Expected kind doesn't match literal kind. */
		return false;
	}
	kind = (kind > 0 ? kind : ikind);

	if (kind > sizeof(*value))
	{
		/* TODO - Error: REAL kind too large for us to handle. */
		return false;
	}

	if (i < size)
	{
		/* This should never happen, and represents
		   a mismatch between the parser and semantic rules. */
		return false;
	}

	if (okind) *okind = kind;
	return true;
}

static ofc_sema_typeval_t* ofc_sema_typeval__real_literal(
	const ofc_parse_literal_t* literal,
	const ofc_sema_type_t* type)
{
	if (!literal)
		return NULL;

	if (type && (type->type
		!= OFC_SEMA_TYPE_REAL))
		return NULL;

	/* TODO - Handle BOZ literals and Holleriths. */
	if (literal->type != OFC_PARSE_LITERAL_NUMBER)
		return NULL;

	unsigned tkind = (type ? type->kind : 0);

	ofc_sema_typeval_t typeval = { .type = type };

	unsigned kind = 0;
	if (!ofc_sema_typeval__real(
		literal->number, tkind,
		&typeval.real, &kind))
		return NULL;

	if (!typeval.type)
	{
		typeval.type = ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_REAL, kind, false, false, false);

		if (!typeval.type)
		{
			/* This should never happen. */
			return NULL;
		}
	}

	return ofc_sema_typeval__alloc(typeval);
}

static ofc_sema_typeval_t* ofc_sema_typeval__complex_literal(
	const ofc_parse_literal_t* literal,
	const ofc_sema_type_t* type)
{
	if (!literal)
		return NULL;

	if (type && (type->type
		!= OFC_SEMA_TYPE_COMPLEX))
		return NULL;

	if (literal->type != OFC_PARSE_LITERAL_COMPLEX)
		return NULL;

	unsigned tkind = (type ? type->kind : 0);

	ofc_sema_typeval_t typeval = { .type = type };

	unsigned rkind = 0;
	unsigned ikind = 0;

	if (!ofc_sema_typeval__real(
		literal->complex.real, tkind,
		&typeval.complex.real, &rkind)
		|| !ofc_sema_typeval__real(
			literal->complex.imaginary, tkind,
			&typeval.complex.imaginary, &ikind))
		return NULL;

	unsigned kind = (rkind > ikind ? rkind : ikind);

	if (!typeval.type)
	{
		typeval.type = ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_COMPLEX, kind, false, false, false);

		if (!typeval.type)
		{
			/* This should never happen. */
			return NULL;
		}
	}

	return ofc_sema_typeval__alloc(typeval);
}

static ofc_sema_typeval_t* ofc_sema_typeval__character_literal(
	const ofc_parse_literal_t* literal,
	const ofc_sema_type_t* type)
{
	if (!literal)
		return NULL;

	bool is_byte = false;
	if (type)
	{
		switch (type->type)
		{
			case OFC_SEMA_TYPE_CHARACTER:
				break;
			case OFC_SEMA_TYPE_BYTE:
				is_byte = true;
				break;
			default:
				return NULL;
		}
	}

	switch (literal->type)
	{
		case OFC_PARSE_LITERAL_CHARACTER:
		case OFC_PARSE_LITERAL_HOLLERITH:
			break;
		default:
			return NULL;
	}

	if (!literal->string->base)
		return NULL;

	unsigned size = literal->string->size;

	if (type)
	{
		if (type->kind > 1)
		{
			/* TODO - Error: Wide strings not supported. */
			return NULL;
		}

		size = ofc_sema_type_size(type);
	}

	ofc_sema_typeval_t typeval = { .type = type };

	if (!typeval.type)
	{
		typeval.type = ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_CHARACTER, 0, false, false, false);

		if (!typeval.type)
		{
			/* This should never happen. */
			return NULL;
		}
	}

	if (is_byte)
	{
		if (size != 1)
			return NULL;

		typeval.integer = literal->string->base[0];
	}
	else
	{
		typeval.character = (char*)malloc(sizeof(char) * size);
		if (!typeval.character)
			return NULL;

		if (literal->string->size > size)
		{
			memcpy(
				typeval.character,
				literal->string->base, size);

			/* TODO - Warning: String truncated. */
		}
		else
		{
			memcpy(
				typeval.character,
				literal->string->base,
				literal->string->size);

			if (literal->string->size < size)
			{
				unsigned offset = literal->string->size;
				unsigned ssize = (size - offset);
				memset(&typeval.character[offset], ' ', ssize);

				/* TODO - Warning: String padded. */
			}
		}
	}

	return ofc_sema_typeval__alloc(typeval);
}

static ofc_sema_typeval_t* ofc_sema_typeval__logical_literal(
	const ofc_parse_literal_t* literal,
	const ofc_sema_type_t* type)
{
	if (!literal)
		return NULL;

	bool is_byte = false;
	if (type)
	{
		switch (type->type)
		{
			case OFC_SEMA_TYPE_LOGICAL:
				break;
			case OFC_SEMA_TYPE_BYTE:
				is_byte = true;
				break;
			default:
				return NULL;
		}
	}

	if (literal->type
		!= OFC_PARSE_LITERAL_LOGICAL)
		return NULL;

	ofc_sema_typeval_t typeval = { .type = type };

	if (!typeval.type)
	{
		typeval.type = ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_LOGICAL, 0, false, false, false);

		if (!typeval.type)
		{
			/* This should never happen. */
			return NULL;
		}
	}

	if (is_byte)
		typeval.integer = literal->logical;
	else
		typeval.logical = literal->logical;

	return ofc_sema_typeval__alloc(typeval);
}

static ofc_sema_typeval_t* ofc_sema_typeval__byte_literal(
	const ofc_parse_literal_t* literal,
	const ofc_sema_type_t* type)
{
	if (!literal)
		return NULL;

	if (type && (type->type
		!= OFC_SEMA_TYPE_BYTE))
		return NULL;

	switch (literal->type)
	{
		case OFC_PARSE_LITERAL_NUMBER:
		case OFC_PARSE_LITERAL_BINARY:
		case OFC_PARSE_LITERAL_OCTAL:
		case OFC_PARSE_LITERAL_HEX:
			return ofc_sema_typeval__integer_literal(
				literal, type);
		case OFC_PARSE_LITERAL_LOGICAL:
			return ofc_sema_typeval__logical_literal(
				literal, type);
		case OFC_PARSE_LITERAL_CHARACTER:
			return ofc_sema_typeval__character_literal(
				literal, type);
			return NULL;
		default:
			break;
	}

	return NULL;
}



ofc_sema_typeval_t* ofc_sema_typeval_literal(
	const ofc_parse_literal_t* literal,
	const ofc_sema_type_t* type)
{
	if (!literal) return NULL;

	if (!type)
	{
		ofc_sema_typeval_t* typeval = NULL;
		if (!typeval) typeval = ofc_sema_typeval__integer_literal(literal, type);
		if (!typeval) typeval = ofc_sema_typeval__real_literal(literal, type);
		if (!typeval) typeval = ofc_sema_typeval__logical_literal(literal, type);
		if (!typeval) typeval = ofc_sema_typeval__complex_literal(literal, type);
		/* Byte can never be auto-detected. */
		if (!typeval) typeval = ofc_sema_typeval__character_literal(literal, type);

		return typeval;
	}

	switch (type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			return ofc_sema_typeval__logical_literal(literal, type);
		case OFC_SEMA_TYPE_INTEGER:
			return ofc_sema_typeval__integer_literal(literal, type);
		case OFC_SEMA_TYPE_REAL:
			return ofc_sema_typeval__real_literal(literal, type);
		case OFC_SEMA_TYPE_COMPLEX:
			return ofc_sema_typeval__complex_literal(literal, type);
		case OFC_SEMA_TYPE_BYTE:
			return ofc_sema_typeval__byte_literal(literal, type);
		case OFC_SEMA_TYPE_CHARACTER:
			return ofc_sema_typeval__character_literal(literal, type);

		default:
			return NULL;
	}
}

void ofc_sema_typeval_delete(
	ofc_sema_typeval_t* typeval)
{
	if (!typeval)
		return;

	if (typeval->type
		&& (typeval->type->type == OFC_SEMA_TYPE_CHARACTER))
		free(typeval->character);

	free(typeval);
}


unsigned ofc_sema_typeval_size(
	const ofc_sema_typeval_t* typeval)
{
	if (!typeval)
		return 0;
	return ofc_sema_type_size(typeval->type);
}

ofc_sema_typeval_t* ofc_sema_typeval_copy(
	const ofc_sema_typeval_t* typeval)
{
	if (!typeval || !typeval->type)
		return NULL;

	ofc_sema_typeval_t* copy
		= (ofc_sema_typeval_t*)malloc(
			sizeof(ofc_sema_typeval_t));
	if (!copy) return NULL;

	memcpy(copy, typeval,
		sizeof(ofc_sema_typeval_t));

	if (copy->type->type == OFC_SEMA_TYPE_CHARACTER)
	{
		copy->character
			= strdup(typeval->character);
		if (!copy->character)
		{
			free(copy);
			return NULL;
		}
	}

	return copy;
}

ofc_sema_typeval_t* ofc_sema_typeval_cast(
	const ofc_sema_typeval_t* typeval,
	const ofc_sema_type_t* type)
{
	if (!typeval || !typeval->type)
		return NULL;
	if (!type || ofc_sema_type_compare(type, typeval->type))
		return ofc_sema_typeval_copy(typeval);

	/* TODO - Report lossy casts. */

	if ((type->type == OFC_SEMA_TYPE_CHARACTER)
		&& (typeval->type->type == OFC_SEMA_TYPE_CHARACTER))
	{
		/* TODO - Truncate or pad string. */
		return NULL;
	}

	bool invalid_cast = false;
	bool large_literal = false;

	switch (type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
		case OFC_SEMA_TYPE_BYTE:
			break;
		case OFC_SEMA_TYPE_INTEGER:
			large_literal = (type->kind > 8);
			break;
		case OFC_SEMA_TYPE_REAL:
		case OFC_SEMA_TYPE_COMPLEX:
			large_literal = (type->kind > 10);
			break;
		default:
			invalid_cast = true;
			break;
	}

	if (large_literal)
	{
		/* TODO - Error: Literal too large for compiler. */
		return NULL;
	}

	if (type->type == typeval->type->type)
		return ofc_sema_typeval_copy(typeval);

	ofc_sema_typeval_t tv;
	tv.type = type;

	switch (type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			switch (typeval->type->type)
			{
				case OFC_SEMA_TYPE_INTEGER:
				case OFC_SEMA_TYPE_BYTE:
					tv.logical = (typeval->integer != 0);
					break;
				default:
					invalid_cast = true;
					break;
			}
			break;

		case OFC_SEMA_TYPE_INTEGER:
			switch (typeval->type->type)
			{
				case OFC_SEMA_TYPE_LOGICAL:
					tv.integer = (typeval->logical ? 1 : 0);
					break;
				case OFC_SEMA_TYPE_REAL:
					tv.integer = (int64_t)typeval->real;
					break;
				case OFC_SEMA_TYPE_COMPLEX:
					tv.integer = (int64_t)typeval->complex.real;
					break;
				case OFC_SEMA_TYPE_BYTE:
					tv.integer = typeval->integer;
					break;
				default:
					invalid_cast = true;
					break;
			}
			/* TODO - Clamp to target type. */
			break;

		case OFC_SEMA_TYPE_REAL:
			switch (typeval->type->type)
			{
				case OFC_SEMA_TYPE_INTEGER:
				case OFC_SEMA_TYPE_BYTE:
					tv.real = (long double)typeval->integer;
					break;
				case OFC_SEMA_TYPE_COMPLEX:
					tv.real = typeval->complex.real;
					break;
				default:
					invalid_cast = true;
					break;
			}
			break;

		case OFC_SEMA_TYPE_COMPLEX:
			switch (typeval->type->type)
			{
				case OFC_SEMA_TYPE_INTEGER:
				case OFC_SEMA_TYPE_BYTE:
					tv.complex.real = (long double)typeval->integer;
					break;
				case OFC_SEMA_TYPE_REAL:
					tv.complex.real = typeval->real;
					break;
				default:
					invalid_cast = true;
					break;
			}
			tv.complex.imaginary = 0.0;
			break;

		case OFC_SEMA_TYPE_BYTE:
			switch (typeval->type->type)
			{
				case OFC_SEMA_TYPE_LOGICAL:
					tv.integer = (typeval->logical ? 1 : 0);
					break;
				case OFC_SEMA_TYPE_REAL:
					tv.integer = (int64_t)typeval->real;
					break;
				case OFC_SEMA_TYPE_COMPLEX:
					tv.integer = (int64_t)typeval->complex.real;
					break;
				case OFC_SEMA_TYPE_BYTE:
					tv.integer = typeval->integer;
					break;
				default:
					invalid_cast = true;
					break;
			}
			/* TODO - Modulo to -128..127 */
			break;

		default:
			invalid_cast = true;
			break;
	}

	if (invalid_cast)
	{
		/* TODO - Error: Can't cast TYPE to TYPE. */
		return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}


bool ofc_sema_typeval_get_logical(
	const ofc_sema_typeval_t* typeval,
	bool* logical)
{
	if (!typeval || !typeval->type
		|| !ofc_sema_type_is_logical(typeval->type))
		return false;

	/* TODO - Casting. */

	if (logical)
	{
		if (typeval->type->type
			== OFC_SEMA_TYPE_BYTE)
			*logical = (typeval->integer != 0);
		else
			*logical = typeval->logical;
	}

	return true;
}

bool ofc_sema_typeval_get_integer(
	const ofc_sema_typeval_t* typeval,
	int64_t* integer)
{
	if (!typeval || !typeval->type
		|| !ofc_sema_type_is_integer(typeval->type))
		return false;

	/* TODO - Casting. */

	if (integer)
		*integer = typeval->integer;
	return true;
}

bool ofc_sema_typeval_get_real(
	const ofc_sema_typeval_t* typeval,
	long double* real)
{
	if (!typeval || !typeval->type
		|| (typeval->type->type != OFC_SEMA_TYPE_REAL))
		return false;

	if (real)
		*real = typeval->real;
	return true;
}

bool ofc_sema_typeval_get_complex(
	const ofc_sema_typeval_t* typeval,
	long double* real, long double* imaginary)
{
	if (!typeval || !typeval->type
		|| (typeval->type->type != OFC_SEMA_TYPE_COMPLEX))
		return false;

	/* TODO - Casting. */

	if (real)
		*real = typeval->complex.real;
	if (imaginary)
		*real = typeval->complex.imaginary;
	return true;
}

bool ofc_sema_typeval_get_character(
	const ofc_sema_typeval_t* typeval,
	const char** character)
{
	if (!typeval || !typeval->type)
		return false;

	if (typeval->type->type
		== OFC_SEMA_TYPE_BYTE)
	{
		if (character)
			*character = (const char*)&typeval->integer;
		return true;
	}

	/* TODO - Casting. */

	if (typeval->type->type
		!= OFC_SEMA_TYPE_CHARACTER)
		return false;

	if (character)
		*character = typeval->character;
	return true;
}



ofc_sema_typeval_t* ofc_sema_typeval_power(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.real = powl(a->real, b->real);
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX power. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			{
				long double r = powl(
					a->integer, b->integer);
				tv.integer = (int64_t)r;
				if ((long double)tv.integer != r)
					return NULL;
			}
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_multiply(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.real = a->real * b->real;
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX multiply. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			{
				int64_t r = a->integer * b->integer;
				if ((r / b->integer)
					!= a->integer)
					return NULL;
				tv.integer = r;
			}
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_concat(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| (a->type->type != OFC_SEMA_TYPE_CHARACTER)
		|| (b->type->type != OFC_SEMA_TYPE_CHARACTER))
		return NULL;

	if (a->type->kind == 0)
		return ofc_sema_typeval_copy(b);
	if (b->type->kind == 0)
		return ofc_sema_typeval_copy(a);

	unsigned len = (a->type->kind + b->type->kind);

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_CHARACTER, len,
		false, false, false);
	if (!tv.type) return NULL;

	tv.character = (char*)malloc(sizeof(char) * len);
	if (!tv.character) return NULL;

	memcpy(tv.character, a->character, a->type->kind);
	memcpy(&tv.character[a->type->kind], b->character, b->type->kind);

	ofc_sema_typeval_t* ret
		= ofc_sema_typeval__alloc(tv);
	if (!ret) free(tv.character);
	return ret;
}

ofc_sema_typeval_t* ofc_sema_typeval_divide(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.real = a->real / b->real;
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX divide. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			if (b->integer == 0)
			{
				/* TODO - Error: Divide by zero. */
				return NULL;
			}
			tv.integer = a->integer / b->integer;
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_add(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.real = a->real + b->real;
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX addition. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			/* TODO - Detect overflow. */
			tv.integer = a->integer + b->integer;
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_subtract(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.real = a->real - b->real;
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX addition. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			/* TODO - Detect overflow. */
			tv.integer = a->integer - b->integer;
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_negate(
	const ofc_sema_typeval_t* a)
{
	if (!a || !a->type)
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.real = -a->real;
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX negation. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.integer = -a->integer;
			if (-tv.integer != a->integer)
			{
				/* TODO - Error: Overflow in constant negate. */
				return NULL;
			}
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_eq(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.logical = (a->real == b->real);
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX equality. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer == b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_ne(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.logical = (a->real != b->real);
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX inequality. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer != b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_lt(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.logical = (a->real < b->real);
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX lower than. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer < b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_le(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.logical = (a->real <= b->real);
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX lower than or equal. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer <= b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_gt(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.logical = (a->real > b->real);
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX greater than. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer > b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_ge(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_REAL:
			tv.logical = (a->real >= b->real);
			break;
		case OFC_SEMA_TYPE_COMPLEX:
			/* TODO - COMPLEX greater or equal. */
			return NULL;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer >= b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_not(
	const ofc_sema_typeval_t* a)
{
	if (!a || !a->type)
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			tv.logical = !a->logical;
			break;
		case OFC_SEMA_TYPE_INTEGER:
			tv.integer = ~a->integer;
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_and(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			tv.logical = (a->logical && b->logical);
			break;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.integer = a->integer & b->integer;
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_or(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = a->type;

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			tv.logical = (a->logical || b->logical);
			break;
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			tv.integer = a->integer | b->integer;
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_eqv(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			tv.logical = (a->logical == b->logical);
			break;
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer == b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}

ofc_sema_typeval_t* ofc_sema_typeval_neqv(
	const ofc_sema_typeval_t* a,
	const ofc_sema_typeval_t* b)
{
	if (!a || !a->type
		|| !b || !b->type
		|| !ofc_sema_type_compare(
			a->type, b->type))
		return NULL;

	ofc_sema_typeval_t tv;
	tv.type = ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_LOGICAL, 0,
		false, false, false);

	switch (a->type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			tv.logical = (a->logical != b->logical);
			break;
		case OFC_SEMA_TYPE_BYTE:
			tv.logical = (a->integer != b->integer);
			break;
		default:
			return NULL;
	}

	return ofc_sema_typeval__alloc(tv);
}