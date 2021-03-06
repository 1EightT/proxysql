#define MAX_FDS_PER_SESSION 2
#define MIN_FDS_PER_THREAD  1024

typedef struct __fdb_hash_t fdb_hash_t;
typedef struct __fdb_hashes_group_t fdb_hashes_group_t;
typedef struct __fdb_hash_entry fdb_hash_entry;

typedef struct __fdb_system_var_t {
    long long hash_purge_time;
    long long hash_purge_loop;
    unsigned int hash_expire_max;
    unsigned int hash_expire_default;
	int purge_threshold_pct_min;
	int purge_threshold_pct_max;
} fdb_system_var_t;


EXTERN fdb_system_var_t fdb_system_var;
EXTERN fdb_hash_t **fdb_hashes;

enum enum_timer { 
	TIMER_array2buffer,
	TIMER_buffer2array,
	TIMER_read_from_net,
	TIMER_write_to_net,
	TIMER_processdata,
	TIMER_find_queue,
	TIMER_poll
};


typedef struct _timer {
	unsigned long long total, begin;
} timer;


enum enum_resultset_progress {
	RESULTSET_WAITING,
	RESULTSET_COLUMN_COUNT,
	RESULTSET_COLUMN_DEFINITIONS,
	RESULTSET_EOF1,
	RESULTSET_ROWS,
	RESULTSET_COMPLETED
};

enum mysql_server_status {
	MYSQL_SERVER_STATUS_OFFLINE_HARD = 0,
	MYSQL_SERVER_STATUS_OFFLINE_SOFT = 1,
	MYSQL_SERVER_STATUS_SHUNNED = 2,
	MYSQL_SERVER_STATUS_ONLINE = 3,
};

typedef struct _queue_t {
	void *buffer;
	int size;
	int head;
	int tail;
} queue_t;


// structure that defines mysql protocol header
typedef struct _mysql_hdr {
   u_int pkt_length:24, pkt_id:8;
} mysql_hdr;

typedef struct _pkt {
	int length;
	void *data;
} pkt;

typedef struct _mysql_server {
//	char *name;
	char *address;
	uint16_t port;
	int read_only;
	enum mysql_server_status status;
	uint16_t flags;
	unsigned int connections;
	unsigned char alive;
} mysql_server;

typedef struct _bytes_stats {
	uint64_t bytes_recv;
	uint64_t bytes_sent;
} bytes_stats;

typedef struct _mysql_uni_ds_t {
	queue_t queue;
	GPtrArray *pkts;
	int partial;
	pkt *mypkt;
	mysql_hdr hdr;
} mysql_uni_ds_t;

typedef struct _mysql_cp_entry_t {
	MYSQL *conn;
	unsigned long long expire;
} mysql_cp_entry_t;

typedef struct _mysql_connpool {
	char *hostname;
	char *username;
	char *password;
	char *db;
	unsigned int port;
//  GPtrArray *used_conns;  // temporary (?) disabled
	GPtrArray *free_conns;
} mysql_connpool;


typedef struct _mysql_data_stream_t mysql_data_stream_t;
typedef struct _mysql_session_t mysql_session_t;
typedef struct _free_pkts_t free_pkts_t;
typedef struct _shared_trash_stack_t shared_trash_stack_t;

struct _mysql_data_stream_t {
	mysql_session_t *sess;	// this MUST always the first, because will be overwritten when pushed in a trash stack
	uint64_t pkts_recv;
	uint64_t pkts_sent;
	bytes_stats bytes_info;
	mysql_uni_ds_t input;
	mysql_uni_ds_t output;
	int fd;
	int active_transaction;
	gboolean active;
//	mysql_server *server_ptr;
//	mysql_cp_entry_t *mycpe;
	void (*shut_soft) (mysql_data_stream_t *);
	void (*shut_hard) (mysql_data_stream_t *);
	int (*array2buffer) (mysql_data_stream_t *);
	int (*buffer2array) (mysql_data_stream_t *);
	int (*read_from_net) (mysql_data_stream_t *);
	int (*write_to_net) (mysql_data_stream_t *);
};


struct _free_pkts_t {
	GTrashStack *stack;
	GPtrArray *blocks;	
};


struct _shared_trash_stack_t {
	pthread_mutex_t mutex;
	GTrashStack *stack;
	GPtrArray *blocks;
	int size;
	int incremental;	
};

typedef struct _query_rule_t { // use g_slice_alloc 
	GRegex *regex;
	int rule_id;
	int flagIN;
	char *username;
	char *schemaname;
	char *match_pattern; // use g_malloc/g_free
	int negate_match_pattern;
	int flagOUT;
	char *replace_pattern; // use g_malloc/g_free
	int destination_hostgroup;
	int audit_log;
	int performance_log;
	int cache_tag;
	int invalidate_cache_tag;
	char *invalidate_cache_pattern; // use g_malloc/g_free
	int cache_ttl;
	unsigned int hits;
} query_rule_t;


