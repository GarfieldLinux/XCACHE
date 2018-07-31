/******************************************************************************
*
* Copyright (C) Chaoyong Zhou
* Email: bgnvendor@163.com
* QQ: 2796796
*
*******************************************************************************/
#ifdef __cplusplus
extern "C"{
#endif/*__cplusplus*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "type.h"
#include "mm.h"
#include "log.h"
#include "cstring.h"

#include "carray.h"
#include "cvector.h"

#include "cbc.h"
#include "ctimer.h"
#include "cbtimer.h"
#include "cmisc.h"

#include "task.h"

#include "csocket.h"

#include "cmpie.h"

#include "crb.h"
#include "chttp.h"
#include "crfshttp.h"
#include "crfsmon.h"
#include "crfsconhash.h"

#include "cload.h"

#include "findex.inc"

static UINT32   g_crfsmon_rfs_node_pos = 0;

#define CRFSMON_MD_CAPACITY()                  (cbc_md_capacity(MD_CRFSMON))

#define CRFSMON_MD_GET(crfsmon_md_id)     ((CRFSMON_MD *)cbc_md_get(MD_CRFSMON, (crfsmon_md_id)))

#define CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id)  \
    ((CMPI_ANY_MODI != (crfsmon_md_id)) && ((NULL_PTR == CRFSMON_MD_GET(crfsmon_md_id)) || (0 == (CRFSMON_MD_GET(crfsmon_md_id)->usedcounter))))

/**
*   for test only
*
*   to query the status of CRFSMON Module
*
**/
void crfsmon_print_module_status(const UINT32 crfsmon_md_id, LOG *log)
{
    CRFSMON_MD *crfsmon_md;
    UINT32 this_crfsmon_md_id;

    for( this_crfsmon_md_id = 0; this_crfsmon_md_id < CRFSMON_MD_CAPACITY(); this_crfsmon_md_id ++ )
    {
        crfsmon_md = CRFSMON_MD_GET(this_crfsmon_md_id);

        if ( NULL_PTR != crfsmon_md && 0 < crfsmon_md->usedcounter )
        {
            sys_log(log,"CRFSMON Module # %ld : %ld refered\n",
                    this_crfsmon_md_id,
                    crfsmon_md->usedcounter);
        }
    }

    return ;
}

/**
*
*   free all static memory occupied by the appointed CRFSMON module
*
*
**/
UINT32 crfsmon_free_module_static_mem(const UINT32 crfsmon_md_id)
{
    //CRFSMON_MD  *crfsmon_md;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_free_module_static_mem: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        /*note: here do not exit but return only*/
        return ((UINT32)-1);
    }
#endif/*CRFSMON_DEBUG_SWITCH*/

    //crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    free_module_static_mem(MD_CRFSMON, crfsmon_md_id);

    return 0;
}

/**
*
* start CRFSMON module
*
**/
UINT32 crfsmon_start()
{
    CRFSMON_MD *crfsmon_md;
    UINT32      crfsmon_md_id;

    TASK_BRD   *task_brd;

    task_brd = task_brd_default_get();

    cbc_md_reg(MD_CRFSMON , 32);

    crfsmon_md_id = cbc_md_new(MD_CRFSMON, sizeof(CRFSMON_MD));
    if(CMPI_ERROR_MODI == crfsmon_md_id)
    {
        return (CMPI_ERROR_MODI);
    }

    /* initialize new one CRFSMON module */
    crfsmon_md = (CRFSMON_MD *)cbc_md_get(MD_CRFSMON, crfsmon_md_id);
    crfsmon_md->usedcounter   = 0;

    /* create a new module node */
    init_static_mem();

    /*initialize CRFS_NODE vector*/
    cvector_init(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), 16, MM_CRFS_NODE, CVECTOR_LOCK_DISABLE, LOC_CRFSMON_0001);

    if(SWITCH_ON == CRFSMON_CONHASH_SWITCH)
    {
        CRFSMON_MD_CRFSCONHASH(crfsmon_md) = crfsconhash_new(CRFSMON_CONHASH_DEFAULT_HASH_ALGO);
    }
    else
    {
        CRFSMON_MD_CRFSCONHASH(crfsmon_md) = NULL_PTR;
    }

    CRFSMON_MD_HOT_PATH_HASH_FUNC(crfsmon_md) = chash_algo_fetch(CRFSMON_HOT_PATH_HASH_ALGO);

    /*initialize HOT PATH RB TREE*/
    crb_tree_init(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md),
                    (CRB_DATA_CMP)crfs_hot_path_cmp,
                    (CRB_DATA_FREE)crfs_hot_path_free,
                    (CRB_DATA_PRINT)crfs_hot_path_print);

    crfsmon_md->usedcounter = 1;

    tasks_cfg_push_add_worker_callback(TASK_BRD_LOCAL_TASKS_CFG(task_brd),
                                       (const char *)"crfsmon_callback_when_add",
                                       crfsmon_md_id,
                                       (UINT32)crfsmon_callback_when_add);

    tasks_cfg_push_del_worker_callback(TASK_BRD_LOCAL_TASKS_CFG(task_brd),
                                       (const char *)"crfsmon_callback_when_del",
                                       crfsmon_md_id,
                                       (UINT32)crfsmon_callback_when_del);

    csig_atexit_register((CSIG_ATEXIT_HANDLER)crfsmon_end, crfsmon_md_id);

    dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "[DEBUG] crfsmon_start: start CRFSMON module #%ld\n", crfsmon_md_id);

    if(SWITCH_ON == CRFSMONHTTP_SWITCH && CMPI_FWD_RANK == CMPI_LOCAL_RANK)
    {
        /*http server*/
        if(EC_TRUE == task_brd_default_check_csrv_enabled() && 0 == crfsmon_md_id)
        {
            if(EC_FALSE == chttp_defer_request_queue_init())
            {
                dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_start: init crfshttp defer request queue failed\n");
                crfsmon_end(crfsmon_md_id);
                return (CMPI_ERROR_MODI);
            }

            crfshttp_log_start();
            task_brd_default_bind_http_srv_modi(crfsmon_md_id);
            /*reuse RFS HTTP*/
            chttp_rest_list_push((const char *)CRFSHTTP_REST_API_NAME, crfshttp_commit_request);
        }
    }
    return ( crfsmon_md_id );
}

