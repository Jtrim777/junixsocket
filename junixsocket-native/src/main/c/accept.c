/*
 * junixsocket
 *
 * Copyright 2009-2021 Christian Kohlschütter
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include "accept.h"

#include "exceptions.h"
#include "filedescriptors.h"
#include "address.h"
#include "polling.h"

/*
 * Class:     org_newsclub_net_unix_NativeUnixSocket
 * Method:    accept
 * Signature: ([BLjava/io/FileDescriptor;Ljava/io/FileDescriptor;JI)Z
 */
JNIEXPORT jboolean JNICALL Java_org_newsclub_net_unix_NativeUnixSocket_accept(
                                                                          JNIEnv * env, jclass clazz CK_UNUSED, jbyteArray addr, jobject fdServer,
                                                                          jobject fd, jlong expectedInode, int timeout)
{
    CK_ARGUMENT_POTENTIALLY_UNUSED(timeout);

    struct sockaddr_un su;
    socklen_t suLength = initSu(env, &su, addr);
    if(suLength == 0) return false;

    int serverHandle = _getFD(env, fdServer);

    if(expectedInode > 0) {
        struct stat fdStat;

        // It's OK when the file's gone, but not OK if it refers to another inode.
        int statRes = stat(su.sun_path, &fdStat);
        if(statRes == 0) {
            ino_t statInode = fdStat.st_ino;

            if(statInode != (ino_t)expectedInode) {
                // inode mismatch -> someone else took over this socket address
                _closeFd(env, fdServer, serverHandle);
                _throwErrnumException(env,
                                      ECONNABORTED, NULL);
                return false;
            }
        }
    }

#if defined(junixsocket_use_poll_for_accept)
    {
        int ret = pollWithTimeout(env, fdServer, serverHandle, timeout);
        if(ret == 0) {
            _throwErrnumException(env, ETIMEDOUT, fdServer);
            return false;
        } else if(ret < 0) {
            return false;
        }
    }
#endif

    int socketHandle;
    int errnum = 0;
    do {
#if defined(junixsocket_have_accept4)
        socketHandle = accept4(serverHandle, (struct sockaddr *)&su, &suLength, SOCK_CLOEXEC);
        if(socketHandle == -1 && errno == ENOSYS) {
            socketHandle = accept(serverHandle, (struct sockaddr *)&su, &suLength);
        }
#else
        socketHandle = accept(serverHandle, (struct sockaddr *)&su, &suLength);
#endif
    } while(socketHandle == -1 && (errnum = socket_errno) == EINTR);
    
    if(socketHandle == -1) {
        if(checkNonBlocking(serverHandle, errnum)) {
            // non-blocking socket, nothing to accept
        } else {
            _throwErrnumException(env, errnum, fdServer);
        }
        return false;
    }

#if !defined(junixsocket_have_accept4)
#  if defined(FD_CLOEXEC)
    fcntl(socketHandle, F_SETFD, FD_CLOEXEC);
#  elif defined(_WIN32)
    HANDLE h = (HANDLE)_get_osfhandle(socketHandle);
    SetHandleInformation(h, HANDLE_FLAG_INHERIT, 0);
#  endif
#endif

    _initFD(env, fd, socketHandle);
    return true;
}
