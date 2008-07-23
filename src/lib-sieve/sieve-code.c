#include "lib.h"
#include "str.h"
#include "str-sanitize.h"

#include "sieve-common.h"
#include "sieve-extensions-private.h"
#include "sieve-actions.h"
#include "sieve-binary.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "sieve-code.h"

#include <stdio.h>

/* 
 * Coded stringlist
 */

struct sieve_coded_stringlist {
	const struct sieve_runtime_env *runenv;
	sieve_size_t start_address;
	sieve_size_t end_address;
	sieve_size_t current_offset;
	int length;
	int index;
};

static struct sieve_coded_stringlist *sieve_coded_stringlist_create
(const struct sieve_runtime_env *renv, 
	 sieve_size_t start_address, sieve_size_t length, sieve_size_t end)
{
	struct sieve_coded_stringlist *strlist;
	
	if ( end > sieve_binary_get_code_size(renv->sbin) ) 
  		return NULL;
    
	strlist = t_new(struct sieve_coded_stringlist, 1);
	strlist->runenv = renv;
	strlist->start_address = start_address;
	strlist->current_offset = start_address;
	strlist->end_address = end;
	strlist->length = length;
	strlist->index = 0;
  
	return strlist;
}

bool sieve_coded_stringlist_next_item
(struct sieve_coded_stringlist *strlist, string_t **str) 
{
	sieve_size_t address;
	*str = NULL;
  
	if ( strlist->index >= strlist->length ) 
		return TRUE;
	else {
		address = strlist->current_offset;
  	
		if ( sieve_opr_string_read(strlist->runenv, &address, str) ) {
			strlist->index++;
			strlist->current_offset = address;
			return TRUE;
		}
	}  
  
	return FALSE;
}

void sieve_coded_stringlist_reset(struct sieve_coded_stringlist *strlist) 
{  
	strlist->current_offset = strlist->start_address;
	strlist->index = 0;
}

int sieve_coded_stringlist_get_length
	(struct sieve_coded_stringlist *strlist)
{
	return strlist->length;
}

sieve_size_t sieve_coded_stringlist_get_end_address
(struct sieve_coded_stringlist *strlist)
{
	return strlist->end_address;
}

sieve_size_t sieve_coded_stringlist_get_current_offset
	(struct sieve_coded_stringlist *strlist)
{
	return strlist->current_offset;
}

bool sieve_coded_stringlist_read_all
(struct sieve_coded_stringlist *strlist, pool_t pool,
	const char * const **list_r)
{
	bool result = FALSE;
	ARRAY_DEFINE(items, const char *);
	string_t *item;
	
	sieve_coded_stringlist_reset(strlist);
	
	p_array_init(&items, pool, 4);
	
	item = NULL;
	while ( (result=sieve_coded_stringlist_next_item(strlist, &item)) && 
		item != NULL ) {
		const char *stritem = p_strdup(pool, str_c(item));
		
		array_append(&items, &stritem, 1);
	}
	
	(void)array_append_space(&items);
	*list_r = array_idx(&items, 0);

	return result;
}

static bool sieve_coded_stringlist_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address, 
	sieve_size_t length, sieve_size_t end)
{
	unsigned int i;
	
	if ( end > sieve_binary_get_code_size(denv->sbin) ) 
  		return FALSE;
    
	sieve_code_dumpf(denv, "STRLIST [%d] (END %08x)", length, end);
	sieve_code_descend(denv);
	
	for ( i = 0; i < length; i++ ) {
		if ( !sieve_opr_string_dump(denv, address) ) 
			return FALSE;

		if ( *address > end ) 
			return FALSE;
	}

	if ( *address != end ) return FALSE;
	
	sieve_code_ascend(denv);
		
	return TRUE;
}
	
/*
 * Source line
 */

void sieve_code_source_line_emit
(struct sieve_binary *sbin, unsigned int source_line)
{
    (void)sieve_binary_emit_integer(sbin, source_line);
}