/**
*
* end CRFSMON module
*
**/
void crfsmon_end(const UINT32 crfsmon_md_id)
{
    CRFSMON_MD *crfsmon_md;

    TASK_BRD   *task_brd;

    task_brd = task_brd_default_get();

    csig_atexit_unregister((CSIG_ATEXIT_HANDLER)crfsmon_end, crfsmon_md_id);

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);
    if(NULL_PTR == crfsmon_md)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_end: crfsmon_md_id = %ld not exist.\n", crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }

    /* if the module is occupied by others,then decrease counter only */
    if ( 1 < crfsmon_md->usedcounter )
    {
        crfsmon_md->usedcounter --;
        return ;
    }

    if ( 0 == crfsmon_md->usedcounter )
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_end: crfsmon_md_id = %ld is not started.\n", crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }

    cvector_clean(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (CVECTOR_DATA_CLEANER)crfs_node_free, LOC_CRFSMON_0002);
    if(NULL_PTR != CRFSMON_MD_CRFSCONHASH(crfsmon_md))
    {
        crfsconhash_free(CRFSMON_MD_CRFSCONHASH(crfsmon_md));
        CRFSMON_MD_CRFSCONHASH(crfsmon_md) = NULL_PTR;
    }

    CRFSMON_MD_HOT_PATH_HASH_FUNC(crfsmon_md) = NULL_PTR;
    crb_tree_clean(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md));

    tasks_cfg_erase_add_worker_callback(TASK_BRD_LOCAL_TASKS_CFG(task_brd),
                                      (const char *)"crfsmon_callback_when_add",
                                      crfsmon_md_id,
                                      (UINT32)crfsmon_callback_when_add);

    tasks_cfg_erase_del_worker_callback(TASK_BRD_LOCAL_TASKS_CFG(task_brd),
                                      (const char *)"crfsmon_callback_when_del",
                                      crfsmon_md_id,
                                      (UINT32)crfsmon_callback_when_del);

    /* free module : */

    crfsmon_md->usedcounter = 0;

    dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "crfsmon_end: stop CRFSMON module #%ld\n", crfsmon_md_id);
    cbc_md_free(MD_CRFSMON, crfsmon_md_id);

    return ;
}

CRFS_NODE *crfs_node_new()
{
    CRFS_NODE *crfs_node;
    alloc_static_mem(MM_CRFS_NODE, &crfs_node, LOC_CRFSMON_0003);
    if(NULL_PTR != crfs_node)
    {
        crfs_node_init(crfs_node);
    }
    return (crfs_node);
}

EC_BOOL crfs_node_init(CRFS_NODE *crfs_node)
{
    if(NULL_PTR != crfs_node)
    {
        CRFS_NODE_TCID(crfs_node)   = CMPI_ERROR_TCID;
        CRFS_NODE_IPADDR(crfs_node) = CMPI_ERROR_IPADDR;
        CRFS_NODE_PORT(crfs_node)   = CMPI_ERROR_SRVPORT;
        CRFS_NODE_MODI(crfs_node)   = CMPI_ERROR_MODI;
        CRFS_NODE_STATE(crfs_node)  = CRFS_NODE_IS_ERR;

    }
    return (EC_TRUE);
}

EC_BOOL crfs_node_clean(CRFS_NODE *crfs_node)
{
    if(NULL_PTR != crfs_node)
    {
        CRFS_NODE_TCID(crfs_node)   = CMPI_ERROR_TCID;
        CRFS_NODE_IPADDR(crfs_node) = CMPI_ERROR_IPADDR;
        CRFS_NODE_PORT(crfs_node)   = CMPI_ERROR_SRVPORT;
        CRFS_NODE_MODI(crfs_node)   = CMPI_ERROR_MODI;
        CRFS_NODE_STATE(crfs_node)  = CRFS_NODE_IS_ERR;

    }
    return (EC_TRUE);
}

EC_BOOL crfs_node_free(CRFS_NODE *crfs_node)
{
    if(NULL_PTR != crfs_node)
    {
        crfs_node_clean(crfs_node);
        free_static_mem(MM_CRFS_NODE, crfs_node, LOC_CRFSMON_0004);
    }

    return (EC_TRUE);
}

EC_BOOL crfs_node_clone(const CRFS_NODE *crfs_node_src, CRFS_NODE *crfs_node_des)
{
    if(NULL_PTR != crfs_node_src && NULL_PTR != crfs_node_des)
    {
        CRFS_NODE_TCID(crfs_node_des)   = CRFS_NODE_TCID(crfs_node_src);
        CRFS_NODE_IPADDR(crfs_node_des) = CRFS_NODE_IPADDR(crfs_node_src);
        CRFS_NODE_PORT(crfs_node_des)   = CRFS_NODE_PORT(crfs_node_src);
        CRFS_NODE_MODI(crfs_node_des)   = CRFS_NODE_MODI(crfs_node_src);
        CRFS_NODE_STATE(crfs_node_des)  = CRFS_NODE_STATE(crfs_node_src);
    }

    return (EC_TRUE);
}

EC_BOOL crfs_node_is_up(const CRFS_NODE *crfs_node)
{
    if(CRFS_NODE_IS_UP == CRFS_NODE_STATE(crfs_node))
    {
        return (EC_TRUE);
    }

    return (EC_FALSE);
}

EC_BOOL crfs_node_is_valid(const CRFS_NODE *crfs_node)
{
    if(CMPI_ERROR_TCID == CRFS_NODE_TCID(crfs_node))
    {
        return (EC_FALSE);
    }

    if(CMPI_ERROR_IPADDR == CRFS_NODE_IPADDR(crfs_node))
    {
        return (EC_FALSE);
    }

    if(CMPI_ERROR_SRVPORT == CRFS_NODE_PORT(crfs_node))
    {
        return (EC_FALSE);
    }

    if(CMPI_ERROR_MODI == CRFS_NODE_MODI(crfs_node))
    {
        return (EC_FALSE);
    }

    if(CRFS_NODE_IS_ERR == CRFS_NODE_STATE(crfs_node))
    {
        return (EC_FALSE);
    }
    return (EC_TRUE);
}

int crfs_node_cmp(const CRFS_NODE *crfs_node_1st, const CRFS_NODE *crfs_node_2nd)
{
    if(CRFS_NODE_TCID(crfs_node_1st) > CRFS_NODE_TCID(crfs_node_2nd))
    {
        return (1);
    }

    if(CRFS_NODE_TCID(crfs_node_1st) < CRFS_NODE_TCID(crfs_node_2nd))
    {
        return (-1);
    }

    if(CRFS_NODE_MODI(crfs_node_1st) > CRFS_NODE_MODI(crfs_node_2nd))
    {
        return (1);
    }

    if(CRFS_NODE_MODI(crfs_node_1st) < CRFS_NODE_MODI(crfs_node_2nd))
    {
        return (-1);
    }

    return (0);
}

const char *crfs_node_state(const CRFS_NODE *crfs_node)
{
    if(CRFS_NODE_IS_UP == CRFS_NODE_STATE(crfs_node))
    {
        return (const char *)"UP";
    }
    if(CRFS_NODE_IS_DOWN == CRFS_NODE_STATE(crfs_node))
    {
        return (const char *)"DOWN";
    }

    if(CRFS_NODE_IS_ERR == CRFS_NODE_STATE(crfs_node))
    {
        return (const char *)"ERR";
    }

    return (const char *)"UNKOWN";
}

void crfs_node_print(const CRFS_NODE *crfs_node, LOG *log)
{
    sys_log(log, "crfs_node_print: crfs_node %p: tcid %s, srv %s:%ld, modi %ld, state %s\n", crfs_node,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );
    return;
}

void crfsmon_crfs_node_print(const UINT32 crfsmon_md_id, LOG *log)
{
    CRFSMON_MD *crfsmon_md;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_print: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    cvector_print(log, CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (CVECTOR_DATA_PRINT)crfs_node_print);

    return;
}

