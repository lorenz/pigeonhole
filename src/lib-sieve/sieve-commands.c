#include <stdio.h>

#include "sieve-ast.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-binary.h"

#include "sieve-commands-private.h"
#include "sieve-code.h"
#include "sieve-interpreter.h"

/* Default arguments implemented in this file */

static bool arg_number_generate
	(struct sieve_generator *generator, struct sieve_ast_argument *arg, 
		struct sieve_command_context *context);
static bool arg_string_generate
	(struct sieve_generator *generator, struct sieve_ast_argument *arg, 
		struct sieve_command_context *context);
static bool arg_string_list_validate
	(struct sieve_validator *validator, struct sieve_ast_argument **arg, 
		struct sieve_command_context *context);
static bool arg_string_list_generate
	(struct sieve_generator *generator, struct sieve_ast_argument *arg, 
		struct sieve_command_context *context);

const struct sieve_argument number_argument = { 
	"@number", 
	NULL, NULL, NULL, NULL,
	arg_number_generate 
};

const struct sieve_argument string_argument = { 
	"@string", 
	NULL, NULL, NULL, NULL,
	arg_string_generate 
};

const struct sieve_argument string_list_argument = { 
	"@string-list", 
	NULL, NULL,
	arg_string_list_validate, 
	NULL, 
	arg_string_list_generate 
};	

static bool arg_number_generate
(struct sieve_generator *generator, struct sieve_ast_argument *arg, 
	struct sieve_command_context *context ATTR_UNUSED)
{
	struct sieve_binary *sbin = sieve_generator_get_binary(generator);
	
	sieve_opr_number_emit(sbin, sieve_ast_argument_number(arg));

	return TRUE;
}

static bool arg_string_generate
(struct sieve_generator *generator, struct sieve_ast_argument *arg, 
	struct sieve_command_context *context ATTR_UNUSED)
{
	struct sieve_binary *sbin = sieve_generator_get_binary(generator);

	sieve_opr_string_emit(sbin, sieve_ast_argument_str(arg));
  
	return TRUE;
}

static bool arg_string_list_validate
(struct sieve_validator *validator, struct sieve_ast_argument **arg, 
	struct sieve_command_context *context)
{
	struct sieve_ast_argument *stritem;

	stritem = sieve_ast_strlist_first(*arg);	
	while ( stritem != NULL ) {
		if ( !sieve_validator_argument_activate(validator, context, stritem, FALSE) )
			return FALSE;
			
		stritem = sieve_ast_strlist_next(stritem);
	}

	return TRUE;	
}

static bool emit_string_list_operand
(struct sieve_generator *generator, const struct sieve_ast_argument *strlist,
	struct sieve_command_context *context)
{	
	struct sieve_binary *sbin = sieve_generator_get_binary(generator);
	void *list_context;
	struct sieve_ast_argument *stritem;
   	
	sieve_opr_stringlist_emit_start
		(sbin, sieve_ast_strlist_count(strlist), &list_context);

	stritem = sieve_ast_strlist_first(strlist);
	while ( stritem != NULL ) {
		if ( !sieve_generate_argument(generator, stritem, context) )
			return FALSE;
			
		stritem = sieve_ast_strlist_next(stritem);
	}

	sieve_opr_stringlist_emit_end(sbin, list_context);
	
	return TRUE;
}

static bool arg_string_list_generate
(struct sieve_generator *generator, struct sieve_ast_argument *arg, 
	struct sieve_command_context *context)
{
	if ( sieve_ast_argument_type(arg) == SAAT_STRING ) {
		return ( sieve_generate_argument(generator, arg, context) );

	} else if ( sieve_ast_argument_type(arg) == SAAT_STRING_LIST ) {
		bool result = TRUE;
		
		if ( sieve_ast_strlist_count(arg) == 1 ) 
			return ( sieve_generate_argument
				(generator, sieve_ast_strlist_first(arg), context) );
		else {
			T_BEGIN { 
				result=emit_string_list_operand(generator, arg, context);
			} T_END;
		}

		return result;
	}
	
	return FALSE;
}

/* Trivial tests implemented in this file */

static bool tst_false_generate
	(struct sieve_generator *generator, 
		struct sieve_command_context *context ATTR_UNUSED,
		struct sieve_jumplist *jumps, bool jump_true);
static bool tst_true_generate
	(struct sieve_generator *generator, 
		struct sieve_command_context *ctx ATTR_UNUSED,
		struct sieve_jumplist *jumps, bool jump_true);

static const struct sieve_command tst_false = { 
	"false", 
	SCT_TEST, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL, 
	tst_false_generate 
};

static const struct sieve_command tst_true = { 
	"true", 
	SCT_TEST, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL, 
	tst_true_generate 
};

/* Trivial commands implemented in this file */

static bool cmd_stop_generate
	(struct sieve_generator *generator, 
		struct sieve_command_context *ctx ATTR_UNUSED);
static bool cmd_stop_validate
	(struct sieve_validator *validator, struct sieve_command_context *ctx);
	
const struct sieve_command cmd_stop = { 
	"stop", 
	SCT_COMMAND, 
	0, 0, FALSE, FALSE,
	NULL, NULL,  
	cmd_stop_validate, 
	cmd_stop_generate, 
	NULL 
};

/* Lists of core tests and commands */

const struct sieve_command *sieve_core_tests[] = {
	&tst_false, &tst_true,

	&tst_address, &tst_header, &tst_exists, &tst_size, 	
	&tst_not, &tst_anyof, &tst_allof
};

const unsigned int sieve_core_tests_count = N_ELEMENTS(sieve_core_tests);