typedef struct _global_query_rules_t {
	pthread_rwlock_t rwlock;
	GPtrArray *query_rules;
} global_query_rules_t;


typedef struct _proxysql_mysql_thread_t {
	int thread_id;
	free_pkts_t free_pkts;
//	GPtrArray *QC_rules;   // regex should be thread-safe, use just a global one
//	int QCRver;
	GPtrArray *sessions;
} proxy_mysql_thread_t;


typedef struct _mysql_query_metadata_t {
	pkt *p;
	GChecksum *query_checksum;
	int flagOUT;
	int rewritten;
	int cache_ttl;
	int destination_hostgroup;
	int audit_log;
	int performance_log;
	int mysql_query_cache_hit;
	char *query;
	int query_len;
} mysql_query_metadata_t ;


typedef struct _mysql_backend_t mysql_backend_t;
struct _mysql_backend_t {
	// attributes
	int fd;
	mysql_server *server_ptr;
	mysql_data_stream_t *server_myds;
	mysql_cp_entry_t *server_mycpe;
	bytes_stats server_bytes_at_cmd;
	// methods
	void (*reset) (mysql_backend_t *, int);
};

struct _mysql_session_t {
	proxy_mysql_thread_t *handler_thread;
	int healthy;
	int admin;
	int client_fd;
	int server_fd;
	int master_fd;
	int slave_fd;
	int status;
	int force_close_backends;
	int ret;	// generic return status
	struct pollfd fds[MAX_FDS_PER_SESSION];
	int nfds;
	int last_server_poll_fd;
	bytes_stats server_bytes_at_cmd;
	enum enum_server_command client_command;
	enum enum_resultset_progress resultset_progress;
	unsigned long long resultset_size;
	mysql_query_metadata_t query_info;
	gboolean query_to_cache; // must go into query_info
	GPtrArray *resultset; 
	mysql_server *server_ptr;
/* obsoleted by hostgroup : BEGIN
	mysql_server *master_ptr;
	mysql_server *slave_ptr;
obsoleted by hostgroup : END */
	mysql_data_stream_t *client_myds;
	mysql_data_stream_t *server_myds;
/* obsoleted by hostgroup : BEGIN
	mysql_data_stream_t *master_myds;
	mysql_data_stream_t *slave_myds;
obsoleted by hostgroup : END */
//	mysql_data_stream_t *idle_server_myds;
	mysql_cp_entry_t *server_mycpe;
/* obsoleted by hostgroup : BEGIN
	mysql_cp_entry_t *master_mycpe;
	mysql_cp_entry_t *slave_mycpe;
obsoleted by hostgroup : END */
	GPtrArray *mybes;

//	mysql_cp_entry_t *idle_server_mycpe;
	char *mysql_username;
	char *mysql_password;
	char *mysql_schema_cur;
	char *mysql_schema_new;	
	char scramble_buf[21];
	timer *timers;
	gboolean mysql_query_cache_hit; // must go into query_info
	gboolean mysql_server_reconnect;
	gboolean send_to_slave; // must go into query_info

//	public methods
	int (*conn_poll) (mysql_session_t *);
	gboolean (*sync_net) (mysql_session_t *, int);
	//void (*buffer2array_2) (mysql_session_t *);
	//void (*array2buffer_2) (mysql_session_t *);
	void (*check_fds_errors) (mysql_session_t *);
//	int (*process_client_pkts) (mysql_session_t *);
//	void (*process_server_pkts) (mysql_session_t *);
	int (*remove_all_backends_offline_soft) (mysql_session_t *);
	void (*close) (mysql_session_t *);
	int (*handler) (mysql_session_t *);
	void (*process_authentication_pkt) (mysql_session_t *);
//	private methods
//	void (*read_from_net_2) (mysql_session_t *);
//	void (*write_to_net_2) (mysql_session_t *, int);

};



