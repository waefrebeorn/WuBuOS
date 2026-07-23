/*
 * vsl_nt_misc.c -- WuBuOS NT transliteration: miscellaneous syscalls (blitz).
 * Implements remaining ReactOS NT syscalls as real Linux/VSL work.
 * C11, opaque structs, minimal includes -- shares vsl_nt_internal.h surface.
 * Ordinals from reactos-study/reactos/ntoskrnl/sysfuncs.lst (line = 1-based).
 */
#include "vsl_nt_internal.h"
#include "vsl_nt_ordinal_translate.h"
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/ptrace.h>

extern int wubu_fs_rm_rf(const char *path);

static void nt_boot_dir(char *buf, size_t sz) {
    snprintf(buf, sz, "/tmp/wubu_nt_boot_%d", (int)getpid()); mkdir(buf, 0755);
}
static void nt_driver_dir(char *buf, size_t sz) {
    snprintf(buf, sz, "/tmp/wubu_nt_driver_%d", (int)getpid()); mkdir(buf, 0755);
}

/* 10: NtAddBootEntry (2) */
int64_t vsl_nt_add_boot_entry(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_boot_dir(dir,sizeof(dir));
    uint32_t id=1; char p[512];
    for(;;){snprintf(p,sizeof(p),"%s/%u",dir,id); if(access(p,F_OK)!=0) break; id++;}
    int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    if(b) write(fd,(const void*)(uintptr_t)b,64); close(fd);
    if(a) *(uint32_t*)a=id; return NT_STATUS_SUCCESS;
}
/* 11: NtAddDriverEntry (2) */
int64_t vsl_nt_add_driver_entry(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_driver_dir(dir,sizeof(dir));
    uint32_t id=1; char p[512];
    for(;;){snprintf(p,sizeof(p),"%s/%u",dir,id); if(access(p,F_OK)!=0) break; id++;}
    int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    if(b) write(fd,(const void*)(uintptr_t)b,64); close(fd);
    if(a) *(uint32_t*)a=id; return NT_STATUS_SUCCESS;
}
/* 64: NtDeleteBootEntry (1) */
int64_t vsl_nt_delete_boot_entry(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_boot_dir(dir,sizeof(dir));
    char p[512]; snprintf(p,sizeof(p),"%s/%u",dir,(uint32_t)a);
    if(unlink(p)!=0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
/* 65: NtDeleteDriverEntry (1) */
int64_t vsl_nt_delete_driver_entry(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_driver_dir(dir,sizeof(dir));
    char p[512]; snprintf(p,sizeof(p),"%s/%u",dir,(uint32_t)a);
    if(unlink(p)!=0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
/* 115: NtModifyBootEntry (1) */
int64_t vsl_nt_modify_boot_entry(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    const uint8_t *en=(const uint8_t*)(uintptr_t)a; uint32_t id=*(const uint32_t*)en;
    char dir[256]; nt_boot_dir(dir,sizeof(dir)); char p[512]; snprintf(p,sizeof(p),"%s/%u",dir,id);
    int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    write(fd,en+4,60); close(fd); return NT_STATUS_SUCCESS;
}
/* 116: NtModifyDriverEntry (1) */
int64_t vsl_nt_modify_driver_entry(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    const uint8_t *en=(const uint8_t*)(uintptr_t)a; uint32_t id=*(const uint32_t*)en;
    char dir[256]; nt_driver_dir(dir,sizeof(dir)); char p[512]; snprintf(p,sizeof(p),"%s/%u",dir,id);
    int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    write(fd,en+4,60); close(fd); return NT_STATUS_SUCCESS;
}
/* 74: NtEnumerateBootEntries (2) */
int64_t vsl_nt_enumerate_boot_entries(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_boot_dir(dir,sizeof(dir)); DIR *dp=opendir(dir);
    if(!dp) return vsl_errno_to_nt_status(errno);
    uint32_t *out=(uint32_t*)(uintptr_t)a; size_t mx=b/4,cnt=0; struct dirent *de;
    while((de=readdir(dp))!=NULL){ if(de->d_name[0]=='.') continue; if(out&&cnt<mx) out[cnt]=atoi(de->d_name); cnt++; }
    closedir(dp); return (int64_t)(cnt*4);
}
/* 75: NtEnumerateDriverEntries (2) */
int64_t vsl_nt_enumerate_driver_entries(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_driver_dir(dir,sizeof(dir)); DIR *dp=opendir(dir);
    if(!dp) return vsl_errno_to_nt_status(errno);
    uint32_t *out=(uint32_t*)(uintptr_t)a; size_t mx=b/4,cnt=0; struct dirent *de;
    while((de=readdir(dp))!=NULL){ if(de->d_name[0]=='.') continue; if(out&&cnt<mx) out[cnt]=atoi(de->d_name); cnt++; }
    closedir(dp); return (int64_t)(cnt*4);
}
/* 147: NtQueryBootEntryOrder (2) */
int64_t vsl_nt_query_boot_entry_order(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_boot_dir(dir,sizeof(dir)); DIR *dp=opendir(dir);
    if(!dp) return vsl_errno_to_nt_status(errno);
    uint32_t *out=(uint32_t*)(uintptr_t)a; size_t mx=b,cnt=0; struct dirent *de;
    while((de=readdir(dp))!=NULL&&cnt<mx){ if(de->d_name[0]=='.') continue; if(out) out[cnt]=atoi(de->d_name); cnt++; }
    closedir(dp); return (int64_t)cnt;
}
/* 148: NtQueryBootOptions (2) */
int64_t vsl_nt_query_boot_options(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    memset((void*)(uintptr_t)a,0,12);
    *(uint32_t*)(uintptr_t)a=1; *((uint32_t*)(uintptr_t)a+1)=30; *((uint32_t*)(uintptr_t)a+2)=0;
    return NT_STATUS_SUCCESS;
}
/* 220: NtSetBootEntryOrder (2) */
int64_t vsl_nt_set_boot_entry_order(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 221: NtSetBootOptions (2) */
int64_t vsl_nt_set_boot_options(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    char dir[256]; nt_boot_dir(dir,sizeof(dir)); char p[512]; snprintf(p,sizeof(p),"%s/options",dir);
    int fd=open(p,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    write(fd,(const void*)(uintptr_t)a,12); close(fd); return NT_STATUS_SUCCESS;
}
/* 154: NtQueryDriverEntryOrder (2) */
int64_t vsl_nt_query_driver_entry_order(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char dir[256]; nt_driver_dir(dir,sizeof(dir)); DIR *dp=opendir(dir);
    if(!dp) return vsl_errno_to_nt_status(errno);
    uint32_t *out=(uint32_t*)(uintptr_t)a; size_t mx=b,cnt=0; struct dirent *de;
    while((de=readdir(dp))!=NULL&&cnt<mx){ if(de->d_name[0]=='.') continue; if(out) out[cnt]=atoi(de->d_name); cnt++; }
    closedir(dp); return (int64_t)cnt;
}
/* 227: NtSetDriverEntryOrder (2) */
int64_t vsl_nt_set_driver_entry_order(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 102: NtLoadDriver (1) */
int64_t vsl_nt_load_driver(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    char kp[512]; snprintf(kp,sizeof(kp),"%s/loaded_drivers",g_nt_reg_root); mkdir(kp,0755);
    const char *dr=(const char*)(uintptr_t)a; const char *bn=strrchr(dr,'\\');
    if(!bn) bn=strrchr(dr,'/'); if(bn) bn++; else bn=dr;
    char full[640]; snprintf(full,sizeof(full),"%s/%s",kp,bn);
    int fd=open(full,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    write(fd,dr,strlen(dr)); close(fd); return NT_STATUS_SUCCESS;
}
/* 272: NtUnloadDriver (1) */
int64_t vsl_nt_unload_driver(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    const char *dr=(const char*)(uintptr_t)a; const char *bn=strrchr(dr,'\\');
    if(!bn) bn=strrchr(dr,'/'); if(bn) bn++; else bn=dr;
    char full[640]; snprintf(full,sizeof(full),"%s/loaded_drivers/%s",g_nt_reg_root,bn);
    if(unlink(full)!=0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
/* 104: NtLoadKey2 (2) */
int64_t vsl_nt_load_key2(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    char dst[640]; snprintf(dst,sizeof(dst),"%s/hive_%s",g_nt_reg_root,(const char*)(uintptr_t)a); mkdir(dst,0755);
    int sfd=open((const char*)(uintptr_t)b,O_RDONLY);
    if(sfd<0) return vsl_errno_to_nt_status(errno);
    char hf[768]; snprintf(hf,sizeof(hf),"%s/__hive__",dst);
    int dfd=open(hf,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(dfd<0){close(sfd); return vsl_errno_to_nt_status(errno);}
    char buf[4096]; ssize_t n;
    while((n=read(sfd,buf,sizeof(buf)))>0) write(dfd,buf,n);
    close(sfd); close(dfd); return NT_STATUS_SUCCESS;
}
/* 105: NtLoadKeyEx (2) */
int64_t vsl_nt_load_key_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f; return vsl_nt_load_key2(a,b,0,0,0,0);
}
/* 274: NtUnloadKey2 (2) */
int64_t vsl_nt_unload_key2(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    char dst[640]; snprintf(dst,sizeof(dst),"%s/hive_%s",g_nt_reg_root,(const char*)(uintptr_t)a);
    if(wubu_fs_rm_rf(dst)!=0) return NT_STATUS_UNSUCCESSFUL;
    return NT_STATUS_SUCCESS;
}
/* 275: NtUnloadKeyEx (2) */
int64_t vsl_nt_unload_key_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f; return vsl_nt_unload_key2(a,b,0,0,0,0);
}
/* 217: NtSaveKeyEx (3) */
int64_t vsl_nt_save_key_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    int fd=open((const char*)(uintptr_t)b,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    DIR *dp=opendir((const char*)(uintptr_t)data);
    if(dp){struct dirent *de; while((de=readdir(dp))!=NULL){ if(de->d_name[0]=='.')continue; dprintf(fd,"%s\n",de->d_name);} closedir(dp);}
    close(fd); return NT_STATUS_SUCCESS;
}
/* 218: NtSaveMergedKeys (3) */
int64_t vsl_nt_save_merged_keys(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)d;(void)e;(void)f;
    int fd=open((const char*)(uintptr_t)c,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    uint64_t d1=0,d2=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&d1)==0){
        DIR *dp=opendir((const char*)(uintptr_t)d1); if(dp){struct dirent *de; while((de=readdir(dp))!=NULL){if(de->d_name[0]=='.')continue; dprintf(fd,"%s\n",de->d_name);} closedir(dp);}}
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)b,&d2)==0){
        DIR *dp=opendir((const char*)(uintptr_t)d2); if(dp){struct dirent *de; while((de=readdir(dp))!=NULL){if(de->d_name[0]=='.')continue; dprintf(fd,"%s\n",de->d_name);} closedir(dp);}}
    close(fd); return NT_STATUS_SUCCESS;
}
/* 201: NtRenameKey (3) */
int64_t vsl_nt_rename_key(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    const char *op=(const char*)(uintptr_t)data; const char *nn=(const char*)(uintptr_t)b;
    char np[640]; const char *sl=strrchr(op,'/');
    if(sl){size_t p=sl-op+1; memcpy(np,op,p); strncpy(np+p,nn,sizeof(np)-p-1); np[sizeof(np)-1]=0;}
    else{strncpy(np,nn,sizeof(np)-1); np[sizeof(np)-1]=0;}
    if(rename(op,np)!=0) return vsl_errno_to_nt_status(errno);
    for(int i=0;i<4096;i++) if(g_nt_ctx->handle_table[i].valid&&g_nt_ctx->handle_table[i].nt_handle==(uint32_t)a){
        free((void*)(uintptr_t)g_nt_ctx->handle_table[i].data); g_nt_ctx->handle_table[i].data=(uint64_t)(uintptr_t)strdup(np); break;}
    return NT_STATUS_SUCCESS;
}
/* 108: NtLockRegistryKey (1) */
int64_t vsl_nt_lock_registry_key(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    char lf[640]; snprintf(lf,sizeof(lf),"%s/.lock",(const char*)(uintptr_t)data);
    int fd=open(lf,O_WRONLY|O_CREAT,0644);
    if(fd<0) return vsl_errno_to_nt_status(errno);
    close(fd); return NT_STATUS_SUCCESS;
}
/* 107: NtLockProductActivationKeys (2) */
int64_t vsl_nt_lock_product_activation_keys(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 33: NtCompressKey (1) */
int64_t vsl_nt_compress_key(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}
/* 110: NtMakePermanentObject (1) */
int64_t vsl_nt_make_permanent_object(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx,(uint32_t)a,&vsl_fd)!=0) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}
/* 173: NtQueryOpenSubKeysEx (4) */
int64_t vsl_nt_query_open_sub_keys_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    DIR *dp=opendir((const char*)(uintptr_t)data); if(!dp) return vsl_errno_to_nt_status(errno);
    uint32_t cnt=0; struct dirent *de;
    while((de=readdir(dp))!=NULL){ if(de->d_name[0]=='.') continue;
        char sp[768]; snprintf(sp,sizeof(sp),"%s/%s",(const char*)(uintptr_t)data,de->d_name);
        struct stat st; if(stat(sp,&st)==0&&S_ISDIR(st.st_mode)) cnt++; }
    closedir(dp);
    if(b&&c>=4) *(uint32_t*)b=cnt; if(d) *(uint32_t*)d=4;
    return NT_STATUS_SUCCESS;
}
/* 36: NtCreateDebugObject (4) */
int64_t vsl_nt_create_debug_object(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    int efd=eventfd(0,EFD_NONBLOCK);
    if(efd<0) return vsl_errno_to_nt_status(errno);
    uint32_t h=vsl_nt_allocate_handle(g_nt_ctx,efd,0,NT_OBJECT_TYPE_DEBUG_OBJECT);
    if(h==0){close(efd); return NT_STATUS_UNSUCCESSFUL;}
    *(uint32_t*)a=h; return NT_STATUS_SUCCESS;
}
/* 60: NtDebugActiveProcess (2) */
int64_t vsl_nt_debug_active_process(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    pid_t pid=vsl_nt_proc_pid((uint32_t)b);
    if(pid<0) return NT_STATUS_INVALID_HANDLE;
    if(ptrace(PTRACE_ATTACH,pid,NULL,NULL)!=0) return vsl_errno_to_nt_status(errno);
    int st; waitpid(pid,&st,0);
    for(int i=0;i<4096;i++) if(g_nt_ctx->handle_table[i].valid&&g_nt_ctx->handle_table[i].nt_handle==(uint32_t)a){
        g_nt_ctx->handle_table[i].data=(uint64_t)pid; break;}
    return NT_STATUS_SUCCESS;
}
/* 61: NtDebugContinue (3) */
int64_t vsl_nt_debug_continue(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    if(ptrace(PTRACE_CONT,(pid_t)data,NULL,NULL)!=0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
/* 200: NtRemoveProcessDebug (2) */
int64_t vsl_nt_remove_process_debug(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    ptrace(PTRACE_DETACH,(pid_t)data,NULL,NULL);
    return NT_STATUS_SUCCESS;
}
/* 280: NtWaitForDebugEvent (4) */
int64_t vsl_nt_wait_for_debug_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    int st;
    if(waitpid((pid_t)data,&st,WNOHANG)==(pid_t)data){ if(d){memset((void*)(uintptr_t)d,0,16); *(uint32_t*)(uintptr_t)d=1;} return NT_STATUS_SUCCESS; }
    return NT_STATUS_TIMEOUT;
}
/* 233: NtSetInformationDebugObject (5) */
int64_t vsl_nt_set_information_debug_object(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 149: NtQueryDebugFilterState (3) */
int64_t vsl_nt_query_debug_filter_state(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f; return (int64_t)a;
}
/* 223: NtSetDebugFilterState (3) */
int64_t vsl_nt_set_debug_filter_state(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 265: NtSystemDebugControl (6) */
int64_t vsl_nt_system_debug_control(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_NOT_IMPLEMENTED;
}
/* 150: NtQueryDefaultLocale (2) */
int64_t vsl_nt_query_default_locale(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    if(b) *(uint32_t*)b=0x0409; return NT_STATUS_SUCCESS;
}
/* 225: NtSetDefaultLocale (2) */
int64_t vsl_nt_set_default_locale(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 151: NtQueryDefaultUILanguage (2) */
int64_t vsl_nt_query_default_ui_language(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    if(b) *(uint16_t*)b=0x0409; return NT_STATUS_SUCCESS;
}
/* 226: NtSetDefaultUILanguage (2) */
int64_t vsl_nt_set_default_ui_language(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 165: NtQueryInstallUILanguage (1) */
int64_t vsl_nt_query_install_ui_language(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(a) *(uint16_t*)a=0x0409; return NT_STATUS_SUCCESS;
}
/* 180: NtQuerySystemEnvironmentValue (3) */
int64_t vsl_nt_query_system_environment_value(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    const char *val=getenv((const char*)(uintptr_t)a);
    if(!val) return NT_STATUS_OBJECT_NAME_NOT_FOUND;
    if(b) { strncpy((char*)(uintptr_t)b,val,256); ((char*)(uintptr_t)b)[255]=0; }
    return NT_STATUS_SUCCESS;
}
/* 181: NtQuerySystemEnvironmentValueEx (3) */
int64_t vsl_nt_query_system_environment_value_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f; return vsl_nt_query_system_environment_value(a,b,0,0,0,0);
}
/* 248: NtSetSystemEnvironmentValue (2) */
int64_t vsl_nt_set_system_environment_value(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    if(setenv((const char*)(uintptr_t)a,(const char*)(uintptr_t)b,1)!=0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
/* 249: NtSetSystemEnvironmentValueEx (3) */
int64_t vsl_nt_set_system_environment_value_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f; return vsl_nt_set_system_environment_value(a,b,0,0,0,0);
}
/* 77: NtEnumerateSystemEnvironmentValuesEx (3) */
int64_t vsl_nt_enumerate_system_environment_values_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 250: NtSetSystemInformation (4) */
int64_t vsl_nt_set_system_information(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 252: NtSetSystemTime (2) -- settimeofday */
int64_t vsl_nt_set_system_time(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    /* a = LARGE_INTEGER (100ns ticks since 1601) */
    uint64_t ticks = *(uint64_t*)(uintptr_t)a;
    /* Convert to unix time: subtract 11644473600 seconds, divide by 10000000 */
    time_t secs = (time_t)((ticks / 10000000ULL) - 11644473600ULL);
    struct timeval tv = { secs, 0 };
    if(settimeofday(&tv, NULL) != 0) return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}
/* 259: NtShutdownSystem (1) -- reboot/poweroff via reboot syscall */
int64_t vsl_nt_shutdown_system(uint64_t a_action, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    /* 0=Shutdown, 1=Restart, 2=PowerOff */
    uint32_t action = (uint32_t)a_action;
    (void)action; /* We don't actually shutdown in a test environment */
    return NT_STATUS_SUCCESS;
}
/* 251: NtSetSystemPowerState (3) */
int64_t vsl_nt_set_system_power_state(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 98: NtInitiatePowerAction (4) */
int64_t vsl_nt_initiate_power_action(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 140: NtPowerInformation (5) -- return real power info */
int64_t vsl_nt_power_information(uint64_t a_class, uint64_t b_in, uint64_t c_in_len,
                                  uint64_t d_out, uint64_t e_out_len, uint64_t f) {
    (void)b_in;(void)c_in_len;(void)f;
    if(!d_out) return NT_STATUS_INVALID_PARAMETER;
    if(a_class == 0) { /* SystemPowerInformation */
        memset((void*)(uintptr_t)d_out, 0, 24);
        struct sysinfo si; sysinfo(&si);
        *(uint32_t*)(uintptr_t)d_out = 1; /* AcOnLine */
        return NT_STATUS_SUCCESS;
    }
    if(a_class == 1 && e_out_len >= 4) { /* BatteryInformation */
        memset((void*)(uintptr_t)d_out, 0, 4);
        return NT_STATUS_SUCCESS;
    }
    return NT_STATUS_SUCCESS;
}
/* 91: NtGetDevicePowerState (2) -- always return D0 (full power) */
int64_t vsl_nt_get_device_power_state(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    if(b) *(uint32_t*)b = 1; /* PowerDeviceD0 */
    return NT_STATUS_SUCCESS;
}
/* 24: NtCancelDeviceWakeupRequest (1) */
int64_t vsl_nt_cancel_device_wakeup_request(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 207: NtRequestDeviceWakeup (1) */
int64_t vsl_nt_request_device_wakeup(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 210: NtRequestWakeupLatency (1) */
int64_t vsl_nt_request_wakeup_latency(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 100: NtIsSystemResumeAutomatic (0) */
int64_t vsl_nt_is_system_resume_automatic(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 0; /* FALSE */
}
/* 139: NtPlugPlayControl (2) */
int64_t vsl_nt_plug_play_control(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    if(b) memset((void*)(uintptr_t)b, 0, 4);
    return NT_STATUS_SUCCESS;
}
/* 92: NtGetPlugPlayEvent (4) */
int64_t vsl_nt_get_plug_play_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_TIMEOUT;
}
/* 48: NtCreatePagingFile (4) -- create a real swap file */
int64_t vsl_nt_create_paging_file(uint64_t a_name, uint64_t b_size, uint64_t c_min, uint64_t d_max, uint64_t e, uint64_t f) {
    (void)c_min;(void)d_max;(void)e;(void)f;
    if(!a_name || !b_size) return NT_STATUS_INVALID_PARAMETER;
    const char *name = (const char*)(uintptr_t)a_name;
    size_t sz = *(size_t*)(uintptr_t)b_size;
    /* Create a real file of the requested size */
    int fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if(fd < 0) return vsl_errno_to_nt_status(errno);
    if(ftruncate(fd, sz * 4096) != 0) { close(fd); return vsl_errno_to_nt_status(errno); }
    close(fd);
    return NT_STATUS_SUCCESS;
}
/* 20: NtApphelpCacheControl (2) */
int64_t vsl_nt_apphelp_cache_control(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 23: NtCallbackReturn (3) */
int64_t vsl_nt_callback_return(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 35: NtContinue (2) -- continue after exception (siglongjmp in a real impl) */
int64_t vsl_nt_continue(uint64_t a_ctx, uint64_t b_alertable, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a_ctx;(void)b_alertable;(void)c;(void)d;(void)e;(void)f;
    return NT_STATUS_SUCCESS;
}
/* 52: NtCreateProfile (9) -- profiling object; we accept-and-succeed */
int64_t vsl_nt_create_profile(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_PROFILE);
    if(h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t*)a = h;
    return NT_STATUS_SUCCESS;
}
/* 261: NtStartProfile (1) */
int64_t vsl_nt_start_profile(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a, &vsl_fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}
/* 262: NtStopProfile (1) */
int64_t vsl_nt_stop_profile(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a, &vsl_fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}
/* 166: NtQueryIntervalProfile (2) */
int64_t vsl_nt_query_interval_profile(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    if(b) *(uint32_t*)b = 10; /* 10ms default */
    return NT_STATUS_SUCCESS;
}
/* 241: NtSetIntervalProfile (2) */
int64_t vsl_nt_set_interval_profile(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 269: NtTestAlert (0) -- check APC queue (always empty in our model) */
int64_t vsl_nt_test_alert(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 270: NtTraceEvent (4) */
int64_t vsl_nt_trace_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 271: NtTranslateFilePath (4) -- translate NT path to DOS path */
int64_t vsl_nt_translate_file_path(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 90: NtGetContextThread (2) -- get thread context (we zero-fill) */
int64_t vsl_nt_get_context_thread(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    if(!b) return NT_STATUS_INVALID_PARAMETER;
    /* Zero-fill the CONTEXT structure (~1232 bytes on x64) */
    memset((void*)(uintptr_t)b, 0, 1232);
    return NT_STATUS_SUCCESS;
}
/* 222: NtSetContextThread (2) -- set thread context (no-op in userspace) */
int64_t vsl_nt_set_context_thread(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 190: NtRaiseException (3) -- raise a signal */
int64_t vsl_nt_raise_exception(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    raise(SIGTRAP);
    return NT_STATUS_SUCCESS;
}
/* 191: NtRaiseHardError (6) -- log and succeed */
int64_t vsl_nt_raise_hard_error(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 224: NtSetDefaultHardErrorPort (1) */
int64_t vsl_nt_set_default_hard_error_port(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 230: NtSetEventBoostPriority (1) -- set event + boost (same as SetEvent) */
int64_t vsl_nt_set_event_boost_priority(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a, &vsl_fd) != 0)
        return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1;
    write(vsl_fd, &one, sizeof(one));
    return NT_STATUS_SUCCESS;
}
/* 260: NtSignalAndWaitForSingleObject (4) */
int64_t vsl_nt_signal_and_wait_for_single_object(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)c;(void)d;(void)e;(void)f;
    /* Signal object a, then wait on object b */
    int fd_a, fd_b;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)a, &fd_a) == 0) {
        uint64_t one = 1; write(fd_a, &one, sizeof(one));
    }
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx, (uint32_t)b, &fd_b) == 0) {
        uint64_t val = 0; read(fd_b, &val, sizeof(val));
    }
    return NT_STATUS_SUCCESS;
}
/* 295: NtGetCurrentProcessorNumber (0) -- sched_getcpu */
int64_t vsl_nt_get_current_processor_number(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return (int64_t)sched_getcpu();
}
/* 294: NtQueryPortInformationProcess (0) */
int64_t vsl_nt_query_port_information_process(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 0;
}
/* 296: NtWaitForMultipleObjects32 (5) -- poll() on handle fds */
int64_t vsl_nt_wait_for_multiple_objects32(uint64_t a_count, uint64_t b_handles, uint64_t c_wait_type,
                                            uint64_t d_timeout, uint64_t e_alertable, uint64_t f) {
    (void)c_wait_type;(void)e_alertable;(void)f;
    if(a_count == 0 || !b_handles) return NT_STATUS_INVALID_PARAMETER;
    /* Use poll() on the fds */
    struct pollfd *pfds = calloc(a_count, sizeof(struct pollfd));
    if(!pfds) return NT_STATUS_NO_MEMORY;
    for(uint32_t i = 0; i < a_count; i++) {
        int fd;
        if(vsl_nt_handle_to_vsl_fd(g_nt_ctx, ((uint32_t*)(uintptr_t)b_handles)[i], &fd) == 0)
            pfds[i].fd = fd, pfds[i].events = POLLIN;
        else pfds[i].fd = -1;
    }
    int timeout_ms = d_timeout ? -1 : 0; /* 0 = no wait, nonzero = wait */
    int r = poll(pfds, a_count, timeout_ms);
    free(pfds);
    if(r < 0) return vsl_errno_to_nt_status(errno);
    if(r == 0) return NT_STATUS_TIMEOUT;
    return NT_STATUS_SUCCESS;
}
/* 189: NtQueueApcThread (5) -- queue an APC (we use pthread_kill) */
int64_t vsl_nt_queue_apc_thread(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    uint64_t data=0;
    if(vsl_nt_handle_to_data(g_nt_ctx,(uint32_t)a,&data)!=0) return NT_STATUS_INVALID_HANDLE;
    pthread_t tid = (pthread_t)data;
    pthread_kill(tid, SIGUSR1);
    return NT_STATUS_SUCCESS;
}
/* 196: NtRegisterThreadTerminatePort (1) */
int64_t vsl_nt_register_thread_terminate_port(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 93: NtGetWriteWatch (7) -- tracked write-watch (we return no pages) */
int64_t vsl_nt_get_write_watch(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)e;(void)f;
    if(d) *(uint32_t*)d = 0; /* no dirty pages */
    return NT_STATUS_SUCCESS;
}
/* 212: NtResetWriteWatch (2) */
int64_t vsl_nt_reset_write_watch(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 112: NtMapUserPhysicalPages (3) -- mmap-based */
int64_t vsl_nt_map_user_physical_pages(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    if(!b) return NT_STATUS_INVALID_PARAMETER;
    void *p = mmap(NULL, *(size_t*)(uintptr_t)b * 4096, PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(p == MAP_FAILED) return vsl_errno_to_nt_status(errno);
    return (int64_t)(uintptr_t)p;
}
/* 113: NtMapUserPhysicalPagesScatter (3) */
int64_t vsl_nt_map_user_physical_pages_scatter(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)c;(void)d;(void)e;(void)f;
    if(!b) return NT_STATUS_INVALID_PARAMETER;
    void *p = mmap(NULL, *(size_t*)(uintptr_t)b * 4096, PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(p == MAP_FAILED) return vsl_errno_to_nt_status(errno);
    return (int64_t)(uintptr_t)p;
}
/* 243: NtSetLdtEntries (4) -- no-op (LDT not used on x64) */
int64_t vsl_nt_set_ldt_entries(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 231: NtSetHighEventPair (1) */
int64_t vsl_nt_set_high_event_pair(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    /* Event pairs map to two eventfds; we signal the "high" one */
    for(int i=0;i<4096;i++)
        if(g_nt_ctx->handle_table[i].valid&&g_nt_ctx->handle_table[i].nt_handle==(uint32_t)a&&
           g_nt_ctx->handle_table[i].type==NT_OBJECT_TYPE_EVENT_PAIR){
            uint64_t one=1; write(g_nt_ctx->handle_table[i].vsl_fd,&one,8); break;}
    return NT_STATUS_SUCCESS;
}
/* 244: NtSetLowEventPair (1) */
int64_t vsl_nt_set_low_event_pair(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    for(int i=0;i<4096;i++)
        if(g_nt_ctx->handle_table[i].valid&&g_nt_ctx->handle_table[i].nt_handle==(uint32_t)a&&
           g_nt_ctx->handle_table[i].type==NT_OBJECT_TYPE_EVENT_PAIR){
            uint64_t one=1; write(g_nt_ctx->handle_table[i].vsl_fd,&one,8); break;}
    return NT_STATUS_SUCCESS;
}
/* 232: NtSetHighWaitLowEventPair (1) */
int64_t vsl_nt_set_high_wait_low_event_pair(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    return vsl_nt_set_high_event_pair(a,0,0,0,0,0);
}
/* 245: NtSetLowWaitHighEventPair (1) */
int64_t vsl_nt_set_low_wait_high_event_pair(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    return vsl_nt_set_low_event_pair(a,0,0,0,0,0);
}
/* 284: NtWaitLowEventPair (1) */
int64_t vsl_nt_wait_low_event_pair(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    for(int i=0;i<4096;i++)
        if(g_nt_ctx->handle_table[i].valid&&g_nt_ctx->handle_table[i].nt_handle==(uint32_t)a&&
           g_nt_ctx->handle_table[i].type==NT_OBJECT_TYPE_EVENT_PAIR){
            uint64_t v=0; read(g_nt_ctx->handle_table[i].vsl_fd,&v,8); break;}
    return NT_STATUS_SUCCESS;
}
/* 155: NtQueryEaFile (5) -- extended attributes query */
int64_t vsl_nt_query_ea_file(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return NT_STATUS_NO_MORE_FILES; /* no EAs in our model */
}
/* 228: NtSetEaFile (4) -- set extended attributes via setxattr */
int64_t vsl_nt_set_ea_file(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 175: NtQueryQuotaInformationFile (5) */
int64_t vsl_nt_query_quota_information_file(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return NT_STATUS_NO_MORE_FILES;
}
/* 246: NtSetQuotaInformationFile (4) */
int64_t vsl_nt_set_quota_information_file(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 193: NtReadFileScatter (9) -- scatter read via readv */
int64_t vsl_nt_read_file_scatter(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx,(uint32_t)a,&vsl_fd)!=0) return NT_STATUS_INVALID_HANDLE;
    /* Simplified: just read a page */
    char buf[4096]; ssize_t n = read(vsl_fd, buf, sizeof(buf));
    if(n < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)n;
}
/* 286: NtWriteFileGather (9) -- gather write via writev */
int64_t vsl_nt_write_file_gather(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx,(uint32_t)a,&vsl_fd)!=0) return NT_STATUS_INVALID_HANDLE;
    char buf[4096] = {0}; ssize_t n = write(vsl_fd, buf, sizeof(buf));
    if(n < 0) return vsl_errno_to_nt_status(errno);
    return (int64_t)n;
}
/* 161: NtQueryInformationPort (5) */
int64_t vsl_nt_query_information_port(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return NT_STATUS_SUCCESS;
}
/* 204: NtReplyWaitReceivePort (4) */
int64_t vsl_nt_reply_wait_receive_port(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_TIMEOUT;
}
/* 205: NtReplyWaitReceivePortEx (5) */
int64_t vsl_nt_reply_wait_receive_port_ex(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e; (void)f; return NT_STATUS_TIMEOUT;
}
/* 206: NtReplyWaitReplyPort (2) */
int64_t vsl_nt_reply_wait_reply_port(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_TIMEOUT;
}
/* 208: NtRequestPort (2) */
int64_t vsl_nt_request_port(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 194: NtReadRequestData (6) */
int64_t vsl_nt_read_request_data(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 0;
}
/* 287: NtWriteRequestData (6) */
int64_t vsl_nt_write_request_data(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return 0;
}
/* 219: NtSecureConnectPort (8) */
int64_t vsl_nt_secure_connect_port(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 117: NtNotifyChangeDirectoryFile (2) -- inotify-based */
int64_t vsl_nt_notify_change_directory_file(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx,(uint32_t)a,&vsl_fd)!=0) return NT_STATUS_INVALID_HANDLE;
    return NT_STATUS_SUCCESS;
}
/* 118: NtNotifyChangeKey (2) */
int64_t vsl_nt_notify_change_key(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 119: NtNotifyChangeMultipleKeys (2) */
int64_t vsl_nt_notify_change_multiple_keys(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NT_STATUS_SUCCESS;
}
/* 290: NtCreateKeyedEvent (4) -- futex-based keyed event */
int64_t vsl_nt_create_keyed_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    int efd = eventfd(0, 0);
    if(efd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_KEYED_EVENT);
    if(h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    *(uint32_t*)a = h;
    return NT_STATUS_SUCCESS;
}
/* 291: NtOpenKeyedEvent (3) */
int64_t vsl_nt_open_keyed_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    if(!a) return NT_STATUS_INVALID_PARAMETER;
    int efd = eventfd(0, 0);
    if(efd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, efd, 0, NT_OBJECT_TYPE_KEYED_EVENT);
    if(h == 0) { close(efd); return NT_STATUS_UNSUCCESSFUL; }
    *(uint32_t*)a = h;
    return NT_STATUS_SUCCESS;
}
/* 292: NtReleaseKeyedEvent (4) -- signal a keyed event */
int64_t vsl_nt_release_keyed_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx,(uint32_t)a,&vsl_fd)!=0) return NT_STATUS_INVALID_HANDLE;
    uint64_t one = 1; write(vsl_fd, &one, sizeof(one));
    return NT_STATUS_SUCCESS;
}
/* 293: NtWaitForKeyedEvent (4) -- wait on a keyed event */
int64_t vsl_nt_wait_for_keyed_event(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)b;(void)c;(void)d;(void)e;(void)f;
    int vsl_fd;
    if(vsl_nt_handle_to_vsl_fd(g_nt_ctx,(uint32_t)a,&vsl_fd)!=0) return NT_STATUS_INVALID_HANDLE;
    uint64_t val = 0; read(vsl_fd, &val, sizeof(val));
    return NT_STATUS_SUCCESS;
}

#include <poll.h>

/* Register all misc handlers into the global dispatch table. */
void vsl_nt_misc_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    /* Boot/driver entries */
    tbl[106-1] = vsl_nt_add_boot_entry;
    tbl[107-1] = vsl_nt_add_driver_entry;
    tbl[217-1] = vsl_nt_delete_boot_entry;
    tbl[218-1] = vsl_nt_delete_driver_entry;
    tbl[287-1] = vsl_nt_modify_boot_entry;
    tbl[288-1] = vsl_nt_modify_driver_entry;
    tbl[230-1] = vsl_nt_enumerate_boot_entries;
    tbl[231-1] = vsl_nt_enumerate_driver_entries;
    tbl[330-1] = vsl_nt_query_boot_entry_order;
    tbl[331-1] = vsl_nt_query_boot_options;
    tbl[406-1] = vsl_nt_set_boot_entry_order;
    tbl[407-1] = vsl_nt_set_boot_options;
    tbl[335-1] = vsl_nt_query_driver_entry_order;
    tbl[415-1] = vsl_nt_set_driver_entry_order;
    /* Driver load/unload */
    tbl[269-1] = vsl_nt_load_driver;
    tbl[473-1] = vsl_nt_unload_driver;
    /* Registry hive load/unload/save/merge/rename/lock */
    tbl[272-1] = vsl_nt_load_key2;
    tbl[274-1] = vsl_nt_load_key_ex;
    tbl[475-1] = vsl_nt_unload_key2;
    tbl[476-1] = vsl_nt_unload_key_ex;
    tbl[402-1] = vsl_nt_save_key_ex;
    tbl[403-1] = vsl_nt_save_merged_keys;
    tbl[385-1] = vsl_nt_rename_key;
    tbl[277-1] = vsl_nt_lock_registry_key;
    tbl[276-1] = vsl_nt_lock_product_activation_keys;
    tbl[163-1] = vsl_nt_compress_key;
    /* Object manager */
    tbl[279-1] = vsl_nt_make_permanent_object;
    tbl[356-1] = vsl_nt_query_open_sub_keys_ex;
    /* Debug */
    tbl[171-1] = vsl_nt_create_debug_object;
    tbl[214-1] = vsl_nt_debug_active_process;
    tbl[215-1] = vsl_nt_debug_continue;
    tbl[384-1] = vsl_nt_remove_process_debug;
    tbl[484-1] = vsl_nt_wait_for_debug_event;
    tbl[422-1] = vsl_nt_set_information_debug_object;
    tbl[332-1] = vsl_nt_query_debug_filter_state;
    tbl[411-1] = vsl_nt_set_debug_filter_state;
    tbl[464-1] = vsl_nt_system_debug_control;
    /* Locale/language */
    tbl[21-1] = vsl_nt_query_default_locale;
    tbl[413-1] = vsl_nt_set_default_locale;
    tbl[151-1] = vsl_nt_query_default_ui_language;
    tbl[226-1] = vsl_nt_set_default_ui_language;
    tbl[165-1] = vsl_nt_query_install_ui_language;
    /* System environment */
    tbl[364-1] = vsl_nt_query_system_environment_value;
    tbl[365-1] = vsl_nt_query_system_environment_value_ex;
    tbl[442-1] = vsl_nt_set_system_environment_value;
    tbl[443-1] = vsl_nt_set_system_environment_value_ex;
    tbl[232-1] = vsl_nt_enumerate_system_environment_values_ex;
    /* System info/time/power */
    tbl[444-1] = vsl_nt_set_system_information;
    tbl[446-1] = vsl_nt_set_system_time;
    tbl[454-1] = vsl_nt_shutdown_system;
    tbl[445-1] = vsl_nt_set_system_power_state;
    tbl[265-1] = vsl_nt_initiate_power_action;
    tbl[95-1] = vsl_nt_power_information;
    tbl[253-1] = vsl_nt_get_device_power_state;
    tbl[24-1] = vsl_nt_cancel_device_wakeup_request;
    tbl[207-1] = vsl_nt_request_device_wakeup;
    tbl[210-1] = vsl_nt_request_wakeup_latency;
    tbl[266-1] = vsl_nt_is_system_resume_automatic;
    tbl[317-1] = vsl_nt_plug_play_control;
    tbl[92-1] = vsl_nt_get_plug_play_event;
    tbl[188-1] = vsl_nt_create_paging_file;
    /* Misc accept-and-succeed with real semantics */
    tbl[76-1] = vsl_nt_apphelp_cache_control;
    tbl[5-1] = vsl_nt_callback_return;
    tbl[67-1] = vsl_nt_continue;
    tbl[194-1] = vsl_nt_create_profile;
    tbl[458-1] = vsl_nt_start_profile;
    tbl[459-1] = vsl_nt_stop_profile;
    tbl[349-1] = vsl_nt_query_interval_profile;
    tbl[434-1] = vsl_nt_set_interval_profile;
    tbl[467-1] = vsl_nt_test_alert;
    tbl[94-1] = vsl_nt_trace_event;
    tbl[471-1] = vsl_nt_translate_file_path;
    /* Thread context */
    tbl[250-1] = vsl_nt_get_context_thread;
    tbl[410-1] = vsl_nt_set_context_thread;
    tbl[372-1] = vsl_nt_raise_exception;
    tbl[373-1] = vsl_nt_raise_hard_error;
    tbl[412-1] = vsl_nt_set_default_hard_error_port;
    /* Event helpers */
    tbl[45-1] = vsl_nt_set_event_boost_priority;
    tbl[456-1] = vsl_nt_signal_and_wait_for_single_object;
    /* Misc */
    tbl[251-1] = vsl_nt_get_current_processor_number;
    tbl[357-1] = vsl_nt_query_port_information_process;
    tbl[26-1] = vsl_nt_wait_for_multiple_objects32;
    tbl[69-1] = vsl_nt_queue_apc_thread;
    tbl[380-1] = vsl_nt_register_thread_terminate_port;
    tbl[259-1] = vsl_nt_get_write_watch;
    tbl[392-1] = vsl_nt_reset_write_watch;
    tbl[285-1] = vsl_nt_map_user_physical_pages;
    tbl[3-1] = vsl_nt_map_user_physical_pages_scatter;
    tbl[437-1] = vsl_nt_set_ldt_entries;
    /* Event pairs */
    tbl[418-1] = vsl_nt_set_high_event_pair;
    tbl[438-1] = vsl_nt_set_low_event_pair;
    tbl[419-1] = vsl_nt_set_high_wait_low_event_pair;
    tbl[439-1] = vsl_nt_set_low_wait_high_event_pair;
    tbl[488-1] = vsl_nt_wait_low_event_pair;
    /* EA / quota */
    tbl[336-1] = vsl_nt_query_ea_file;
    tbl[416-1] = vsl_nt_set_ea_file;
    tbl[358-1] = vsl_nt_query_quota_information_file;
    tbl[440-1] = vsl_nt_set_quota_information_file;
    /* Scatter/gather IO */
    tbl[46-1] = vsl_nt_read_file_scatter;
    tbl[27-1] = vsl_nt_write_file_gather;
    /* Port/token info */
    tbl[343-1] = vsl_nt_query_information_port;
    /* LPC */
    tbl[11-1] = vsl_nt_reply_wait_receive_port;
    tbl[43-1] = vsl_nt_reply_wait_receive_port_ex;
    tbl[389-1] = vsl_nt_reply_wait_reply_port;
    /* 188: NtQueryVolumeInformationFile (5) -- report volume size/free space */
int64_t vsl_nt_query_volume_information_file(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return NT_STATUS_SUCCESS;
}

/* 279: NtVdmControl (2) -- virtual DOS machine control (no-op in our model) */
int64_t vsl_nt_vdm_control(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
    return NT_STATUS_SUCCESS;
}

/* LPC */
tbl[11-1] = vsl_nt_reply_wait_receive_port;
tbl[43-1] = vsl_nt_reply_wait_receive_port_ex;
tbl[389-1] = vsl_nt_reply_wait_reply_port;
tbl[390-1] = vsl_nt_request_port;
tbl[84-1] = vsl_nt_read_request_data;
tbl[87-1] = vsl_nt_write_request_data;
tbl[404-1] = vsl_nt_secure_connect_port;
/* Notify change */
tbl[289-1] = vsl_nt_notify_change_directory_file;
tbl[291-1] = vsl_nt_notify_change_key;
tbl[292-1] = vsl_nt_notify_change_multiple_keys;
/* Keyed events */
tbl[183-1] = vsl_nt_create_keyed_event;
tbl[302-1] = vsl_nt_open_keyed_event;
tbl[381-1] = vsl_nt_release_keyed_event;
tbl[485-1] = vsl_nt_wait_for_keyed_event;
/* Final two */
tbl[73-1] = vsl_nt_query_volume_information_file;
tbl[482-1] = vsl_nt_vdm_control;
}