void crfsmon_crfs_node_list(const UINT32 crfsmon_md_id, CSTRING *cstr)
{
    CRFSMON_MD *crfsmon_md;
    UINT32      pos;
    UINT32      num;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_list: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    num = cvector_size(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md));
    for(pos = 0; pos < num; pos ++)
    {
        CRFS_NODE *crfs_node;
        crfs_node = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
        if(NULL_PTR == crfs_node)
        {
            cstring_format(cstr, "[%ld/%ld] (null)\n", pos, num);
            continue;
        }

        cstring_format(cstr,
                    "[%ld/%ld] (tcid %s, srv %s:%ld, modi %ld, state %s)\n", pos, num,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );

        dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_node_list: [%ld] cstr:\n%.*s\n", pos,
                    (uint32_t)CSTRING_LEN(cstr), (char *)CSTRING_STR(cstr));

    }

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_node_list: list result:\n%.*s\n",
                    (uint32_t)CSTRING_LEN(cstr), (char *)CSTRING_STR(cstr));
    return;
}

EC_BOOL crfsmon_crfs_node_num(const UINT32 crfsmon_md_id, UINT32 *num)
{
    CRFSMON_MD *crfsmon_md;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_num: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    (*num) = cvector_size(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md));
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_add(const UINT32 crfsmon_md_id, const CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_t;
    UINT32      pos;

    TASK_BRD   *task_brd;
    TASKS_CFG  *tasks_cfg;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_add: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    /*check validity*/
    if(CMPI_ERROR_TCID == CRFS_NODE_TCID(crfs_node) || CMPI_ERROR_MODI == CRFS_NODE_MODI(crfs_node))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "warn:crfsmon_crfs_node_add: crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) is invalid\n",
                    crfs_node,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );
        return (EC_FALSE);
    }

    pos = cvector_search_front(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (const void *)crfs_node, (CVECTOR_DATA_CMP)crfs_node_cmp);
    if(CVECTOR_ERR_POS != pos)/*found duplicate*/
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "warn:crfsmon_crfs_node_add: found duplicate crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                    crfs_node,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );
        return (EC_TRUE);
    }

    task_brd = task_brd_default_get();

    tasks_cfg = sys_cfg_search_tasks_cfg(TASK_BRD_SYS_CFG(task_brd), CRFS_NODE_TCID(crfs_node), CMPI_ANY_MASK, CMPI_ANY_MASK);
    if(NULL_PTR == tasks_cfg)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_node_add: not searched tasks cfg of tcid %s\n",
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)));
        return (EC_FALSE);
    }

    crfs_node_t = crfs_node_new();
    if(NULL_PTR == crfs_node_t)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                "error:crfsmon_crfs_node_add: new crfs_node failed before insert crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                crfs_node,
                c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                CRFS_NODE_MODI(crfs_node),
                crfs_node_state(crfs_node)
                );

        return (EC_FALSE);
    }

    crfs_node_clone(crfs_node, crfs_node_t);

    CRFS_NODE_IPADDR(crfs_node_t) = TASKS_CFG_SRVIPADDR(tasks_cfg);
    CRFS_NODE_PORT(crfs_node_t)   = TASKS_CFG_CSRVPORT(tasks_cfg); /*http port*/

    CRFS_NODE_STATE(crfs_node_t)  = CRFS_NODE_IS_UP;/*when initialization*/

    cvector_push(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (const void *)crfs_node_t);

    if(NULL_PTR != CRFSMON_MD_CRFSCONHASH(crfsmon_md))
    {
        if(EC_FALSE ==crfsconhash_add_node(CRFSMON_MD_CRFSCONHASH(crfsmon_md),
                                            (uint32_t)CRFS_NODE_TCID(crfs_node_t),
                                            CRFSMON_CONHASH_REPLICAS))
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                            "error:crfsmon_crfs_node_add: add crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) to connhash failed\n",
                            crfs_node_t,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                            CRFS_NODE_MODI(crfs_node_t),
                            crfs_node_state(crfs_node_t)
                            );

            cvector_pop(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md));
            crfs_node_free(crfs_node_t);
            return (EC_FALSE);
        }

        dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                        "[DEBUG] crfsmon_crfs_node_add: add crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) to connhash done\n",
                        crfs_node_t,
                        c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                        c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                        CRFS_NODE_MODI(crfs_node_t),
                        crfs_node_state(crfs_node_t)
                        );
    }

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                    "[DEBUG] crfsmon_crfs_node_add: add crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) done\n",
                    crfs_node_t,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                    CRFS_NODE_MODI(crfs_node_t),
                    crfs_node_state(crfs_node_t)
                    );
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_del(const UINT32 crfsmon_md_id, const CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_t;
    UINT32      pos;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_del: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    pos = cvector_search_front(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (const void *)crfs_node, (CVECTOR_DATA_CMP)crfs_node_cmp);
    if(CVECTOR_ERR_POS == pos)
    {
        dbg_log(SEC_0155_CRFSMON, 1)(LOGSTDOUT,
                    "warn:crfsmon_crfs_node_del: not found crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );
        return (EC_TRUE);
    }

    crfs_node_t = cvector_erase(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
    if(NULL_PTR == crfs_node_t)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "warn:crfsmon_crfs_node_del: erase crfs_node is null\n");
        return (EC_TRUE);
    }

    if(NULL_PTR != CRFSMON_MD_CRFSCONHASH(crfsmon_md))
    {
        if(EC_FALSE ==crfsconhash_del_node(CRFSMON_MD_CRFSCONHASH(crfsmon_md),
                                            (uint32_t)CRFS_NODE_TCID(crfs_node_t))
        )
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                            "error:crfsmon_crfs_node_del: del crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) from connhash failed\n",
                            crfs_node_t,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                            CRFS_NODE_MODI(crfs_node_t),
                            crfs_node_state(crfs_node_t)
                            );
        }
        else
        {
            dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                            "[DEBUG] crfsmon_crfs_node_del: del crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) from connhash done\n",
                            crfs_node_t,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                            CRFS_NODE_MODI(crfs_node_t),
                            crfs_node_state(crfs_node_t)
                            );
        }
    }

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                    "[DEBUG] crfsmon_crfs_node_del: erase crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) done\n",
                    crfs_node_t,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                    CRFS_NODE_MODI(crfs_node_t),
                    crfs_node_state(crfs_node_t)
                    );

    crfs_node_free(crfs_node_t);
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_set_up(const UINT32 crfsmon_md_id, const CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_t;
    UINT32      pos;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_set_up: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    pos = cvector_search_front(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (const void *)crfs_node, (CVECTOR_DATA_CMP)crfs_node_cmp);
    if(CVECTOR_ERR_POS == pos)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_set_up: not found crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );
        return (EC_FALSE);
    }

    crfs_node_t = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
    if(NULL_PTR == crfs_node_t)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_set_up: found crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s) at pos %ld but it is null\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node),
                    pos
                    );
        return (EC_FALSE);
    }

    if(NULL_PTR != CRFSMON_MD_CRFSCONHASH(crfsmon_md))
    {
        if(EC_FALSE ==crfsconhash_up_node(CRFSMON_MD_CRFSCONHASH(crfsmon_md),
                                            (uint32_t)CRFS_NODE_TCID(crfs_node_t))
        )
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                            "error:crfsmon_crfs_node_set_up: set up crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) in connhash failed\n",
                            crfs_node_t,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                            CRFS_NODE_MODI(crfs_node_t),
                            crfs_node_state(crfs_node_t)
                            );
            return (EC_FALSE);
        }
        else
        {
            dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                            "[DEBUG] crfsmon_crfs_node_set_up: set up crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) in connhash done\n",
                            crfs_node_t,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                            CRFS_NODE_MODI(crfs_node_t),
                            crfs_node_state(crfs_node_t)
                            );
        }
    }

    CRFS_NODE_STATE(crfs_node_t) = CRFS_NODE_IS_UP; /*set up*/

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                    "[DEBUG] crfsmon_crfs_node_set_up: set up crfs_node_t %p (tcid %s, srv %s:%ld, modi %ld, state %s) done\n",
                    crfs_node_t,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                    CRFS_NODE_MODI(crfs_node_t),
                    crfs_node_state(crfs_node_t)
                    );
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_set_down(const UINT32 crfsmon_md_id, const CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_t;
    UINT32      pos;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_set_down: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    pos = cvector_search_front(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (const void *)crfs_node, (CVECTOR_DATA_CMP)crfs_node_cmp);
    if(CVECTOR_ERR_POS == pos)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_set_down: not found crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );
        return (EC_FALSE);
    }

    crfs_node_t = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
    if(NULL_PTR == crfs_node_t)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_set_down: found crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s) at pos %ld but it is null\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node),
                    pos
                    );
        return (EC_FALSE);
    }

    if(NULL_PTR != CRFSMON_MD_CRFSCONHASH(crfsmon_md))
    {
        if(EC_FALSE ==crfsconhash_down_node(CRFSMON_MD_CRFSCONHASH(crfsmon_md),
                                            (uint32_t)CRFS_NODE_TCID(crfs_node_t))
        )
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                            "error:crfsmon_crfs_node_set_down: set down crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) in connhash failed\n",
                            crfs_node_t,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                            CRFS_NODE_MODI(crfs_node_t),
                            crfs_node_state(crfs_node_t)
                            );
            return (EC_FALSE);
        }
        else
        {
            dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                            "[DEBUG] crfsmon_crfs_node_set_down: set down crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) in connhash done\n",
                            crfs_node_t,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                            CRFS_NODE_MODI(crfs_node_t),
                            crfs_node_state(crfs_node_t)
                            );
        }
    }

    CRFS_NODE_STATE(crfs_node_t) = CRFS_NODE_IS_DOWN; /*set down*/

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                    "[DEBUG] crfsmon_crfs_node_set_down: set down crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s) done\n",
                    crfs_node_t,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                    CRFS_NODE_MODI(crfs_node_t),
                    crfs_node_state(crfs_node_t)
                    );
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_is_up(const UINT32 crfsmon_md_id, const CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_t;
    UINT32      pos;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_is_up: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    pos = cvector_search_front(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (const void *)crfs_node, (CVECTOR_DATA_CMP)crfs_node_cmp);
    if(CVECTOR_ERR_POS == pos)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_is_up: not found crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node)
                    );
        return (EC_FALSE);
    }

    crfs_node_t = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
    if(NULL_PTR == crfs_node_t)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_is_up: found crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s) at pos %ld but it is null\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                    CRFS_NODE_MODI(crfs_node),
                    crfs_node_state(crfs_node),
                    pos
                    );
        return (EC_FALSE);
    }

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                    "[DEBUG] crfsmon_crfs_node_is_up: check crfs_node_t %p (tcid %s, srv %s:%ld, modi %ld, state %s) done\n",
                    crfs_node_t,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                    CRFS_NODE_MODI(crfs_node_t),
                    crfs_node_state(crfs_node_t)
                    );

    if(CRFS_NODE_IS_UP == CRFS_NODE_STATE(crfs_node_t))
    {
        return (EC_TRUE);
    }

    return (EC_FALSE);
}