const struct sieve_command *sieve_core_commands[] = {
	&cmd_stop, &cmd_keep, &cmd_discard,

	&cmd_require, &cmd_if, &cmd_elsif, &cmd_else, &cmd_redirect
};

const unsigned int sieve_core_commands_count = N_ELEMENTS(sieve_core_commands);
	
/* Command context */

struct sieve_command_context *sieve_command_prev_context	
	(struct sieve_command_context *context) 
{
	struct sieve_ast_node *node = sieve_ast_node_prev(context->ast_node);
	
	if ( node != NULL ) {
		return node->context;
	}
	
	return NULL;
}

struct sieve_command_context *sieve_command_parent_context	
	(struct sieve_command_context *context) 
{
	struct sieve_ast_node *node = sieve_ast_node_parent(context->ast_node);
	
	if ( node != NULL ) {
		return node->context;
	}
	
	return NULL;
}

struct sieve_command_context *sieve_command_context_create
	(struct sieve_ast_node *cmd_node, const struct sieve_command *command,
		struct sieve_command_registration *reg)
{
	struct sieve_command_context *cmd;
	
	cmd = p_new(sieve_ast_node_pool(cmd_node), struct sieve_command_context, 1);
	
	cmd->ast_node = cmd_node;	
	cmd->command = command;
	cmd->cmd_reg = reg;
	
	cmd->block_exit_command = NULL;
	
	return cmd;
}

const char *sieve_command_type_name(const struct sieve_command *command) {
	switch ( command->type ) {
	case SCT_NONE: return "command of unspecified type (bug)";
	case SCT_TEST: return "test";
	case SCT_COMMAND: return "command";
	default:
		break;
	}
	return "??COMMAND-TYPE??";
}

struct sieve_ast_argument *sieve_command_add_dynamic_tag
(struct sieve_command_context *cmd, const struct sieve_argument *tag, 
	int id_code)
{
	struct sieve_ast_argument *arg;
	
	if ( cmd->first_positional != NULL )
		arg = sieve_ast_argument_tag_insert
			(cmd->first_positional, tag->identifier, cmd->ast_node->source_line);
	else
		arg = sieve_ast_argument_tag_create
			(cmd->ast_node, tag->identifier, cmd->ast_node->source_line);
	
	arg->argument = tag;
	arg->arg_id_code = id_code;
	
	return arg;
}

struct sieve_ast_argument *sieve_command_find_argument
(struct sieve_command_context *cmd, const struct sieve_argument *argument)
{
	struct sieve_ast_argument *arg = sieve_ast_argument_first(cmd->ast_node);
		
	/* Visit tagged and optional arguments */
	while ( arg != NULL ) {
		if ( arg->argument == argument ) 
			return arg;
			
		arg = sieve_ast_argument_next(arg);
	}
	
	return arg;
}

/* Use this function with caution. The command commits to exiting the block.
 * When it for some reason does not, the interpretation will break later on, 
 * because exiting jumps are not generated when they would otherwise be 
 * necessary.
 */
void sieve_command_exit_block_unconditionally
	(struct sieve_command_context *cmd)
{
	struct sieve_command_context *parent = sieve_command_parent_context(cmd);

	/* Only the first unconditional exit is of importance */
	if ( parent != NULL && parent->block_exit_command == NULL ) 
		parent->block_exit_command = cmd;
}

bool sieve_command_block_exits_unconditionally
	(struct sieve_command_context *cmd)
{
	return ( cmd->block_exit_command != NULL );
}

/* Operations */

static bool opc_stop_execute
	(const struct sieve_operation *op, 
		const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation cmd_stop_operation = { 
	"STOP",
	NULL,
	SIEVE_OPERATION_STOP,
	NULL, 
	opc_stop_execute 
};

static bool opc_stop_execute
(const struct sieve_operation *op ATTR_UNUSED, 
	const struct sieve_runtime_env *renv,  
	sieve_size_t *address ATTR_UNUSED)
{	
	sieve_runtime_trace(renv, "STOP");
	
	sieve_interpreter_interrupt(renv->interp);

	return TRUE;
}


/* Code generation for trivial commands and tests */

static bool cmd_stop_validate
	(struct sieve_validator *validator ATTR_UNUSED, 
		struct sieve_command_context *ctx)
{
	sieve_command_exit_block_unconditionally(ctx);
	
	return TRUE;
}

static bool cmd_stop_generate
	(struct sieve_generator *generator, 
		struct sieve_command_context *ctx ATTR_UNUSED) 
{
	sieve_operation_emit_code(
		sieve_generator_get_binary(generator), &cmd_stop_operation, -1);
	return TRUE;
}

static bool tst_false_generate
	(struct sieve_generator *generator, 
		struct sieve_command_context *context ATTR_UNUSED,
		struct sieve_jumplist *jumps, bool jump_true)
{
	struct sieve_binary *sbin = sieve_generator_get_binary(generator);

	if ( !jump_true ) {
		sieve_operation_emit_code(sbin, &sieve_jmp_operation, -1);
		sieve_jumplist_add(jumps, sieve_binary_emit_offset(sbin, 0));
	}
	
	return TRUE;
}

static bool tst_true_generate
	(struct sieve_generator *generator, 
		struct sieve_command_context *ctx ATTR_UNUSED,
		struct sieve_jumplist *jumps, bool jump_true)
{
	struct sieve_binary *sbin = sieve_generator_get_binary(generator);

	if ( jump_true ) {
		sieve_operation_emit_code(sbin, &sieve_jmp_operation, -1);
		sieve_jumplist_add(jumps, sieve_binary_emit_offset(sbin, 0));
	}
	
	return TRUE;
}

