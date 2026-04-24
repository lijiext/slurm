/*****************************************************************************\
 *  jobcomp_custom.c - custom JSON slurm job completion logging plugin.
\*****************************************************************************/

#include "config.h"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/stat.h>

#include "src/common/slurm_protocol_defs.h"
#include "src/common/slurm_jobcomp.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/uid.h"

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 */
const char plugin_name[]       	= "Job completion custom JSON logging plugin";
const char plugin_type[]       	= "jobcomp/custom";
const uint32_t plugin_version	= SLURM_VERSION_NUMBER;

static pthread_mutex_t  file_lock = PTHREAD_MUTEX_INITIALIZER;
static char *           log_name  = NULL;
static int              job_comp_fd = -1;

/*
 * _escape_json - escape special characters for JSON string values
 * RET xmalloc'd string, must be freed with xfree()
 */
static char *_escape_json(const char *str)
{
	char *out = NULL;
	const char *p;

	if (!str)
		return xstrdup("");

	for (p = str; *p; p++) {
		switch (*p) {
		case '\\':
			xstrcat(out, "\\\\");
			break;
		case '\"':
			xstrcat(out, "\\\"");
			break;
		case '\b':
			xstrcat(out, "\\b");
			break;
		case '\f':
			xstrcat(out, "\\f");
			break;
		case '\n':
			xstrcat(out, "\\n");
			break;
		case '\r':
			xstrcat(out, "\\r");
			break;
		case '\t':
			xstrcat(out, "\\t");
			break;
		default:
			if (*p < 32) {
				/* Control characters */
				xstrfmtcat(out, "\\u%04x", (unsigned char)*p);
			} else {
				xstrcatchar(out, *p);
			}
			break;
		}
	}

	return out ? out : xstrdup("");
}

/*
 * init() is called when the plugin is loaded, before any other functions
 * are called.  Put global initialization here.
 */
extern int init ( void )
{
	return SLURM_SUCCESS;
}

extern int fini ( void )
{
	if (job_comp_fd >= 0)
		close(job_comp_fd);
	xfree(log_name);
	return SLURM_SUCCESS;
}

/*
 * The remainder of this file implements the standard Slurm job completion
 * logging API.
 */

extern int jobcomp_p_set_location(char *location)
{
	int rc = SLURM_SUCCESS;

	if (location == NULL) {
		return SLURM_ERROR;
	}
	xfree(log_name);
	log_name = xstrdup(location);

	slurm_mutex_lock( &file_lock );
	if (job_comp_fd >= 0)
		close(job_comp_fd);
	job_comp_fd = open(location, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (job_comp_fd == -1) {
		error("%s: open %s: %m", plugin_type, location);
		rc = SLURM_ERROR;
	} else {
		/* Ensure correct permissions even if umask is restrictive */
		(void) fchmod(job_comp_fd, 0644);
	}
	slurm_mutex_unlock( &file_lock );
	return rc;
}

extern int jobcomp_p_log_record(job_record_t *job_ptr)
{
	int rc = SLURM_SUCCESS;
	char *json = NULL;
	char *user_name = NULL;
	char *state_str = NULL;
	char *esc_name = NULL, *esc_user = NULL, *esc_part = NULL;
	char *esc_nodes = NULL, *esc_qos = NULL, *esc_tres = NULL;
	char *esc_resv = NULL;
	char *array_task_str = NULL, *het_offset_str = NULL;
	time_t submit_time = 0;
	long elapsed = 0;
	long cpu_time = 0;

	if (job_comp_fd < 0) {
		return SLURM_ERROR;
	}

	user_name = uid_to_string(job_ptr->user_id);
	state_str = job_state_string(job_ptr->job_state & JOB_STATE_BASE);
	
	/* Escape string fields for safe JSON */
	esc_name  = _escape_json(job_ptr->name);
	esc_user  = _escape_json(user_name);
	esc_part  = _escape_json(job_ptr->partition);
	esc_nodes = _escape_json(job_ptr->nodes);
	esc_qos   = _escape_json(job_ptr->qos_ptr ? job_ptr->qos_ptr->name : NULL);
	esc_tres  = _escape_json(job_ptr->tres_alloc_str);
	esc_resv  = _escape_json(job_ptr->resv_name);

	/* Handle Array and HetJob IDs/Offsets (convert NO_VAL to null) */
	if (job_ptr->array_task_id == NO_VAL)
		array_task_str = xstrdup("null");
	else
		xstrfmtcat(array_task_str, "%u", job_ptr->array_task_id);

	if (job_ptr->het_job_offset == NO_VAL)
		het_offset_str = xstrdup("null");
	else
		xstrfmtcat(het_offset_str, "%u", job_ptr->het_job_offset);
		
	if (job_ptr->details)
		submit_time = job_ptr->details->submit_time;

	if (job_ptr->start_time && job_ptr->end_time >= job_ptr->start_time)
		elapsed = (long)(job_ptr->end_time - job_ptr->start_time);
	
	cpu_time = elapsed * job_ptr->total_cpus;

	/* 构建 JSON 字符串，包含复杂调度字段 */
	xstrfmtcat(json, "{\"job_id\": %u, \"array_job_id\": %u, \"array_task_id\": %s, \"het_job_id\": %u, \"het_job_offset\": %s, \"job_name\": \"%s\", \"user\": \"%s\", \"partition\": \"%s\", \"resv_name\": \"%s\", \"state\": \"%s\", \"submit\": %ld, \"start\": %ld, \"end\": %ld, \"elapsed_raw\": %ld, \"cpus\": %u, \"cpu_time_raw\": %ld, \"nodes\": \"%s\", \"qos\": \"%s\", \"tres\": \"%s\", \"restart_cnt\": %u}\n",
		   job_ptr->job_id, job_ptr->array_job_id, array_task_str,
		   job_ptr->het_job_id, het_offset_str,
		   esc_name, esc_user, esc_part, esc_resv,
		   state_str, (long)submit_time, (long)job_ptr->start_time, (long)job_ptr->end_time, 
		   elapsed, job_ptr->total_cpus, cpu_time, esc_nodes, 
		   esc_qos, esc_tres, job_ptr->restart_cnt);

	slurm_mutex_lock( &file_lock );
	if (job_comp_fd >= 0) {
		size_t len = strlen(json);
		size_t offset = 0;
		while (offset < len) {
			ssize_t wrote = write(job_comp_fd, json + offset, len - offset);
			if (wrote <= 0) {
				if (wrote == -1 && (errno == EAGAIN || errno == EINTR))
					continue;
				rc = SLURM_ERROR;
				break;
			}
			offset += (size_t)wrote;
		}
	} else {
		rc = SLURM_ERROR;
	}
	slurm_mutex_unlock( &file_lock );

	xfree(user_name);
	xfree(esc_name);
	xfree(esc_user);
	xfree(esc_part);
	xfree(esc_nodes);
	xfree(esc_qos);
	xfree(esc_tres);
	xfree(esc_resv);
	xfree(array_task_str);
	xfree(het_offset_str);
	xfree(json);
	return rc;
}

extern List jobcomp_p_get_jobs(slurmdb_job_cond_t *job_cond)
{
	return NULL;
}
