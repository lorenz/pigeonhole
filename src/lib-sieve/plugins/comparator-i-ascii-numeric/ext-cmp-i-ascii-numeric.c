/* Extension comparator-i;ascii-numeric
 * ------------------------------------
 *
 * Author: Stephan Bosch
 * Specification: RFC 2244
 * Implementation: full
 * Status: experimental, largely untested
 * 
 */
 
#include "sieve-common.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-comparators.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include <ctype.h>

/* 
 * Forward declarations 
 */

static const struct sieve_operand my_comparator_operand;
const struct sieve_comparator i_ascii_numeric_comparator;

static bool ext_cmp_i_ascii_numeric_load(int ext_id);
static bool ext_cmp_i_ascii_numeric_validator_load(struct sieve_validator *validator);

/* 
 * Extension definitions 
 */

static int ext_my_id;

const struct sieve_extension comparator_i_ascii_numeric_extension = { 
	"comparator-i;ascii-numeric", 
	&ext_my_id,
	ext_cmp_i_ascii_numeric_load,
	ext_cmp_i_ascii_numeric_validator_load,
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS, 
	SIEVE_EXT_DEFINE_OPERAND(my_comparator_operand)
};

static bool ext_cmp_i_ascii_numeric_load(int ext_id)
{
	ext_my_id = ext_id;

	return TRUE;
}

/* Actual extension implementation */

/* Extension access structures */

static int cmp_i_ascii_numeric_compare
	(const struct sieve_comparator *cmp, 
		const char *val1, size_t val1_size, const char *val2, size_t val2_size);

static const struct sieve_extension_obj_registry ext_comparators =
	SIEVE_EXT_DEFINE_COMPARATOR(i_ascii_numeric_comparator);
	
static const struct sieve_operand my_comparator_operand = { 
	"comparator-i;ascii-numeric", 
	&comparator_i_ascii_numeric_extension,
	0, 
	&sieve_comparator_operand_class,
	&ext_comparators
};

const struct sieve_comparator i_ascii_numeric_comparator = { 
	SIEVE_OBJECT("i;ascii-numeric", &my_comparator_operand, 0),
	SIEVE_COMPARATOR_FLAG_ORDERING | SIEVE_COMPARATOR_FLAG_EQUALITY,
	cmp_i_ascii_numeric_compare,
	NULL,
	NULL
};

/* Load extension into validator */

static bool ext_cmp_i_ascii_numeric_validator_load(struct sieve_validator *validator)
{
	sieve_comparator_register(validator, &i_ascii_numeric_comparator);
	return TRUE;
}

/* Implementation */

static int cmp_i_ascii_numeric_compare
	(const struct sieve_comparator *cmp ATTR_UNUSED, 
		const char *val, size_t val_size, const char *key, size_t key_size)
{	
	const char *vend = val + val_size;
	const char *kend = key + key_size;
	const char *vp = val;
	const char *kp = key;
	
	/* Ignore leading zeros */

	while ( *vp == '0' && vp < vend )  
		vp++;

	while ( *kp == '0' && kp < kend )  
		kp++;

	while ( vp < vend && kp < kend ) {
		if ( !i_isdigit(*vp) || !i_isdigit(*kp) ) 
			break;

		if ( *vp != *kp ) 
			break;

		vp++;
		kp++;	
	}

	if ( vp == vend || !i_isdigit(*vp) ) {
		if ( kp == kend || !i_isdigit(*kp) ) 
			return 0;
		else	
			return -1;
	} else if ( kp == kend || !i_isdigit(*kp) )  
		return 1;
		
	return (*vp > *kp);
}