EC_BOOL crfsmon_crfs_node_get_by_pos(const UINT32 crfsmon_md_id, const UINT32 pos, CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_t;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_get_by_pos: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    if(CVECTOR_ERR_POS == pos)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_get_by_pos: pos is error\n");
        return (EC_FALSE);
    }

    crfs_node_t = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
    if(NULL_PTR == crfs_node_t)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_get_by_pos: found crfs_node at pos %ld but it is null\n", pos);
        return (EC_FALSE);
    }

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                    "[DEBUG] crfsmon_crfs_node_get_by_pos: found crfs_node_t %p (tcid %s, srv %s:%ld, modi %ld, state %s) at pos %ld\n",
                    crfs_node_t,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                    CRFS_NODE_MODI(crfs_node_t),
                    crfs_node_state(crfs_node_t),
                    pos
                    );

    crfs_node_clone(crfs_node_t, crfs_node);
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_get_by_tcid(const UINT32 crfsmon_md_id, const UINT32 tcid, const UINT32 modi, CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_searched;
    CRFS_NODE   crfs_node_t;
    UINT32      pos;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_get_by_tcid: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    CRFS_NODE_TCID(&crfs_node_t) = tcid;
    CRFS_NODE_MODI(&crfs_node_t) = modi;

    pos = cvector_search_front(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), (const void *)&crfs_node_t, (CVECTOR_DATA_CMP)crfs_node_cmp);
    if(CVECTOR_ERR_POS == pos)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_get_by_tcid: not found crfs_node with (tcid %s, modi %ld)\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    CRFS_NODE_MODI(crfs_node)
                    );
        return (EC_FALSE);
    }

    crfs_node_searched = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
    if(NULL_PTR == crfs_node_searched)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                    "error:crfsmon_crfs_node_get_by_tcid: found crfs_node with (tcid %s, modi %ld) at pos %ld but it is null\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                    CRFS_NODE_MODI(crfs_node),
                    pos
                    );
        return (EC_FALSE);
    }

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                    "[DEBUG] crfsmon_crfs_node_get_by_tcid: found crfs_node_t %p (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                    crfs_node_searched,
                    c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_searched)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_searched)), CRFS_NODE_PORT(crfs_node_searched),
                    CRFS_NODE_MODI(crfs_node_searched),
                    crfs_node_state(crfs_node_searched)
                    );

    crfs_node_clone(crfs_node_searched, crfs_node);

    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_get_by_hash(const UINT32 crfsmon_md_id, const UINT32 hash, CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;
    CRFS_NODE  *crfs_node_t;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_node_get_by_hash: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    if(NULL_PTR != CRFSMON_MD_CRFSCONHASH(crfsmon_md))
    {
        CRFSCONHASH_RNODE *crfsconhash_rnode;

        crfsconhash_rnode = crfsconhash_lookup_rnode(CRFSMON_MD_CRFSCONHASH(crfsmon_md), hash);
        if(NULL_PTR == crfsconhash_rnode)
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                        "error:crfsmon_crfs_node_get_by_hash: lookup rnode in connhash failed where hash %ld\n", hash);
            return (EC_FALSE);
        }

        if(EC_FALSE == crfsconhash_rnode_is_up(crfsconhash_rnode))
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                        "error:crfsmon_crfs_node_get_by_hash: found rnode (tcid %s, replicas %u, status %s) in connhash where hash %ld but it is not up \n",
                        c_word_to_ipv4(CRFSCONHASH_RNODE_TCID(crfsconhash_rnode)),
                        CRFSCONHASH_RNODE_REPLICAS(crfsconhash_rnode),
                        crfsconhash_rnode_status(crfsconhash_rnode),
                        hash);
            return (EC_FALSE);
        }

        return crfsmon_crfs_node_get_by_tcid(crfsmon_md_id, CRFSCONHASH_RNODE_TCID(crfsconhash_rnode), 0, crfs_node);
    }
    else
    {
        UINT32      num;
        UINT32      pos;

        num  = cvector_size(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md));

        pos  = (hash % num);

        crfs_node_t = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);
        if(NULL_PTR == crfs_node_t)
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT,
                        "error:crfsmon_crfs_node_get_by_hash: found crfs_node at pos %ld but it is null where hash %ld\n", pos, hash);
            return (EC_FALSE);
        }

        dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                        "[DEBUG] crfsmon_crfs_node_get_by_hash: found crfs_node_t %p (tcid %s, srv %s:%ld, modi %ld, state %s) at pos %ld where hash %ld\n",
                        crfs_node_t,
                        c_word_to_ipv4(CRFS_NODE_TCID(crfs_node_t)),
                        c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node_t)), CRFS_NODE_PORT(crfs_node_t),
                        CRFS_NODE_MODI(crfs_node_t),
                        crfs_node_state(crfs_node_t),
                        pos, hash
                        );

        crfs_node_clone(crfs_node_t, crfs_node);
    }
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_get_by_path(const UINT32 crfsmon_md_id, const uint8_t *path, const uint32_t path_len, CRFS_NODE *crfs_node)
{
    UINT32      hash;

    hash = c_crc32_short((uint8_t *)path, path_len);

    return crfsmon_crfs_node_get_by_hash(crfsmon_md_id, hash, crfs_node);
}

