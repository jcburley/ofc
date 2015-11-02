#include <ofc/sema.h>


static ofc_sema_decl_t* ofc_sema_decl__type_name(
	const ofc_sema_type_t* type,
	ofc_str_ref_t name)
{
	if (!type)
		return NULL;

	ofc_sema_decl_t* decl
		= (ofc_sema_decl_t*)malloc(
			sizeof(ofc_sema_decl_t));
	if (!decl) return NULL;

	decl->type = type;
	decl->name = name;

	if ((type->type == OFC_SEMA_TYPE_ARRAY)
		|| (type->type == OFC_SEMA_TYPE_STRUCTURE))
	{
		decl->init_array = NULL;
	}
	else
	{
		decl->init = NULL;
	}

	decl->equiv = NULL;

	decl->is_static    = type->is_static;
	decl->is_volatile  = type->is_volatile;
	decl->is_automatic = type->is_automatic;
	decl->is_target    = false;

	return decl;
}

/* We must consume array since it may be used by the type system
   even if this function returns NULL. */
static ofc_sema_decl_t* ofc_sema_decl_implicit__name(
	const ofc_sema_scope_t* scope,
	ofc_str_ref_t name, ofc_sema_array_t* array)
{
	if (!scope)
	{
		ofc_sema_array_delete(array);
		return NULL;
	}

	if (ofc_str_ref_empty(name))
	{
		ofc_sema_array_delete(array);
		return NULL;
	}

	const ofc_sema_type_t* type
		= ofc_sema_implicit_get(
			scope->implicit, name.base[0]);

	if (array)
	{
		const ofc_sema_type_t* atype
			= ofc_sema_type_create_array(
				type, array,
				type->is_static,
				type->is_automatic,
				type->is_volatile);
		if (!atype)
			ofc_sema_array_delete(array);
		type = atype;
	}

	return ofc_sema_decl__type_name(
		type, name);
}

ofc_sema_decl_t* ofc_sema_decl_implicit_name(
	const ofc_sema_scope_t* scope,
	ofc_str_ref_t name)
{
	return ofc_sema_decl_implicit__name(
		scope, name, NULL);
}

ofc_sema_decl_t* ofc_sema_decl_implicit_lhs(
	const ofc_sema_scope_t* scope,
	const ofc_parse_lhs_t* lhs)
{
	if (!scope || !lhs)
		return NULL;

	ofc_sema_array_t* array = NULL;
	ofc_str_ref_t base_name;
	if (!ofc_parse_lhs_base_name(
		*lhs, &base_name))
		return NULL;

    switch (lhs->type)
	{
		case OFC_PARSE_LHS_VARIABLE:
			break;
		case OFC_PARSE_LHS_ARRAY:
			array = ofc_sema_array(
				scope, NULL, lhs->array.index);
			if (!array) return NULL;
			break;
		default:
			break;
	}

	/* 'array' is always consumed. */
	return ofc_sema_decl_implicit__name(
		scope, base_name, array);
}

static ofc_sema_decl_t* ofc_sema_decl__decl(
	const ofc_sema_scope_t* scope,
	const ofc_sema_type_t*  type,
	const ofc_parse_decl_t* decl)
{
	if (!decl || !type)
		return NULL;

	switch (decl->lhs->type)
	{
		case OFC_PARSE_LHS_VARIABLE:
		case OFC_PARSE_LHS_ARRAY:
			break;
		default:
			return NULL;
	}

	ofc_str_ref_t base_name;
	if (!ofc_parse_lhs_base_name(
		*(decl->lhs), &base_name))
		return NULL;

	const ofc_sema_type_t* atype
		= ofc_sema_lhs_decl_type(
			scope, type, decl->lhs);
	if (!atype) return NULL;

	ofc_sema_decl_t* sdecl
		= ofc_sema_decl__type_name(
			atype, base_name);

	if (decl->init_expr)
	{
		ofc_sema_expr_t* init_expr
			= ofc_sema_expr(scope, decl->init_expr);
		if (!init_expr)
		{
			ofc_sema_decl_delete(sdecl);
			return NULL;
		}

		bool initialized = ofc_sema_decl_init(
			scope, sdecl, init_expr);
		ofc_sema_expr_delete(init_expr);

		if (!initialized)
		{
			ofc_sema_decl_delete(sdecl);
			return NULL;
		}
	}
	else if (decl->init_clist)
	{
		/* TODO - CList initializer resolution. */
		ofc_sema_decl_delete(sdecl);
		return NULL;
	}

	return sdecl;
}

