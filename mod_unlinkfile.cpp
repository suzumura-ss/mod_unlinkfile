/* 
 * Copyright 2011 Toshiyuki Suzumura
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
 *
 */

extern "C" {
  #include "httpd.h"
  #include "http_protocol.h"
  #include "http_config.h"
  #include "http_request.h"
  #include "http_log.h"
  #include "ap_config.h"
  #include "apr_strings.h"
};

#define AP_LOG_VERBOSE(rec, fmt, ...) //ap_log_rerror(APLOG_MARK, APLOG_DEBUG,  0, rec, fmt, ##__VA_ARGS__)
#define AP_LOG_DEBUG(rec, fmt, ...) ap_log_rerror(APLOG_MARK, APLOG_DEBUG,  0, rec, fmt, ##__VA_ARGS__)
#define AP_LOG_INFO(rec, fmt, ...)  ap_log_rerror(APLOG_MARK, APLOG_INFO,   0, rec, "[reproxy] " fmt, ##__VA_ARGS__)
#define AP_LOG_WARN(rec, fmt, ...)  ap_log_rerror(APLOG_MARK, APLOG_WARNING,0, rec, "[reproxy] " fmt, ##__VA_ARGS__)
#define AP_LOG_ERR(rec, fmt, ...)   ap_log_rerror(APLOG_MARK, APLOG_ERR,    0, rec, "[reproxy] " fmt, ##__VA_ARGS__)

#define UNLINK   "X-Unlink-File"

static const char X_UNINK[] = UNLINK;
extern "C" module AP_MODULE_DECLARE_DATA unlinkfile_module;

struct unlink_conf {
  int   enabled;
};

//
// Utils.
//
static const char* get_and_unset_header(apr_table_t* tbl, const char* key)
{
  const char* value = apr_table_get(tbl, key);
  if(value) apr_table_unset(tbl, key);
  return value;
}
  

//
// Output filter.
//
static apr_status_t unlink_output_filter(ap_filter_t* f, apr_bucket_brigade* in_bb)
{
  request_rec* rec =f->r;
  const char* unlink_file;
  apr_status_t result;

  AP_LOG_VERBOSE(rec, "Incoming %s.", __FUNCTION__);

  // Pass thru by request types.
  if(rec->status!=HTTP_OK || rec->main!=NULL || rec->header_only
    || (rec->handler!= NULL && strcmp(rec->handler, "default-handler") == 0)) goto PASS_THRU;

  AP_LOG_VERBOSE(rec, "-- Checking responce headers.");

  // Obtain and erase x-reproxy-url header or pass through.
  unlink_file = get_and_unset_header(rec->headers_out, X_UNINK);
  if(unlink_file== NULL || unlink_file[0]=='\0') {
    unlink_file = get_and_unset_header(rec->err_headers_out, X_UNINK);
    if(unlink_file==NULL || unlink_file[0]=='\0') goto PASS_THRU;
  }

  // Unlink
  AP_LOG_VERBOSE(rec, "-- Unlinking file: %s.", unlink_file);
  result = apr_file_remove(unlink_file, rec->pool);
  if(result!=APR_SUCCESS) {
    char buf[100];
    AP_LOG_ERR(rec, UNLINK ": Unink '%s' failed. %s(%d).", unlink_file, apr_strerror(result, buf, 100), result);
  }
 
PASS_THRU:
  AP_LOG_VERBOSE(rec, "-- Filter done.");
  ap_remove_output_filter(f);
  return ap_pass_brigade(f->next, in_bb);
}


// Add output filter if it is enabled.
static void unlink_insert_output_filter(request_rec* rec)
{
  AP_LOG_VERBOSE(rec, "Incoming %s.", __FUNCTION__);
  unlink_conf* conf = (unlink_conf*)ap_get_module_config(rec->per_dir_config, &unlinkfile_module);
  if(conf->enabled) ap_add_output_filter(X_UNINK, NULL, rec, rec->connection);
}


//
// Configurators, and Register.
// 
static void* config_create(apr_pool_t* p, char* path)
{
  unlink_conf* conf = (unlink_conf*)apr_palloc(p, sizeof(unlink_conf));
  conf->enabled = FALSE;

  return conf;
}

static const command_rec config_cmds[] = {
  AP_INIT_FLAG(X_UNINK, (cmd_func)ap_set_flag_slot, (void*)APR_OFFSETOF(unlink_conf, enabled), OR_OPTIONS, "{On|Off}"),
  { NULL },
};

static void register_hooks(apr_pool_t *p)
{
  ap_register_output_filter(X_UNINK, unlink_output_filter, NULL, AP_FTYPE_CONTENT_SET);
  ap_hook_insert_filter(unlink_insert_output_filter, NULL, NULL, APR_HOOK_REALLY_LAST);
}


// Dispatch list for API hooks.
module AP_MODULE_DECLARE_DATA unlinkfile_module = {
  STANDARD20_MODULE_STUFF, 
  config_create,  // create per-dir    config structures.
  NULL,           // merge  per-dir    config structures.
  NULL,           // create per-server config structures.
  NULL,           // merge  per-server config structures.
  config_cmds,    // table of config file commands.
  register_hooks  // register hooks.
};
