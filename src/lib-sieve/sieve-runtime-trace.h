#ifndef __SIEVE_TRACE_H
#define __SIEVE_TRACE_H

#include "sieve-common.h"
#include "sieve-runtime.h"

/*
 * Runtime trace
 */

/* Trace configuration */

static inline bool sieve_runtime_trace_active
(const struct sieve_runtime_env *renv, sieve_trace_level_t trace_level) 
{
	return ( renv->trace_stream != NULL && trace_level <= renv->trace_level );
}

/* Trace errors */

void _sieve_runtime_trace_error
	(const struct sieve_runtime_env *renv, const char *fmt, va_list args);	

void _sieve_runtime_trace_operand_error
	(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
		const char *field_name, const char *fmt, va_list args);

static inline void sieve_runtime_trace_error
	(const struct sieve_runtime_env *renv, const char *fmt, ...)
		ATTR_FORMAT(2, 3);

static inline void sieve_runtime_trace_operand_error
	(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd, 
		const char *field_name, const char *fmt, ...) ATTR_FORMAT(4, 5);

static inline void sieve_runtime_trace_error
	(const struct sieve_runtime_env *renv, const char *fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	if ( renv->trace_stream != NULL )
		_sieve_runtime_trace_error(renv, fmt, args);	
	va_end(args);
}

static inline void sieve_runtime_trace_operand_error
	(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd, 
		const char *field_name, const char *fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	if ( renv->trace_stream != NULL )
		_sieve_runtime_trace_operand_error(renv, oprnd, field_name, fmt, args);
	va_end(args);
}

/* Trace info */

void _sieve_runtime_trace
	(const struct sieve_runtime_env *renv, const char *fmt, va_list args);

static inline void sieve_runtime_trace
	(const struct sieve_runtime_env *renv, sieve_trace_level_t trace_level,
		const char *fmt, ...) ATTR_FORMAT(3, 4);

static inline void sieve_runtime_trace
(const struct sieve_runtime_env *renv, sieve_trace_level_t trace_level,
	const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if ( renv->trace_stream != NULL && trace_level <= renv->trace_level ) {
		_sieve_runtime_trace(renv, fmt, args);
	}

	va_end(args);
}

void _sieve_runtime_trace_address
	(const struct sieve_runtime_env *renv, sieve_size_t address, 
		const char *fmt, va_list args);

static inline void sieve_runtime_trace_address
	(const struct sieve_runtime_env *renv, sieve_trace_level_t trace_level,
		sieve_size_t address, const char *fmt, ...) ATTR_FORMAT(4, 5);

static inline void sieve_runtime_trace_address
(const struct sieve_runtime_env *renv, sieve_trace_level_t trace_level,
	sieve_size_t address, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if ( renv->trace_stream != NULL && trace_level <= renv->trace_level ) {
		_sieve_runtime_trace_address(renv, address, fmt, args);
	}

	va_end(args);
}

static inline void sieve_runtime_trace_here
(const struct sieve_runtime_env *renv, sieve_trace_level_t trace_level,
	const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if ( renv->trace_stream != NULL && trace_level <= renv->trace_level ) {
		_sieve_runtime_trace_address(renv, renv->pc, fmt, args);
	}

	va_end(args);
}

/* Trace boundaries */

void _sieve_runtime_trace_begin(const struct sieve_runtime_env *renv);
void _sieve_runtime_trace_end(const struct sieve_runtime_env *renv);

static inline void sieve_runtime_trace_begin
(const struct sieve_runtime_env *renv)
{
	if ( renv->trace_stream != NULL )
		_sieve_runtime_trace_begin(renv);
}

static inline void sieve_runtime_trace_end
(const struct sieve_runtime_env *renv)
{
	if ( renv->trace_stream != NULL )
		_sieve_runtime_trace_end(renv);
}

#endif /* __SIEVE_TRACE_H */