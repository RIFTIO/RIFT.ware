/* STANDARD_RIFT_IO_COPYRIGHT */

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
