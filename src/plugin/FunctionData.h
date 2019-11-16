/****************************************************************************
 * Copyright (C) 2018 Maschell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#ifndef _FUNCTION_DATA_H_
#define _FUNCTION_DATA_H_

#include <wups.h>
#include <string>

class FunctionData {

public:
    FunctionData(void * paddress, void * vaddress, const char * name, wups_loader_library_type_t library, void * target, void * call_addr) {
        this->paddress = paddress;
        this->vaddress = vaddress;
        this->name = name;
        this->library = library;
        this->replaceAddr = target;
        this->replaceCall = call_addr;
    }

    ~FunctionData() {

    }

    std::string getName() {
        return this->name;
    }

    wups_loader_library_type_t getLibrary() {
        return this->library;
    }

    void * getPhysicalAddress() {
        return paddress;
    }
    void * getVirtualAddress() {
        return vaddress;
    }

    void * getReplaceAddress() {
        return replaceAddr;
    }

    void * getReplaceCall() {
        return replaceCall;
    }

private:
    void * paddress = NULL;
    void * vaddress = NULL;
    std::string name;
    wups_loader_library_type_t library;
    void * replaceAddr = NULL;
    void * replaceCall = NULL;
};


#endif