bool sieve_code_source_line_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
    sieve_size_t number = 0;

	sieve_code_mark(denv);
    if (sieve_binary_read_integer(denv->sbin, address, &number) ) {
        sieve_code_dumpf(denv, "(source line: %ld)", (long) number);

        return TRUE;
    }

    return FALSE;
}

bool sieve_code_source_line_read
(const struct sieve_runtime_env *renv, sieve_size_t *address,
	unsigned int *source_line)
{
	sieve_size_t number;

    if ( !sieve_binary_read_integer(renv->sbin, address, &number) )
		return FALSE;

	*source_line = number;
	return TRUE;
}

/*
 * Core operands
 */
 
const struct sieve_operand number_operand;
const struct sieve_operand string_operand;
const struct sieve_operand stringlist_operand;
extern const struct sieve_operand comparator_operand;
extern const struct sieve_operand match_type_operand;
extern const struct sieve_operand address_part_operand;

const struct sieve_operand *sieve_operands[] = {
	NULL, /* SIEVE_OPERAND_OPTIONAL */
	&number_operand,
	&string_operand,
	&stringlist_operand,
	&comparator_operand,
	&match_type_operand,
	&address_part_operand,
}; 

const unsigned int sieve_operand_count =
	N_ELEMENTS(sieve_operands);

static struct sieve_extension_obj_registry oprd_default_reg =
	SIEVE_EXT_DEFINE_OPERANDS(sieve_operands);

/* 
 * Operand functions 
 */

sieve_size_t sieve_operand_emit_code
	(struct sieve_binary *sbin, const struct sieve_operand *opr)
{
	return sieve_extension_emit_obj
		(sbin, &oprd_default_reg, opr, operands, opr->extension);
}

static const struct sieve_extension_obj_registry *
	sieve_operand_registry_get
(struct sieve_binary *sbin, unsigned int ext_index)
{
	const struct sieve_extension *ext;
	
	if ( (ext=sieve_binary_extension_get_by_index(sbin, ext_index)) 
		== NULL )
		return NULL;
		
	return &(ext->operands);
}

const struct sieve_operand *sieve_operand_read
	(struct sieve_binary *sbin, sieve_size_t *address) 
{
	return sieve_extension_read_obj
		(struct sieve_operand, sbin, address, &oprd_default_reg, 
			sieve_operand_registry_get);
}

bool sieve_operand_optional_present
	(struct sieve_binary *sbin, sieve_size_t *address)
{	
	sieve_size_t tmp_addr = *address;
	unsigned int op = -1;
	
	if ( sieve_binary_read_byte(sbin, &tmp_addr, &op) && 
		(op == SIEVE_OPERAND_OPTIONAL) ) {
		*address = tmp_addr;
		return TRUE;
	}
	
	return FALSE;
}

bool sieve_operand_optional_read
	(struct sieve_binary *sbin, sieve_size_t *address, int *id_code)
{
	if ( sieve_binary_read_code(sbin, address, id_code) ) 
		return TRUE;
	
	*id_code = 0;

	return FALSE;
}

/* 
 * Operand definitions
 */
 
/* Number */

static bool opr_number_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static bool opr_number_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address, 
		sieve_size_t *number);

const struct sieve_opr_number_interface number_interface = { 
	opr_number_dump, 
	opr_number_read
};

const struct sieve_operand_class number_class = 
	{ "number" };
	
const struct sieve_operand number_operand = { 
	"@number", 
	NULL, SIEVE_OPERAND_NUMBER,
	&number_class,
	&number_interface 
};

/* String */

static bool opr_string_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static bool opr_string_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address, string_t **str);

const struct sieve_opr_string_interface string_interface ={ 
	opr_string_dump,
	opr_string_read
};
	
const struct sieve_operand_class string_class = 
	{ "string" };
	
