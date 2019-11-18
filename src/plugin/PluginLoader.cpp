/* based on module.c
 *   by Alex Chadwick
 *
 * Copyright (C) 2014, Alex Chadwick
 * Modified 2018, Maschell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "ElfTools.h"
#include "PluginData.h"
#include "PluginLoader.h"
#include "DynamicLinkingHelper.h"
#include "utils/StringTools.h"
#include "common/retain_vars.h"

PluginLoader * PluginLoader::instance = NULL;

std::vector<PluginInformation *> PluginLoader::getPluginInformation(const char * path) {
    std::vector<PluginInformation *> result;
    struct dirent *dp;
    DIR *dfd = NULL;

    if(path == NULL) {
        DEBUG_FUNCTION_LINE("Path was NULL\n");
        return result;
    }

    if ((dfd = opendir(path)) == NULL) {
        DEBUG_FUNCTION_LINE("Couldn't open dir %s\n",path);
        return result;
    }

    while ((dp = readdir(dfd)) != NULL) {
        struct stat stbuf ;
        std::string full_file_path = StringTools::strfmt("%s/%s",path,dp->d_name);
        StringTools::RemoveDoubleSlashs(full_file_path);
        if( stat(full_file_path.c_str(),&stbuf ) == -1 ) {
            DEBUG_FUNCTION_LINE("Unable to stat file: %s\n",full_file_path.c_str()) ;
            continue;
        }

        if ( ( stbuf.st_mode & S_IFMT ) == S_IFDIR ) { // Skip directories
            continue;
        } else {
            DEBUG_FUNCTION_LINE("Found file: %s\n",full_file_path.c_str()) ;
            PluginInformation * plugin = PluginInformation::loadPluginInformation(full_file_path);
            if(plugin != NULL) {
                DEBUG_FUNCTION_LINE("Found plugin %s by %s. Built on %s Size: %d kb \n",plugin->getName().c_str(),plugin->getAuthor().c_str(),plugin->getBuildTimestamp().c_str(),plugin->getSize()/1024) ;
                DEBUG_FUNCTION_LINE("Description: %s \n",plugin->getDescription().c_str()) ;
                result.push_back(plugin);
            } else {
                DEBUG_FUNCTION_LINE("%s is not a valid plugin\n",full_file_path.c_str()) ;
            }
        }
    }
    if(dfd != NULL) {
        closedir(dfd);
    }

    return result;
}

std::vector<PluginInformation *> PluginLoader::getPluginsLoadedInMemory() {
    std::vector<PluginInformation *> pluginInformation;
    for(int32_t i = 0; i < gbl_replacement_data.number_used_plugins; i++) {
        replacement_data_plugin_t * pluginInfo = &gbl_replacement_data.plugin_data[i];
        PluginInformation * curPlugin = PluginInformation::loadPluginInformation(pluginInfo->path);
        if(curPlugin != NULL) {
            pluginInformation.push_back(curPlugin);
        }
    }
    return pluginInformation;
}

bool PluginLoader::loadAndLinkPlugins(std::vector<PluginInformation *> pluginInformation) {
    std::vector<PluginData *> loadedPlugins;
    bool success = true;
    for(size_t i = 0; i < pluginInformation.size(); i++) {
        PluginInformation * cur_info = pluginInformation[i];
        PluginData * pluginData = loadAndLinkPlugin(cur_info);
        if(pluginData == NULL) {
            DEBUG_FUNCTION_LINE("loadAndLinkPlugins failed for %d\n",i) ;
            success = false;
            continue;
        } else {
            loadedPlugins.push_back(pluginData);
        }
    }

    PluginLoader::flushCache();

    if(success) {
        copyPluginDataIntoGlobalStruct(loadedPlugins);
    } else {
        PluginLoader::resetPluginLoader();
        memset((void*)&gbl_replacement_data,0,sizeof(gbl_replacement_data));
    }

    clearPluginData(loadedPlugins);

    return success;
}

void PluginLoader::clearPluginData(std::vector<PluginData *> pluginData) {
    for(size_t i = 0; i < pluginData.size(); i++) {
        PluginData * curPluginData = pluginData[i];
        if(curPluginData != NULL) {
            delete curPluginData;
        }
    }
}

void PluginLoader::clearPluginInformation(std::vector<PluginInformation *> pluginInformation) {
    for(size_t i = 0; i < pluginInformation.size(); i++) {
        PluginInformation * curPluginInformation = pluginInformation[i];
        if(curPluginInformation != NULL) {
            delete curPluginInformation;
        }
    }
}

PluginData * PluginLoader::loadAndLinkPlugin(PluginInformation * pluginInformation) {
    PluginData * result = NULL;
    int32_t fd = -1;
    Elf *elf = NULL;

    if(pluginInformation == NULL) {
        DEBUG_FUNCTION_LINE("pluginInformation was NULL\n");
        goto exit_error;
    }

    if(pluginInformation->getSize() > ((uint32_t) getAvailableSpace())) {
        DEBUG_FUNCTION_LINE("Not enough space left to loader the plugin into memory %08X %08X\n",pluginInformation->getSize(),getAvailableSpace());
        goto exit_error;
    }

    /* check for compile errors */
    if (elf_version(EV_CURRENT) == EV_NONE) {
        goto exit_error;
    }

    fd = open(pluginInformation->getPath().c_str(), O_RDONLY, 0);

    if (fd == -1) {
        DEBUG_FUNCTION_LINE("failed to open '%s' \n", pluginInformation->getPath().c_str());
        goto exit_error;
    }

    elf = elf_begin(fd, ELF_C_READ, NULL);

    if (elf == NULL) {
        DEBUG_FUNCTION_LINE("elf was NULL\n");
        goto exit_error;
    }

    result = new PluginData(pluginInformation);
    if(result == NULL) {
        DEBUG_FUNCTION_LINE("Failed to create object\n");
        goto exit_error;
    }

    if(!this->loadAndLinkElf(result, elf, this->getCurrentStoreAddress())) {
        delete result;
        result = NULL;
    }

