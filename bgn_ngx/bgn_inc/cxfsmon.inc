/******************************************************************************
*
* Copyright (C) Chaoyong Zhou
* Email: bgnvendor@163.com
* QQ: 2796796
*
*******************************************************************************/
#ifndef _CXFSMON_INC
#define _CXFSMON_INC

#include "findex.inc"

FUNC_ADDR_NODE g_cxfsmon_func_addr_list[ ] = {
{
/* -- void cxfsmon_print_module_status(const UINT32 cxfsmon_md_id, LOG *log); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_print_module_status,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_print_module_status",
/*func index      */     FI_cxfsmon_print_module_status,
/*func ret type   */     e_dbg_void,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IO,},
/*func para type  */     {e_dbg_UINT32,e_dbg_LOG_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- UINT32 cxfsmon_free_module_static_mem(const UINT32 cxfsmon_md_id); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_free_module_static_mem,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_free_module_static_mem",
/*func index      */     FI_cxfsmon_free_module_static_mem,
/*func ret type   */     e_dbg_UINT32,
/*func para num   */     1,
/*func para direct*/     {E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,},
/*func para val   */     0, 0, {0},
},
{
/* -- UINT32 cxfsmon_start(); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_start,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_start",
/*func index      */     FI_cxfsmon_start,
/*func ret type   */     e_dbg_UINT32,
/*func para num   */     0,
/*func para direct*/     {E_DIRECT_END,},
/*func para type  */     {e_dbg_void,},
/*func para val   */     0, 0, {0},
},
{
/* -- void cxfsmon_end(const UINT32 cxfsmon_md_id); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_end,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_end",
/*func index      */     FI_cxfsmon_end,
/*func ret type   */     e_dbg_void,
/*func para num   */     1,
/*func para direct*/     {E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,},
/*func para val   */     0, 0, {0},
},
{
/* -- void cxfsmon_cxfs_node_print(const UINT32 cxfsmon_md_id, LOG *log); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_print,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_print",
/*func index      */     FI_cxfsmon_cxfs_node_print,
/*func ret type   */     e_dbg_void,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IO,},
/*func para type  */     {e_dbg_UINT32,e_dbg_LOG_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_add(const UINT32 cxfsmon_md_id, const CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_add,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_add",
/*func index      */     FI_cxfsmon_cxfs_node_add,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_del(const UINT32 cxfsmon_md_id, const CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_del,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_del",
/*func index      */     FI_cxfsmon_cxfs_node_del,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_set_up(const UINT32 cxfsmon_md_id, const CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_set_up,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_set_up",
/*func index      */     FI_cxfsmon_cxfs_node_set_up,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_set_down(const UINT32 cxfsmon_md_id, const CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_set_down,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_set_down",
/*func index      */     FI_cxfsmon_cxfs_node_set_down,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_is_up(const UINT32 cxfsmon_md_id, const CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_is_up,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_is_up",
/*func index      */     FI_cxfsmon_cxfs_node_is_up,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_get_by_pos(const UINT32 cxfsmon_md_id, const UINT32 pos, CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_get_by_pos,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_get_by_pos",
/*func index      */     FI_cxfsmon_cxfs_node_get_by_pos,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     3,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,E_DIRECT_OUT,},
/*func para type  */     {e_dbg_UINT32,e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_get_by_tcid(const UINT32 cxfsmon_md_id, const UINT32 tcid, const UINT32 modi, CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_get_by_tcid,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_get_by_tcid",
/*func index      */     FI_cxfsmon_cxfs_node_get_by_tcid,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     4,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,E_DIRECT_IN,E_DIRECT_OUT,},
/*func para type  */     {e_dbg_UINT32,e_dbg_UINT32,e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- void cxfsmon_cxfs_node_list(const UINT32 cxfsmon_md_id, CSTRING *cstr); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_list,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_list",
/*func index      */     FI_cxfsmon_cxfs_node_list,
/*func ret type   */     e_dbg_void,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_OUT,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CSTRING_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_num(const UINT32 cxfsmon_md_id, UINT32 *num); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_num,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_num",
/*func index      */     FI_cxfsmon_cxfs_node_num,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_OUT,},
/*func para type  */     {e_dbg_UINT32,e_dbg_UINT32_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_node_get_by_hash(const UINT32 cxfsmon_md_id, const UINT32 hash, CXFS_NODE *cxfs_node); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_node_get_by_hash,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_node_get_by_hash",
/*func index      */     FI_cxfsmon_cxfs_node_get_by_hash,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     3,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,E_DIRECT_OUT,},
/*func para type  */     {e_dbg_UINT32,e_dbg_UINT32,e_dbg_CXFS_NODE_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_hot_path_add(const UINT32 cxfsmon_md_id, const CSTRING *path); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_hot_path_add,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_hot_path_add",
/*func index      */     FI_cxfsmon_cxfs_hot_path_add,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CSTRING_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_hot_path_del(const UINT32 cxfsmon_md_id, const CSTRING *path); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_hot_path_del,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_hot_path_del",
/*func index      */     FI_cxfsmon_cxfs_hot_path_del,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CSTRING_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_hot_path_exist(const UINT32 cxfsmon_md_id, const CSTRING *path); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_hot_path_exist,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_hot_path_exist",
/*func index      */     FI_cxfsmon_cxfs_hot_path_exist,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CSTRING_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- void cxfsmon_cxfs_hot_path_print(const UINT32 cxfsmon_md_id, LOG *log); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_hot_path_print,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_hot_path_print",
/*func index      */     FI_cxfsmon_cxfs_hot_path_print,
/*func ret type   */     e_dbg_void,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IO,},
/*func para type  */     {e_dbg_UINT32,e_dbg_LOG_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_hot_path_load(const UINT32 cxfsmon_md_id, const CSTRING *path); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_hot_path_load,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_hot_path_load",
/*func index      */     FI_cxfsmon_cxfs_hot_path_load,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     2,
/*func para direct*/     {E_DIRECT_IN,E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,e_dbg_CSTRING_ptr,},
/*func para val   */     0, 0, {0},
},
{
/* -- EC_BOOL cxfsmon_cxfs_hot_path_unload(const UINT32 cxfsmon_md_id); -- */
/*func module     */     MD_CXFSMON,
/*func logic addr */     (UINT32)cxfsmon_cxfs_hot_path_unload,
/*func beg addr   */     0,
/*func end addr   */     0,
/*func addr offset*/     0,
/*func name       */     "cxfsmon_cxfs_hot_path_unload",
/*func index      */     FI_cxfsmon_cxfs_hot_path_unload,
/*func ret type   */     e_dbg_EC_BOOL,
/*func para num   */     1,
/*func para direct*/     {E_DIRECT_IN,},
/*func para type  */     {e_dbg_UINT32,},
/*func para val   */     0, 0, {0},
},
};

UINT32 g_cxfsmon_func_addr_list_len = sizeof(g_cxfsmon_func_addr_list)/sizeof(g_cxfsmon_func_addr_list[0]);
#endif/*_CXFSMON_INC*/