EC_BOOL crfsmon_crfs_node_set_start_pos(const UINT32 crfsmon_md_id, const UINT32 start_pos)
{
    g_crfsmon_rfs_node_pos = start_pos;
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_node_search_up(const UINT32 crfsmon_md_id, CRFS_NODE *crfs_node)
{
    CRFSMON_MD *crfsmon_md;

    UINT32      crfs_node_num;
    UINT32      crfs_node_pos;

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    crfs_node_num = cvector_size(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md));
    if(0 == crfs_node_num)
    {
        return (EC_FALSE);
    }

    g_crfsmon_rfs_node_pos = (g_crfsmon_rfs_node_pos + 1) % crfs_node_num;

    for(crfs_node_pos = g_crfsmon_rfs_node_pos; crfs_node_pos < crfs_node_num; crfs_node_pos ++)
    {
        CRFS_NODE    *crfs_node_t;

        crfs_node_t = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), crfs_node_pos);

        if(NULL_PTR != crfs_node_t
        && EC_TRUE == crfs_node_is_up(crfs_node_t))
        {
            crfs_node_clone(crfs_node_t, crfs_node);
            return (EC_TRUE);
        }
    }

    return (EC_FALSE);
}

EC_BOOL crfsmon_crfs_store_http_srv_get_hot(const UINT32 crfsmon_md_id, const CSTRING *path, UINT32 *tcid, UINT32 *srv_ipaddr, UINT32 *srv_port)
{
    //CRFSMON_MD *crfsmon_md;

    CRFS_NODE   crfs_node;

    TASK_BRD   *task_brd;
    TASKS_CFG  *tasks_cfg;

    char       *dirname;

    CSTRING     cache_path;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_store_http_srv_get_hot: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    //crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    /*hot cache path*/
    dirname = c_dirname((char *)cstring_get_str(path));
    if(NULL_PTR == dirname)
    {
        return (EC_FALSE);
    }

    cstring_set_str(&cache_path, (const UINT8 *)dirname);/*mount only*/

    if(EC_FALSE == crfsmon_crfs_hot_path_exist(crfsmon_md_id, &cache_path))
    {
        safe_free(dirname, LOC_CRFSMON_0005);
        return (EC_FALSE);
    }

    if(EC_FALSE == crfsmon_crfs_node_search_up(crfsmon_md_id, &crfs_node))
    {
        safe_free(dirname, LOC_CRFSMON_0006);
        return (EC_FALSE);
    }

    dbg_log(SEC_0155_CRFSMON, 6)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_store_http_srv_get_hot: "
                "hot path '%s' => crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                dirname,
                c_word_to_ipv4(CRFS_NODE_TCID(&crfs_node)),
                c_word_to_ipv4(CRFS_NODE_IPADDR(&crfs_node)), CRFS_NODE_PORT(&crfs_node),
                CRFS_NODE_MODI(&crfs_node),
                crfs_node_state(&crfs_node));

    safe_free(dirname, LOC_CRFSMON_0007);

    task_brd = task_brd_default_get();

    tasks_cfg = sys_cfg_search_tasks_cfg(TASK_BRD_SYS_CFG(task_brd), CRFS_NODE_TCID(&crfs_node), CMPI_ANY_MASK, CMPI_ANY_MASK);
    if(NULL_PTR == tasks_cfg)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_store_http_srv_get: not searched tasks cfg of tcid %s\n",
                            c_word_to_ipv4(CRFS_NODE_TCID(&crfs_node)));
        return (EC_FALSE);
    }

    if(NULL_PTR != tcid)
    {
        (*tcid) = TASKS_CFG_TCID(tasks_cfg);
    }

    if(NULL_PTR != srv_ipaddr)
    {
        (*srv_ipaddr) = TASKS_CFG_SRVIPADDR(tasks_cfg);
    }

    if(NULL_PTR != srv_port)
    {
        (*srv_port) = TASKS_CFG_CSRVPORT(tasks_cfg); /*http port*/
    }

    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_store_http_srv_get(const UINT32 crfsmon_md_id, const CSTRING *path, UINT32 *tcid, UINT32 *srv_ipaddr, UINT32 *srv_port)
{
    //CRFSMON_MD *crfsmon_md;

    CRFS_NODE   crfs_node;
    UINT32      hash;

    TASK_BRD   *task_brd;
    TASKS_CFG  *tasks_cfg;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_store_http_srv_get: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    //crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    /*hot cache path*/
    if(CRFSMON_HOT_PATH_SWITCH == SWITCH_ON
    && EC_TRUE == crfsmon_crfs_store_http_srv_get_hot(crfsmon_md_id, path, tcid, srv_ipaddr, srv_port))
    {
        return (EC_TRUE);
    }

    /*not hot cache path*/
    hash = c_crc32_short(CSTRING_STR(path), (size_t)CSTRING_LEN(path));

    crfs_node_init(&crfs_node);
    if(EC_FALSE == crfsmon_crfs_node_get_by_hash(crfsmon_md_id, hash, &crfs_node))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_store_http_srv_get: get crfs_node with crfsmon_md_id %ld and hash %ld failed\n",
                    crfsmon_md_id, hash);

        crfs_node_clean(&crfs_node);
        return (EC_FALSE);
    }

    if(EC_FALSE == crfs_node_is_up(&crfs_node))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_store_http_srv_get: crfs_node (tcid %s, srv %s:%ld, modi %ld, state %s) is not up\n",
                    c_word_to_ipv4(CRFS_NODE_TCID(&crfs_node)),
                    c_word_to_ipv4(CRFS_NODE_IPADDR(&crfs_node)), CRFS_NODE_PORT(&crfs_node),
                    CRFS_NODE_MODI(&crfs_node),
                    crfs_node_state(&crfs_node));

        crfs_node_clean(&crfs_node);
        return (EC_FALSE);
    }

    task_brd = task_brd_default_get();

    tasks_cfg = sys_cfg_search_tasks_cfg(TASK_BRD_SYS_CFG(task_brd), CRFS_NODE_TCID(&crfs_node), CMPI_ANY_MASK, CMPI_ANY_MASK);
    if(NULL_PTR == tasks_cfg)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_store_http_srv_get: not searched tasks cfg of tcid %s\n",
                            c_word_to_ipv4(CRFS_NODE_TCID(&crfs_node)));
        return (EC_FALSE);
    }

    if(NULL_PTR != tcid)
    {
        (*tcid) = TASKS_CFG_TCID(tasks_cfg);
    }

    if(NULL_PTR != srv_ipaddr)
    {
        (*srv_ipaddr) = TASKS_CFG_SRVIPADDR(tasks_cfg);
    }

    if(NULL_PTR != srv_port)
    {
        (*srv_port) = TASKS_CFG_CSRVPORT(tasks_cfg); /*http port*/
    }
    crfs_node_clean(&crfs_node);

    return (EC_TRUE);
}

