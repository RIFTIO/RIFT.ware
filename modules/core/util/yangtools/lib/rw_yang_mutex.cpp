
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */



/*!
 * @file rw_yang_mutex.cpp
 *
 * Global mutex support.
 */

#include <rw_yang_mutex.hpp>


namespace rw_yang {
namespace GlobalMutex {

/*
 * mutex_t has constexpr default constructor and so safe from the
 * "static initialization order fiasco".
 */
mutex_t g_mutex;

}
}