exit_error:
    if (elf != NULL) {
        elf_end(elf);
    }
    if (fd != -1) {
        close(fd);
    }
    return result;
}

bool PluginLoader::loadAndLinkElf(PluginData * pluginData, Elf *elf, void * startAddress) {
    if(pluginData == NULL || elf == NULL || startAddress == NULL) {
        return false;
    }

    uint32_t curAddress = (uint32_t) startAddress;
    uint32_t firstCurAddress = (uint32_t) startAddress;

    Elf_Scn *scn;
    size_t symtab_count, section_count, shstrndx, symtab_strndx, entries_count, hooks_count;
    Elf32_Sym *symtab = NULL;
    uint8_t **destinations = NULL;
    wups_loader_entry_t *entries = NULL;
    wups_loader_hook_t *hooks = NULL;
    bool result = false;

    int32_t i = 1;

    std::vector<wups_loader_entry_t *> entry_t_list;
    std::vector<wups_loader_hook_t *> hook_t_list;

    std::vector<FunctionData *> function_data_list;
    std::vector<HookData *> hook_data_list;

    if (!ElfTools::loadElfSymtab(elf, &symtab, &symtab_count, &symtab_strndx)) {
        goto exit_error;
    }

    if(symtab == NULL) {
        goto exit_error;
    }

    if (elf_getshdrnum(elf, &section_count) != 0) {
        goto exit_error;
    }
    if (elf_getshdrstrndx(elf, &shstrndx) != 0) {
        goto exit_error;
    }

    destinations = (uint8_t **) malloc(sizeof(uint8_t *) * section_count);

    DEBUG_FUNCTION_LINE("Copy sections\n");

    for (scn = elf_nextscn(elf, NULL); scn != NULL; scn = elf_nextscn(elf, scn)) {
        Elf32_Shdr *shdr;

        shdr = elf32_getshdr(scn);
        if (shdr == NULL) {
            continue;
        }

        if ((shdr->sh_type == SHT_PROGBITS || shdr->sh_type == SHT_NOBITS) &&
                (shdr->sh_flags & SHF_ALLOC)) {

            const char *name;

            destinations[elf_ndxscn(scn)] = NULL;

            name = elf_strptr(elf, shstrndx, shdr->sh_name);
            if (name == NULL) {
                DEBUG_FUNCTION_LINE("name is null\n");
                continue;
            }

            if (strcmp(name, ".wups.meta") == 0) {
                continue;
            } else if (strcmp(name, ".wups.load") == 0) {
                if (entries != NULL) {
                    DEBUG_FUNCTION_LINE("entries != NULL\n");
                    goto exit_error;
                }

                entries_count = shdr->sh_size / sizeof(wups_loader_entry_t);
                entries = (wups_loader_entry_t *) malloc(sizeof(wups_loader_entry_t) * entries_count);

                if (entries == NULL) {
                    DEBUG_FUNCTION_LINE("entries == NULL\n");
                    goto exit_error;
                }

                // We need to subtract the sh_addr because it will be added later in the relocations.
                destinations[elf_ndxscn(scn)] = (uint8_t *)entries - (shdr->sh_addr);

                if (!ElfTools::elfLoadSection(elf, scn, shdr, entries)) {
                    DEBUG_FUNCTION_LINE("elfLoadSection failed\n");
                    goto exit_error;
                }

                ElfTools::elfLoadSymbols(elf_ndxscn(scn), entries- (shdr->sh_addr), symtab, symtab_count);

                for(size_t i = 0; i< entries_count; i++) {
                    entry_t_list.push_back(&entries[i]);
                }
            } else if (strcmp(name, ".wups.hooks") == 0) {
                if (hooks != NULL) {
                    DEBUG_FUNCTION_LINE("hooks != NULL\n");
                    goto exit_error;
                }

                hooks_count = shdr->sh_size / sizeof(wups_loader_hook_t);
                hooks = (wups_loader_hook_t *) malloc(sizeof(wups_loader_hook_t) * hooks_count);

                if (hooks == NULL) {
                    DEBUG_FUNCTION_LINE("hooks == NULL\n");
                    goto exit_error;
                }

                // We need to subtract the sh_addr because it will be added later in the relocations.
                uint32_t destination = (uint32_t)hooks - (shdr->sh_addr);
                destinations[elf_ndxscn(scn)] = (uint8_t *)destination;
                if (!ElfTools::elfLoadSection(elf, scn, shdr, hooks)) {
                    DEBUG_FUNCTION_LINE("elfLoadSection failed\n");
                    goto exit_error;
                }
                ElfTools::elfLoadSymbols(elf_ndxscn(scn), (void *) destination, symtab, symtab_count);

                for(size_t i = 0; i< hooks_count; i++) {
                    hook_t_list.push_back(&hooks[i]);
                }

            } else {
                uint32_t destination = firstCurAddress + shdr->sh_addr;
                destinations[elf_ndxscn(scn)] = (uint8_t *) firstCurAddress;

                if((uint32_t) destination + shdr->sh_size > (uint32_t) this->endAddress) {
                    DEBUG_FUNCTION_LINE("Not enough space to load function %s into memory at %08X.\n",name,destination);
                    goto exit_error;
                }

                DEBUG_FUNCTION_LINE("Copy section %s to %08X\n",name,destination);
                if (!ElfTools::elfLoadSection(elf, scn, shdr, (void*) destination)) {
                    DEBUG_FUNCTION_LINE("elfLoadSection failed\n");
                    goto exit_error;
                }
                ElfTools::elfLoadSymbols(elf_ndxscn(scn), (void*) firstCurAddress, symtab, symtab_count);

                if(strcmp(name, ".bss") == 0){
                    pluginData->setBSSLocation(destination, shdr->sh_size);
                    DEBUG_FUNCTION_LINE("Saved .bss section info. Location: %08X size: %08X\n", destination, shdr->sh_size);
                }

                curAddress = ROUNDUP(destination + shdr->sh_size,0x100);
            }
        }
    }

    for (scn = elf_nextscn(elf, NULL); scn != NULL; scn = elf_nextscn(elf, scn)) {
        Elf32_Shdr *shdr;

        shdr = elf32_getshdr(scn);
        if (shdr == NULL) {
            continue;
        }
        const char *name;

        name = elf_strptr(elf, shstrndx, shdr->sh_name);
        if (name == NULL) {
            DEBUG_FUNCTION_LINE("name is null\n");
            continue;
        }
        if(shdr->sh_type == 0x80000002) {
            ImportRPLInformation * info = ImportRPLInformation::createImportRPLInformation(i,name);
            if(info != NULL) {
                pluginData->addImportRPLInformation(info);
            }
        }
        i++;
    }
    i = 0;

    for (scn = elf_nextscn(elf, NULL); scn != NULL; scn = elf_nextscn(elf, scn)) {
        Elf32_Shdr *shdr;

        shdr = elf32_getshdr(scn);
        if (shdr == NULL) {
            continue;
        }

        const char *name;

        name = elf_strptr(elf, shstrndx, shdr->sh_name);
        if (name == NULL) {
            DEBUG_FUNCTION_LINE("name is null\n");
            continue;
        }
        if ((shdr->sh_type == SHT_PROGBITS || shdr->sh_type == SHT_NOBITS) &&
                (shdr->sh_flags & SHF_ALLOC) &&
                destinations[elf_ndxscn(scn)] != NULL) {

            DEBUG_FUNCTION_LINE("Linking (%d)... %s\n",i++,name);
            if (!ElfTools::elfLink(elf, elf_ndxscn(scn), destinations[elf_ndxscn(scn)], symtab, symtab_count, symtab_strndx, true, pluginData)) {
                DEBUG_FUNCTION_LINE("elfLink failed\n");
                goto exit_error;
            }
        }
    }
    DEBUG_FUNCTION_LINE("Linking done \n");

    for(size_t j=0; j<hook_t_list.size(); j++) {
        wups_loader_hook_t * hook = hook_t_list[j];

        DEBUG_FUNCTION_LINE("Saving hook of plugin \"%s\". Type: %08X, target: %08X\n",pluginData->getPluginInformation()->getName().c_str(),hook->type,(void*) hook->target);
        HookData * hook_data = new HookData((void *) hook->target,hook->type);
        pluginData->addHookData(hook_data);
    }

    for(size_t j=0; j<entry_t_list.size(); j++) {
        wups_loader_entry_t * cur_function = entry_t_list[j];
        DEBUG_FUNCTION_LINE("Saving function \"%s\" of plugin \"%s\". PA:%08X VA:%08X Library: %08X, target: %08X, call_addr: %08X\n",cur_function->_function.name,pluginData->getPluginInformation()->getName().c_str(),cur_function->_function.physical_address,cur_function->_function.virtual_address, cur_function->_function.library,cur_function->_function.target, (void *) cur_function->_function.call_addr);
        FunctionData * function_data = new FunctionData((void *) cur_function->_function.physical_address,(void *) cur_function->_function.virtual_address, cur_function->_function.name, cur_function->_function.library, (void *) cur_function->_function.target, (void *) cur_function->_function.call_addr);
        pluginData->addFunctionData(function_data);
    }

    this->setCurrentStoreAddress((void *) curAddress);

    result = true;
exit_error:
    if (!result) {
        DEBUG_FUNCTION_LINE("exit_error\n");
    }
    if (destinations != NULL) {
        free(destinations);
    }
    if (symtab != NULL) {
        free(symtab);
    }
    if (hooks != NULL) {
        free(hooks);
    }
    if (entries != NULL) {
        free(entries);
    }
    return result;
}