bool ofc_sema_decl(
	ofc_sema_scope_t* scope,
	const ofc_parse_stmt_t* stmt)
{
	if (!stmt || !scope || !scope->decl
		|| !stmt->decl.type || !stmt->decl.decl
		|| (stmt->type != OFC_PARSE_STMT_DECL))
		return false;

	const ofc_sema_type_t* type = ofc_sema_type(
		scope, stmt->decl.type);
	if (!type) return false;

	unsigned count = stmt->decl.decl->count;
	if (count == 0) return false;

	ofc_sema_decl_t* decl[count];

	unsigned i;
	for (i = 0; i < count; i++)
	{
		decl[i] = ofc_sema_decl__decl(
			scope, type, stmt->decl.decl->decl[i]);

		if (decl[i] && ofc_sema_decl_list_find(
			scope->decl, decl[i]->name))
		{
			/* TODO - Allow redeclaration as long as it matches original. */
			ofc_sema_decl_delete(decl[i]);
			decl[i] = NULL;
		}

		if (!decl[i])
		{
			unsigned j;
			for (j = 0; j < i; j++)
				ofc_sema_decl_delete(decl[j]);
			return false;
		}
	}

	for (i = 0; i < count; i++)
	{
		if (!ofc_sema_decl_list_add(
			scope->decl, decl[i]))
		{
			/* This should never happen. */
			abort();
			return false;
		}
	}

	return true;
}

void ofc_sema_decl_delete(
	ofc_sema_decl_t* decl)
{
	if (!decl)
		return;

	ofc_sema_equiv_delete(decl->equiv);
	ofc_sema_typeval_delete(decl->init);
	free(decl);
}


bool ofc_sema_decl_init(
	const ofc_sema_scope_t* scope,
	ofc_sema_decl_t* decl,
	const ofc_sema_expr_t* init)
{
	if (!decl || !init)
		return false;

    const ofc_sema_typeval_t* ctv
		= ofc_sema_expr_constant(init);
	if (!ctv)
	{
		ofc_sema_scope_error(scope, init->src,
			"Initializer element not constant.");
		return false;
	}

	if (ofc_sema_decl_is_composite(decl))
	{
		/* TODO - Support F90 "(/ 0, 1 /)" array initializers. */
		ofc_sema_scope_error(scope, init->src,
			"Can't initialize non-scalar declaration with expression.");
		return false;
	}

	ofc_sema_typeval_t* tv
		= ofc_sema_typeval_cast(
			scope, ctv, decl->type);
	if (!tv) return false;

	if (ofc_sema_decl_is_initialized(decl))
	{
		bool redecl = ofc_sema_typeval_compare(
			tv, decl->init);
		ofc_sema_typeval_delete(tv);

		if (redecl)
		{
			ofc_sema_scope_warning(scope, init->src,
				"Duplicate initialization.");
		}
		else
		{
			ofc_sema_scope_error(scope, init->src,
				"Can't re-initialize initialized declaration.");
			return false;
		}
	}

	decl->init = tv;
	return true;
}


unsigned ofc_sema_decl_size(
	const ofc_sema_decl_t* decl)
{
	if (!decl)
		return 0;
	return ofc_sema_type_size(
		decl->type);
}

unsigned ofc_sema_decl_elem_count(
	const ofc_sema_decl_t* decl)
{
	if (!decl)
		return 0;
	return ofc_sema_type_elem_count(
		decl->type);
}

bool ofc_sema_decl_is_composite(
	const ofc_sema_decl_t* decl)
{
	if (!decl)
		return false;
	return ofc_sema_type_is_composite(decl->type);
}

bool ofc_sema_decl_is_initialized(
	const ofc_sema_decl_t* decl)
{
	if (!decl || !decl->type)
		return false;

	if ((decl->type->type == OFC_SEMA_TYPE_ARRAY)
		|| (decl->type->type == OFC_SEMA_TYPE_STRUCTURE))
		return (decl->init_array != NULL);

	return (decl->init != NULL);
}

const ofc_sema_type_t* ofc_sema_decl_type(
	const ofc_sema_decl_t* decl)
{
	return (decl ? decl->type : NULL);
}