/*when add a csocket_cnode (->tasks_node)*/
EC_BOOL crfsmon_callback_when_add(const UINT32 crfsmon_md_id, TASKS_NODE *tasks_node)
{
    CRFSMON_MD *crfsmon_md;

    UINT32      pos;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_callback_when_add: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_callback_when_add: "
                        "tasks_node (tcid %s, srv %s:%ld)\n",
                        c_word_to_ipv4(TASKS_NODE_TCID(tasks_node)),
                        c_word_to_ipv4(TASKS_NODE_SRVIPADDR(tasks_node)), TASKS_NODE_SRVPORT(tasks_node));

    for(pos = 0; pos < cvector_size(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md)); pos ++)
    {
        CRFS_NODE  *crfs_node;

        crfs_node = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);

        if(TASKS_NODE_TCID(tasks_node)      == CRFS_NODE_TCID(crfs_node)
        && TASKS_NODE_SRVIPADDR(tasks_node) == CRFS_NODE_IPADDR(crfs_node)
        && TASKS_NODE_SRVPORT(tasks_node)   == CRFS_NODE_PORT(crfs_node))
        {
            dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT,
                            "[DEBUG] crfsmon_callback_when_add: set up crfs_node_t %p (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                            crfs_node,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                            CRFS_NODE_MODI(crfs_node),
                            crfs_node_state(crfs_node)
                            );
            return crfsmon_crfs_node_set_up(crfsmon_md_id, crfs_node);
        }
    }

    return (EC_TRUE);
}

/*when del a csocket_cnode (->tasks_node)*/
EC_BOOL crfsmon_callback_when_del(const UINT32 crfsmon_md_id, TASKS_NODE *tasks_node)
{
    CRFSMON_MD *crfsmon_md;

    UINT32      pos;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_callback_when_del: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_callback_when_del: "
                        "tasks_node (tcid %s, srv %s:%ld)\n",
                        c_word_to_ipv4(TASKS_NODE_TCID(tasks_node)),
                        c_word_to_ipv4(TASKS_NODE_SRVIPADDR(tasks_node)), TASKS_NODE_SRVPORT(tasks_node));

    if(EC_FALSE == cvector_is_empty(TASKS_NODE_CSOCKET_CNODE_VEC(tasks_node)))
    {
        return (EC_TRUE);
    }

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    for(pos = 0; pos < cvector_size(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md)); pos ++)
    {
        CRFS_NODE  *crfs_node;

        crfs_node = cvector_get(CRFSMON_MD_CRFS_NODE_VEC(crfsmon_md), pos);

        if(TASKS_NODE_TCID(tasks_node)      == CRFS_NODE_TCID(crfs_node)
        && TASKS_NODE_SRVIPADDR(tasks_node) == CRFS_NODE_IPADDR(crfs_node)
        && TASKS_NODE_SRVPORT(tasks_node)   == CRFS_NODE_PORT(crfs_node))
        {
            dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_callback_when_del: "
                            "set down crfs_node %p (tcid %s, srv %s:%ld, modi %ld, state %s)\n",
                            crfs_node,
                            c_word_to_ipv4(CRFS_NODE_TCID(crfs_node)),
                            c_word_to_ipv4(CRFS_NODE_IPADDR(crfs_node)), CRFS_NODE_PORT(crfs_node),
                            CRFS_NODE_MODI(crfs_node),
                            crfs_node_state(crfs_node)
                            );
            return crfsmon_crfs_node_set_down(crfsmon_md_id, crfs_node);
        }
    }

    return (EC_TRUE);
}

CRFS_HOT_PATH *crfs_hot_path_new()
{
    CRFS_HOT_PATH *crfs_hot_path;

    alloc_static_mem(MM_CRFS_HOT_PATH, &crfs_hot_path, LOC_CRFSMON_0008);
    if(NULL_PTR != crfs_hot_path)
    {
        crfs_hot_path_init(crfs_hot_path);
    }
    return (crfs_hot_path);
}

EC_BOOL crfs_hot_path_init(CRFS_HOT_PATH *crfs_hot_path)
{
    if(NULL_PTR != crfs_hot_path)
    {
        CRFS_HOT_PATH_HASH(crfs_hot_path) = 0;

        cstring_init(CRFS_HOT_PATH_CSTR(crfs_hot_path), NULL_PTR);
    }
    return (EC_TRUE);
}

EC_BOOL crfs_hot_path_clean(CRFS_HOT_PATH *crfs_hot_path)
{
    if(NULL_PTR != crfs_hot_path)
    {
        CRFS_HOT_PATH_HASH(crfs_hot_path) = 0;

        cstring_clean(CRFS_HOT_PATH_CSTR(crfs_hot_path));
    }
    return (EC_TRUE);
}

EC_BOOL crfs_hot_path_free(CRFS_HOT_PATH *crfs_hot_path)
{
    if(NULL_PTR != crfs_hot_path)
    {
        crfs_hot_path_clean(crfs_hot_path);

        free_static_mem(MM_CRFS_HOT_PATH, crfs_hot_path, LOC_CRFSMON_0009);
    }

    return (EC_TRUE);
}

EC_BOOL crfs_hot_path_clone(CRFS_HOT_PATH *crfs_hot_path_des, const CRFS_HOT_PATH *crfs_hot_path_src)
{
    if(NULL_PTR != crfs_hot_path_src && NULL_PTR != crfs_hot_path_des)
    {
        CRFS_HOT_PATH_HASH(crfs_hot_path_des) = CRFS_HOT_PATH_HASH(crfs_hot_path_src);

        cstring_clone(CRFS_HOT_PATH_CSTR(crfs_hot_path_src), CRFS_HOT_PATH_CSTR(crfs_hot_path_des));
    }

    return (EC_TRUE);
}

