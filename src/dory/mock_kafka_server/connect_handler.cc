/* <dory/mock_kafka_server/connect_handler.cc>

   ----------------------------------------------------------------------------
   Copyright 2013-2014 if(we)

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

   Implements <dory/mock_kafka_server/connect_handler.h>.
 */

#include <dory/mock_kafka_server/connect_handler.h>

#include <algorithm>
#include <cassert>
#include <memory>

#include <base/debug_log.h>
#include <socket/address.h>

using namespace Base;
using namespace Dory;
using namespace Dory::MockKafkaServer;
using namespace Socket;

void TConnectHandler::OnEvent(int fd, short /*flags*/) {
  assert(this);
  TAddress client_address;
  TFd client_socket(IfLt0(Accept(fd, client_address)));
  RunThread(ClientHandlerFactory.CreateClientHandler(
      PortMap, PortOffset, Ss, std::move(client_socket)));
}