typedef struct _global_variables {
	pthread_rwlock_t rwlock_global;
	pthread_rwlock_t rwlock_usernames;

	int shutdown;

	unsigned char protocol_version;
	char *mysql_server_version;
	uint16_t server_capabilities;
	uint8_t server_language;
	uint16_t server_status;

	uint32_t	thread_id;


	int merge_configfile_db;

	gint core_dump_file_size;
	int stack_size;
	gint proxy_mysql_port;
	gint proxy_admin_port;
	int backlog;
	int verbose;
	int print_statistics_interval;
	
	gboolean enable_timers;

	int mysql_poll_timeout;
	int mysql_poll_timeout_maintenance;

	int mysql_maintenance_timeout;

	int mysql_threads;	
	gboolean mysql_auto_reconnect_enabled;
	gboolean mysql_query_cache_enabled;
	gboolean mysql_query_cache_precheck;
	int mysql_query_cache_partitions;
	unsigned int mysql_query_cache_default_timeout;
	unsigned long long mysql_wait_timeout;
	unsigned long long mysql_query_cache_size;
	unsigned long long mysql_max_resultset_size;
	int mysql_max_query_size;

	int mysql_hostgroups;

	// this user needs only USAGE grants
	// and it is use only to create a connection
	unsigned char *mysql_usage_user;
	unsigned char *mysql_usage_password;
	
	unsigned char *proxy_admin_user;
	unsigned char *proxy_admin_password;

	unsigned char *mysql_default_schema;
	unsigned char *mysql_socket;

//	unsigned int count_masters;
//	unsigned int count_slaves;
//	GPtrArray *servers_masters;
//	GPtrArray *servers_slaves;
//	gchar **mysql_servers_name;	// used to parse config file
	GHashTable *usernames;
//	gchar **mysql_users_name; // used to parse config file
//	gchar **mysql_users_pass; // used to parse config file
	GPtrArray *mysql_users_name;
	GPtrArray *mysql_users_pass;
//	unsigned int mysql_connections_max;
//	unsigned int mysql_connections_cur;
//	GPtrArray *mysql_connections;
//	unsigned int net_buffer_size;
//	unsigned int conn_queue_allocator_blocks;
//	GPtrArray *conn_queue_allocator;
//	GPtrArray *QC_rules;
//	int QCRver;
} global_variables;


typedef struct _global_mysql_servers {
	pthread_rwlock_t rwlock;
	unsigned int mysql_connections_max;
	unsigned int mysql_connections_cur;
	unsigned int servers_count;
	unsigned int count_masters;
	unsigned int count_slaves;
	gchar **mysql_servers_name;	// used to parse config file
	GPtrArray *servers;
	GPtrArray *servers_masters;
	GPtrArray *servers_slaves;	
	GPtrArray *mysql_connections;
	gboolean mysql_use_masters_for_reads;	
	GPtrArray *mysql_hostgroups;
} global_mysql_servers;


enum MySQL_response_type {
	OK_Packet,
	ERR_Packet,
	EOF_Packet,
	UNKNOWN_Packet,
};


typedef struct _mem_block_t {
	GPtrArray *used;
	GPtrArray *free;
	void *mem;
} mem_block_t;


typedef struct _mem_superblock_t {
	pthread_mutex_t mutex;
	GPtrArray *blocks;
	int size;
	int incremental;
} mem_superblock_t;


/* ProxyIPC is a struct used for inter-thread communication between the admin thread and the the mysql threads
because mysql threads are normally blocked on poll(), the best way to wake them up is to send them a signal on a pipe
fdIn and fdOut represents the two endpoints of the pipe
The data should should be the follow:
- admin thread sends a message in each mysql thread queue
- admin thread sends a byte to all fdIn
- all the mysql threads will wake up reading from fdOut 
- all the mysql threads will read the message from their async queue
- all the mysql threads will perform an action and send an ack to the admin thread
- all the mysql threads may enter in a maintenance mode and just wait on async queue, or go back in the main loop
*/
typedef struct _ProxyIPC {
	int *fdIn;
	int *fdOut;
	GAsyncQueue **queue;
} ProxyIPC;


enum debug_module {
	PROXY_DEBUG_GENERIC,
	PROXY_DEBUG_NET,
	PROXY_DEBUG_PKT_ARRAY,
	PROXY_DEBUG_POLL,
	PROXY_DEBUG_MYSQL_COM,
	PROXY_DEBUG_MYSQL_SERVER,
	PROXY_DEBUG_MYSQL_CONNECTION,
	PROXY_DEBUG_MYSQL_RW_SPLIT,
	PROXY_DEBUG_MYSQL_AUTH,
	PROXY_DEBUG_MEMORY,
	PROXY_DEBUG_ADMIN,
	PROXY_DEBUG_SQLITE,
	PROXY_DEBUG_IPC,
	PROXY_DEBUG_QUERY_CACHE,
	PROXY_DEBUG_UNKNOWN
};


typedef struct _debug_level debug_level;
typedef struct _admin_sqlite_table_def_t admin_sqlite_table_def_t;

typedef struct _global_variable_entry_t global_variable_entry_t;

struct _global_variable_entry_t {
	const char *group_name;
	const char *key_name;
	GOptionArg arg;
	void *arg_data;
	const char *description;
	long long value_min;
	long long value_max;
	long long value_round;
	int value_multiplier;
	long long int_default;
	const char *char_default;
	void (*func_pre)(global_variable_entry_t *);
	void (*func_post)(global_variable_entry_t *);
};


//#define MYSQL_SERVER_STATUS_OFFLINE	0
