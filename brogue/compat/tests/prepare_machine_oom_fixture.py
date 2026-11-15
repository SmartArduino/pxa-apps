#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Prepare a private fault-injection TU for exact CPS scratch-ownership helpers.

This changes only test copies; the shipping generator/template remain untouched.
Link with the same oracle malloc/calloc/realloc/free wrappers and a fixture entry
that invokes oracle_cps_oom_fixture(mode). Modes0/1/2/4/5 MUST fatally stop only after
ORACLE_OOM_CLEANUP reports zero new allocations; mode3 is the normal free path.
"""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import argparse,hashlib,importlib.util,json,re
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);compat=Path(__file__).resolve().parents[1];app=compat.parent;source=app/'vendor/src/brogue/Architect.c'
spec=importlib.util.spec_from_file_location('oom_architect_generator',compat/'generate_architect.py');g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g);normal=a.output/'Architect-normal.c';g.generate(source,normal);text=normal.read_text()
replacements=[
 ('struct pxa_machine_frame *frame = malloc(sizeof(*frame));','struct pxa_machine_frame *frame = oracle_context_malloc(sizeof(*frame));'),
 ('machineData *data = malloc(sizeof(machineData));','machineData *data = oracle_machine_data_malloc(sizeof(machineData));'),
 ('short **grid = malloc(DCOLS * sizeof(short *));','short **grid = oracle_grid_rows_malloc(DCOLS * sizeof(short *));'),
 ('grid[0] = malloc(DROWS * DCOLS * sizeof(short));','grid[0] = oracle_grid_cells_malloc(DROWS * DCOLS * sizeof(short));'),
 ('    brogueAssert(false);\n    abort();\n}\n\nstatic machineData *pxa_machine_allocate_data',
  '    oracle_oom_cleanup_receipt();\n    brogueAssert(false);\n    abort();\n}\n\nstatic machineData *pxa_machine_allocate_data')]
for before,after in replacements:
 assert text.count(before)==1,before;text=text.replace(before,after,1)
marker='/* A frame contains every original local and argument,';assert text.count(marker)==1
text=text.replace(marker,'static void *oracle_context_malloc(size_t);\nstatic void *oracle_machine_data_malloc(size_t);\nstatic void *oracle_grid_rows_malloc(size_t);\nstatic void *oracle_grid_cells_malloc(size_t);\nstatic void oracle_oom_cleanup_receipt(void);\n\n'+marker,1)
fixture=r"""
/* Private fault injection: these probes are absent from the shipping template. */
extern size_t oracle_allocation_count(void);
extern size_t oracle_allocation_new_count(void);
extern void oracle_allocation_mark_baseline(void);
static unsigned oracle_fail_context, oracle_fail_data, oracle_context_calls, oracle_data_calls;
static unsigned oracle_fail_rows, oracle_fail_cells, oracle_rows_calls, oracle_cells_calls;
static void *oracle_borrowed;
static void oracle_oom_bad(const char *why) {
    fprintf(stderr,"ORACLE_OOM_TEST_FAIL %s\n",why);__builtin_trap();
}
static void *oracle_context_malloc(size_t size) {
    ++oracle_context_calls;
    if (oracle_fail_context) return NULL;
    return malloc(size);
}
static void *oracle_machine_data_malloc(size_t size) {
    ++oracle_data_calls;
    if (oracle_fail_data) return NULL;
    return malloc(size);
}
static void *oracle_grid_rows_malloc(size_t size) {
    ++oracle_rows_calls;
    if(oracle_fail_rows)return NULL;
    return malloc(size);
}
static void *oracle_grid_cells_malloc(size_t size) {
    ++oracle_cells_calls;
    if(oracle_fail_cells)return NULL;
    return malloc(size);
}
static void oracle_oom_cleanup_receipt(void) {
    size_t fresh=oracle_allocation_new_count();
    size_t live=oracle_allocation_count();
    if (fresh!=0 || live!=1) oracle_oom_bad("scratch leak or borrowed owner destroyed");
    fprintf(stderr,"ORACLE_OOM_CLEANUP new=%zu borrowed_live=%zu context_calls=%u data_calls=%u grid_rows_calls=%u grid_cells_calls=%u\n",fresh,live,oracle_context_calls,oracle_data_calls,oracle_rows_calls,oracle_cells_calls);
    fflush(stderr);
}
int oracle_cps_oom_fixture(unsigned mode) {
    struct pxa_machine_args args={.bp=-1,.originX=-1,.originY=-1};
    struct pxa_machine_frame *top=NULL;
    oracle_borrowed=malloc(64);
    if(!oracle_borrowed)oracle_oom_bad("borrowed setup allocation");
    memset(oracle_borrowed,0,64);
    oracle_allocation_mark_baseline();
    if(mode==0)oracle_fail_context=1;
    if(mode==3) {
        top=pxa_machine_allocate_frame(NULL,args);
        top->__P_FIELD__=pxa_machine_allocate_data(top);
        memset(top->__P_FIELD__,0,sizeof(machineData));
        top->__MAP_FIELD__=pxa_machine_allocate_grid(top);
        pxa_machine_free_data(top,top->__P_FIELD__);
        pxa_machine_free_grid(top,top->__MAP_FIELD__);
        if(top->owned_data || top->owned_grid)oracle_oom_bad("normal ownership not released");
        free(top);oracle_oom_cleanup_receipt();
        free(oracle_borrowed);
        if(oracle_allocation_count()!=0)oracle_oom_bad("normal cleanup residual");
        fprintf(stderr,"ORACLE_OOM_NORMAL_DONE new=0 live=0\n");return 0;
    }
    for(unsigned n=0;n<3;++n) {
        machineData *data;
        top=pxa_machine_allocate_frame(top,args);
        data=pxa_machine_allocate_data(top);memset(data,0,sizeof(*data));
        top->__P_FIELD__=data;top->__MAP_FIELD__=pxa_machine_allocate_grid(top);
        /* Borrowed/transferred references MUST NOT be logically rolled back. */
        data->spawnedItems[0]=(item *)oracle_borrowed;
        data->spawnedMonsters[0]=(creature *)oracle_borrowed;
    }
    if(mode==1)oracle_fail_context=1;
    else if(mode==2)oracle_fail_data=1;
    else if(mode==4)oracle_fail_rows=1;
    else if(mode==5)oracle_fail_cells=1;
    else oracle_oom_bad("unknown mode");
    top=pxa_machine_allocate_frame(top,args);
    top->__P_FIELD__=pxa_machine_allocate_data(top);
    if(mode==4 || mode==5)(void)pxa_machine_allocate_grid(top);
    oracle_oom_bad("fatal allocation unexpectedly returned");return -1;
}
"""
p_fields=set(re.findall(r'frame->(l\d+_p) = NULL;',text));map_fields=set(re.findall(r'frame->(l\d+_distanceMap) = NULL;',text));assert len(p_fields)==len(map_fields)==1
fixture=fixture.replace('__P_FIELD__',p_fields.pop()).replace('__MAP_FIELD__',map_fields.pop());out=a.output/'Architect-oom-fixture.c';out.write_text(text+fixture)
receipt={'source_only':True,'shipping_template_unchanged':True,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'normal_generated_sha256':hashlib.sha256(normal.read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'instrumented_allocation_sites':4,'fatal_report_hook':1,'modes':{'0':'root context failure','1':'nested context failure with three owned data+grid ancestors','2':'nested machineData allocation failure with active ancestors','3':'normal scratch free/owner clear/borrowed lifetime','4':'grid row-header malloc fails with four active contexts','5':'grid cell-block malloc fails, frees local row header before active-context unwind'},'expected':'0/1/2/4/5cleanup new0 borrowed1 then original fatal assert/abort;3live0 normalreturn','not_gameplay_acceptance':True};(a.output/'preparation.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
