

extern belle_sip_stack_t * stack;
extern belle_sip_provider_t *prov;
extern const char *test_domain;
int call_endeed;
extern int register_init(void);
extern int register_uninit(void);
extern belle_sip_request_t* register_user(belle_sip_stack_t * stack
		,belle_sip_provider_t *prov
		,const char *transport
		,int use_transaction
		,const char* username,const char* outbound) ;
extern void unregister_user(belle_sip_stack_t * stack
					,belle_sip_provider_t *prov
					,belle_sip_request_t* initial_request
					,int use_transaction);