int crfs_hot_path_cmp(const CRFS_HOT_PATH *crfs_hot_path_1st, const CRFS_HOT_PATH *crfs_hot_path_2nd)
{
    if(CRFS_HOT_PATH_HASH(crfs_hot_path_1st) > CRFS_HOT_PATH_HASH(crfs_hot_path_2nd))
    {
        return (1);
    }

    if(CRFS_HOT_PATH_HASH(crfs_hot_path_1st) < CRFS_HOT_PATH_HASH(crfs_hot_path_2nd))
    {
        return (-1);
    }

    return cstring_cmp(CRFS_HOT_PATH_CSTR(crfs_hot_path_1st), CRFS_HOT_PATH_CSTR(crfs_hot_path_2nd));
}

void crfs_hot_path_print(const CRFS_HOT_PATH *crfs_hot_path, LOG *log)
{
    sys_log(log, "crfs_hot_path_print: crfs_hot_path %p: hash %u, str '%s'\n", crfs_hot_path,
                    CRFS_HOT_PATH_HASH(crfs_hot_path),
                    (char *)cstring_get_str(CRFS_HOT_PATH_CSTR(crfs_hot_path))
                    );
    return;
}

EC_BOOL crfsmon_crfs_hot_path_add(const UINT32 crfsmon_md_id, const CSTRING *path)
{
    CRFSMON_MD      *crfsmon_md;

    CRB_NODE        *crb_node;
    CRFS_HOT_PATH   *crfs_hot_path;

    UINT8            path_last_char;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_hot_path_add: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    /*check validity*/
    if(NULL_PTR == path)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_add: "
                                                "path is null\n");
        return (EC_FALSE);
    }

    if(EC_TRUE == cstring_is_empty(path))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_add: "
                                                "path is empty\n");
        return (EC_FALSE);
    }

    if(EC_FALSE == cstring_get_char(path, cstring_get_len(path) - 1, &path_last_char)
    || '/' == (char)path_last_char)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_add: "
                                                "invalid path '%s'\n",
                                                (char *)cstring_get_str(path));
        return (EC_FALSE);
    }

    crfs_hot_path = crfs_hot_path_new();
    if(NULL_PTR == crfs_hot_path)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_add: "
                                                "new crfs_hot_path failed\n");
        return (EC_FALSE);
    }

    /*init*/
    CRFS_HOT_PATH_HASH(crfs_hot_path) = CRFSMON_MD_HOT_PATH_HASH_FUNC(crfsmon_md)(
                                                            cstring_get_len(path),
                                                            cstring_get_str(path));

    cstring_clone(path, CRFS_HOT_PATH_CSTR(crfs_hot_path));

    crb_node = crb_tree_insert_data(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md), (void *)crfs_hot_path);
    if(NULL_PTR == crb_node)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_add: "
                                                "add hot path '%s' failed\n",
                                                (char *)cstring_get_str(path));
        crfs_hot_path_free(crfs_hot_path);
        return (EC_FALSE);
    }

    if(CRB_NODE_DATA(crb_node) != crfs_hot_path)/*found duplicate*/
    {
        dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_add: "
                                                "found duplicated hot path '%s'\n",
                                                (char *)cstring_get_str(path));
        crfs_hot_path_free(crfs_hot_path);
        return (EC_TRUE);
    }

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_add: "
                                            "add hot path '%s' done\n",
                                            (char *)cstring_get_str(path));
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_hot_path_del(const UINT32 crfsmon_md_id, const CSTRING *path)
{
    CRFSMON_MD      *crfsmon_md;

    CRB_NODE        *crb_node_searched;
    CRFS_HOT_PATH    crfs_hot_path_t;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_hot_path_del: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    /*init*/
    CRFS_HOT_PATH_HASH(&crfs_hot_path_t) = CRFSMON_MD_HOT_PATH_HASH_FUNC(crfsmon_md)(
                                                            cstring_get_len(path),
                                                            cstring_get_str(path));

    cstring_set_str(CRFS_HOT_PATH_CSTR(&crfs_hot_path_t), cstring_get_str(path));

    crb_node_searched = crb_tree_search_data(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md), (void *)&crfs_hot_path_t);
    if(NULL_PTR == crb_node_searched)
    {
        dbg_log(SEC_0155_CRFSMON, 5)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_del: "
                                                "not found hot path '%s'\n",
                                                (char *)cstring_get_str(path));
        return (EC_FALSE);
    }

    crb_tree_delete(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md), crb_node_searched);

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_del: "
                                            "del hot path '%s' done\n",
                                            (char *)cstring_get_str(path));
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_hot_path_exist(const UINT32 crfsmon_md_id, const CSTRING *path)
{
    CRFSMON_MD      *crfsmon_md;

    CRB_NODE        *crb_node_searched;
    CRFS_HOT_PATH    crfs_hot_path_t;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_hot_path_exist: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    if(EC_TRUE == crb_tree_is_empty(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md)))
    {
        return (EC_FALSE);
    }

    /*init*/
    CRFS_HOT_PATH_HASH(&crfs_hot_path_t) = CRFSMON_MD_HOT_PATH_HASH_FUNC(crfsmon_md)(
                                                            cstring_get_len(path),
                                                            cstring_get_str(path));

    cstring_set_str(CRFS_HOT_PATH_CSTR(&crfs_hot_path_t), cstring_get_str(path));

    crb_node_searched = crb_tree_search_data(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md), (void *)&crfs_hot_path_t);
    if(NULL_PTR == crb_node_searched)
    {
        dbg_log(SEC_0155_CRFSMON, 5)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_exist: "
                                                "not found hot path '%s'\n",
                                                (char *)cstring_get_str(path));
        return (EC_FALSE);
    }
    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_exist: "
                                            "found hot path '%s'\n",
                                            (char *)cstring_get_str(path));
    return (EC_TRUE);
}

void crfsmon_crfs_hot_path_print(const UINT32 crfsmon_md_id, LOG *log)
{
    CRFSMON_MD *crfsmon_md;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_hot_path_print: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    crb_tree_print(log, CRFSMON_MD_HOT_PATH_TREE(crfsmon_md));

    return;
}