const struct sieve_operand string_operand = { 
	"@string", 
	NULL, SIEVE_OPERAND_STRING,
	&string_class,
	&string_interface
};	

/* String List */

static bool opr_stringlist_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static struct sieve_coded_stringlist *opr_stringlist_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_opr_stringlist_interface stringlist_interface = { 
	opr_stringlist_dump, 
	opr_stringlist_read
};

const struct sieve_operand_class stringlist_class = 
	{ "string-list" };

const struct sieve_operand stringlist_operand =	{ 
	"@string-list", 
	NULL, SIEVE_OPERAND_STRING_LIST,
	&stringlist_class, 
	&stringlist_interface
};
	
/* 
 * Operand implementations 
 */
 
/* Number */

void sieve_opr_number_emit(struct sieve_binary *sbin, sieve_size_t number) 
{
	(void) sieve_operand_emit_code(sbin, &number_operand);
	(void) sieve_binary_emit_integer(sbin, number);
}

bool sieve_opr_number_dump_data
(const struct sieve_dumptime_env *denv, const struct sieve_operand *operand,
	sieve_size_t *address) 
{
	const struct sieve_opr_number_interface *intf;

	if ( !sieve_operand_is_number(operand) ) 
		return FALSE;
		
	intf = (const struct sieve_opr_number_interface *) operand->interface; 
	
	if ( intf->dump == NULL )
		return FALSE;

	return intf->dump(denv, address);  
}

bool sieve_opr_number_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address) 
{
	const struct sieve_operand *operand;
	
	sieve_code_mark(denv);
	
	operand = sieve_operand_read(denv->sbin, address);

	return sieve_opr_number_dump_data(denv, operand, address);
}

bool sieve_opr_number_read_data
(const struct sieve_runtime_env *renv, const struct sieve_operand *operand,
	sieve_size_t *address, sieve_size_t *number)
{
	const struct sieve_opr_number_interface *intf;
		
	if ( !sieve_operand_is_number(operand) ) 
		return FALSE;	
		
	intf = (const struct sieve_opr_number_interface *) operand->interface; 
	
	if ( intf->read == NULL )
		return FALSE;

	return intf->read(renv, address, number);  
}

bool sieve_opr_number_read
(const struct sieve_runtime_env *renv, sieve_size_t *address, 
	sieve_size_t *number)
{
	const struct sieve_operand *operand = sieve_operand_read(renv->sbin, address);
		
	return sieve_opr_number_read_data(renv, operand, address, number);
}

static bool opr_number_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address) 
{
	sieve_size_t number = 0;
	
	if (sieve_binary_read_integer(denv->sbin, address, &number) ) {
		sieve_code_dumpf(denv, "NUM: %ld", (long) number);

		return TRUE;
	}
	
	return FALSE;
}

static bool opr_number_read
(const struct sieve_runtime_env *renv, sieve_size_t *address, 
	sieve_size_t *number)
{ 
	return sieve_binary_read_integer(renv->sbin, address, number);
}

/* String */

void sieve_opr_string_emit(struct sieve_binary *sbin, string_t *str)
{
	(void) sieve_operand_emit_code(sbin, &string_operand);
	(void) sieve_binary_emit_string(sbin, str);
}

bool sieve_opr_string_dump_data
(const struct sieve_dumptime_env *denv, const struct sieve_operand *operand,
	sieve_size_t *address) 
{
	const struct sieve_opr_string_interface *intf;
	
	if ( !sieve_operand_is_string(operand) ) 
		return FALSE;
		
	intf = (const struct sieve_opr_string_interface *) operand->interface; 
	
	if ( intf->dump == NULL ) 
		return FALSE;

	return intf->dump(denv, address);  
}

bool sieve_opr_string_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address) 
{
	const struct sieve_operand *operand;
	
	sieve_code_mark(denv);
	operand = sieve_operand_read(denv->sbin, address);

	return sieve_opr_string_dump_data(denv, operand, address);
}