static const ofc_str_ref_t* ofc_sema_decl__key(
	const ofc_sema_decl_t* decl)
{
	return (decl ? &decl->name : NULL);
}

bool ofc_sema_decl_list__remap(
	ofc_sema_decl_list_t* list)
{
    if (!list)
		return false;

	if (list->map)
		ofc_hashmap_delete(list->map);



	return (list->map != NULL);
}

ofc_sema_decl_list_t* ofc_sema_decl_list__create(
	bool case_sensitive, bool is_ref)
{
	ofc_sema_decl_list_t* list
		= (ofc_sema_decl_list_t*)malloc(
			sizeof(ofc_sema_decl_list_t));
	if (!list) return NULL;

	list->case_sensitive = case_sensitive;

	list->count  = 0;
	list->decl   = NULL;
	list->is_ref = is_ref;

	list->map = ofc_hashmap_create(
		(void*)(list->case_sensitive
			? ofc_str_ref_ptr_hash
			: ofc_str_ref_ptr_hash_ci),
		(void*)(list->case_sensitive
			? ofc_str_ref_ptr_equal
			: ofc_str_ref_ptr_equal_ci),
		(void*)ofc_sema_decl__key, NULL);
	if (!list->map)
	{
		free(list);
		return NULL;
	}

	return list;
}

ofc_sema_decl_list_t* ofc_sema_decl_list_create(
	bool case_sensitive)
{
	return ofc_sema_decl_list__create(
		case_sensitive, false);
}

ofc_sema_decl_list_t* ofc_sema_decl_list_create_ref(
	bool case_sensitive)
{
	return ofc_sema_decl_list__create(
		case_sensitive, true);
}

void ofc_sema_decl_list_delete(
	ofc_sema_decl_list_t* list)
{
	if (!list)
		return;

	ofc_hashmap_delete(list->map);

	if (!list->is_ref)
	{
		unsigned i;
		for (i = 0; i < list->count; i++)
			ofc_sema_decl_delete(list->decl[i]);
	}

	free(list->decl);

	free(list);
}

bool ofc_sema_decl_list_add(
	ofc_sema_decl_list_t* list,
	ofc_sema_decl_t* decl)
{
	if (!list || !decl
		|| list->is_ref)
		return false;

	/* Check for duplicate definitions. */
	if (ofc_sema_decl_list_find(
		list, decl->name))
	{
		/* TODO - Ignore duplicate so long as it matches. */
		return false;
	}

    ofc_sema_decl_t** ndecl
		= (ofc_sema_decl_t**)realloc(list->decl,
			(sizeof(ofc_sema_decl_t*) * (list->count + 1)));
	if (!ndecl) return false;
	list->decl = ndecl;

	if (!ofc_hashmap_add(
		list->map, decl))
		return false;

	list->decl[list->count++] = decl;
	return true;
}

bool ofc_sema_decl_list_add_ref(
	ofc_sema_decl_list_t* list,
	const ofc_sema_decl_t* decl)
{
	if (!list || !decl
		|| !list->is_ref)
		return false;

	/* Check for duplicate definitions. */
	if (ofc_sema_decl_list_find(
		list, decl->name))
		return false;

    const ofc_sema_decl_t** ndecl
		= (const ofc_sema_decl_t**)realloc(list->decl_ref,
			(sizeof(const ofc_sema_decl_t*) * (list->count + 1)));
	if (!ndecl) return false;
	list->decl_ref = ndecl;

	if (!ofc_hashmap_add(
		list->map, (void*)decl))
		return false;

	list->decl_ref[list->count++] = decl;
	return true;
}

const ofc_sema_decl_t* ofc_sema_decl_list_find(
	const ofc_sema_decl_list_t* list, ofc_str_ref_t name)
{
	if (!list)
		return NULL;

	return ofc_hashmap_find(
		list->map, &name);
}

ofc_sema_decl_t* ofc_sema_decl_list_find_modify(
	ofc_sema_decl_list_t* list, ofc_str_ref_t name)
{
	if (!list)
		return NULL;

	return ofc_hashmap_find_modify(
		list->map, &name);
}

const ofc_hashmap_t* ofc_sema_decl_list_map(
	const ofc_sema_decl_list_t* list)
{
	return (list ? list->map : NULL);
}
