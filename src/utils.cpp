#include <string>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <malloc.h>

#include <utils/logger.h>
#include <wups.h>

#include "utils.h"
#include <utils/utils.h>
#include <utils/logger.h>
#include "common/retain_vars.h"
#include "utils/overlay_helper.h"
#include "utils/mem_utils.h"
#include "kernel/kernel_utils.h"

void CallHook(wups_loader_hook_type_t hook_type) {
    CallHookEx(hook_type,-1);
}

bool HasHookCallHook(wups_loader_hook_type_t hook_type) {
    for(int32_t plugin_index=0; plugin_index<gbl_replacement_data.number_used_plugins; plugin_index++) {
        replacement_data_plugin_t * plugin_data = &gbl_replacement_data.plugin_data[plugin_index];

        for(int32_t j=0; j<plugin_data->number_used_hooks; j++) {
            replacement_data_hook_t * hook_data = &plugin_data->hooks[j];
            if(hook_data->type == hook_type) {
                return true;
            }
        }
    }
    return false;
}

 static const char** hook_names = (const char *[]){
                                 "WUPS_LOADER_HOOK_INIT_OVERLAY",
                                 "WUPS_LOADER_HOOK_INIT_KERNEL",
                                 "WUPS_LOADER_HOOK_INIT_VID_MEM",
                                 "WUPS_LOADER_HOOK_INIT_WUT_MALLOC",
                                 "WUPS_LOADER_HOOK_FINI_WUT_MALLOC",
                                 "WUPS_LOADER_HOOK_INIT_WUT_DEVOPTAB",
                                 "WUPS_LOADER_HOOK_FINI_WUT_DEVOPTAB",
                                 "WUPS_LOADER_HOOK_INIT_WUT_NEWLIB",
                                 "WUPS_LOADER_HOOK_FINI_WUT_NEWLIB",
                                 "WUPS_LOADER_HOOK_INIT_WUT_STDCPP",
                                 "WUPS_LOADER_HOOK_FINI_WUT_STDCPP",
                                 "WUPS_LOADER_HOOK_INIT_PLUGIN",
                                 "WUPS_LOADER_HOOK_DEINIT_PLUGIN",
                                 "WUPS_LOADER_HOOK_APPLICATION_START",
                                 "WUPS_LOADER_HOOK_FUNCTIONS_PATCHED",
                                 "WUPS_LOADER_HOOK_RELEASE_FOREGROUND",
                                 "WUPS_LOADER_HOOK_ACQUIRED_FOREGROUND",
                                 "WUPS_LOADER_HOOK_APPLICATION_END",
                                 "WUPS_LOADER_HOOK_CONFIRM_RELEASE_FOREGROUND",
                                 "WUPS_LOADER_HOOK_SAVES_DONE_READY_TO_RELEASE",
                                 "WUPS_LOADER_HOOK_VSYNC",
                                 "WUPS_LOADER_HOOK_GET_CONFIG",
                                 "WUPS_LOADER_HOOK_VID_DRC_DRAW",
                                 "WUPS_LOADER_HOOK_VID_TV_DRAW",
                                 "WUPS_LOADER_HOOK_APPLET_START"};