/*format: /<domain>/path */
STATIC_CAST static EC_BOOL __crfsmon_parse_hot_path_line(const UINT32 crfsmon_md_id, char *crfsmon_host_path_start, char *crfsmon_host_path_end)
{
    //CRFSMON_MD          *crfsmon_md;

    char                *p;
    CSTRING              path;

    //crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    /*locate the first char which is not space*/

    for(p = crfsmon_host_path_start;isspace(*p); p ++)
    {
        /*do nothing*/
    }

    if('\0' == (*p))
    {
        dbg_log(SEC_0155_CRFSMON, 6)(LOGSTDOUT, "[DEBUG] __crfsmon_parse_hot_path_line: "
                                                "skip empty line '%.*s'\n",
                                                (uint32_t)(crfsmon_host_path_end - crfsmon_host_path_start),
                                                crfsmon_host_path_start);
        /*skip empty line*/
        return (EC_TRUE);
    }

    if('#' == (*p))
    {
        /*skip commented line*/
        dbg_log(SEC_0155_CRFSMON, 6)(LOGSTDOUT, "[DEBUG] __crfsmon_parse_hot_path_line: "
                                                "skip commented line '%.*s'\n",
                                                (uint32_t)(crfsmon_host_path_end - crfsmon_host_path_start),
                                                crfsmon_host_path_start);
        return (EC_TRUE);
    }

    dbg_log(SEC_0155_CRFSMON, 6)(LOGSTDOUT, "[DEBUG] __crfsmon_parse_hot_path_line: "
                                            "handle line '%.*s'\n",
                                            (uint32_t)(crfsmon_host_path_end - crfsmon_host_path_start),
                                            crfsmon_host_path_start);

    c_str_trim_space(p);
    cstring_set_str(&path, (const UINT8 *)p);

    if(EC_FALSE == crfsmon_crfs_hot_path_add(crfsmon_md_id, &path))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:__crfsmon_parse_hot_path_line: "
                                                "insert '%s' failed\n",
                                                p);
        return (EC_FALSE);
    }

    dbg_log(SEC_0155_CRFSMON, 5)(LOGSTDOUT, "[DEBUG] __crfsmon_parse_hot_path_line: "
                                            "insert '%s' done\n",
                                            p);
    return (EC_TRUE);
}

STATIC_CAST static EC_BOOL __crfsmon_parse_hot_path_file(const UINT32 crfsmon_md_id, char *crfsmon_hot_path_start, char *crfsmon_hot_path_end)
{
    char        *crfsmon_hot_path_line_start;
    uint32_t     crfsmon_hot_path_line_no;

    crfsmon_hot_path_line_start = crfsmon_hot_path_start;
    crfsmon_hot_path_line_no    = 1;

    while(crfsmon_hot_path_line_start < crfsmon_hot_path_end)
    {
        char  *crfsmon_hot_path_line_end;

        crfsmon_hot_path_line_end = crfsmon_hot_path_line_start;

        while(crfsmon_hot_path_line_end < crfsmon_hot_path_end)
        {
            if('\n' == (*crfsmon_hot_path_line_end ++)) /*also works for line-terminator '\r\n'*/
            {
                break;
            }
        }

        if(crfsmon_hot_path_line_end > crfsmon_hot_path_end)
        {
            break;
        }

        *(crfsmon_hot_path_line_end - 1) = '\0'; /*insert string terminator*/

        dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "error:__crfsmon_parse_hot_path_file: "
                                                "to parse line %u# '%.*s' failed\n",
                                                crfsmon_hot_path_line_no,
                                                (uint32_t)(crfsmon_hot_path_line_end - crfsmon_hot_path_line_start),
                                                crfsmon_hot_path_line_start);

        if(EC_FALSE == __crfsmon_parse_hot_path_line(crfsmon_md_id, crfsmon_hot_path_line_start, crfsmon_hot_path_line_end))
        {
            dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:__crfsmon_parse_hot_path_file: "
                                                    "parse line %u# '%.*s' failed\n",
                                                    crfsmon_hot_path_line_no,
                                                    (uint32_t)(crfsmon_hot_path_line_end - crfsmon_hot_path_line_start),
                                                    crfsmon_hot_path_line_start);
            return (EC_FALSE);
        }

        crfsmon_hot_path_line_no ++;

        crfsmon_hot_path_line_start = crfsmon_hot_path_line_end;
    }

    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_hot_path_load(const UINT32 crfsmon_md_id, const CSTRING *path)
{
    //CRFSMON_MD  *crfsmon_md;

    const char  *fname;
    UINT32       fsize;
    UINT32       offset;
    UINT8       *fcontent;
    int          fd;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_hot_path_load: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    //crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    if(EC_TRUE == cstring_is_empty(path))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "path is empty\n");
        return (EC_FALSE);
    }

    fname = (char *)cstring_get_str(path);

    if(EC_FALSE == c_file_exist(fname))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "file '%s' not exist\n",
                                                fname);
        return (EC_FALSE);
    }

    dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_load: "
                                            "file '%s' exist\n",
                                            fname);

    if(EC_FALSE == c_file_access(fname, F_OK | R_OK))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "access file '%s' failed\n",
                                                fname);
        return (EC_FALSE);
    }

    dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_load: "
                                            "access file '%s' done\n",
                                            fname);

    fd = c_file_open(fname, O_RDONLY, 0666);
    if(ERR_FD == fd)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "open file '%s' failed\n",
                                                fname);
        return (EC_FALSE);
    }

    if(EC_FALSE == c_file_size(fd, &fsize))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "get size of '%s' failed\n",
                                                fname);
        c_file_close(fd);
        return (EC_FALSE);
    }

    if(0 == fsize)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "file '%s' size is 0\n",
                                                fname);
        c_file_close(fd);
        return (EC_FALSE);
    }

    fcontent = safe_malloc(fsize, LOC_CRFSMON_0010);
    if(NULL_PTR == fcontent)
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "malloc %ld bytes for file '%s' failed\n",
                                                fsize, fname);
        c_file_close(fd);
        return (EC_FALSE);
    }

    offset = 0;
    if(EC_FALSE == c_file_load(fd, &offset, fsize, fcontent))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "load file '%s' failed\n",
                                                fname);
        c_file_close(fd);
        safe_free(fcontent, LOC_CRFSMON_0011);
        return (EC_FALSE);
    }
    c_file_close(fd);

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_load: "
                                            "load file '%s' from disk done\n",
                                            fname);

    /*parse*/
    if(EC_FALSE == __crfsmon_parse_hot_path_file(crfsmon_md_id, (char *)fcontent, (char *)(fcontent + fsize)))
    {
        dbg_log(SEC_0155_CRFSMON, 0)(LOGSTDOUT, "error:crfsmon_crfs_hot_path_load: "
                                                "parse file '%s' failed\n",
                                                fname);
        safe_free(fcontent, LOC_CRFSMON_0012);
        return (EC_FALSE);
    }
    safe_free(fcontent, LOC_CRFSMON_0013);

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_load: "
                                            "parse file '%s' done\n",
                                            fname);
    return (EC_TRUE);
}

EC_BOOL crfsmon_crfs_hot_path_unload(const UINT32 crfsmon_md_id)
{
    CRFSMON_MD  *crfsmon_md;

#if ( SWITCH_ON == CRFSMON_DEBUG_SWITCH )
    if ( CRFSMON_MD_ID_CHECK_INVALID(crfsmon_md_id) )
    {
        sys_log(LOGSTDOUT,
                "error:crfsmon_crfs_hot_path_unload: crfsmon module #0x%lx not started.\n",
                crfsmon_md_id);
        dbg_exit(MD_CRFSMON, crfsmon_md_id);
    }
#endif/*CRFS_DEBUG_SWITCH*/

    crfsmon_md = CRFSMON_MD_GET(crfsmon_md_id);

    crb_tree_clean(CRFSMON_MD_HOT_PATH_TREE(crfsmon_md));

    dbg_log(SEC_0155_CRFSMON, 9)(LOGSTDOUT, "[DEBUG] crfsmon_crfs_hot_path_load: "
                                            "unload done\n");
    return (EC_TRUE);
}

#ifdef __cplusplus
}
#endif/*__cplusplus*/