bool sieve_opr_string_read_data
(const struct sieve_runtime_env *renv, const struct sieve_operand *operand,
	sieve_size_t *address, string_t **str)
{
	const struct sieve_opr_string_interface *intf;
	
	if ( operand == NULL || operand->class != &string_class ) 
		return FALSE;
		
	intf = (const struct sieve_opr_string_interface *) operand->interface; 
	
	if ( intf->read == NULL )
		return FALSE;

	return intf->read(renv, address, str);  
}

bool sieve_opr_string_read
(const struct sieve_runtime_env *renv, sieve_size_t *address, string_t **str)
{
	const struct sieve_operand *operand = sieve_operand_read(renv->sbin, address);

	return sieve_opr_string_read_data(renv, operand, address, str);
}

static void _dump_string
(const struct sieve_dumptime_env *denv, string_t *str) 
{
	if ( str_len(str) > 80 )
		sieve_code_dumpf(denv, "STR[%ld]: \"%s", 
			(long) str_len(str), str_sanitize(str_c(str), 80));
	else
		sieve_code_dumpf(denv, "STR[%ld]: \"%s\"", 
			(long) str_len(str), str_sanitize(str_c(str), 80));		
}

bool opr_string_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address) 
{
	string_t *str; 
	
	if ( sieve_binary_read_string(denv->sbin, address, &str) ) {
		_dump_string(denv, str);   		
		
		return TRUE;
	}
  
	return FALSE;
}

static bool opr_string_read
  (const struct sieve_runtime_env *renv, sieve_size_t *address, string_t **str)
{ 	
	return sieve_binary_read_string(renv->sbin, address, str);
}

/* String list */

void sieve_opr_stringlist_emit_start
	(struct sieve_binary *sbin, unsigned int listlen, void **context)
{
	sieve_size_t *end_offset = t_new(sieve_size_t, 1);

	/* Emit byte identifying the type of operand */	  
	(void) sieve_operand_emit_code(sbin, &stringlist_operand);
  
	/* Give the interpreter an easy way to skip over this string list */
	*end_offset = sieve_binary_emit_offset(sbin, 0);
	*context = (void *) end_offset;

	/* Emit the length of the list */
	(void) sieve_binary_emit_integer(sbin, (int) listlen);
}

void sieve_opr_stringlist_emit_item
	(struct sieve_binary *sbin, void *context ATTR_UNUSED, string_t *item)
{
	(void) sieve_opr_string_emit(sbin, item);
}

void sieve_opr_stringlist_emit_end
	(struct sieve_binary *sbin, void *context)
{
	sieve_size_t *end_offset = (sieve_size_t *) context;

	(void) sieve_binary_resolve_offset(sbin, *end_offset);
}

bool sieve_opr_stringlist_dump_data
(const struct sieve_dumptime_env *denv, const struct sieve_operand *operand,
	sieve_size_t *address) 
{
	if ( operand == NULL )
		return FALSE;
	
	if ( operand->class == &stringlist_class ) {
		const struct sieve_opr_stringlist_interface *intf =
			(const struct sieve_opr_stringlist_interface *) operand->interface; 
		
		if ( intf->dump == NULL )
			return FALSE;

		return intf->dump(denv, address); 
	} else if ( operand->class == &string_class ) {
		const struct sieve_opr_string_interface *intf =
			(const struct sieve_opr_string_interface *) operand->interface; 
	
		if ( intf->dump == NULL ) 
			return FALSE;

		return intf->dump(denv, address);  
	}
	
	return FALSE;
}

bool sieve_opr_stringlist_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address) 
{
	const struct sieve_operand *operand;

	sieve_code_mark(denv);
	operand = sieve_operand_read(denv->sbin, address);

	return sieve_opr_stringlist_dump_data(denv, operand, address);
}