void CallHookEx(wups_loader_hook_type_t hook_type, int32_t plugin_index_needed) {
    for(int32_t plugin_index=0; plugin_index<gbl_replacement_data.number_used_plugins; plugin_index++) {
        replacement_data_plugin_t * plugin_data = &gbl_replacement_data.plugin_data[plugin_index];
        if(plugin_index_needed != -1 && plugin_index_needed != plugin_index) {
            continue;
        }

        //DEBUG_FUNCTION_LINE("Checking hook functions for %s.\n",plugin_data->plugin_name);
        //DEBUG_FUNCTION_LINE("Found hooks: %d\n",plugin_data->number_used_hooks);
        for(int32_t j=0; j<plugin_data->number_used_hooks; j++) {
            replacement_data_hook_t * hook_data = &plugin_data->hooks[j];
            if(hook_data->type == hook_type) {
                DEBUG_FUNCTION_LINE("Calling hook of type %s for plugin %s\n",hook_names[hook_data->type],plugin_data->plugin_name);
                void * func_ptr = hook_data->func_pointer;
                //TODO: Switch cases depending on arguments etc.
                // Adding arguments!
                if(func_ptr != NULL) {
                    //DEBUG_FUNCTION_LINE("function pointer is %08x\n",func_ptr);
                   if(hook_type == WUPS_LOADER_HOOK_INIT_OVERLAY) {
                        /*wups_loader_init_overlay_args_t args;
                        args.overlayfunction_ptr = &overlay_helper;
                        args.textureconvertfunction_ptr = &TextureUtils::convertImageToTexture;
                        args.drawtexturefunction_ptr = (void (*)(void*,void*,float,float,int32_t,int32_t,float)) &TextureUtils::drawTexture;
                        ((void (*)(wups_loader_init_overlay_args_t))((uint32_t*)func_ptr) )(args);*/
                    } else if(hook_type == WUPS_LOADER_HOOK_INIT_PLUGIN) {
                        ((void (*)(void))((uint32_t*)func_ptr) )();
                    } else if(hook_type == WUPS_LOADER_HOOK_DEINIT_PLUGIN) {
                        ((void (*)(void))((uint32_t*)func_ptr) )();
                    } else if(hook_type == WUPS_LOADER_HOOK_APPLICATION_START) {
                        wups_loader_app_started_args_t args;
                        memset(&args,0,sizeof(args));
                        if(plugin_data->kernel_allowed && plugin_data->kernel_init_done) {
                            args.kernel_access = true;
                        }
                        ((void (*)(wups_loader_app_started_args_t))((uint32_t*)func_ptr) )(args);
                    } else if(hook_type == WUPS_LOADER_HOOK_FUNCTIONS_PATCHED) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_APPLICATION_END) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_VSYNC) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_RELEASE_FOREGROUND) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_ACQUIRED_FOREGROUND) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_APPLET_START) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_INIT_WUT_MALLOC) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_FINI_WUT_MALLOC) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_INIT_WUT_DEVOPTAB) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_FINI_WUT_DEVOPTAB) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_INIT_WUT_NEWLIB) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_FINI_WUT_NEWLIB) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_INIT_WUT_STDCPP) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_FINI_WUT_STDCPP) {
                        ((void (*)(void))((uint32_t*)func_ptr))();
                    } else if(hook_type == WUPS_LOADER_HOOK_INIT_KERNEL) {
                        // Only call the hook if kernel is allowed.
                        if(plugin_data->kernel_allowed) {
                            wups_loader_init_kernel_args_t args;
                            args.kern_read_ptr = &kern_read;
                            args.kern_write_ptr = &kern_write;
                            args.kern_copy_data_ptr = &KernelCopyData;
                            ((void (*)(wups_loader_init_kernel_args_t))((uint32_t*)func_ptr) )(args);
                            plugin_data->kernel_init_done = true;
                        }
                    } else if(hook_type == WUPS_LOADER_HOOK_INIT_VID_MEM) {
                        wups_loader_init_vid_mem_args_t args;
                        args.vid_mem_alloc_ptr = &MemoryUtils::alloc;
                        args.vid_mem_free_ptr = &MemoryUtils::free;
                        ((void (*)(wups_loader_init_vid_mem_args_t))((uint32_t*)func_ptr) )(args);
                    } else if(hook_type == WUPS_LOADER_HOOK_VID_DRC_DRAW) {
                        /*wups_loader_vid_buffer_t args;
                        args.color_buffer_ptr = &g_vid_main_cbuf;
                        args.tv_texture_ptr = &g_vid_tvTex;
                        args.drc_texture_ptr = &g_vid_drcTex;
                        args.sampler_ptr = &g_vid_sampler;
                        ((void (*)(wups_loader_vid_buffer_t))((uint32_t*)func_ptr) )(args);*/
                    } else if(hook_type == WUPS_LOADER_HOOK_VID_TV_DRAW) {
                        /*wups_loader_vid_buffer_t args;
                        args.color_buffer_ptr = &g_vid_main_cbuf;
                        args.tv_texture_ptr = &g_vid_tvTex;
                        args.drc_texture_ptr = &g_vid_drcTex;
                        args.sampler_ptr = &g_vid_sampler;
                        ((void (*)(wups_loader_vid_buffer_t))((uint32_t*)func_ptr) )(args);*/
                    } else {
                        DEBUG_FUNCTION_LINE("ERROR: HOOK TYPE WAS NOT IMPLEMENTED %08X \n",hook_type);
                    }
                } else {
                    DEBUG_FUNCTION_LINE("Failed to call hook. It was not defined\n");
                }
            }
        }
    }
}