void PluginLoader::copyPluginDataIntoGlobalStruct(std::vector<PluginData *> plugins) {
    // Reset data
    memset((void*)&gbl_replacement_data,0,sizeof(gbl_replacement_data));
    DynamicLinkingHelper::getInstance()->clearAll();
    int32_t plugin_index = 0;
    // Copy data to global struct.
    for(size_t i = 0; i< plugins.size(); i++) {
        PluginData * cur_plugin = plugins.at(i);
        PluginInformation * cur_pluginInformation = cur_plugin->getPluginInformation();

        // Relocation
        std::vector<RelocationData *> relocationData = cur_plugin->getRelocationDataList();
        for(size_t j = 0; j < relocationData.size(); j++) {
            if(!DynamicLinkingHelper::getInstance()->addReloationEntry(relocationData[j])) {
                DEBUG_FUNCTION_LINE("Adding relocation for %s failed. It won't be loaded.\n",cur_pluginInformation->getName().c_str());
                continue;
            } else {
                //relocationData[j]->printInformation();
            }
        }

        // Other
        std::vector<FunctionData *> function_data_list = cur_plugin->getFunctionDataList();
        std::vector<HookData *> hook_data_list = cur_plugin->getHookDataList();
        if(plugin_index >= MAXIMUM_PLUGINS ) {
            DEBUG_FUNCTION_LINE("Maximum of %d plugins reached. %s won't be loaded!\n",MAXIMUM_PLUGINS,cur_pluginInformation->getName().c_str());
            continue;
        }
        if(function_data_list.size() > MAXIMUM_FUNCTION_PER_PLUGIN) {
            DEBUG_FUNCTION_LINE("Plugin %s would replace to many function (%d, maximum is %d). It won't be loaded.\n",cur_pluginInformation->getName().c_str(),function_data_list.size(),MAXIMUM_FUNCTION_PER_PLUGIN);
            continue;
        }
        if(hook_data_list.size() > MAXIMUM_HOOKS_PER_PLUGIN) {
            DEBUG_FUNCTION_LINE("Plugin %s would set too many hooks (%d, maximum is %d). It won't be loaded.\n",cur_pluginInformation->getName().c_str(),hook_data_list.size(),MAXIMUM_HOOKS_PER_PLUGIN);
            continue;
        }

        replacement_data_plugin_t * plugin_data = &gbl_replacement_data.plugin_data[plugin_index];

#warning TODO: add GUI option to let the user choose
        plugin_data->kernel_allowed = true;
        plugin_data->kernel_init_done = false;

        plugin_data->bssAddr = cur_plugin->getBSSAddr();
        plugin_data->bssSize = cur_plugin->getBSSSize();
        plugin_data->sbssAddr = cur_plugin->getSBSSAddr();
        plugin_data->sbssSize = cur_plugin->getSBSSSize();

        strncpy(plugin_data->plugin_name,cur_pluginInformation->getName().c_str(),MAXIMUM_PLUGIN_NAME_LENGTH-1);
        strncpy(plugin_data->path,cur_pluginInformation->getPath().c_str(),MAXIMUM_PLUGIN_PATH_NAME_LENGTH-1);

        for(size_t j = 0; j < function_data_list.size(); j++) {
            replacement_data_function_t * function_data = &plugin_data->functions[j];
            FunctionData * cur_function = function_data_list[j];

            if(strlen(cur_function->getName().c_str()) > MAXIMUM_FUNCTION_NAME_LENGTH-1) {
                DEBUG_FUNCTION_LINE("Couldn not add function \"%s\" for plugin \"%s\" function name is too long.\n",cur_function->getName().c_str(),plugin_data->plugin_name);
                continue;
            }

            DEBUG_FUNCTION_LINE("Adding function \"%s\" for plugin \"%s\"\n",cur_function->getName().c_str(),plugin_data->plugin_name);

            //TODO: Warning/Error if string is too long.

            strncpy(function_data->function_name,cur_function->getName().c_str(),MAXIMUM_FUNCTION_NAME_LENGTH-1);

            function_data->library = cur_function->getLibrary();
            function_data->replaceAddr = (uint32_t) cur_function->getReplaceAddress();
            function_data->replaceCall = (uint32_t) cur_function->getReplaceCall();
            function_data->physicalAddr = (uint32_t) cur_function->getPhysicalAddress();
            function_data->virtualAddr = (uint32_t) cur_function->getVirtualAddress();

            plugin_data->number_used_functions++;
        }

        DEBUG_FUNCTION_LINE("Entries for plugin \"%s\": %d\n",plugin_data->plugin_name,plugin_data->number_used_functions);

        for(size_t j = 0; j < hook_data_list.size(); j++) {
            replacement_data_hook_t * hook_data = &plugin_data->hooks[j];

            HookData * hook_entry = hook_data_list[j];

            DEBUG_FUNCTION_LINE("Set hook for plugin \"%s\" of type %08X to target %08X\n",plugin_data->plugin_name,hook_entry->getType(),(void*) hook_entry->getFunctionPointer());
            hook_data->func_pointer = (void*) hook_entry->getFunctionPointer();
            hook_data->type         = hook_entry->getType();
            plugin_data->number_used_hooks++;
        }

        DEBUG_FUNCTION_LINE("Hooks for plugin \"%s\": %d\n",plugin_data->plugin_name,plugin_data->number_used_hooks);

        plugin_index++;
        gbl_replacement_data.number_used_plugins++;
    }
    DCFlushRange((void*)&gbl_replacement_data,sizeof(gbl_replacement_data));
    ICInvalidateRange((void*)&gbl_replacement_data,sizeof(gbl_replacement_data));
}

uint32_t PluginLoader::getMemoryFromDataSection(size_t align, size_t size) {
    uint32_t ptr = (uint32_t)gbl_common_data_ptr;
    ptr = (ptr + (align - 1)) & -align;  // Round up to align boundary
    uint32_t result = ptr;

    if((result + size) >= (ptr + sizeof(gbl_common_data))) {
        DEBUG_FUNCTION_LINE("No more space =( %08X > %08X\n",(result + size),(ptr + sizeof(gbl_common_data)));
        return 0;
    }
    ptr += size;
    gbl_common_data_ptr = (char *) ptr;

    return result;
}