struct sieve_coded_stringlist *sieve_opr_stringlist_read_data
  (const struct sieve_runtime_env *renv, const struct sieve_operand *operand,
  	sieve_size_t op_address, sieve_size_t *address)
{
	if ( operand == NULL )
		return NULL;
		
	if ( operand->class == &stringlist_class ) {
		const struct sieve_opr_stringlist_interface *intf = 
			(const struct sieve_opr_stringlist_interface *) operand->interface;
			
		if ( intf->read == NULL ) 
			return NULL;

		return intf->read(renv, address);  
	} else if ( operand->class == &string_class ) {
		/* Special case, accept single string as string list as well. */
		const struct sieve_opr_string_interface *intf = 
			(const struct sieve_opr_string_interface *) operand->interface;
				
		if ( intf->read == NULL || !intf->read(renv, address, NULL) ) {
			return NULL;
		}
		
		return sieve_coded_stringlist_create(renv, op_address, 1, *address); 
	}	
	
	return NULL;
}

struct sieve_coded_stringlist *sieve_opr_stringlist_read
  (const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	sieve_size_t op_address = *address;
	const struct sieve_operand *operand = sieve_operand_read(renv->sbin, address);
	
	return sieve_opr_stringlist_read_data(renv, operand, op_address, address);
}

static bool opr_stringlist_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address) 
{
	sieve_size_t pc = *address;
	sieve_size_t end; 
	sieve_size_t length = 0; 
 	int end_offset;

	if ( !sieve_binary_read_offset(denv->sbin, address, &end_offset) )
		return FALSE;

	end = pc + end_offset;

	if ( !sieve_binary_read_integer(denv->sbin, address, &length) ) 
		return FALSE;	
  	
	return sieve_coded_stringlist_dump(denv, address, length, end); 
}

static struct sieve_coded_stringlist *opr_stringlist_read
  (const struct sieve_runtime_env *renv, sieve_size_t *address )
{
	struct sieve_coded_stringlist *strlist;
	sieve_size_t pc = *address;
	sieve_size_t end; 
	sieve_size_t length = 0;  
	int end_offset;
	
	if ( !sieve_binary_read_offset(renv->sbin, address, &end_offset) )
		return NULL;

	end = pc + end_offset;

	if ( !sieve_binary_read_integer(renv->sbin, address, &length) ) 
  	return NULL;	
  	
	strlist = sieve_coded_stringlist_create(renv, *address, length, end); 

	/* Skip over the string list for now */
	*address = end;
  
	return strlist;
}  

/* 
 * Operations
 */
 
/* Declaration of opcodes defined in this file */

static bool opc_jmp_dump
	(const struct sieve_operation *op, 
		const struct sieve_dumptime_env *denv, sieve_size_t *address);

static bool opc_jmp_execute
	(const struct sieve_operation *op, 
		const struct sieve_runtime_env *renv, sieve_size_t *address);
static bool opc_jmptrue_execute
	(const struct sieve_operation *op, 
		const struct sieve_runtime_env *renv, sieve_size_t *address);
