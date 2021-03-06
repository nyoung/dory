/* <server/unix_stream_server.h>

   ----------------------------------------------------------------------------
   Copyright 2016 Dave Peterson <dave@dspeterson.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   ----------------------------------------------------------------------------

   Class for server that uses UNIX domain stream sockets for communication with
   clients.
 */

#pragma once

#include <cassert>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <base/no_copy_semantics.h>
#include <base/opt.h>
#include <server/stream_server_base.h>

namespace Server {

  /* This may be either instantiated directly or used as a base class. */
  class TUnixStreamServer : public TStreamServerBase {
    NO_COPY_SEMANTICS(TUnixStreamServer);

    public:
    TUnixStreamServer(int backlog, const char *path,
        TConnectionHandlerApi *connection_handler,
        const TFatalErrorHandler &fatal_error_handler);

    TUnixStreamServer(int backlog, const std::string &path,
        TConnectionHandlerApi *connection_handler,
        const TFatalErrorHandler &fatal_error_handler)
        : TUnixStreamServer(backlog, path.c_str(), connection_handler,
              fatal_error_handler) {
    }

    TUnixStreamServer(int backlog, const char *path,
        TConnectionHandlerApi *connection_handler,
        TFatalErrorHandler &&fatal_error_handler);

    TUnixStreamServer(int backlog, const std::string &path,
        TConnectionHandlerApi *connection_handler,
        TFatalErrorHandler &&fatal_error_handler)
        : TUnixStreamServer(backlog, path.c_str(), connection_handler,
              std::move(fatal_error_handler)) {
    }

    virtual ~TUnixStreamServer() noexcept {
    }

    const std::string &GetPath() const noexcept {
      assert(this);
      return Path;
    }

    const struct sockaddr_un &GetClientAddr() const noexcept {
      assert(this);
      return ClientAddr;
    }

    /* Specify a value to chmod() the socket file to the next time it is
       created.  If unspecified, the umask determines the permission bits. */
    void SetMode(mode_t mode);

    /* Specify that the next time the socket file is created, its mode will be
       determined by the umask.  This is the default behavior if SetMode() has
       not been called. */
    void ClearMode() {
      assert(this);
      Mode.Reset();
    }

    protected:
    virtual void InitListeningSocket(Base::TFd &sock) override;

    virtual void CloseListeningSocket(Base::TFd &sock) override;

    private:
    void UnlinkPath();

    const std::string Path;

    Base::TOpt<mode_t> Mode;

    struct sockaddr_un ClientAddr;
  };  // TUnixStreamServer

}  // Server