static bool opc_jmpfalse_execute
	(const struct sieve_operation *op, 
		const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation sieve_jmp_operation = { 
	"JMP",
	NULL,
	SIEVE_OPERATION_JMP,
	opc_jmp_dump, 
	opc_jmp_execute 
};

const struct sieve_operation sieve_jmptrue_operation = { 
	"JMPTRUE",
	NULL,
	SIEVE_OPERATION_JMPTRUE,
	opc_jmp_dump, 
	opc_jmptrue_execute 
};

const struct sieve_operation sieve_jmpfalse_operation = { 
	"JMPFALSE",
	NULL,
	SIEVE_OPERATION_JMPFALSE,
	opc_jmp_dump, 
	opc_jmpfalse_execute 
};
	
extern const struct sieve_operation cmd_stop_operation;
extern const struct sieve_operation cmd_keep_operation;
extern const struct sieve_operation cmd_discard_operation;
extern const struct sieve_operation cmd_redirect_operation;

extern const struct sieve_operation tst_address_operation;
extern const struct sieve_operation tst_header_operation;
extern const struct sieve_operation tst_exists_operation;
extern const struct sieve_operation tst_size_over_operation;
extern const struct sieve_operation tst_size_under_operation;

const struct sieve_operation *sieve_operations[] = {
	NULL, 
	
	&sieve_jmp_operation,
	&sieve_jmptrue_operation, 
	&sieve_jmpfalse_operation,
	
	&cmd_stop_operation,
	&cmd_keep_operation,
	&cmd_discard_operation,
	&cmd_redirect_operation,

	&tst_address_operation,
	&tst_header_operation,
	&tst_exists_operation,
	&tst_size_over_operation,
	&tst_size_under_operation
}; 

const unsigned int sieve_operations_count =
	N_ELEMENTS(sieve_operations);

static struct sieve_extension_obj_registry oprt_default_reg =
	SIEVE_EXT_DEFINE_OPERATIONS(sieve_operations);

/* Operation functions */

sieve_size_t sieve_operation_emit_code
	(struct sieve_binary *sbin, const struct sieve_operation *op)
{
	return sieve_extension_emit_obj
		(sbin, &oprt_default_reg, op, operations, op->extension);
}

static const struct sieve_extension_obj_registry *
	sieve_operation_registry_get
(struct sieve_binary *sbin, unsigned int ext_index)
{
	const struct sieve_extension *ext;
	
	if ( (ext=sieve_binary_extension_get_by_index(sbin, ext_index)) 
		== NULL )
		return NULL;
		
	return &(ext->operations);
}

const struct sieve_operation *sieve_operation_read
	(struct sieve_binary *sbin, sieve_size_t *address) 
{
	return sieve_extension_read_obj
		(struct sieve_operation, sbin, address, &oprt_default_reg, 
			sieve_operation_registry_get);
}

const char *sieve_operation_read_string
	(struct sieve_binary *sbin, sieve_size_t *address) 
{
	return sieve_extension_read_obj_string
		(sbin, address, &oprt_default_reg, 
			sieve_operation_registry_get);
}
	
/* Code dump for core commands */

static bool opc_jmp_dump
(const struct sieve_operation *op,
	const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	unsigned int pc = *address;
	int offset;
	
	if ( sieve_binary_read_offset(denv->sbin, address, &offset) ) 
		sieve_code_dumpf(denv, "%s %d [%08x]", 
			op->mnemonic, offset, pc + offset);
	else
		return FALSE;
	
	return TRUE;
}	
			
/* Code dump for trivial operations */

bool sieve_operation_string_dump
(const struct sieve_operation *op,
	const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "%s", op->mnemonic);

	return 
		sieve_opr_string_dump(denv, address);
}

/* Code execution for core commands */

static bool opc_jmp_execute
(const struct sieve_operation *op ATTR_UNUSED, 
	const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED) 
{
	sieve_runtime_trace(renv, "JMP");
	
	if ( !sieve_interpreter_program_jump(renv->interp, TRUE) )
		return FALSE;
	
	return TRUE;
}	
		
static bool opc_jmptrue_execute
(const struct sieve_operation *op ATTR_UNUSED, 
	const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED)
{	
	bool result = sieve_interpreter_get_test_result(renv->interp);
	
	sieve_runtime_trace(renv, "JMPTRUE (%s)", result ? "true" : "false");
	
	if ( !sieve_interpreter_program_jump(renv->interp, result) )
		return FALSE;
	
	return TRUE;
}

static bool opc_jmpfalse_execute
(const struct sieve_operation *op ATTR_UNUSED, 
	const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED)
{	
	bool result = sieve_interpreter_get_test_result(renv->interp);
	
	sieve_runtime_trace(renv, "JMPFALSE (%s)", result ? "true" : "false" );
	
	if ( !sieve_interpreter_program_jump(renv->interp, !result) )
		return FALSE;
			
	return TRUE;
}	
